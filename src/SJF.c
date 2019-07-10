/*! \file SJF.c
    \brief Implementation of the SJF scheduling algorithm.
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agios_counters.h"
#include "agios_request.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "scheduling_algorithms.h"

/**
 * answers if a queue could be selected to process requests, given a current minimum queue size. The queue may only be selected if it has requests in it and its size is smaller than the provided min size.
 * @param queue the queue.
 * @param min_size the current minimum queue size.
 * @return true or false if the queue is to be selected or not.
 */
bool SJF_check_queue(struct queue_t *queue, int64_t min_size)
{
	if (queue->current_size <= 0) return false; //we dont have requests, cannot select this queue
	else {
		if (queue->current_size < min_size) return true;
		else return false;
	}
}
/**
 * goes over the whole hashtable to find the shortest queue. The caller must NOT hold the mutex for any line of the hashtable.
 * @param current_hash the line of the hashtable where the returned request is (it will be modified by this function). 
 * @return the shortest queue that contains requests, NULL if we can't find one.
 */
struct queue_t *SJF_get_shortest_job(int32_t *current_hash)
{
	struct agios_list_head *reqfile_l; /**< used to access the line of the hashtable. */
	int64_t min_size = LONG_MAX; /**< used to keep track of the shortest queue. */
	struct queue_t *chosen_queue=NULL; /**< used to keep track of the shortest queue (and returned at the end). */
	int32_t chosen_hash=0; /**< used to keep track of the shortest queue. */
	struct file_t *req_file; /**< used to go over all files in a line of the hashtable. */
	int32_t evaluated_reqfiles=0; /**< counter of how many files were checked. */
	
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) { //go over all lines of the hashtable
		reqfile_l = hashtable_lock(i);
		agios_list_for_each_entry (req_file, reqfile_l, hashlist) { //go over all files in this line
			if ((!agios_list_empty(&req_file->write_queue.list)) || 
				(!agios_list_empty(&req_file->read_queue.list))) { //if at least one of the queues has requests in it	 
				assert((req_file->read_queue.current_size > 0) || (req_file->write_queue.current_size > 0));  //sanity check
				evaluated_reqfiles++; //we count only files that have requests 
				if (SJF_check_queue(&req_file->read_queue, min_size)) {
					min_size = req_file->read_queue.current_size;
					chosen_queue = &req_file->read_queue;
					chosen_hash = i;
				}
				if (SJF_check_queue(&req_file->write_queue, min_size)) { //this is NOT an else because the write queue could be smaller
					min_size = req_file->write_queue.current_size;
					chosen_queue = &req_file->write_queue;
					chosen_hash = i;
				}
			} //end if at least one of the queues is not empty
		} //end of for all files
		hashtable_unlock(i);
		if (evaluated_reqfiles >= current_filenb) break; //shortcut out in case we know the rest of the hashtable is empty
	} //end go over all the hashtable
	if (!chosen_queue) return NULL;
	else {
		*current_hash = chosen_hash;
		return chosen_queue;
	}
}
/**
 * main function for the SJF scheduler. Selects requests, processes and then cleans up them. Returns only after consuming all requests, or earlier if notified by the process_requests_step2 function. 
 * @return 0 (because we will never decide to sleep)
 */
int64_t SJF(void)
{	
	int32_t SJF_current_hash=0; /**< the line of the hashtable we are going to take requests from. */
	struct queue_t *SJF_current_queue; /**< the queue from which we will take requests. */
	struct request_t *req; /**< the request we will process. */
	bool SJF_stop=false; /**< the return of the process_requests_step2 function may notify us it is time to stop because of a periodic event. */
	struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */

	while ((current_reqnb > 0) && (SJF_stop == false)) {
		/*1. find the shortest queue*/
		SJF_current_queue = SJF_get_shortest_job(&SJF_current_hash);
		if (SJF_current_queue) {
			hashtable_lock(SJF_current_hash); //it is possible that between unlocking in the get_shortest_job function and locking here new requests were added and this is no longer the shortest queue, but we don't care that much.
			/*2. select its first request and process it*/	
			assert(!agios_list_empty(&SJF_current_queue->list)); //sanity check
			req = agios_list_entry(SJF_current_queue->list.next, struct request_t, related);
			if (req) {
				/*removes the request from the hastable*/
				hashtable_del_req(req);
				/*sends it back to the file system*/
				info = process_requests_step1(req, SJF_current_hash);
				generic_post_process(req);
				hashtable_unlock(SJF_current_hash);
				SJF_stop = process_requests_step2(info);
			} else hashtable_unlock(SJF_current_hash);
		}
	}
	return 0;
}

