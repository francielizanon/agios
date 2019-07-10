/*! \file agios_cancel_request.c
    \brief Implementation of the agios_cancel_request function, called by the user to give up of a queued request.

    ALL requests added with agios_add_request must be either notified with agios_release_request (after being processed) or cancelled with agios_cancel_request, otherwise information about them will continue to exist in memory.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "agios.h"
#include "agios_counters.h"
#include "common_functions.h"
#include "data_structures.h"
#include "hash.h"
#include "mylist.h"
#include "req_hashtable.h"
#include "req_timeline.h"

/** 
 * function used to remove a request from the scheduling queues
 * @param file_id the file handle associated with the request.
 * @param type is RT_READ or RT_WRITE.
 * @param len is the size of the request (in bytes).
 * @param offset is the position of the file to be accessed (in bytes).
 * @return true or false for success 
 */
//removes a request from the scheduling queues
//returns 1 if success
bool agios_cancel_request(char *file_id, 
			int32_t type, 
			int64_t len, 
			int64_t offset)  
{
	struct file_t *req_file; /**< used to look for information about the file accessed by the request */
	int32_t hash = get_hashtable_position(file_id); /**< the position of the hashtable where information about the file is */ 
	struct agios_list_head *list; /**< used to iterate over the line of the hashtable, and then over the queue */
	struct request_t *req; /**< used to iterate over the queue */
	struct request_t *aux_req; /**< used to iterate over the requests inside a virtual request */
	bool found=false;
	bool using_hashtable;

	PRINT_FUNCTION_NAME;
	//first acquire lock, we need to be careful because the data structure might me migrated while we are trying to do that
	using_hashtable = acquire_adequate_lock(hash);
	//now we have the appropriated lock
	list = &hashlist[hash];
	//find the structure for this file 
	agios_list_for_each_entry (req_file, list, hashlist) {
		if (strcmp(req_file->file_id, file_id) == 0) {
			found = true;
			break;
		}
	}
	if (!found) { //that makes no sense, we are trying to cancel a request which was never added!!!
		debug("PANIC! We cannot find the file structure for this request %s", file_id);
		if (using_hashtable) hashtable_unlock(hash);
		else timeline_unlock();
		return false;
	}
	debug("REMOVING a request from file %s:", req_file->file_id );
	//get the relevant queue
	if (using_hashtable) {
		if (type == RT_WRITE) list = &req_file->write_queue.list;
		else list = &req_file->read_queue.list;
	} else list = &timeline;
	//find the request in the queue and remove it
	found = false;
	agios_list_for_each_entry (req, list, related) { //linearly search for this request in the queue. To each request in the queue, there are two possibilities: either it is a simple request, than we can just compare, or it is a virtual request, than we might have to look into the sub-requests of the virtual one
		if (req->reqnb == 1) { //simple request
			if ((req->len == len) && (req->offset == offset)) {
				//we found it
				found = true;
				//update information about the file and request counters
				req->globalinfo->current_size -= req->len;
				req->globalinfo->req_file->timeline_reqnb--;
				if (req->globalinfo->req_file->timeline_reqnb == 0) dec_current_filenb();
				dec_current_reqnb(hash);
	 			//finally, free the structure
				request_cleanup(req);
				break;
			}
		} else { //aggregated request, the one we're looking for could be inside it
			if ((req->offset <= offset) && (req->offset + req->len >= offset+len)) { //no need to look if the request we're looking for is not inside this one
				agios_list_for_each_entry (aux_req, &req->reqs_list, related) {
					if ((aux_req->len == len) && (aux_req->offset == offset)) {
						bool first; /**< used to mark the first subrequest we visit */
						struct request_t *tmp; /**< used to iterate over all sub-requests of this virtual request to update its information */
						//we found it
						found = true;
						//remove it from the virtual request
						agios_list_del(&aux_req->related);
						//we need to update offset and len for the aggregated request without this one (and also timestamp)
						first = true; 
						//we will recalculate offset and len of the aggregation by going over all sub-requests
						agios_list_for_each_entry (tmp, &req->reqs_list, related) {
							if (first) {
								first = false;
								req->offset = tmp->offset;
								req->len = tmp->len;
								req->arrival_time = tmp->arrival_time;
								req->timestamp = tmp->timestamp;
							} else {
								if (tmp->offset < req->offset) {
									req->len += req->offset - tmp->offset;
									req->offset = tmp->offset;
								}
								if ((tmp->offset + tmp->len) > (req->offset + req->len)) {
									req->len += (tmp->offset + tmp->len) - (req->offset + req->len);
								}
								if (tmp->arrival_time < req->arrival_time) req->arrival_time = tmp->arrival_time;
								if (tmp->timestamp < req->timestamp) req->timestamp = tmp->timestamp;
							}	
						} //end for all requests inside this virtual request
						//now let's update aggregated request information
						req->reqnb--;
						if (req->reqnb == 1) { //it was a virtual request, now it's not anymore
							struct agios_list_head *prev, *next; /**< used to place the sub-request in the place of the virtual request in the queue */
							//remove the virtual request from the queue and add its only request in its place
							prev = req->related.prev;
							next = req->related.next;
							agios_list_del(&req->related);
							tmp = agios_list_entry(req->reqs_list.next, struct request_t, related);
							__agios_list_add(&tmp->related, prev, next);
							req->reqnb = 1; //otherwise the request_cleanup function will try to free the sub requests, that is not what we want here
							request_cleanup(req);
						}
						//the request is out of the queue, so now we update information about the file and request counters
						aux_req->globalinfo->current_size -= aux_req->len;
						aux_req->globalinfo->req_file->timeline_reqnb--;
						if(aux_req->globalinfo->req_file->timeline_reqnb == 0)
							dec_current_filenb();
						dec_current_reqnb(hash);
				 		//finally, free the structure
						request_cleanup(aux_req);
						break;
					}
				} //end for all requests inside the virtual request
				if (found) break; 
			} //end if request is inside a virtual request
		} //end comparing to a virtual request
	} //end going over all requests in the queue
	if (!found) debug("PANIC! Could not find the request %ld %ld to file %s\n", offset, len, file_id);
	//release data structure lock
	if (using_hashtable) hashtable_unlock(hash);
	else timeline_unlock();
	return true;
}
