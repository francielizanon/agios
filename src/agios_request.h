/*! \file agios_request.h
    \brief Definitions of the data structures used to make queues of requests and to hold information about files (and access statistics).
 */

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "mylist.h"

struct request_t;
/*! \struct queue_statistics_t 
    \brief the statistics we keep for each queue (one for write and another for read) of each file in the system
 */
struct queue_statistics_t  {
	int64_t processedreq_nb; /**< number of processed requests */
	int64_t receivedreq_nb; /**< number of received requests */
	int64_t processed_req_size; /**< total amount of served data */
	int64_t processed_bandwidth; /**< average bytes per ns */
	int64_t releasedreq_nb; /**< number of released requests */
	//statistics on request size
	int64_t avg_req_size; /**< iteratively calculated average request size (among received requests). */ 
	int64_t avg_time_between_requests; /**< iteratively calculated time between requests' arrival times. */
	int64_t avg_distance; /**< iteratively calculated average offset difference between consecutive requests */
	//number of performed aggregations and of aggregated requests
	int64_t 	aggs_no;	/**< number of performed aggregations */ 
	int64_t 	avg_agg_size;  /**< iteratively calculated average aggregation size (in number of requests) */
};
/*! \struct queue_t
    \brief A queue of requests with associated information and statistics.

    We have two queue_t structs per file in the library. If we are using a scheduling algorithm that does not use the hashtable, the queue_t structs will already exist and hold up-to-date information, the only difference is that they will not hold a list of requests.
 */	
struct queue_t {
	struct agios_list_head list ; /**< the queue of requests */
	struct agios_list_head dispatch; /**< contains requests which were already scheduled, but not released yet */
	struct file_t *req_file; /**< a pointer to the struct with information about this file */
	//fields used by aIOLi (and also some of them are used by MLF)
	int64_t laststartoff ; /**< used by aIOLi for shift phenomenon detection */
	int64_t lastfinaloff ; /**< used by aIOLi for shift phenomenon detection */
	int64_t predictedoff ; /**< used by aIOLi for shift phenomenon detection */
	int32_t nextquantum; /**< used by aIOLi to keep track of quanta */
	int64_t shift_phenomena; /**< counter used to make decisions regarding waiting times (for aIOLi) */
	int64_t better_aggregation; /**< counter used to make decisions regarding waiting times (for aIOLi) */
	//fields used to keep statistics
	struct queue_statistics_t stats;  /**< statistics */
	int64_t current_size; /**< sum of all its requests' sizes (even if they overlap). Used by SJF and some statistics */ 
	int32_t lastaggregation ;	/**< Number of request contained in the last processed virtual request. Used to help deciding on waiting times */ 
	int32_t	best_agg; /**< best aggregation performed to this queue. Used to help deciding on waiting times */ 
	struct timespec last_req_time; /**< timestamp of the last time we received a request for this one, used to keep statistics on time between requests */
	int64_t last_received_finaloffset; /**< offset+len of the last request received to this queue, used to keep statistics on offset distance between consecutive requests */
};
/*! \struct file_t
    \brief Holds information about one file that has received requests in this library

    The file_t structure is identified by the file_id and added to the hashtable. It holds two queues, one for reads and another for writes.
    @see queue_t
 */
struct file_t {
	char *file_id; /**< the file handle */
	struct queue_t read_queue; /**< read queue */
	struct queue_t write_queue; /**< write queue */
	int64_t timeline_reqnb; /**< counter for knowing how many requests in the timeline are accessing this file */
	struct agios_list_head hashlist; /**< to insert this structure in a list (hashtable position or timeline_files) */ 
	//used by aIOLi and SJF to handle waiting times (they apply to the whole file, not only the queue)
	int32_t waiting_time; /**< for how long should we be waiting */
	struct timespec waiting_start; /**< since when are we waiting */
	int64_t first_request_time; /**< arrival time of the first request to this file, all requests' arrival times will be relative to this one */
};
/*! \struct request_t
    \brief The structure holding information about one request in the system.

    It is created when a request is added and destroyed after release or cancel. It is added to queue_t of the appropriated file or to the timeline (depending on the scheduling algorithm being used). This structure might alternatively be a "virtual request", composed of a list of aggregated requests.
 */
struct request_t { 
	char *file_id;  /**< file handle */
	int64_t arrival_time; /**< arrival time of the request to AGIOS */
	int64_t dispatch_timestamp; /**< timestamp of when the request was given back to the user */ 
	int32_t type; /**< RT_READ or RT_WRITE */
	int64_t offset; /**< position of the file in bytes */
	int64_t len; /**< request size in bytes */
	int32_t queue_id; /**< an identifier of the queue to be used for this request, relevant for SW and TWINS only */
	int64_t sw_priority; /**< value calculated by the SW algorithm to insert the request into the queue */
	int64_t user_id;  /**< value passed by AGIOS' user (for knowing which request is this one)*/
	int32_t sched_factor; /**< used by MLF and aIOLi */
	int64_t timestamp; /**< the arrival order at the scheduler (a global value incremented each time a request arrives so the current value is given to that request as its timestamp)*/
	/*request's position inside data structures*/
	struct agios_list_head related; /**< for including in hashtable or timeline */ 
	struct queue_t *globalinfo; /**< pointer for the related list inside the file (list of reads or  writes) */
	/*for aggregations*/
	int32_t reqnb; /**< for virtual requests (real requests), it is the number of requests aggregated into this one. */
	struct agios_list_head reqs_list; /**< list of requests inside this virtual request*/
	struct request_t *agg_head; /**< pointer to the virtual request structure (if this one is part of an aggregation) */
	struct agios_list_head list;  /**< to be inserted as part of a virtual request */
};

void request_cleanup(struct request_t *aux_req);
void list_of_requests_cleanup(struct agios_list_head *list);
void print_request(struct request_t *req);
