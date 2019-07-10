/*! \file NOOP.c
    \brief Implementation of the NOOP scheduling algorithm.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "agios_request.h"
#include "common_functions.h"
#include "mylist.h"
#include "NOOP.h"
#include "process_request.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"

/** 
 * NOOP schedule function. Usually NOOP means not having a schedule function. However, when we dynamically change from another algorithm to NOOP, we may still have requests on queue. So we just process all of them. 
 * @return 0, because we will never decide to sleep 
 */
int64_t NOOP(void)
{
	struct agios_list_head *list; 
	struct request_t *req;
	bool stop_processing=false;
	int32_t hash;
	struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */

	while(!stop_processing) 
	{
		list = timeline_lock(); //we give a change to new requests by locking and unlocking to every rquest. Otherwise agios_add_request would never get the lock.
		stop_processing = agios_list_empty(list);
		if (!stop_processing) { //if the list is not empty
			//just take one request and process it
			req = timeline_oldest_req(&hash);
			debug("NOOP is processing leftover requests %s %ld %ld", req->file_id, req->offset, req->len);
			info = process_requests_step1(req, hash);
			generic_post_process(req);
			timeline_unlock();	
			stop_processing = process_requests_step2(info);
		} else timeline_unlock();	
	}
	return 0;
}

