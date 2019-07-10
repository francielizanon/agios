/*! \file performance.c
    \brief The performance module, that keeps track of performance observed with different scheduling algorithms.
 */
#pragma once

#include "agios_request.h"
#include "mylist.h"

extern int64_t agios_processed_reqnb; 

struct performance_entry_t //information about one time period, corresponding to one scheduling algorithm selection
{
	int64_t timestamp;	/**< timestamp of when we started this time period. */
	int32_t alg; /**< scheduling algorithm in use in this time period. */
	int64_t bandwidth; /**< average bandwidth in this time period. */
	int64_t size; /**< the sum of size of every request in this time period. */
	int64_t reqnb; /**< the number of requests released from this time period. */
	struct agios_list_head list; /**< to be inserted in a list. */
};

extern struct performance_entry_t *current_performance_entry; 
extern pthread_mutex_t performance_mutex; 

void cleanup_performance_module(void);
int64_t get_current_performance_bandwidth(void);
bool performance_set_new_algorithm(int32_t alg);
struct performance_entry_t * get_request_entry(struct request_t *req);
void print_all_performance_data(void);
