/*! \file data_structures.c
    \brief Functions to initialize and migrate between data structures.

    @see agios_request.h
    @see req_hashtable.c
    @see req_timeline.c
    @see agios_add_request.c 
    Depending on the scheduling algorithm being used, requests will be organized in different data structures. For instance, aIOLi and SJF use a hashtable, TO and TO-agg use a timeline, and TWINS uses multiple timelines. No matter the data structure used to hold the requests, AGIOS will always maintain the hashtable, because it is used for the statistics. 
*/

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "agios_counters.h"
#include "agios_request.h"
#include "common_functions.h"
#include "hash.h"
#include "mylist.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"
#include "statistics.h"

void put_all_requests_in_timeline(struct agios_list_head *queue, struct file_t *req_file, int32_t hash);
void put_all_requests_in_hashtable(struct agios_list_head *list);

/**
 * function called to move a request from the hashtable to the timeline. If the request is a virtual one (composed of multiple actual requests) and the new scheduling algorithm does not allow aggregations, the request will be separated and all its parts will be added to the timeline.
 * @param req the request to be moved.
 * @param hash the line of the hashtable from where this request came.
 * @param req_file the file accessed by this request.
 */
void put_this_request_in_timeline(struct request_t *req, 
					int32_t hash, 
					struct file_t *req_file)
{
	//remove from queue
	agios_list_del(&req->related);
	if ((req->reqnb > 1) && (current_scheduler->max_aggreg_size <= 1)) {
		//this is a virtual request, we need to break it into parts
		put_all_requests_in_timeline(&req->reqs_list, req_file, hash);
		//the parts were added to the timeline, the "super-request" has to be freed
		if (req->file_id) free(req->file_id);
		free(req);
	}
	else timeline_add_req(req, hash, req_file); //put in timeline

}
/** 
 * function called to move all requests from a list of requests (either a queue from the hashtable or an aggregated request) to the timeline.
 * @see put_this_request_in_timeline
 * @param queue the list of requests.
 * @param req_file the file from which this list came from.
 * @param hash the line of the hashtable from which this list came from.
 */
void put_all_requests_in_timeline(struct agios_list_head *queue, 
					struct file_t *req_file, 
					int32_t hash)
{
	struct request_t *req; /**< used to iterate over all requests in the list */
	struct request_t *aux_req=NULL; /**< used so we don't move and free the request before moving the iterator to the next one, otherwise the loop breaks. */

	agios_list_for_each_entry (req, queue, related) { //go through all requests
		if (aux_req) put_this_request_in_timeline(aux_req, hash, req_file);
		aux_req = req;
	}
	if (aux_req) put_this_request_in_timeline(aux_req, hash, req_file);
}
/** 
 * function used to move a request from the timeline to the hashtable. If this request is a virtual one (composed of multiple actual requests) and the new scheduling algorithm does not allow aggregations, the request will be separated and all its sub-requests will be added to the hashtable separately.
 * @param req the request.
 */
void put_req_in_hashtable(struct request_t *req)
{
	int32_t hash = get_hashtable_position(req->file_id); /**< the line of the hashtable corresponding to this request's file */

	//remove the request from the timeline
	agios_list_del(&req->related);
	if ((req->reqnb > 1) && (current_scheduler->max_aggreg_size <= 1)) {
		put_all_requests_in_hashtable(&req->reqs_list);
		//free the virtual request (which used to have many sub-requests but that is now empty)
		if (req->file_id) free(req->file_id);
		free(req);
	} else hashtable_add_req(req, hash, req->globalinfo->req_file);
}
/**
 * function used to move a list of requests from the timeline to the hashtable.
 * @see put_req_in_hashtable
 * @param list the list of requests.
 */
void put_all_requests_in_hashtable(struct agios_list_head *list)
{
	struct request_t *req; /**< used to iterate over all requests in the list */
	struct request_t *aux_req=NULL; /**< used to avoid moving and freeing the request before moving the iterator to the next one, otherwise the loop breaks. */

	agios_list_for_each_entry (req, list, related) { //go over all requests
		if (aux_req) put_req_in_hashtable(aux_req);
		aux_req = req;
	}
	if (aux_req) put_req_in_hashtable(aux_req);
}
/** 
 * This function gets all requests from the hashtable and moves them to the timeline. NO OTHER THREAD may be using any of these data structures. This will be used while migrating between scheduling algorithms, so after calling lock_all_data_structures.
 */
void migrate_from_hashtable_to_timeline()
{
	struct agios_list_head *hash_list; /**< used to point to each line of the hashtable */
	struct file_t *req_file; /**< used to iterate over each line of the hashtable */

	//we will mess with the data structures and don't even use locks, since here we are certain no one else is messing with them
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) { //go through the whole hashtable, one position at a time
		hash_list = &hashlist[i];
		agios_list_for_each_entry (req_file, hash_list, hashlist) { //go though all files in this line of the hashtable
			//get all requests from it and put them in the timeline
			put_all_requests_in_timeline(&req_file->read_queue.list, req_file, i);
			put_all_requests_in_timeline(&req_file->write_queue.list, req_file, i);
		}
	}
}
/** 
 * This function gets all requests from the timeline and moves them to the hashtable. NO OTHER THREAD may be using any of these data structures. This will be used while migrating between scheduling algorithms, so after calling lock_all_data_structures.
 */
void migrate_from_timeline_to_hashtable()
{
	/*! \todo make this TWINS-friendly if we ever want twins to be an option for dynamic algorithms */
	put_all_requests_in_hashtable(&timeline);
}
/**
 * Locks all data structures used for requests and files. This is not supposed to be used for normal library functions. We only use it at initialization, to guarantee the user won't try to add new requests while we did not decide on the scheduling algorithm yet. Moreover, we use these functions while migrating between scheduling algorithms.
 */
void lock_all_data_structures(void)
{
	PRINT_FUNCTION_NAME;
	timeline_lock();
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) hashtable_lock(i);
	PRINT_FUNCTION_EXIT;
}
/**
 * Unlocks all data structures used for requests and files. This is not supposed to be used for normal library functions. We only use it at initialization, to guarantee the user won't try to add new requests while we did not decide on the scheduling algorithm yet. Moreover, we use these functions while migrating between scheduling algorithms.
 */
void unlock_all_data_structures(void)
{
	PRINT_FUNCTION_NAME;
	for (int32_t i=0; i<AGIOS_HASH_ENTRIES; i++) hashtable_unlock(i);
	timeline_unlock();
	PRINT_FUNCTION_EXIT;
}

/**
 * This function allocates AGIOS data structures and initializes related locks.
 * @param max_queue_id is the value provided to agios_init to indicate what is the maximum identifier expected to be provided to agios_add_request.
 * @return true or false for success.
 */
bool allocate_data_structures(int32_t max_queue_id)
{
	reset_global_stats(); //puts all statistics to zero 
	if (!timeline_init(max_queue_id)) return false; //initializes the timeline
	if (!hashtable_init()) return false; 
	//put request and file counters to 0
	current_reqnb = 0;
	current_filenb=0;
	//block all data structures so the user cannot start adding requests while we are not ready (we need to select a scheduling algorithm first)
	lock_all_data_structures();
	return true;
}
/** 
 * function called to acquire the lock for the data structure currently in use. It is either the hashtable or the timeline depending on the scheduling algorithm being used. The catch is that we will call lock, but while we are waiting the scheduler in use may have changed, so we need to be sure we are holding the adequate lock before adding a request (or looking for it to cancel or release). If we don't, then the request is being added to a ghost data structure (they both exist even when not in use), from where it will never be recoved to be processed. The caller has to check the return value to be sure to unlock the right data structure after using it.
 * @param hash the line of the hashtable containing information about the file being accessed.
 * @return true if the request is to be added to the hashtable, false for the timeline. 
 */
bool acquire_adequate_lock(int32_t hash)
{
	bool previous_needs_hashtable;  /**< Used to control the used data structure in the case it is being changed while this function is running */

	while (true) { //we'll break out of this loop when we are sure to have acquired the lock for the right data structure
		//check if the current scheduler uses the hashtable or not and then acquire the right lock
		previous_needs_hashtable = current_scheduler->needs_hashtable;
		if (previous_needs_hashtable) hashtable_lock(hash);
		else timeline_lock();
		//the problem is that the scheduling algorithm could have changed while we were waiting to acquire the lock, and then it is possible we have the wrong lock. 
		if (previous_needs_hashtable != current_scheduler->needs_hashtable) {
			//the other thread has migrated scheduling algorithms (and data structure) while we were waiting for the lock (so the lock is no longer the adequate one)
			if (previous_needs_hashtable) hashtable_unlock(hash);
			else timeline_unlock();
		}
		else break; //everything is fine, we got the right lock (and now that we have it, other threads cannot change the scheduling algorithm
	} 
	return previous_needs_hashtable;
}
/**
 * Function called to cleanup data structures used by AGIOS to keep requests (at the end of its execution).
 */
void cleanup_data_structures(void)
{
	hashtable_cleanup();
	timeline_cleanup();
}

