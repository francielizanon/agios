/*! \file data_structures.c
    \brief Headers of unctions to initialize and migrate between data structures.
 */
#pragma once

#include <stdbool.h>

void migrate_from_hashtable_to_timeline();
void migrate_from_timeline_to_hashtable();
void lock_all_data_structures();
void unlock_all_data_structures();
bool allocate_data_structures(int32_t max_app_id);
void cleanup_data_structures(void);
bool acquire_adequate_lock(int32_t hash);
