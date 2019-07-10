/*! \file req_timeline.c
    \brief Implementation of the timeline, used as request queue to some scheduling algorithms.
 */
#pragma once

#include "agios_request.h"

extern struct agios_list_head timeline;
extern struct agios_list_head *multi_timeline;
extern int32_t multi_timeline_size;

struct agios_list_head *timeline_lock(void);
void timeline_unlock(void);
bool timeline_add_req(struct request_t *req, int32_t hash, struct file_t *given_req_file);
void reorder_timeline(void);
struct request_t *timeline_oldest_req(int32_t *hash);
bool timeline_init(int32_t max_queue_id);
void timeline_cleanup(void);
void print_timeline(void);
