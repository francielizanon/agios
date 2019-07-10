/*! \file waiting_common.h
    \brief Headers for functions used by aIOLi and MLF.
*/

#pragma once
#include "agios_request.h"

void update_waiting_time_counters(struct file_t *req_file, 
					int32_t *shortest_waiting_time);
bool check_selection(struct request_t *req, 
			struct file_t *req_file);
void increment_sched_factor(struct request_t *req);
void waiting_algorithms_postprocess(struct request_t *req);
bool call_step2_for_info_list(struct agios_list_head *info_list);
