/*! \file TO.c
    \brief Implementation of the timeorder and timeorder with aggregations scheduling algorithms. Their processing phases is the same, the only difference is in the requests insertion in the queue.
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agios_counters.h"
#include "process_request.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"

/**
 * repeatedly process the first request of the timeline, until process_requests notify us to stop.
 * @return 0 (because we will never decide to sleep) 
 */
int64_t timeorder(void)
{
	struct request_t *req;	/**< the request we will process. */
	bool TO_stop = false; /**< is it time to stop and go back to the agios thread to do a periodic event? */
	int32_t hash; /**< the hashtable line which contains information about the request we will process. */
	struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */

	while ((current_reqnb > 0) && (TO_stop == false)) {
		timeline_lock();
		req = timeline_oldest_req(&hash);
		assert(req); //sanity check
		info = process_requests_step1(req, hash); 
		generic_post_process(req);
		timeline_unlock();
		TO_stop = process_requests_step2(info);
	}
	return 0;
}



