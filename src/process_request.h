/*! \file process_request.h
    \brief Implementation of the processing of requests, when they are sent back to the user through the callback functions.
 */
#pragma once

#include "agios_request.h"

/* \struct agios_client is a struct used for a single variable, user_callbacks, filled by the agios_init function to store the pointers to the user-provided callbacks, used to process requests. 
 */
struct agios_client {
	void * (* process_request_cb)(int64_t req_id); /**< a function to process a single request. */
	void * (* process_requests_cb)(int64_t *reqs, int32_t reqnb); /**< a function to process a list of requests at once. This one might be NULL if the user did not provide it. */
};
/* \struct processing_info_t is a struct to hold information about one or more requests that are to be processed. It is filled by the process_requests_step1 function and used in the process_requests_step2 to send requests back to the user through the provided callbacks. 
 */
struct processing_info_t {
	int64_t *user_ids; /**< a list of requests, each request is represented by the user_id field, provided to agios_add_request as a request identifier that makes sense to the user */
	int32_t reqnb; /**< the lenght of the user_ids list (number of requests) */
	struct agios_list_head list; /**< used to be inserted in a list (for MLF and aIOLi only) */
};

extern struct agios_client user_callbacks;	

struct processing_info_t *process_requests_step1(struct request_t *head_req, int32_t hash);
bool process_requests_step2(struct processing_info_t *info);
