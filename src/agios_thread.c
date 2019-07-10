/*! \file agios_thread.c
    \brief Implementation of the agios thread.

    The agios thread stays in a loop of calling a scheduler to process new requests and waiting for new requests to arrive.
*/
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "agios_config.h"
#include "agios_counters.h"
#include "agios_thread.h"
#include "common_functions.h"
#include "data_structures.h"
#include "performance.h"
#include "scheduling_algorithms.h"
#include "statistics.h"

static pthread_cond_t g_request_added_cond = PTHREAD_COND_INITIALIZER;  /**< Used to let the agios thread know that we have new requests. */
static pthread_mutex_t g_request_added_mutex = PTHREAD_MUTEX_INITIALIZER; /**< Used to protect the request_added_cond. */
static bool g_agios_thread_stop = false; /**< Set to true when the agios_exit function calls stop_the_agios_thread. */
static struct timespec g_last_algorithm_update; //the time at the last time we've selected an algorithm
static struct io_scheduler_instance_t *g_dynamic_scheduler=NULL; /**< The scheduling algorithm chosen in the configuration parameters. */ 

/**
 * function called when a new request is added to wake up the agios thread in case it is sleeping waiting for new requests.
 */
void signal_new_req_to_agios_thread(void)
{
	pthread_mutex_lock(&g_request_added_mutex);
	pthread_cond_signal(&g_request_added_cond);
	pthread_mutex_unlock(&g_request_added_mutex);
}
/**
 * function called by the agios_exit function to let the agios thread know we are finishing the execution.
 */
void stop_the_agios_thread(void)
{
	g_agios_thread_stop = true; //we chose not to protect this variable with a mutex because there is a single thread writing to it and a single thread reading it, the worse it could happen is that the agios thread does not read it correctly, but then it will read it later. It is not a big deal if the agios_exit call waits a little longer.
	//we signal the agios thread so it will wake up if it is sleeping
	signal_new_req_to_agios_thread();
}
/**
 * used to test if it is time to update the scheduling algorithm.
 * @return true or false.
 */
bool is_time_to_change_scheduler(void)
{
	if ((g_dynamic_scheduler->is_dynamic) && 
		(config_agios_select_algorithm_period >= 0) &&
		(agios_processed_reqnb >= config_agios_select_algorithm_min_reqnumber)) { 
		if (get_nanoelapsed(g_last_algorithm_update) >= config_agios_select_algorithm_period) return true;
	} 
	return false;
} 
/**
 * Fills a struct timespec (used by sleeping functions) with a provided value in nanoseconds
 * @param value_ns the value in nanoseconds
 * @param str the struct timespec to be filled
 */
void fill_struct_timespec(int32_t value_ns, struct timespec *str)
{
	str->tv_sec = value_ns / 1000000000;
	str->tv_nsec = value_ns % 1000000000;
}
/** 
 * the main function executed by the agios thread, which is responsible for processing requests that have been added to AGIOS.
 */
void * agios_thread(void *arg)
{
	struct timespec timeout; /**< Used to set a timeout for pthread_cond_timedwait, so the thread periodically checks if it has to end. */
	int32_t remaining_time = 0; /**< Used to calculate how long until we change the scheduling algorithm again */
	int32_t scheduler_waiting_time = 0; /**< Used to receive instructions from the scheduling algorithms to sleep for some time before calling them again (even if we have queued requests to be processed) */

	//find out which I/O scheduling algorithm we need to use
	g_dynamic_scheduler = initialize_scheduler(config_agios_default_algorithm); //if the scheduler has an init function, it will be called
	//a dynamic scheduling algorithm is a scheduling algorithm that periodically selects other scheduling algorithms to be used
	if (!g_dynamic_scheduler->is_dynamic) { //we are NOT using a dynamic scheduling algorithm
		current_alg = config_agios_default_algorithm;
		current_scheduler = g_dynamic_scheduler;  
	} else { //we ARE using a dynamic scheduler
		//with which algorithm should we start?
		current_alg = config_agios_starting_algorithm; 
		current_scheduler = initialize_scheduler(current_alg);
		agios_gettime(&g_last_algorithm_update);	//we will change the algorithm periodically		
	}
	performance_set_new_algorithm(current_alg);
	debug("selected algorithm: %s", current_scheduler->name);
	//since the current algorithm is decided, we can allow requests to be included
	unlock_all_data_structures();
	
	//execution loop, it only stops when we close the library
	do {
		//check if it is time to change the scheduling algorithm
		if (g_dynamic_scheduler->is_dynamic) {
			if (is_time_to_change_scheduler()) { //it is time to select!
				//make a decision on the next scheduling algorithm
				int32_t next_alg = g_dynamic_scheduler->select_algorithm();
				//change it
				debug("HEY IM CHANGING THE SCHEDULING ALGORITHM\n\n\n\n");
				change_selected_alg(next_alg);
				performance_set_new_algorithm(current_alg);
				reset_all_statistics(); //reset all stats so they will not affect the next selection
				unlock_all_data_structures(); //we can allow new requests to be added now
				agios_gettime(&g_last_algorithm_update); 
				debug("We've changed the scheduling algorithm to %s", current_scheduler->name);
				remaining_time = config_agios_select_algorithm_period;
			} else { //it is NOT time to select
				remaining_time = config_agios_select_algorithm_period - get_nanoelapsed(g_last_algorithm_update);
				if (remaining_time < 0) remaining_time = 0;
			}
		} //end scheduler is dynamic
		//if we have queued requests, try to process them
		if (0 < get_current_reqnb()) { //here we use the mutex to access the variable current_reqnb because we don't want to risk getting an outdated value and then sleeping for nothing
			scheduler_waiting_time = current_scheduler->schedule(); //the scheduler may have a reason to ask us for a sleeping time (for instance, TWINS keeps track of time windows) 
			if (scheduler_waiting_time > 0) { //the scheduling algorithm wants us to sleep for a while, so we'll respect that, and not with a cond_timedwait because this sleep is not to be interrupted by new request arrivals, and is not conditional to not having queued requests (we assume the scheduling algorithm knows what it is doing)
				fill_struct_timespec(agios_min(scheduler_waiting_time, remaining_time), &timeout); //if we are supposed to change the scheduling algorithm before the end of the waiting time provided by the scheduler, we just wait until then
				if (TWINS_SCHEDULER != current_alg) {
					nanosleep(&timeout, NULL);
				} else { //unless of course we are using TWINS. In that case the sleeping time is NOT to be respected unconditionally, we are sleeping because there are no requests to the server being accessed, but if some new requests arrive they could be to that server, and then we should call TWINS again
	 				pthread_mutex_lock(&g_request_added_mutex);
					pthread_cond_timedwait(&g_request_added_cond, &g_request_added_mutex, &timeout);
					pthread_mutex_unlock(&g_request_added_mutex);
				} //end if using TWINS
			} //end if scheduler_waiting_time > 0	
		} else { //we have no requests, so we sleep for a while (the default waiting time is provided in the configuration parameters), but this sleeping uses a conditional variable because we want to be called up if some new requests arrive (not having requests is the only reason why we are sleeping)
			 /* We use a timeout to avoid a situation where we missed the signal and will sleep forever, and
                          * also because we have to check once in a while to see if we should end the execution.
			  */
			//we have two possible scenarios here: first, remaining time is 0, that means we are not using a dynamic scheduler OR that we are, it is time to change the scheduling algorithm, but for some reason we are not ready to change it (because we have not processed enough requests in the period). In that case we may sleep at ease (for the usual amount of time) because if there are no queued requests, nothing will change (no requests will be processed so the decision of not changing the scheduling algorithm will not change). Second, if remaining time is greater than 0, that means we are using a dynamic scheduler AND we it is not yet time to change the scheduling algorithm. If that is supposed to happen earlier than our usual waiting time, we wake up earlier to respect that.
			if (remaining_time > 0) fill_struct_timespec(agios_min(config_waiting_time, remaining_time), &timeout);  
			else fill_struct_timespec(config_waiting_time, &timeout);
	 		pthread_mutex_lock(&g_request_added_mutex);
			pthread_cond_timedwait(&g_request_added_cond, &g_request_added_mutex, &timeout);
			pthread_mutex_unlock(&g_request_added_mutex);
		}
        } while (!g_agios_thread_stop);

	return 0;
}
