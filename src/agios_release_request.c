/*! \file agios_release_request.c
    \brief Implementation of the agios_release_request function, called by the user after processing a request.
 */
#include <string.h>

#include "agios.h"
#include "agios_request.h"
#include "common_functions.h"
#include "data_structures.h"
#include "hash.h"
#include "mylist.h"
#include "performance.h"
#include "req_hashtable.h"
#include "req_timeline.h"

/**
 * This function is called by the release function, when the library user signaled it finished processing a request. In the case of a virtual request, its requests will be signaled separately, so here we are sure to receive a single request.
 * @param req the request that has been released by the user.
 */
void generic_cleanup(struct request_t *req)
{
	//update the processed requests counter
	req->globalinfo->stats.processedreq_nb++;
	//update the data counter
	req->globalinfo->stats.processed_req_size += req->len;
	request_cleanup(req); //remove from the list and free the memory
}
/** 
 * function called by the user after processing a request. Releases the data structures and keeps track of performance.
 @param file_id the file handle
 @param type if RT_READ or RT_WRITE
 @param len the size of the request
 @param offset the position of the file
 @return true or false for success.
 */
bool agios_release_request(char *file_id, 
				int32_t type, 
				int64_t len, int64_t offset)
{
	int32_t hash = get_hashtable_position(file_id); /**< the position of the hashtable where we have to look for this request. */
	bool ret = true; /**< return of the function */
	struct file_t *req_file; /**< used to iterate through the files in a line of the hashtable */
	struct agios_list_head *list; /**< used to access a line of the hashtable */
	struct queue_t *related; /**< used to point to the queue where we should look (read or write). */
	struct request_t *req; /**< used to iterate through all requests to the file. */
	bool found=false; /**< did we find this request in the dispatch queues? */ 
	int64_t elapsed_time; /**< how long has it been since this request was issued? */
	bool using_hashtable; /**< used to ensure we acquire the right lock. */
	struct performance_entry_t *entry; /**< used to access performance information about the right scheduling algorithm */
	int64_t this_bandwidth; /**< the bandwidth measured in the access by this request */

	PRINT_FUNCTION_NAME;

	//first acquire lock. That is a bit complicated because the other thread might be migrating scheduling algorithms (and consequently data structures) while we are doing this. 
	using_hashtable = acquire_adequate_lock(hash);
	//now we are sure to have the lock
	list = &hashlist[hash];
	//find the structure for this file 
	agios_list_for_each_entry (req_file, list, hashlist) {
		if (strcmp(req_file->file_id, file_id) == 0) {
			found = true;
			break;
		}
	}
	if (!found) {
		//that makes no sense, we are trying to release a request which was never added!!!
		debug("PANIC! We cannot find the file structure for this request %s", file_id);
		ret = false; //we cannot simply return here because we are holding the mutex, we have tofree it!
	} else {
		found = false;
#ifdef AGIOS_DEBUG
		debug("Releasing a request from file %s:", req_file->file_id );
		print_hashtable_line(hash);
#endif
		//get the relevant list 
		if (type == RT_WRITE) related = &req_file->write_queue;
		else related = &req_file->read_queue;
		//find the request in the dispatch queue
		agios_list_for_each_entry (req, &related->dispatch, related) {
			if ((req->len == len) && (req->offset == offset)) {
				found =true;
				break;
			}
		}
		if (found) {
			//let's see how long it took to process this request
			elapsed_time = get_nanoelapsed_long(req->arrival_time);
			//update local performance information (we don't update processed_req_size here because it is updated in the generic_cleanup function)
			req->globalinfo->stats.releasedreq_nb++;
			/*! \todo do we need a different precision for bandwidth??? */
			this_bandwidth = req->len/elapsed_time;  //in bytes per nanosecond
			req->globalinfo->stats.processed_bandwidth = update_iterative_average(req->globalinfo->stats.processed_bandwidth, this_bandwidth, req->globalinfo->stats.releasedreq_nb);
			
			//update global performance information
			pthread_mutex_lock(&performance_mutex);
			//we need to figure out to each time slice this request belongs
			entry = get_request_entry(req); //we use the timestamp from when the request was sent for processing, because we want to relate its performance to the scheduling algorithm who choose to process the request
			if (entry) { //we need to check because maybe the request took so long to process we don't even have a record for the scheduling algorithm that issued it
				entry->reqnb++;
				entry->size += req->len;
				entry->bandwidth = update_iterative_average(entry->bandwidth,this_bandwidth, entry->reqnb);
				if (entry == current_performance_entry) { //if this request was issued by the current scheduling algorithm
					agios_processed_reqnb++; //we only count it as a new processed request if it was issued by the current scheduling algorithm
					debug("a request issued by the current scheduling algorithm is back! processed_reqnb is %ld", agios_processed_reqnb);
				}
			} //end if found a performance entry
			pthread_mutex_unlock(&performance_mutex);
			//now we can completely free this request
			generic_cleanup(req);
		} else {
			debug("PANIC! Could not find the request %ld %ld to file %s\n", offset, len, file_id);
			ret = false; // we cannot simply return here because we are holding the mutex, needs to free it!
		}
	} //end if we found the req_file
	//release data structure lock
	if (using_hashtable) hashtable_unlock(hash);
	else timeline_unlock();

	return ret;
}
