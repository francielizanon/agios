/*! \file agios.h
    \brief Interface from users to the AGIOS library. 

    Users start using the library by calling agios_init providing the callbacks to be used to process requests and the path to a configuration file. Then new requests are added to the library with agios_add_request. When the scheduling policy being applied decides it is time to process a request, AGIOS will call the callback functions provided by the user to agios_init. Later the user has to be sure to call agios_release_request to let AGIOS know the request has been processed, or call agios_cancel_request earlier to cancel that request. Before ending, the user must call agios_exit to cleanup all allocated memory.
*/
#pragma once 

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/** \enum 
 *  \brief The type of the request to be provided to agios_add_request.
 */
enum {
	RT_READ = 0,
	RT_WRITE = 1,
};
bool agios_init(void * process_request_user(int64_t req_id), 
		void * process_requests_user(int64_t *reqs, int32_t reqnb), 
		char *config_file, 
		int32_t max_queue_id);
void agios_exit(void);
bool agios_add_request(char *file_id, 
			int32_t type, 
			int64_t offset, 
			int64_t len, 
			int64_t identifier, 
			int32_t queue_id,
			void *callback(int64_t req_id, void* user_info),
			void *user_info);
bool agios_release_request(char *file_id, 
				int32_t type, 
				int64_t len, 
				int64_t offset); 
bool agios_cancel_request(char *file_id, 
				int32_t type, 
				int64_t len, 
				int64_t offset);
/*! \struct agios_metrics_t
    \brief The structure used by agios to report metrics collected over recent accesses. These metrics are relevant to the period since the last reset.
 */
struct agios_metrics_t {
	int64_t total_reqnb; /**< number of received requests. */
	int64_t reads; /**< number of received read requests. */
	int64_t writes; /**< number of received write requests. */
	int64_t avg_time_between_requests; /**< average time between consecutive requests in ns. */
	int64_t avg_request_size; /**< average request size in bytes. */
	int64_t max_request_size; /**< the maximum observed request size in bytes. */
	int64_t filenb;  /**< the number of accessed files. */
	int64_t avg_offset_distance; /**< the average offset distance between consecutive requests to the same file (in bytes).*/
	int64_t served_bytes; /**< the total amount of bytes accessed by requests that were processed and released. */
};
struct agios_metrics_t *agios_get_metrics_and_reset(void);
#ifdef __cplusplus
}
#endif

