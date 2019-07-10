/*! \file req_hashtable.c
    \brief Implementation of the hashtable, used to store information about files and request queues for some scheduling algorithms.
*/
#pragma once

#include "agios_request.h"

#define AGIOS_HASH_SHIFT 6						
#define AGIOS_HASH_ENTRIES		(1 << AGIOS_HASH_SHIFT) 		

extern struct agios_list_head *hashlist;
extern int32_t *hashlist_reqcounter;

bool hashtable_init(void);
void hashtable_cleanup(void);
bool hashtable_add_req(struct request_t *req, 
			int32_t hash_val, 
			struct file_t *given_req_file);
void hashtable_safely_del_req(struct request_t *req);
void hashtable_del_req(struct request_t *req);
struct agios_list_head *hashtable_lock(int32_t index);
struct agios_list_head *hashtable_trylock(int32_t index);
void hashtable_unlock(int32_t index);
void print_hashtable_line(int32_t i);
void print_hashtable(void);
