/*! \file performance.c
    \brief The performance module, that keeps track of performance observed with different scheduling algorithms.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "agios_config.h"
#include "agios_request.h"
#include "common_functions.h"
#include "mylist.h"
#include "performance.h"
#include "scheduling_algorithms.h"

int64_t agios_processed_reqnb; /**< processed (and released) requests counter (relative to the most recently selected scheduling algorithm only). I've decided not to protect it with a mutex although it is used by two threads. The library's user calls the release function, where this variable is modified. The scheduling thread reads it in the process_requests function. Since it is not critical to have the most recent value there, no mutex. */

static AGIOS_LIST_HEAD(performance_info); /**< the list with all information we are holding about performance measurements (each element will be a struct performance_entry_t). */
static int performance_info_len=0; /**< how many entries in performance_info. */
struct performance_entry_t *current_performance_entry; /**< the latest entry to performance_info. */
pthread_mutex_t performance_mutex = PTHREAD_MUTEX_INITIALIZER; /**< to protect the performance_info structure. */

/**
 * function called to clean up this module (at the end of the execution).
 */
void cleanup_performance_module(void)
{
	struct performance_entry_t *entry; /**< used to iterate through all entries in the performance_info list. */
	struct performance_entry_t *aux=NULL; /**< used to avoid freeing an entry before moving the iterator to the next one, otherwise the loop breaks. */

	agios_list_for_each_entry (entry, &performance_info, list) { //goes over all entries
		if (aux) {
			agios_list_del(&aux->list);
			free(aux);
		}
		aux = entry;
	}
	if (aux) {
		agios_list_del(&aux->list);
		free(aux);
	}
}
/**
 * Returns the bandwidth observed so far with the current scheduling algorithm. The caller must NOT hold performance mutex, as this function will lock it.
 * @return the bandwidth observed so far in bytes per ns.
 */
int64_t get_current_performance_bandwidth(void)
{
	int64_t ret; /**< value that will be returned. */

	pthread_mutex_lock(&performance_mutex);
	ret = current_performance_entry->bandwidth;
	pthread_mutex_unlock(&performance_mutex);	
	return ret;
}
/**
 * Function called when a new scheduling algorithm is selected, to add a slot to it in the performance data structures. The caller must NOT hold performance mutex.
 * @param the new scheduling algorithm (its identifier).
 * @return true or false for success.
 */
bool performance_set_new_algorithm(int32_t alg)
{
	struct timespec now; /**< we'll use to get a timestamp for this change. */
	struct performance_entry_t *new = malloc(sizeof(struct performance_entry_t)); /**< new entry to the performance_info list. */
	if (!new) {
		agios_print("PANIC! could not allocate memory for the performance module!");
		return false;
	}
	//fill the new entry
	new->size = 0;
	new->reqnb=0;
	new->bandwidth =0;
	agios_gettime(&now);
	new->timestamp = get_timespec2long(now);
	new->alg = alg;
	//add it to the performance_info list
	pthread_mutex_lock(&performance_mutex);
	agios_list_add_tail(&(new->list), &performance_info);
	current_performance_entry = new;
	performance_info_len++;
	agios_processed_reqnb=0; 
	//need to check if we dont have too many entries
	while (performance_info_len > config_agios_performance_values) {
		agios_list_for_each_entry(new, &performance_info, list) break; //get the first entry //we reuse the new variable since we are no longer using it to hold the new entry (it is already in the list)
		agios_list_del(&new->list); //remove the first of the list 
		free(new);
		performance_info_len--;
	}
	pthread_mutex_unlock(&performance_mutex);
	return true;
}
/** 
 * Function that returns the entry of the scheduling algorithm that was executing when this request was sent for processing (because it makes no sense to account this performance measurement to an algorithm which was not responsible for deciding the execution of this request). The caller MUST hold the performance mutex.
 * @param req the request being released.
 * @return a pointer to the performance info entry, NULL if we can't find te request.
 */
struct performance_entry_t * get_request_entry(struct request_t *req)
{
	struct performance_entry_t *ret = current_performance_entry; /**< the pointer that will be returned. */
	bool found=false; /**< did we find it? */

	while(1) { //we will break out of this loop when we finish going over the whole list without finding the request, or when we find it.
		if (ret->timestamp > req->dispatch_timestamp) { //the request is NOT from this period

			if (ret->list.prev == &performance_info) break; //we reached the end of the list without finding the request.
			ret = agios_list_entry(ret->list.prev, struct performance_entry_t, list); //go to the previous period	
		} else { //the request IS from this period
			found = true;
			break;
		}
	} //end while
	if(!found) return NULL;
	else return ret;
}
/**  
 * Print all performance_info entries, for debug. The caller must hold the performance mutex.
 */
void print_all_performance_data(void)
{
	struct performance_entry_t *aux; /**< used to iterate over all entries in the list. */

	debug("current situation of the performance model:");
	agios_list_for_each_entry (aux, &performance_info, list) {
		debug("%s - %ld bytes, %ld requests, %ld bytes/ns (timestamp %ld)",
			get_algorithm_name_from_index(aux->alg),
			aux->size,
			aux->reqnb,
			aux->bandwidth,
			aux->timestamp);
	}
}
