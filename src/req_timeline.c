/*! \file req_timeline.c
    \brief Implementation of the timeline, used as request queue to some scheduling algorithms.

    There is a timeline (queue) of requests, protected with a single mutex. The insertion order in this queue is usually FIFO, but may differ depending on the used scheduling algorithm. If a max_queue_id was provided to agios_init, the initialization function will also allocate the multi_timeline, a list of max_queue_id+1 request queues, which is used by some scheduling algorithms (for now just TWINS) and is also protected by the same timeline_mutex.
 */
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "agios.h"
#include "agios_add_request.h"
#include "agios_config.h"
#include "agios_request.h"
#include "common_functions.h"
#include "hash.h"
#include "mylist.h"
#include "req_hashtable.h"
#include "scheduling_algorithms.h"

AGIOS_LIST_HEAD(timeline); /**< the request queue. */ 
struct agios_list_head *multi_timeline; /**< multiple request queues, indexed by the queue_id provided by the user with each request to agios_add_request. This structure is used by TWINS. */
int32_t multi_timeline_size=0; /**< number of queues in multi_timeline. */
static pthread_mutex_t timeline_mutex = PTHREAD_MUTEX_INITIALIZER; /**< a lock to access all timeline structures. */

/**
 * called to acquire the timeline lock. It will wait for the lock.
 * @return a pointer to the timeline.
 */
struct agios_list_head *timeline_lock(void)
{
	pthread_mutex_lock(&timeline_mutex);
	return &timeline;
}
/**
 * called to unlock the timeline mutex. 
 */
void timeline_unlock(void)
{
	pthread_mutex_unlock(&timeline_mutex);
}
/**
 * function used by timeline_add_request to add a request to the timeline. This is a separated function because when migrating to or from SW or TWINS we need to completely reorder the timeline, so we'll add requests to a temporary timeline in the process. 
 * @see reorder_timeline
 * @param req @see timeline_add_req
 * @param hash @see timeline_add_req
 * @param given_req_file @see timeline_add_req
 * @param this_timeline a link to the timeline where we should add the request.
 * @return true or false for success.
 */
bool __timeline_add_req(struct request_t *req, 
			int32_t hash, 
			struct file_t *given_req_file, 
			struct agios_list_head *this_timeline)
{
	struct file_t *req_file = given_req_file; /**< used to find the structure holding information about the file being accessed. */
	struct request_t *tmp; /**< used to iterate over the timeline to find the insertion place for the request (depending on the scheduling algorithm being used). */
	int32_t sw_priority; /**< a value that will define the position in the timeline when using the SW scheduling algorithm. */
	struct agios_list_head *insertion_place; /**< the insertion place of the new request in the queue. */

	if (!req_file) { //if a req_file structure has been given, we are actually migrating from hashtable to timeline and will copy the file_t structures, so no need to create new. Also the request pointers are already set, and we don't need to use locks here
		debug("adding request %ld %ld to file %s, app_id %u", req->offset, req->len, req->file_id, req->queue_id);	
		/*find the file and update its informations if needed*/
		req_file = find_req_file(&hashlist[hash], req->file_id); //we store file information in the hashtable 
		if (!req_file) return false;
		if (req_file->first_request_time == 0) req_file->first_request_time = req->arrival_time;
		if (req->type == RT_READ) req->globalinfo = &req_file->read_queue;
		else req->globalinfo = &req_file->write_queue;
		if (current_alg == NOOP_SCHEDULER) return true; //we don't really include requests when using the NOOP scheduler, we just go through this function because we want file_t  structures for statistics
	}
	//the SW scheduling algorithm separates requests into windows
	if (current_alg == SW_SCHEDULER) {
		// Calculate the request priority
		sw_priority = ((req->arrival_time / config_sw_size) * 32768) + req->queue_id; //we assume 32768 here is the maximum value app_id could ever assume (not max_queue_id, but the maximum max_queue_id the user could ever give us). This is an ugly hardcoded value.
		// Find the position to insert the request
		agios_list_for_each_entry (tmp, this_timeline, related) { //go through all requests in the queue
			if (tmp->sw_priority > sw_priority) {
				agios_list_add(&req->related, tmp->related.prev);
				return true;
			}
		}
		// If it was not inserted in the middle of the queue, insert the request in the proper position (the end of the queue)
		agios_list_add_tail(&req->related, this_timeline);
		return true;
	} 
	if (current_alg == TWINS_SCHEDULER) {
		agios_list_add_tail(&req->related, &(multi_timeline[req->queue_id]));
		return true;
	}
	//the TO-agg scheduling algorithm searches the queue for contiguous requests. If it finds any, then aggregate them.	
	if ((current_alg == TOAGG_SCHEDULER) && (current_scheduler->max_aggreg_size > 1)) {	
		agios_list_for_each_entry (tmp, this_timeline, related) { //go through all requests in the queue
			if (tmp->globalinfo == req->globalinfo) { //same type and to the same file
				if (tmp->reqnb < current_scheduler->max_aggreg_size) { //if the virtual request can hold another one
					if (CHECK_AGGREGATE(req,tmp) || CHECK_AGGREGATE(tmp, req)) { //and they are contiguous
						include_in_aggregation(req, &tmp);
						return true;
					}
				} 
			}
		} //end loop going over all requests 
	}
	//if we are here it means the request still has to be inserted in the queue
	if ((!given_req_file) || (current_alg == NOOP_SCHEDULER))  { //if no file_t structure was given, this is a regular new request, so we simply add it to the end of the timeline. If we are here and are using NOOP, it means we are migrating between data structures (because otherwise we would have left the function earlier), and in that case we don't do much regarding ordering because we are going to schedule these requests as soon as possible and we don't care (NOOP does not usually have a queue, this is a special case). */
		debug("request is not aggregated, inserting in the timeline");
		agios_list_add_tail(&req->related, this_timeline); 
	}
	else { //we are rebuilding this queue from the hashtable, so we need to make sure requests are ordered by time (and we are not using the time window algorithm, otherwise it would have called return already (see above)
		debug("request not aggregated and we are reordering the timeline, so looking for its place");
		insertion_place = this_timeline;
		if (!agios_list_empty(this_timeline)) {
			agios_list_for_each_entry (tmp, this_timeline, related) {
				if (tmp->timestamp > req->timestamp) {
					insertion_place = &(tmp->related);
					break;	
				}
			}
		}
		agios_list_add(&req->related, insertion_place->prev);
	}
	return true;
}
/**
 * function called to add a request to the timeline. The caller must hold the timeline mutex before this call. 
 * @param req the new request being added.
 * @param hash the line of the hashtable containing information about the file being accessed.
 * @param given_req_file the information about the file being accessed or NULL if unknown. It is important to notice that: when called by agios_add_request, given_req_file will be NULL. It will only have a different value when this function is being used to migrate between data structures.
 * @return true or false for success.
 */
bool timeline_add_req(struct request_t *req, int32_t hash, struct file_t *given_req_file)
{
	return __timeline_add_req(req, hash, given_req_file, &timeline);
}
/** 
 * This function is called when migrating between two scheduling algorithms when both use timeline and one of them is the TIME_WINDOW or TWINS. In this case, it is necessary to redo the timeline so requests will be processed in the new relevant order.
 */
void reorder_timeline(void)
{
	struct agios_list_head *new_timeline; /**< a temporary timeline used while we are migrating. */
	struct request_t *req; /**< used to iterate over all requests of the queue. */
 	struct request_t *aux_req=NULL;  /**< used to avoid moving a request before moving the iterator to the next one, otherwise the loop breaks. */
	int32_t hash; /**< line of the hashtable where information about a file is. */

	//initialize new timeline structure
	new_timeline = (struct agios_list_head *)malloc(sizeof(struct agios_list_head));
	new_timeline->prev = new_timeline;
	new_timeline->next = new_timeline;
	//get all requests from the previous timeline and include in the new one
	agios_list_for_each_entry (req, &timeline, related) {
		if (aux_req) {
			hash = get_hashtable_position(aux_req->file_id);
			agios_list_del(&aux_req->related);
			__timeline_add_req(aux_req, hash, aux_req->globalinfo->req_file, new_timeline);	
		}
		aux_req = req;
	}	
	if (aux_req) {
		hash = get_hashtable_position(aux_req->file_id);
		agios_list_del(&aux_req->related);
		__timeline_add_req(aux_req, hash, aux_req->globalinfo->req_file, new_timeline);	
	}
	//redefine the pointers and replace the old timeline by the new one
	new_timeline->prev->next = &timeline;
	new_timeline->next->prev = &timeline;
	timeline.next = new_timeline->next;
	timeline.prev = new_timeline->prev;
	free(new_timeline);
}
/**
 * removes the first request from the queue and also calculates its hash. The caller must hold the timeline lock before calling this.
 * @param hash the value that will be updated in this function to hold the line of the hashtable with information about the file that is accessed by the returned request.
 * @return the first request from the queue, removed from it, NULL if the queue is empty. 
 */
struct request_t *timeline_oldest_req(int32_t *hash)
{
	struct request_t *tmp; /**< the request that will be returned. */

	if (agios_list_empty(&timeline)) return NULL;
	tmp = agios_list_entry(timeline.next, struct request_t, related);
	agios_list_del(&tmp->related);
	*hash = get_hashtable_position(tmp->file_id);
	return tmp;
}
/**
 * Initializes data structures used for the timeline, the multi_timeline and the lock. 
 * @param max_queue_id the number of queues in multi_timeline. It is only relevant for TWINS. Pass 0 otherwise to prevent unnecessary memory allocation.
 * @return true or false for success. 
 */
bool timeline_init(int32_t max_queue_id)
{
	init_agios_list_head(&timeline);
	if (max_queue_id > 0) {
		multi_timeline = (struct agios_list_head *) malloc(sizeof(struct agios_list_head)*(max_queue_id+1));
		multi_timeline_size = max_queue_id+1;
		if (!multi_timeline) {
			agios_print("PANIC! No memory to allocate the app timeline for TWINS");
			return false;
		}
		for (int32_t i=0; i< multi_timeline_size; i++) {
			init_agios_list_head(&(multi_timeline[i]));
		}
	}
	return true;
}
/**
 * called at the end of the execution to free allocated data structures.
 */
void timeline_cleanup(void)
{
	list_of_requests_cleanup(&timeline);
	if (multi_timeline_size > 0) {
		for(int32_t i=0; i< multi_timeline_size; i++)
			list_of_requests_cleanup(&multi_timeline[i]);
		free(multi_timeline);
	}
}
/**
 * prints all requests in the timeline, used for debug.
 */
void print_timeline(void)
{
#if AGIOS_DEBUG
	struct request_t *req;
	debug("Current timeline status:");
	debug("Requests:");
	agios_list_for_each_entry (req, &timeline, related) {
		print_request(req);
	}
#endif
}
