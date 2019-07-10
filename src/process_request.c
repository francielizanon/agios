/*! \file process_request.c
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions. 

    That is done by the scheduling algorithms in two steps. First, while still holding the appropriate mutex for the data structure, process_requests_step1 has to be called to add requests in the dispatch, update counters, and fill a struct with information that can be given to the user. Then, in the second step, *after* having unlocked the mutex, the scheduler must call process_requests_step2 providing the struct filles by step1, and this function will use the user-provided callbacks to actually process the requests. That is done in two steps to avoid going back to the user while holding internal locks, and also to be less dependent on the time the user expends in its callbacks.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "agios_counters.h"
#include "agios_request.h"
#include "agios_thread.h"
#include "common_functions.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"

struct agios_client user_callbacks; /**< contains the pointers to the user-provided callbacks to be used to process requests */


/**
 * unlocks the mutex protecting the data structure where the request is being held. 
 * @param hash If we are using the hashtable, then hash gives the line of the table (because we have one mutex per line). If hash= -1, we are using the timeline (a simple list with only one mutex).
 */
void unlock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_unlock(hash);
	else
		timeline_unlock();
}
/** 
 * Locks the mutex protecting the data structure where the request is being held.
 * @param hash If we are using the hashtable, then hash gives the line of the table (because we have one mutex per line). If hash= -1, we are using the timeline (a simple list with only one mutex).
 */
 void lock_structure_mutex(int32_t hash)
{
	if(current_scheduler->needs_hashtable)
		hashtable_lock(hash);
	else
		timeline_lock();
}
/**
 * called when a request is being sent back to the user for processing. It records the timestamp of that happening, and adds the request at the end of a dispatch queue.
 * @param req the request being processed.
 * @param this_time the timestamp of now.
 * @param dispatch the dispatch queue that will receive the request.
 */
void put_this_request_in_dispatch(struct request_t *req, int64_t this_time, struct agios_list_head *dispatch)
{
	agios_list_add_tail(&req->related, dispatch);
	req->dispatch_timestamp = this_time;
	debug("request - size %ld, offset %ld, file %s - going back to the file system", req->len, req->offset, req->file_id);
	req->globalinfo->current_size -= req->len; //when we aggregate overlapping requests, we don't adjust the related list current_size, since it is simply the sum of all requests sizes. For this reason, we have to subtract all requests from it individually when processing a virtual request.
	req->globalinfo->req_file->timeline_reqnb--;
}
/**
 * this function will be called by scheduling algorithms as the first step into processing a request. It will add requests to the dispatch queue, update counters, and fill a structure with user-relevant information to be given to step 2.
 * @param head_req the (possibly virtual) request being processed.
 * @param hash the position of the hashtable where we'll find information about its file.
 * @return a newly allocated processing_info_t structure with a list of requests to be given to the user.
 */
struct processing_info_t *process_requests_step1(struct request_t *head_req, int32_t hash)
{
	struct processing_info_t *info; /**< a structure we will fill to return with the user-relevant information about this request, which will be given to the step 2 */
	struct request_t *req; /**< used to iterate over all requests belonging to this virtual request. */
	struct timespec now;	/**< used to get the dispatch timestamp for the requests. */
	int64_t this_time;	/**< will receive now converted from a struct timespec to a number. */

	assert(head_req);
	assert(head_req->reqnb >= 1);
	//get the timestamp for now, we'll need that to control the performance of request processing
	agios_gettime(&now);
	this_time = get_timespec2long(now);
	//allocate the data structure to hold information about this request
	info = (struct processing_info_t *)malloc(sizeof(struct processing_info_t));
	if (!info) return NULL;
	info->user_ids = (int64_t *)malloc(sizeof(int64_t)*head_req->reqnb);
	if (!info->user_ids) {
		agios_print("PANIC! Cannot allocate memory for AGIOS.");
		free(info);
		return NULL;
	}
	info->reqnb = head_req->reqnb;
	//fill the list of requests
	if (head_req->reqnb > 1) { //a virtual request
 		struct request_t *aux_req=NULL; /**< used to avoid removing a request from the virtual request before moving the iterator to the next one, otherwise the loop breaks. */
		info->reqnb = 0; //we'll use it as a index to fill the inside list, afterwards it will have the same value as before
		agios_list_for_each_entry (req, &head_req->reqs_list, related) { //go through all sub-requests
			if (aux_req) { //we can't just mess with req because the for won't be able to find the next requests after we've modified this one's pointers
				put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
				info->user_ids[info->reqnb]=aux_req->user_id;
				info->reqnb++;
			}
			aux_req = req;
		}
		if (aux_req) {
			put_this_request_in_dispatch(aux_req, this_time, &head_req->globalinfo->dispatch);
			info->user_ids[info->reqnb]=aux_req->user_id;
			info->reqnb++;
		}
	} else { //a simple request
		put_this_request_in_dispatch(head_req, this_time, &head_req->globalinfo->dispatch);
		*(info->user_ids) = head_req->user_id;
	}
	//update requests and files counters
	if (head_req->globalinfo->req_file->timeline_reqnb == 0) dec_current_filenb(); //timeline_reqnb is updated in the put_this_request_in_dispatch function
	dec_many_current_reqnb(hash, head_req->reqnb);
	debug("current status. hashtable[%d] has %d requests, there are %d requests in the scheduler to %d files.", hash, hashlist_reqcounter[hash], current_reqnb, current_filenb); //attention: it could be outdated info since we are not using the lock
	return info;
}
/** 
 * step 2 of the processing of requests by scheduling algorithms. Given a list of user-relevant information about requests to be processed, use the callbacks to process them. This is to be called after calling step 1 AND unlocking the appropriated mutexes.
 * @param info is the processing_info_t struct filled by process_requests_step1, containing a list of the user_id fields of the requests, and the number of requests in the list. (which may be 1). The data structure will be freed by the end of this function.
 * @return true if the scheduling algorithm must stop processing requests and give control back to the agios_thread (because some periodic event is happening), false otherwise.
 */
bool process_requests_step2(struct processing_info_t *info) 
{
	assert(info);
	assert(info->reqnb >= 1);
	if (info->reqnb == 1) { //simplest case, a single request
		user_callbacks.process_request_cb(*(info->user_ids));
	} else { //more than one request
		if (NULL != user_callbacks.process_requests_cb) { //we have a callback for a list of requests
			user_callbacks.process_requests_cb(info->user_ids, info->reqnb);
		} else { //we don't have a callback
			for (int32_t i=0; i < info->reqnb; i++) user_callbacks.process_request_cb(info->user_ids[i]);
		}
	}
	free(info->user_ids);
	free(info);
	//now check if the scheduling algorithms should stop because it is time to periodic events
	return is_time_to_change_scheduler();
}
