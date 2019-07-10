/*! \file waiting_common.c
    \brief Functions used by aIOLi and MLF.

    aIOLi and MLF are the scheduling algorithms that try to predict shift phenomena and better aggregations, and then impose waiting times on files to improve the access pattern. Here we have some functions common to both.
 */

#include "agios_config.h"
#include "common_functions.h"
#include "process_request.h"
#include "scheduling_algorithms.h"
#include "waiting_common.h"
/**
 * function used by AIOLI and MLF when we find a file that is currently waiting. Since we try not to wait (when waiting on one file, we go on processing requests to other files), every time we try to get requests from a file we need to update its waiting time to see if it is still waiting or not. 
 * @param req_file the pointer to the structure containing information about a file that is waiting
 * @param shortest_waiting_time is updated in this function and kept by the caller to find the shortest waiting time among all waiting files.
 */
void update_waiting_time_counters(struct file_t *req_file, 
					int32_t *shortest_waiting_time)
{
	int64_t elapsed = get_nanoelapsed(req_file->waiting_start); /**< for how long has it been waiting? */
	if (req_file->waiting_time > elapsed) { //we have not waited enough
		req_file->waiting_time = req_file->waiting_time - elapsed; //update waiting time
		if (req_file->waiting_time < *shortest_waiting_time) *shortest_waiting_time = req_file->waiting_time;
	} else req_file->waiting_time=0; //we are done waiting for this file
}
/**
 * function used to check if a selected request can proceed or if we should impose waiting time for its file.
 * @param req the selected request to be processed.
 * @param req_file the file for this request.
 * @return true or false to proceed with this request
 */ 
bool check_selection(struct request_t *req, 
			struct file_t *req_file)
{
	/*waiting times are cause by 2 phenomena:*/
	/*1. shift phenomenon. One of the processes issuing requests to this queue is a little delayed, causing a contiguous request to arrive shortly after the other ones*/
	if (req->globalinfo->predictedoff != 0) {
		if (req->offset > req->globalinfo->predictedoff) { //we detected a shift, so we will impose a waiting time for this file
			req_file->waiting_time = config_waiting_time;
		}
		/*set to 0 to avoid starvation*/
		req->globalinfo->predictedoff = 0;
	} 
	/*2. better aggregation. If we just performed a larger aggregation on this queue, we believe we could do it again*/
	else if ((req->offset > req->globalinfo->lastfinaloff) && (req->globalinfo->lastaggregation > req->reqnb)) {
		req_file->waiting_time = config_waiting_time;
		/*set to zero to avoid starvation*/
		req->globalinfo->lastaggregation = 0;
	}
	if(req_file->waiting_time) { //we decided not to proceed with this request and to make this file wait
		agios_gettime(&req_file->waiting_start);
		return false;
	}
	return true;
}
/**
 * this function is used by MLF and by AIOLI. These two schedulers use a sched_factor that increases as request stays in the scheduler queues.
 * @param the request to be updated.
 */
void increment_sched_factor(struct request_t *req)
{
	if(req->sched_factor == 0) req->sched_factor = 1;
	else req->sched_factor = req->sched_factor << 1;
}
/**
 * post process function for scheduling algorithms which use waiting times (AIOLI and MLF).
 * @param req the request that has been processed.
 */
void waiting_algorithms_postprocess(struct request_t *req)
{
	req->globalinfo->lastfinaloff = req->offset + req->len;	
	/*try to detect the shift phenomenon*/
	if ((req->offset < req->globalinfo->laststartoff) && (!req->globalinfo->predictedoff)) {
		req->globalinfo->predictedoff = req->globalinfo->lastfinaloff; 
	}	
	req->globalinfo->laststartoff = req->offset;
	generic_post_process(req);
}
/**
 * used by aIOLi and MLF to call step2 for a list of processing_info_t structures filled by multiple calls to process_requests_step1.
 * @param info_list a list of filled processing_info_t structs. It will be empty (and all elements freed) after the call.
 * @return true if any of the calls to process_requests_step2 returned true, false otherwise.
 */ 
bool call_step2_for_info_list(struct agios_list_head *info_list)
{
	struct processing_info_t *info; /**< used to iterate over the list */
	struct processing_info_t *aux = NULL; /**< used to avoid freeing an info struct before iterating to the next element otherwise we will crash the loop. */
	bool ret = false; /**< each call to step2 will return a boolean, we will return true if any of the returns is true */
	agios_list_for_each_entry (info, info_list, list) {
		if (aux) {
			agios_list_del(&aux->list);
			ret = ret || process_requests_step2(aux);
		}
		aux = info;
	}
	if (aux) {
		agios_list_del(&aux->list);
		ret = ret || process_requests_step2(aux);
	}
	return ret; 
}
