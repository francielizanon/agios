/*! \file aIOLi.c
    \brief Implementation of the aIOLi scheduling algorithm 
 */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "agios_config.h"
#include "agios_counters.h"
#include "agios_request.h"
#include "aIOLi.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "scheduling_algorithms.h"
#include "waiting_common.h"

/**
 * this function answers if it is possible to select a request from this queue. It has the secondary effect of increment the schedule factor of all requests in the queue.
 * @param queue the queue from which we are trying to select requests.
 * @param selected_queue and selected_timestamp will be updated in this function to contain this queue and the timestamp of the request that would be processed.
 * @return true or false for existing requests to be processed in this queue.
 */
bool aIOLi_select_from_list(struct queue_t *queue, 
				struct queue_t **selected_queue, 
				int64_t *selected_timestamp)
{
	bool ret = false; /**< did we find a request that could be processed? */
	struct request_t *req; /**< used to iterate over the requests in this queue */

	agios_list_for_each_entry (req, &queue->list, related) { //iterate over requests in this queue
		increment_sched_factor(req);
		if (&(req->related) == queue->list.next) { //we only try to select the first request from the queue (to respect offset order), but we don't break the loop because we want all requests to have their sched_factor incremented.
			if (req->len <= req->sched_factor*config_aioli_quantum) { //all requests start by a fixed size quantum (aIOLi_QUANTUM), which is increased every step (by increasing the sched_factor). The request can only be processed when its quantum is large enough to fit its size.
				ret = true;
				*selected_queue = queue;
				*selected_timestamp = req->timestamp;
			} //end if request's schedule factor is large enough	
		} //end if this is the first request
	} //end for all requests in the queue
	return ret;
}
/**
 * answers if it is possible to select a request to be processed to a given file. It has the secondary effect of incrementing all schedule factors of the queues it checks with aIOLi_select_from_list.
 * @param req_file the file to be checked.
 * @param selected_queue and selected_timestamp will be updated here to one of the queues from this file (if possible) 
 * @return true or false for we can process requests to this file.
 */
bool aIOLi_select_from_file(struct file_t *req_file, 
				struct queue_t **selected_queue, 
				int64_t *selected_timestamp)
{
	bool ret = false;
	//we try to select read requests before because they are faster
	if (!agios_list_empty(&req_file->read_queue.list)) ret = aIOLi_select_from_list(&req_file->read_queue, selected_queue, selected_timestamp);
	//try to select write requests if we could not select read requests 
	if ((!ret) && (!agios_list_empty(&req_file->write_queue.list))) ret = aIOLi_select_from_list(&req_file->write_queue, selected_queue, selected_timestamp);
	return ret;
}
/**
 * function called by the aIOLi schedule function to select one of the queues to process requests from.
 * @param selected_index an integer that will be modified here to contain the position of the hashtable where the selcted queue is.
 * @param sleeping_time will be modified here to contain for how long we should sleep IN CASE all files are waiting so we have nothing to process. in that case, we return NULL
 * @return a pointer to the selected queue (or NULL if we can't process requests)
 */
struct queue_t *aIOLi_select_queue(int32_t *selected_index, int64_t *sleeping_time)
{
	struct agios_list_head *reqfile_l; /**< used to iterate over the hashtable */
	struct file_t *req_file; /**< used to iterate over a hashtable line */
	int32_t shortest_waiting_time=INT_MAX;	/**< used to find out for how long we need to wait in case all files are currently waiting (hence we cannot process requests) */
	int32_t reqnb; /**< used to check how many requests from a queue could be selected */ 
	struct queue_t *tmp_selected_queue=NULL; /**< the queue that would be selected to a given file */
	int64_t tmp_timestamp; /**< the shortest timestamp from the queue that would be selected to a given file (used to ensure FIFO between different files) */
	struct queue_t *selected_queue = NULL; /**< the selected queue, will be returned */
	int64_t selected_timestamp=INT_MAX; /**< the earliest timestamp from the selected queue, used to ensure FIFO between different files */
	int32_t waiting_options=0; /**< how many files we are skipping because they are currently waiting? */
	struct request_t *req=NULL; /**< used to gather the first request from the selected queue to test if we should make this file wait */ 
		
	//go through all queues in the system to make the best choice
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) { //go through all entries of the hashtable
		reqfile_l = hashtable_lock(i);
		if (!agios_list_empty(reqfile_l)) { 
			agios_list_for_each_entry (req_file, reqfile_l, hashlist) { //go through all the files in this entry of the hashtable
				if (req_file->waiting_time > 0) { //if this file is waiting
					update_waiting_time_counters(req_file, &shortest_waiting_time);	
					if (req_file->waiting_time > 0) waiting_options++;
				}
				if (req_file->waiting_time <= 0) { //this file is not waiting. It is a new if (not an else) because waiting time was updated inside the previous if
					tmp_selected_queue=NULL;
					//see if there are "selectable" requests for this file
					reqnb = aIOLi_select_from_file(req_file, &tmp_selected_queue, &tmp_timestamp );
					if (reqnb > 0) { //there are
						if (tmp_timestamp < selected_timestamp) { //FIFO between the different files
							selected_timestamp = tmp_timestamp;
							selected_queue = tmp_selected_queue;
							*selected_index = i;
						}
					}	
				} //end if this file is not waiting
			} //end for going though all files in this hashtable entry
		} //end if this hashtable entry is not empty
		hashtable_unlock(i);
	} //end for all lines of the hashtable
	if (selected_queue) { //if we were able to select a queue
		hashtable_lock(*selected_index);
		req = agios_list_entry(selected_queue->list.next, struct request_t, related); 
		//test to see if we can proceed with this queue or we should wait for this file
		if (!check_selection(req, selected_queue->req_file)) {
			selected_queue = NULL; 
			*sleeping_time = 0; //we could maybe have selected a new queue, it does not mean we should sleep now, instead we need to ensure this function is called again 
		}
		hashtable_unlock(*selected_index);
	}
	else if (waiting_options) { // we could not select a queue, because all the files are waiting. So we should wait
		*sleeping_time = shortest_waiting_time;
	}
	return selected_queue;
}
/**
 * function called by aIOLi after stopping accessing one of its queues, used to adjust the quantum that will be given to this queue next time.
 * @param used_quantum how much data was accessed.
 * @param quantum how much was the quantum (in amount of data).
 * @return the next quantum to be used by this queue.
 */
int32_t adjust_quantum(int32_t used_quantum, int32_t quantum)
{
	int32_t used_quantum_rate = (used_quantum*100)/quantum; /**< fraction of the quantum that was used */
	int32_t requiredqt; /**< how much we believe was needed */

	//decide how much we think quantum should be
	if (used_quantum_rate >= 175) requiredqt = quantum*2; /*we used at least 75% more than what was given*/
	else if (used_quantum_rate >= 125) requiredqt = (quantum*15)/10; /*we used at least 25% more than what was given*/
	else if (used_quantum_rate >= 75) requiredqt = quantum; /*we used at least 75% of the given quantum*/
	else requiredqt = quantum/2; /*we used less than 75% of the given quantum*/
	//now adjust this value according to some bounds
	if (requiredqt <= 0) requiredqt = config_aioli_quantum; //if we decided to give 0 or less, give it the default value (otherwise this queue will starve)
	else {	
		if (requiredqt > MAX_AGGREG_SIZE) requiredqt = MAX_AGGREG_SIZE;
	}
	return requiredqt;
}
/** 
 * function used to schedule requests. 
 * @return the timeout to be used by the agios thread to sleep, in case we decide to sleep because ALL files are waiting and thus we have nothing to process (even if there are queued requests) 
 */
int64_t aIOLi(void)
{
	struct queue_t *aIOLi_selected_queue=NULL; /**< the queue from each we are taking requests in a given moment */
	int32_t selected_hash = 0; /**< the position from the hashtable we are accessing at a given moment */
	struct request_t *req; /**< the request chosen to be processed at a given moment */
	int32_t current_quantum = 0; /**< the quantum for the current queue being accessed */
	int32_t used_quantum = 0; /**< how much of the current quantum was used so far */
	bool aioli_stop= false; /**< this flag will be returned by the process_requests function to notify us we should stop scheduling requests because it is time for some periodic event */
	bool first_req; /**< used to ensure the first request of a selected queue is always processed (otherwise a small quantum will cause problems */
	int64_t waiting_time; /**< in case all files are currently waiting, for how long we should wait*/
	int64_t ret = 0; /**< the timeout we are returning */
	struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */
	AGIOS_LIST_HEAD(info_list); /**< we will select multiple requests from a queue if the quantum allows, so we'll make a list of the struct processing_info_t structs returned by the multiple calls to process_requests_step1 to call process_requests_step2 later, when we are done with the queue and can unlock the mutex. */

	//we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
	while ((current_reqnb > 0) && (!aioli_stop)) {
		aIOLi_selected_queue = aIOLi_select_queue(&selected_hash, &waiting_time);
		if (aIOLi_selected_queue) { //if we were able to select a queue
			hashtable_lock(selected_hash);
			//here we assume the list is NOT empty. It makes sense, since the other thread could have obtained the mutex, but only to include more requests, which would not make the list empty. If we ever have more than one thread consuming requests, this needs to be ensured somehow.
			/*we selected a queue, so we process requests from it until the quantum runs out*/
			current_quantum = aIOLi_selected_queue->nextquantum;
			used_quantum = 0;
			first_req = true; 
			do {
				//get the first request from this queue (because we need to keep the offset order within each queue
				agios_list_for_each_entry (req, &(aIOLi_selected_queue->list), related) break;

				if ((!first_req) && (req->len > (current_quantum - used_quantum))) break; //we are using leftover quantum, but the next request is not small enough to fit in this space, so we just stop processing requests from this queue. 
				first_req = false; //we are always sure to process at least one request of the queue, even if the quantum is too small
				//if we are here, then we have a request to be processed that fits the quantum
				used_quantum += req->len;
				/*removes the request from the hastable*/
				hashtable_del_req(req);
				/*sends it back*/
				info = process_requests_step1(req, selected_hash);
				agios_list_add_tail(&info->list, &info_list);
				/*cleanup step*/
				waiting_algorithms_postprocess(req);
				//here we used to wait until the request was processed before moving on (as in aioli's original design), but that proved to have very poor performance in modern systems because we want some request parallelism.
			} while ((!agios_list_empty(&aIOLi_selected_queue->list)) && (used_quantum < current_quantum) && (!aioli_stop));
			/*here we either ran out of quantum, or of requests (or we were asked to stop). Adjust the next quantum to be given to this queue considering this*/
			if (used_quantum >= current_quantum) /*ran out of quantum*/
			{		
				if (current_quantum == 0) { //it was the first time executing from this queue, we don't have information enough to decide the next quantum this file should receive, so let's just give it a default value
					aIOLi_selected_queue->nextquantum = config_aioli_quantum;
				}
				else { //we had a quantum and it was enough
					aIOLi_selected_queue->nextquantum = adjust_quantum(used_quantum, current_quantum);
				}
			} //end if we ran out of quantum
			else { /*ran out of requests*/
				if(!aioli_stop) { //if aioli_stop, we have stopped for this queue because it was time to refresh things, not because there were no more requests or quantum left. If we adjust quantum anyway, we would penalize this queue for no reason
					aIOLi_selected_queue->nextquantum = adjust_quantum(used_quantum, current_quantum);
				}
			}
			hashtable_unlock(selected_hash);
			aioli_stop = call_step2_for_info_list(&info_list);
			assert(agios_list_empty(&info_list));
		} //end if we have a selected queue
		else if (waiting_time > 0) { //we may have requests, but we cannot process them because all files are waiting, it is better to return
			ret = waiting_time;
			break; //get out of the while 
		}
	} //end if we have requests
	return ret;
}
