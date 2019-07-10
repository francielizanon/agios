/*! \function statistics.c
    \brief Keeps global statistics (for all accesses) and provides functions to update and manipulate them. Also used to update local statistics (for each queue separately).
 */
#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "agios.h"
#include "common_functions.h"
#include "mylist.h"
#include "req_hashtable.h"
#include "statistics.h"

static struct timespec last_req; /**< time of the last request arrival. */
static struct global_statistics_t global_stats; /**< global statistics. */
static pthread_mutex_t global_statistics_mutex = PTHREAD_MUTEX_INITIALIZER; /**< to protectthe global statistics */

/**
 * function called to update the local statistics to a queue after the arrival of a new request.
 * @param stats the statistics structure of the queue to be updated.
 * @param req the newly arrived request.
 */
void update_local_stats(struct queue_statistics_t *stats, struct request_t *req)
{
	int64_t elapsedtime=0; /**< used to measure the time since the last request's arrival. */
	int64_t this_distance; /**< used to calculate the offset distance to the previous request. */

	//update local statistics on time between requests
	if(stats->receivedreq_nb > 1) { //we can only calculate the offset distance starting from the second request to arrive
		elapsedtime = (req->arrival_time - get_timespec2long(req->globalinfo->last_req_time)); //in ns
		stats->avg_time_between_requests = update_iterative_average(stats->avg_time_between_requests, elapsedtime, stats->receivedreq_nb-1);
	}
	get_long2timespec(req->arrival_time, &req->globalinfo->last_req_time);
	//update local statistics on average offset distance between consecutive requests
	if (stats->receivedreq_nb > 1) { //we can only calculate the offset distance starting from the second request to arrive
		this_distance = req->offset - req->globalinfo->last_received_finaloffset;
		if(this_distance < 0)
			this_distance *= -1;
		stats->avg_distance = update_iterative_average(stats->avg_distance, this_distance, stats->receivedreq_nb -1);
	}
	req->globalinfo->last_received_finaloffset = req->offset + req->len;
	//update local statistics on request size
	stats->avg_req_size = update_iterative_average(stats->avg_req_size, req->len, stats->receivedreq_nb);
}
/**
 * function called when a new request is received, to update the global statistics.
 * @param stats the global statistics structure.
 * @req the newly arrived request.
 */
void update_global_stats_newreq(struct global_statistics_t *stats, 
				struct request_t *req)
{
	int64_t elapsedtime; /**< time since the previous request's arrival.*/

	stats->total_reqnb++;
	//update global statistics on time between requests 
	if (stats->total_reqnb > 1) { //we can only measure time between requests starting from the second request
		elapsedtime = (req->arrival_time - get_timespec2long(last_req)); 
		stats->avg_time_between_requests = update_iterative_average(stats->avg_time_between_requests, elapsedtime, stats->total_reqnb-1);
	}
	get_long2timespec(req->arrival_time, &last_req);
	//update global statistics on request size 
	stats->avg_request_size = update_iterative_average(stats->avg_request_size, req->len, stats->total_reqnb);
	//update global statistics on operation
	if(req->type == RT_READ)
		stats->reads++;
	else
		stats->writes++;
}
/**
 * function called to update the statists after the arrival of a new request. The caller  must hold the hashtable mutex. Must NOT hold the global statistics mutex.
 * @param req the newly arrived requests.
 */
void statistics_newreq(struct request_t *req)
{
	req->globalinfo->stats.receivedreq_nb++;
	//update global statistics
	pthread_mutex_lock(&global_statistics_mutex);
	update_global_stats_newreq(&global_stats, req);
	pthread_mutex_unlock(&global_statistics_mutex);
	//update local statistics
	update_local_stats(&req->globalinfo->stats, req);
}
/**
 * resets all global statistics.
 */
void reset_global_stats(void)
{
	pthread_mutex_lock(&global_statistics_mutex);
	global_stats.total_reqnb =0;
	global_stats.reads = 0;
	global_stats.writes = 0;
	global_stats.avg_time_between_requests = -1;
	global_stats.avg_request_size = -1;
	pthread_mutex_unlock(&global_statistics_mutex);
}
/**
 * called by reset_all_statistics to reset all local statistics from a queue
 */
void reset_stats_queue(struct queue_t *queue)
{
	queue->stats.processedreq_nb = 0;
	queue->stats.receivedreq_nb = 0;
	queue->stats.processed_req_size = 0;
	queue->stats.processed_bandwidth = -1;
	queue->stats.releasedreq_nb = 0;
	queue->stats.avg_req_size = -1;
	queue->stats.avg_time_between_requests = -1;
	queue->stats.avg_distance = -1;
	queue->stats.aggs_no = 0;
	queue->stats.avg_agg_size = -1;
}
/**
 * function called once in a while to completely reset all statistics (local and global) we have been keeping about the access pattern. Must hold ALL mutexes (this function is called after lock_all_data_structures, so no other locks are necessary). 
 */
void reset_all_statistics(void)
{
	struct agios_list_head *list; /**< used to access each line of the hashtable.*/
	struct file_t *req_file; /**< used to iterate over all files in a line of the hashtable. */

	for (int32_t i=0; i< AGIOS_HASH_ENTRIES; i++) {
		list = &hashlist[i];
		agios_list_for_each_entry (req_file, list, hashlist) { //goes over all files of this line of the hashtable
			reset_stats_queue(&req_file->read_queue);
			reset_stats_queue(&req_file->write_queue);
		}
	}
	//reset global statistics as well
	reset_global_stats();
}
/**
 * updates the local statistics for a queue after an aggregation. The size of the aggregation is not provided because it is already in related->lastaggregation.
 * @param related the queue.
 */
void stats_aggregation(struct queue_t *related)
{
	if (related->lastaggregation > 1) {
		related->stats.aggs_no++;
		related->stats.avg_agg_size = update_iterative_average(related->stats.avg_agg_size, related->lastaggregation, related->stats.aggs_no);
		if (related->best_agg < related->lastaggregation) related->best_agg = related->lastaggregation;
	}
}
