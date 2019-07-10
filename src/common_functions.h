/*! \file common_functions.h
    \brief Miscellaneous functions used everywhere in AGIOS source code.
 */

#pragma once

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define agios_gettime(timev)					clock_gettime(CLOCK_MONOTONIC, timev)
#define agios_print(f, a...) 					fprintf(stderr, "AGIOS: " f "\n", ## a)
#define agios_just_print(f, a...) 				fprintf(stderr, f, ## a)
//debug functions
#ifdef AGIOS_DEBUG
#define PRINT_FUNCTION_NAME agios_print("%s\n", __PRETTY_FUNCTION__)
#define PRINT_FUNCTION_EXIT agios_print("%s exited\n", __PRETTY_FUNCTION__)
#define debug(f, a...) agios_print("%s(): " f "\n", __PRETTY_FUNCTION__, ## a)
#else //NOT in debug mode
#define PRINT_FUNCTION_NAME (void)(0)
#define PRINT_FUNCTION_EXIT (void)(0)
#define debug(f, a...) (void)(0)
#endif /*end ifdef AGIOS_DEBUG - else*/


int64_t agios_min(int64_t t1, int64_t t2);
int64_t agios_max(int64_t t1, int64_t t2);
int32_t get_index_max(int32_t *count);
int64_t get_nanoelapsed(struct timespec t1);
int64_t get_timespec2long(struct timespec t);
void get_long2timespec(int64_t t, struct timespec *ret);
int64_t get_nanoelapsed_long(int64_t t1);
double get_ns2s(int64_t t1);
int64_t update_iterative_average(int64_t avg, int64_t value, int64_t count);


