/*! \file agios_add_request.h
    \brief Headers for the implementation of the agios_add_request function, used by the user to add requests to AGIOS.
*/  
#pragma once

#include "agios_request.h"
#include "mylist.h"

/**
 * says if two requests to the same file are contiguous or not.
 */
#define CHECK_AGGREGATE(req,nextreq) \
     ( (req->offset <= nextreq->offset)&& \
         ((req->offset+req->len)>=nextreq->offset))

struct file_t *find_req_file(struct agios_list_head *hash_list, 
					char *file_id);
int32_t insert_aggregations(struct request_t *req, 
				struct agios_list_head *insertion_place, 
				struct agios_list_head *list_head);
void include_in_aggregation(struct request_t *req, struct request_t **agg_req);
