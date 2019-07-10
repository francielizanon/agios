/*! \file agios_counters.h
    \brief Headers to request and file counters.

    @see agios_counters.c
*/

#pragma once

#include <pthread.h>
#include <stdint.h>

extern int current_reqnb;
extern int current_filenb;

int32_t get_current_reqnb(void); 
void inc_current_reqnb(void);
void dec_current_reqnb(int32_t hash);
void dec_many_current_reqnb(int32_t hash, int32_t value);
void inc_current_filenb(void);
void dec_current_filenb(void);
