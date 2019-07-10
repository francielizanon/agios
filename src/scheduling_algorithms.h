/*! \file scheduling_algorithms.h
    \brief Definitions and parameters to all scheduling algorithms, and functions to handle them.
 */

#pragma once

#include "agios_request.h"

#define MAX_AGGREG_SIZE   16 /**< how many requests can be aggregated into a single virtual request (used by many schedulers). */

//identifiers of the scheduling algorithms
#define MLF_SCHEDULER 0
#define TOAGG_SCHEDULER 1
#define SJF_SCHEDULER 2
#define AIOLI_SCHEDULER 3
#define TO_SCHEDULER 4
#define SW_SCHEDULER 5
#define NOOP_SCHEDULER 6
#define TWINS_SCHEDULER 7
#define IO_SCHEDULER_COUNT 8  /*! \warning this has to be updated if adding or removing schedulign algorithms */

struct io_scheduler_instance_t {
	bool (*init)(void); /**< called to initialize the scheduler. MUST return true or false for success. This function is not mandatory, can be NULL. */ 
	void (*exit)(void); /**< called to end a scheduler. MUST return true or false for success. This function is not mandatory, can be NULL. */
	int64_t (*schedule)(void); /**< called to schedule some requests. This function MUST NOT sleep. Instead, a waiting time can be provided to the caller. That waiting time will be respected EVEN IF there are queued requests, so it is to be used wisely. This function is mandatory, except for dynamic schedulers, which can provide NULL. */
	int32_t (*select_algorithm)(void); /**< Normal scheduling algorithms must provide NULL, this function is only provided by dynamic schedulers. It returns the next algorithm to be used. */
	bool needs_hashtable; /**< Does this scheduler uses the hashtable to hold the requests? If not, then timeline is used. */
	int32_t max_aggreg_size; /**< Maximum number of requests to be aggregated at once. */
	bool can_be_dynamically_selected; /**< Can this algorithm be selected by dynamic algorithms? Some algorithms need special conditions (like available trace files or application ids) or are still experimental, so we may not want them to be selected by the dynamic selectors. */
	bool is_dynamic; /**< is this algorithm a dynamic one, which does not schedule requests but instead periodically choses another scheduling algorithm to do so? */
	char name[22]; /**< algorithm name */
	int32_t index; /**< index in the io_schedulers list (also the identifier of the scheduling algorithm, see above) */
};

extern int32_t current_alg;
extern struct io_scheduler_instance_t *current_scheduler;

void change_selected_alg(int32_t new_alg);
bool get_algorithm_from_string(const char *alg, int32_t *index);
char *get_algorithm_name_from_index(int32_t index);
struct io_scheduler_instance_t *find_io_scheduler(int32_t index);
struct io_scheduler_instance_t *initialize_scheduler(int32_t index);
void enable_SW(void);
void generic_post_process(struct request_t *req);

