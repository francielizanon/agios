/*! \file scheduling_algorithms.c
    \brief Definitions and parameters to all scheduling algorithms, and functions to handle them.
*/
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "aIOLi.h"
#include "data_structures.h"
#include "MLF.h"
#include "NOOP.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"
#include "SJF.h"
#include "statistics.h"
#include "SW.h"
#include "TO.h"
#include "TWINS.h"

int32_t current_alg = 0; /**< the identifier of the scheduling algorithm being currently used. @see scheduling_algorithms.h */
struct io_scheduler_instance_t *current_scheduler=NULL; /**< a pointer to the structure describing the scheduling algorithm being currently used. */
/** 
 * the list of all scheduling algorithms and their parameters.
 */
static struct io_scheduler_instance_t io_schedulers[] = { 
		{
			.name = "MLF",
			.index = MLF_SCHEDULER,
			.init = MLF_init,
			.schedule = &MLF,
			.exit = MLF_exit,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.needs_hashtable=true,
			.can_be_dynamically_selected=true,
			.is_dynamic=false,
		},
		{
			.name = "TO-agg",
			.index = TOAGG_SCHEDULER,
			.init = NULL,
			.schedule = &timeorder,  
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.needs_hashtable=false,
			.can_be_dynamically_selected=true,
			.is_dynamic=false,
		},
		{
			.name = "SJF",
			.index = SJF_SCHEDULER,
			.init = NULL,
			.schedule = &SJF,
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.needs_hashtable=true,
			.can_be_dynamically_selected=true,
			.is_dynamic=false,
		},
		{
			.name = "aIOLi",
			.index = AIOLI_SCHEDULER,
			.init = NULL,
			.schedule = &aIOLi,
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = MAX_AGGREG_SIZE,
			.needs_hashtable=true,
			.can_be_dynamically_selected=false,
			.is_dynamic=false,
		},
		{
			.name = "TO",
			.index = TO_SCHEDULER,
			.init = NULL,
			.schedule = &timeorder, 
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.needs_hashtable=false,
			.can_be_dynamically_selected=true,
			.is_dynamic=false,
		},
		{
			.name = "SW",
			.index = SW_SCHEDULER,
			.init = NULL,
			.schedule = &SW,
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.needs_hashtable=false, 
			.can_be_dynamically_selected=false,
			.is_dynamic=false,
		},
		{
			.name = "NOOP",
			.index = NOOP_SCHEDULER,
			.init = NULL,
			.schedule = &NOOP,
			.exit = NULL,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.needs_hashtable= false,
			.can_be_dynamically_selected=true,
			.is_dynamic=false,
		},
		{
			.name = "TWINS",
			.index = TWINS_SCHEDULER,
			.init = &TWINS_init,
			.schedule = &TWINS,
			.exit = &TWINS_exit,
			.select_algorithm = NULL,
			.max_aggreg_size = 1,
			.needs_hashtable = false, 
			.can_be_dynamically_selected = false, //The functions that implement the migration between different scheduling algorithms were not adapted for this algorithm, so it should never be used with a dynamic algorithm until we fix that.  
			.is_dynamic=false,
		}
	};
/**
 * Called to change the current scheduling algorithm and update local parameters. Here we assume the scheduling thread is NOT running, so it won't mess with the structures. This function will acquire the lock to all data structures, must call unlock afterwards 
 * @param new_alg identifier of the new scheduling algorithm.
 */
void change_selected_alg(int32_t new_alg)
{
	int32_t previous_alg; /**< will receive the current_alg while we are changing it to new_alg. */
	struct io_scheduler_instance_t *previous_scheduler; /**< will receive current_scheduler while we are changing it to the new one. */

	//lock all data structures so no one is adding or releasing requests while we migrate
	lock_all_data_structures();
	if (current_alg != new_alg) { //if we are indeed changing something
		//change scheduling algorithm
		previous_scheduler = current_scheduler;
		previous_alg = current_alg;
		current_scheduler = initialize_scheduler(new_alg);
		current_alg = new_alg;
		//do we need to migrate data structure?
		//first situation: both use hashtable
		if (current_scheduler->needs_hashtable && previous_scheduler->needs_hashtable) {
			//the only problem here is if we decreased the maximum aggregation
			//For now we chose to do nothing. If we no longer tolerate aggregations of a certain size, we are not spliting already performed aggregations since this would not benefit us at all. We could rethink that at some point
		}
		//second situation: from hashtable to timeline
		else if (previous_scheduler->needs_hashtable && (!current_scheduler->needs_hashtable)) {
			print_hashtable();
			migrate_from_hashtable_to_timeline();
			print_timeline();
		}
		//third situation: from timeline to hashtable
		else if ((!previous_scheduler->needs_hashtable) && current_scheduler->needs_hashtable) {
			print_timeline();
			migrate_from_timeline_to_hashtable();
			print_hashtable();
		} else { //fourth situation: both algorithms use timeline
			//now it depends on the algorithms. 
			//if we are changing to NOOP, it does not matter because it does not really use the data structure
			//if we are changing from or to SW or TWINS, we need to reorder the list
			//if we are changing to the timeorder with aggregation, we need to reorder the list
			if ((current_alg != NOOP_SCHEDULER) && 
			   ((previous_alg == SW_SCHEDULER) || (current_alg == SW_SCHEDULER) || (current_alg == TWINS_SCHEDULER) || (previous_alg == TWINS_SCHEDULER))) {
				reorder_timeline(); 
			}
		} //end fourth situation 
	} //end if changing the scheduler
}
/**
 * finds and returns the current scheduler indicated by index. If this scheduler needs an initialization function, calls it.
 * @param index the identifier of the scheduler.
 * @return a pointer to the initialized scheduler instance, NULL on error.
 */
struct io_scheduler_instance_t *initialize_scheduler(int32_t index) 
{
	//sanity check
	if ((index >= IO_SCHEDULER_COUNT) || (index < 0)) return NULL;
	if (io_schedulers[index].init) {
		if (!io_schedulers[index].init()) return NULL;
	}
	return &(io_schedulers[index]);
}
/** 
 * finds an returns a scheduler indicated by index.
 * @param index the identifier of the scheduler.
 * @return a pointer to the scheduler instance, NULL on error.
 */
struct io_scheduler_instance_t *find_io_scheduler(int32_t index)
{
	if ((index >= IO_SCHEDULER_COUNT) || (index < 0)) return NULL;
	return &(io_schedulers[index]);
}
/** 
 * finds and returns a scheduler identified by its name
 * @param alg the algorithm name.
 * @param index will be modified to contain the index of the algorithm.
 * @return true or false for errors.
 */
bool get_algorithm_from_string(const char *alg, int32_t *index)
{
	*index = SJF_SCHEDULER; //default in case we can't find it
	for (int32_t i=0; i < IO_SCHEDULER_COUNT; i++)
	{
		if (strcmp(alg, io_schedulers[i].name) == 0) {
			*index = i;
			return true;
		}
	}
	return false; //we will only get here if we did not find it
}
/**
 * finds and returns the name of the scheduler given its identifier.
 * @param index the scheduler identifier
 * @return the scheduler name (a pointer to it), NULL on error.
 */
char * get_algorithm_name_from_index(int32_t index)
{
	if ((index >= IO_SCHEDULER_COUNT) || (index < 0)) return NULL;
	return io_schedulers[index].name;
}
/**
 * used to change the parameter of the SW scheduler to make it possible to be selected dynamically (by default it isn't).
 */
void enable_SW(void)
{
	io_schedulers[SW_SCHEDULER].can_be_dynamically_selected = true;
}
/**
 * Called after processing a request to update some statistics and possibly cleanup a virtual request structure. 
 * @param req the request that was processed.
 */
void generic_post_process(struct request_t *req)
{
	req->globalinfo->lastaggregation = req->reqnb; 
	if (req->reqnb > 1) { //this was an aggregated request
		stats_aggregation(req->globalinfo);
		req->reqnb = 1; //we need to set it like this otherwise the request_cleanup function will try to free the sub-requests, but they were inserted in the dispatch queue and we will only free them after the release
		request_cleanup(req);
	}
}
