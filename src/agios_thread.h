/*! \file agios_thread.h
    \brief Headers of functions used to run and interact with the agios thread.

    @see agios_thread.c
*/
#pragma once


void * agios_thread(void *arg);
void stop_the_agios_thread(void);
void signal_new_req_to_agios_thread(void);
bool is_time_to_change_scheduler(void);
