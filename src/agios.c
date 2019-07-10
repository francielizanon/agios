/*! \file agios.c
    \brief Implementation of the agios_init and agios_exit functions, used to start and end the library.

    Users start using the library by calling agios_init providing the callbacks to be used to process requests and the path to a configuration file. Then new requests are added to the library with agios_add_request. When the scheduling policy being applied decides it is time to process a request, AGIOS will call the callback functions provided by the user to agios_init. Later the user has to be sure to call agios_release_request to let AGIOS know the request has been processed, or call agios_cancel_request earlier to cancel that request. Before ending, the user must call agios_exit to cleanup all allocated memory.
*/
#include <stdbool.h>

#include "agios.h"
#include "agios_config.h"
#include "agios_thread.h"
#include "common_functions.h"
#include "data_structures.h"
#include "performance.h"
#include "process_request.h"
#include "scheduling_algorithms.h"
#include "trace.h"

static pthread_t g_agios_thread; /**< AGIOS thread that will run the AGIOS_thread function.  */

/**
 * function used by agios_exit and agios_init (in case of errors) to clean up all allocated memory.
 */
void cleanup_agios(void)
{
	cleanup_config_parameters();
	cleanup_performance_module();
	cleanup_data_structures();
	if (config_trace_agios) {
		close_agios_trace();
		cleanup_agios_trace();
	}
}

/**
 * function called by the user to initialize AGIOS. It will read parameters, allocate memory and start the AGIOS thread.
 * @param process_request the callback function from the user code used by AGIOS to process a single request. (required)
 * @param process_requests the callback function from the user code used by AGIOS to process a list of requests. (optional)
 * @param config_file the path to a configuration file. If NULL, the DEFAULT_CONFIGFILE will be read instead. If the default configuration file does not exist, it will use default values.
 * @param max_queue_id for schedulers that use multiple queues, one per server/application (TWINS and SW), define the number of queues to be used. If it is not relevant to the used scheduler, it is better to provide 0. With each request being added, a value between 0 and max_queue_id-1 is to be provided.
 * @see agios_config.c
 * @return true of false for success.
 */
bool agios_init(void * process_request_user(int64_t req_id), 
		void * process_requests_user(int64_t *reqs, int32_t reqnb), 
		char *config_file, 
		int32_t max_queue_id)
{
	//check if a callback was provided 
	if (!process_request_user) {
		agios_print("Incorrect parameters to agios_init\n");
		return false; //we don't use the goto cleanup_on_error because we have nothing to clean up
	}
	user_callbacks.process_request_cb = process_request_user;
	user_callbacks.process_requests_cb = process_requests_user;
	if (!read_configuration_file(config_file)) goto cleanup_on_error; 
	if (!allocate_data_structures(max_queue_id)) goto cleanup_on_error;
	//if we are going to generate traces, init the tracing module
	if (config_trace_agios) {
		if (!init_trace_module()) goto cleanup_on_error;
	}
	//init the AGIOS thread
	int32_t ret = pthread_create(&g_agios_thread, NULL, agios_thread, NULL);
	if (ret != 0) {
                agios_print("Unable to start a thread to agios!\n");
		goto cleanup_on_error;
	}
	//success, finish the function call
	return true;
cleanup_on_error:  //used to abort the initialization if anything goes wrong
	cleanup_agios();
	return false;
}

/**
 * function called by the user to stop AGIOS. It will stop the AGIOS thread and free all allocated memory.
 */
void agios_exit(void)
{
	//stop the agios thread
	stop_the_agios_thread();
	pthread_join(g_agios_thread, NULL);
	if (current_scheduler->exit) current_scheduler->exit(); //the exit function is not mandatory for schedulers
	//cleanup memory
	cleanup_agios();
	agios_print("stopped for this client. AGIOS can be used again by calling agios_init\n");
}
