/*! \function statistics.c
    \brief Keeps global statistics (for all accesses) and provides functions to update and manipulate them. Also used to update local statistics (for each queue separately).
 */
#pragma once

#include <stdint.h>

#include "agios_request.h"

struct global_statistics_t
{
	int64_t total_reqnb; /**< number of received requests. We have a similar counter in consumer.c, but this one can be reset, that one is fixed (never set to 0, counts through the whole execution). */
	int64_t reads; /**< number of received read requests. */
	int64_t writes; /**< number of received write requests. */
	int64_t avg_time_between_requests; /**< iteratively calculated average time between consecutive requests. */
	int64_t avg_request_size; /**< iteratively calculated average request size. */
};

void statistics_newreq(struct request_t *req);
void reset_global_stats(void);
void reset_all_statistics(void);
void stats_aggregation(struct queue_t *related);
