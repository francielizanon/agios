/*! \file req_hashtable.c
    \brief Implementation of the hashtable, used to store information about files and request queues for some scheduling algorithms.

    The hashtable has AGIOS_HASH_ENTRIES lines. Files are positioned in the hashtable according to their handles, each line has a collision list ordered by file handle. File structures hold information and statistics about access separated in two queues (write and read). Requests may or may not be in these queues (depending on the scheduling algorithm being used requests may be adde to the timeline). However, requests that were sent back to the user will always be in the dispatch queues of their files (in the hashtable) so they can be easily found. When adding requests to the hashtable, each line uses its own mutex to favor parallelism. However, if requests are being added to the timeline, then a single mutex (the timeline mutex) is used to access the whole hashtable. That was done to prevent deadlocks.
    @see hash.c
    @see req_timeline.c
 */
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "agios.h"
#include "agios_add_request.h"
#include "agios_request.h"
#include "common_functions.h"
#include "hash.h"
#include "mylist.h"
#include "req_hashtable.h"

struct agios_list_head *hashlist;  /**< the hashtable. */
int32_t *hashlist_reqcounter = NULL; /**< how many requests are present in each position from the hashtable (used to speed the search for requests in the scheduling algorithms). */
static pthread_mutex_t *hashlist_locks; /**< one mutex per line of the hashtable. */

/**
 * function called at the beginning of the execution. It initializes data structures and locks. 
 * @return true or false for success. 
 */
bool hashtable_init(void)
{
	//allocate memory
	hashlist = (struct agios_list_head *) malloc(sizeof(struct agios_list_head) * AGIOS_HASH_ENTRIES);
	if (!hashlist) {
		agios_print("AGIOS: cannot allocate memory for req cache\n");
		return false;
	}
	hashlist_locks = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t)*AGIOS_HASH_ENTRIES);
	if (!hashlist_locks) {
		agios_print("AGIOS: cannot allocate memory for req locks\n");
		free(hashlist);
		return false;
	}
	hashlist_reqcounter = (int32_t *)malloc(sizeof(int32_t)*AGIOS_HASH_ENTRIES);
	if (!hashlist_reqcounter) {
		agios_print("AGIOS: cannot allocate memory for req counters\n");
		free(hashlist);
		free(hashlist_locks);
		return false;
	}
	//initialize structures
	for (int32_t i = 0; i < AGIOS_HASH_ENTRIES; i++) {
		init_agios_list_head(&hashlist[i]);
		pthread_mutex_init(&(hashlist_locks[i]), NULL);
		hashlist_reqcounter[i]=0;
	}
	return true;
}
/**
 * funtion called while freeing structures from the hashtable. It cleans up a queue (queue_t) by freeing all requests in its regular and dispatch lists. It does NOT frees the queue_t itself.
 * @param queue the queue to be freed.
 */
void queue_cleanup(struct queue_t *queue)
{
	list_of_requests_cleanup(&queue->list);
	list_of_requests_cleanup(&queue->dispatch);
}
/**
 * called at the end of the execution to clean up the hashtable structures.
 */
void hashtable_cleanup(void)
{
	struct file_t *req_file; /**< used to iterate over all files in a hashtable line. */
	struct file_t *aux_req_file=NULL; /**< used to avoid freeing a file structure before moving the iterator to the next one, otherwise the loop breaks. */

	if (hashlist) {
		for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) { //go through all lines of the hashtable
			if (!agios_list_empty(&hashlist[i])) {
				agios_list_for_each_entry (req_file, &hashlist[i], hashlist) { //go through all file structures
					//go through all requests in the related lists
					queue_cleanup(&req_file->read_queue);
					queue_cleanup(&req_file->write_queue);
					if (req_file->file_id) free(req_file->file_id);
					if (aux_req_file) {
						agios_list_del(&aux_req_file->hashlist);
						free(aux_req_file);
					}
					aux_req_file = req_file;
				} 
				if (aux_req_file) {
					agios_list_del(&aux_req_file->hashlist);
					free(aux_req_file);
					aux_req_file = NULL;
				}
			}
		}
		free(hashlist);
	}
	if (hashlist_locks) free(hashlist_locks);
	if (hashlist_reqcounter) free(hashlist_reqcounter);
}
/**
 * called to add a request to the hashtable. The caller must hold the mutex for the relevant line of the hashtable.
 * @param req the newly arrived request.
 * @param hash_val the line of the hashtable where the file accessed by this request belongs.
 * @param given_req_file to be provided ONLY when using this function to migrate from timeline to hashtable. In that case, it is the file structure.
 * @return true or false for success.
 */ 
bool hashtable_add_req(struct request_t *req, 
			int32_t hash_val, 
			struct file_t *given_req_file)
{
	struct agios_list_head *queue; /**< will receive the queue where the request is to be added (read or write) */
	struct file_t *req_file = given_req_file; /**< the file that is being accessed by this request. */
	struct request_t *tmp; /**< used to find the insertion place for this request. */
	struct agios_list_head *insertion_place; /**< used to find the insertion place for this request. */

	debug("adding request to file %s, offset %ld, size %ld", req->file_id, req->offset, req->len);
	/*finds the file to add to*/
	if (!req_file) { //a file structure was not provided, we have to find/create it and update statistics
		req_file = find_req_file(&hashlist[hash_val], req->file_id);
		if (!req_file) return false;
		/*if it is the first request to this file, we have to store its arrival time. */ 
		if (req_file->first_request_time == 0) req_file->first_request_time = req->arrival_time;
	}
	//choose the appropriate list to add the request
	if (req->type == RT_READ) {
		queue = &req_file->read_queue.list;
		req->globalinfo = &req_file->read_queue;
	} else {
		queue = &req_file->write_queue.list;
		req->globalinfo = &req_file->write_queue;
	}
	/* search for the position in the offset-sorted list. */ 
	insertion_place = queue;
	if (!agios_list_empty(queue)) {
		agios_list_for_each_entry (tmp, queue, related) {
			if ((tmp->offset > req->offset) ||
			    ((tmp->offset == req->offset) &&
			    (tmp->len > req->len))) {
				insertion_place = &(tmp->related);
				break;
			}
		}
	}
	//try to aggregate the request with the neighboors. If it is not possible, just add it in the place we found for it.
	if(!insert_aggregations(req, insertion_place->prev, queue))
		agios_list_add(&req->related, insertion_place->prev);
	return true;
}
/**
 * function called to safely remove a request from the hashtable. The caller must NOT be holding the mutex for the line of the hashtable, as this function will lock and unlock it before touching the request.
 * @param req the request that is being removed.
 */ 
void hashtable_safely_del_req(struct request_t *req)
{
	int32_t hash = get_hashtable_position(req->file_id);
	pthread_mutex_lock(&hashlist_locks[hash]);
	agios_list_del(&req->related);
	pthread_mutex_unlock(&hashlist_locks[hash]);
}
/**
 * alternative to hastable_safely_del_req to remove a request from the hashtable when we ARE holding the mutex to the relevant line.
 * @param req the request to the removed.
 */
void hashtable_del_req(struct request_t *req)
{
	agios_list_del(&req->related);
}
/**
 * function used to acquire the lock to a line of the hashtable.
 * @param index the line of the hashtable we want to access.
 * @return a pointer to the line of the hashtable, which can now be safely accessed. 
 */
struct agios_list_head *hashtable_lock(int32_t index)
{
	assert((index >= 0) && (index < AGIOS_HASH_ENTRIES));
	pthread_mutex_lock(&hashlist_locks[index]);
	return &hashlist[index];
}
/**
 * function used to TRY to acquire the lock to a line of the hashtable. It is not going to wait. 
 * @param index the line of the hashtable we want to access.
 * @return the line of the hashtable if the lock was successfully acquired, NULL otherwise.
 */
struct agios_list_head *hashtable_trylock(int32_t index)
{
	if (0 == pthread_mutex_trylock(&hashlist_locks[index])) return &hashlist[index];
	else return NULL;
}
/**
 * function used to free the lock on a line of the hashtable.
 * @param index the line of the hashtable we want to access.
 */
void hashtable_unlock(int32_t index)
{
	pthread_mutex_unlock(&hashlist_locks[index]);
}
/**
 * prints all contents of a line of the hashtable. Used for debug purposes. The caller must hold the mutex for this line.
 * @param i the line of the hashtable to be printed.
 */
void print_hashtable_line(int32_t i)
{
#if AGIOS_DEBUG
	struct agios_list_head *hash_list; /**< to access the line of the hashtable. */
	struct file_t *req_file; /**< to iterate over all files in the line of the hashtable. */
	struct request_t *req; /**< to iterate over all requests to each file in the line of the hashtable. */

	hash_list = &hashlist[i];
	if (!agios_list_empty(hash_list)) debug("[%d]", i);
	agios_list_for_each_entry (req_file, hash_list, hashlist) { //go over all files
		debug("\t%s", req_file->file_id);
		if (!(agios_list_empty(&req_file->read_queue.list) && 
		      agios_list_empty(&req_file->read_queue.dispatch))) {
			debug("\t\tread");
			agios_list_for_each_entry (req, &req_file->read_queue.list, related) print_request(req);
			debug("\t\tdispatch read");
			agios_list_for_each_entry (req, &req_file->read_queue.dispatch, related) print_request(req);
		}
		if (!(agios_list_empty(&req_file->write_queue.list) && 
		      agios_list_empty(&req_file->write_queue.dispatch))) {
			debug("\t\twrite");
			agios_list_for_each_entry (req, &req_file->write_queue.list, related) print_request(req);
			debug("\t\tdispatch writes");
			agios_list_for_each_entry (req, &req_file->write_queue.dispatch, related) print_request(req);
		}
	}
#endif
}
/** 
 * prints all contents of the hashtable. Used for debug purposes. The user must hold relevant mutexes.
 */
void print_hashtable(void)
{
	debug("Current hashtable status:");
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) { //go through the whole hashtable, one position at a time
		print_hashtable_line(i);
	}
	PRINT_FUNCTION_EXIT;
}
