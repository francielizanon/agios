/*! \file agios_request.c 
    \brief Some functions to deal with struct request_t (used to keep information about requests).
 */
#include <stdlib.h>

#include "agios_request.h"
#include "common_functions.h"

/** 
 * prints information about a request, used for debug.
 * @param req the request
 */
void print_request(struct request_t *req)
{
	if (req->reqnb > 1) {
		struct request_t *aux_req; /**< used to iterate through the requests inside this virtual request. */
		debug("\t\t\t%ld %ld", req->offset, req->len);
		debug("\t\t\t\t\t(virtual request size %d)", req->reqnb);
		agios_list_for_each_entry (aux_req, &req->reqs_list, related) debug("\t\t\t\t\t(%ld %ld %s)", aux_req->offset, aux_req->len, aux_req->file_id);
	} else debug("\t\t\t%ld %ld", req->offset, req->len);
}
/**
 * free all requests from a list of requests.
 * @see request_cleanup
 * @param list the list of requests.
 */
void list_of_requests_cleanup(struct agios_list_head *list)
{
	struct request_t *req; /**< used to iterate over the list of requests. */
	struct request_t *aux_req = NULL; /**< used to avoid freeing the request before moving the iterator to the next one, otherwise the loop breaks. */

	if (!agios_list_empty(list)) { //if the list is not empty
		agios_list_for_each_entry (req, list, related) { //go through all requests in the list
			if (aux_req) request_cleanup(aux_req);
			aux_req = req;
		} 
		if (aux_req) request_cleanup(aux_req);
	} //end if the list was not empty
}
/**
 * free the space used by a struct request_t, which describes a request. If the request is aggregated (with multiple requests inside), it will recursively free these requests as well.
 * @param aux_req the request to be freed.
 */
void request_cleanup(struct request_t *aux_req)
{
	//remove the request from its queue
	agios_list_del(&aux_req->related);
	//see if it is a virtual request
	if (aux_req->reqnb > 1) {
		//free all sub-requests
		list_of_requests_cleanup(&aux_req->reqs_list);
	}
	//free the memory
	if (aux_req->file_id) free(aux_req->file_id);
	free(aux_req);
}
