/*! \file MLF.c
    \brief Implementation of the MLF scheduling algorithm.
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agios_config.h"
#include "agios_counters.h"
#include "common_functions.h"
#include "MLF.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "waiting_common.h"

static int MLF_current_hash=0;  /**< position of the hashtable we are accessing. Used so we do a round robin on the hashtable even across different calls to MLF(). */
static int *MLF_lock_tries=NULL; /**< counter of how many times we tried without success to acquire the lock of a hashtable line. */

/**
 * initalizes the scheduler.
 * @return true or false for success.
 */
bool MLF_init(void)
{
	MLF_lock_tries = malloc(sizeof(int)*(AGIOS_HASH_ENTRIES+1));
	if (!MLF_lock_tries) {
		agios_print("AGIOS: cannot allocate memory for MLF structures\n");
		return false;
	}
	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) MLF_lock_tries[i]=0;
	return true;
}
/**
 * called to stop MLF.
 */
void MLF_exit()
{
	if (MLF_lock_tries) free(MLF_lock_tries);
}
/**
 * Selects a request to be processed from a queue (and updates the schedule factor for all requests in this queue.
 * @param reqlist the queue of requests.
 * @return a pointer to the request to be processed.
 */
struct request_t *applyMLFonlist(struct queue_t *reqlist)
{
	bool found=false;
	struct request_t *req; /**< used to iterate over all requests in the queue. */
	struct request_t *selectedreq=NULL; /**< will receive the selected request. */

	agios_list_for_each_entry (req, &(reqlist->list), related) { //go through all requests in this queue
		/*first, increment the sched_factor. This must be done to ALL requests, every time*/
		increment_sched_factor(req);
		if (!found) { //we select the first request that can be selected
			/*see if the request's quantum is large enough to allow its execution*/
			if ((req->sched_factor*config_mlf_quantum) >= req->len) {
				selectedreq = req; 
				found = true; /*we select the first possible request because we want to process them by offset order, and the list is ordered by offset. However, we do not stop the for loop here because we still have to increment the sched_factor of all requests (which is equivalent to increase their quanta)*/
			}
		} //end if not found
	}
	return selectedreq;
}
/**
 * selects a request to be processed for a file.
 * @param req_file the file to be accessed.
 * @return a pointer to the selected request. 
 */
struct request_t *MLF_select_request(struct file_t *req_file)
{
	struct request_t *req=NULL; /**< will receive the request to be processed. */

	if (!agios_list_empty(&req_file->read_queue.list)) { //if we have read requests
		req = applyMLFonlist(&(req_file->read_queue)); 
	}
	if ((!req) && (!agios_list_empty(&req_file->write_queue.list))) { //if we have not selected a read request already, and we have write requests
		req = applyMLFonlist(&(req_file->write_queue));
	}
	if (req && (!check_selection(req, req_file))) return NULL; //before proceeding with this request, check if we should wait
	return req;
}
/**
 * main function for the MLF scheduler. Selects requests, processes and then cleans up them.
 * @return a waiting time for the agios_thead to sleep in case we decide to sleep, 0 otherwise.
 */ 
int64_t MLF(void)
{	
	struct request_t *req; /**< this will receive the request selected to be processed. */
	struct agios_list_head *reqfile_l; /**< a line of the hashtable */
	struct file_t *req_file; /**< used to iterate over all files in a line of the hashtable */
	int32_t shortest_waiting_time=INT_MAX; /**< will be adapted to the shortest waiting time among all files that are currently waiting. In case we cannot process requests because all of the files are waiting, we will use this to wait the shortest amount of time possible. */
	int32_t starting_hash = MLF_current_hash; /**< from what hash position we are starting to round robin in the hashtable. */
	bool processed_requests = false; /**< could we process any requests while going through the whole hashtable? */
	bool mlf_stop=false; /**< flag that will be set by the process_request_step2 function, to let us know we should stop and give control back to the agios_thread */
	int32_t waiting_time = 0; /**< the waiting time we will return if we leave for not having requests to process (or if all files are waiting, in that case this will receive shortest_waiting_time). */
	struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */
	AGIOS_LIST_HEAD(info_list); /**< we will select multiple requests from a queue if the quantum allows, so we'll make a list of the struct processing_info_t structs returned by the multiple calls to process_requests_step1 to call process_requests_step2 later, when we are done with the queue and can unlock the mutex. */

	/*search through all the files for requests to process*/
	while ((current_reqnb > 0) && (!mlf_stop)) {
		/*try to lock the line of the hashtable. If we can't get it, we will move on to the next line. If a line has been tried without success MAX_MLF_LOCK_TRIES times, we will perform a regular lock to wait until it is available. The idea is to decrease the cost of waiting for locks but without starving queues. */
		reqfile_l = hashtable_trylock(MLF_current_hash);
		if (!reqfile_l) { /*could not get the lock*/
			if (MLF_lock_tries[MLF_current_hash] >= MAX_MLF_LOCK_TRIES) {
				/*we already tried the max number of times, now we will wait until the lock is free*/
				reqfile_l = hashtable_lock(MLF_current_hash);
			} else MLF_lock_tries[MLF_current_hash]++;
		}
		if (reqfile_l) { //if we got the lock. This is NOT an else because we may have modified reqfile_l inside the previous if.
			MLF_lock_tries[MLF_current_hash]=0;
			if (hashlist_reqcounter[MLF_current_hash] > 0) { //see if we have requests for this line of the hashtable
		                agios_list_for_each_entry (req_file, reqfile_l, hashlist) { //go through all files in this line of the hashtable
					/*do a MLF step to this file, potentially selecting a request to be processed, but before we need to see if we are waiting new requests to this file*/
					if (req_file->waiting_time > 0) update_waiting_time_counters(req_file, &shortest_waiting_time);
					req = MLF_select_request(req_file);
					if ((req) && (req_file->waiting_time <= 0)) { //if we could select a request to this file and we are not waiting on it
						/*removes the request from the hastable*/
						hashtable_del_req(req);
						/*sends it back to the file system*/
						/* \todo do not hold the lock when calling step2! */
						info = process_requests_step1(req, MLF_current_hash);
						agios_list_add_tail(&info->list, &info_list);
						processed_requests=true;
						/*cleanup step*/
						waiting_algorithms_postprocess(req);
					} //end if we could select a request and it is ready to be processed
				} //end for all files in the hashtable line
			}
			hashtable_unlock(MLF_current_hash);
			mlf_stop = call_step2_for_info_list(&info_list);
			assert(agios_list_empty(&info_list));
		} //end if we got the lock	
		//now we'll move on to the next line of the hashtable
		if (!mlf_stop) { //if mlf_stop is true, we've left the loop without going through all reqfiles, we should not increase the current hash yet
			MLF_current_hash++;
			if (MLF_current_hash >= AGIOS_HASH_ENTRIES) MLF_current_hash = 0;
			if (MLF_current_hash == starting_hash) { /*it means we already went through all the file structures*/
				if (!processed_requests) { //and we could not process anything even after going through ALL files
					waiting_time = shortest_waiting_time;
					break; //get out of the while
				}
				processed_requests=false; /*restart the counting*/
			}
		} //end if we were not notified to stop
	}//end while
	return waiting_time;
}
