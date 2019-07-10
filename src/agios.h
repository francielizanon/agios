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
			int32_t queue_id);
bool agios_release_request(char *file_id, 
				int32_t type, 
				int64_t len, 
				int64_t offset); 
bool agios_cancel_request(char *file_id, 
				int32_t type, 
				int64_t len, 
				int64_t offset);
#ifdef __cplusplus
}
#endif

