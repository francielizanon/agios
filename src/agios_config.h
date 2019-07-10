/*! \file agios_config.h
    \brief Headers (and extern declarations) of AGIOS configuration parameters.

    Configuration parameters are provided in a configuration file (its name is given by the user in the agios_init function), and read using the libconfig library.
*/
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DEFAULT_CONFIGFILE	"/etc/agios.conf" /**< If a filename is not provided in agios_init, we'll try to read from this one */

bool read_configuration_file(char *config_file);
void cleanup_config_parameters(void);
//about tracing
extern bool config_trace_agios;
extern char *config_trace_agios_file_prefix;
extern char *config_trace_agios_file_sufix;
extern int32_t config_agios_max_trace_buffer_size;
//about scheduling
extern int32_t config_agios_default_algorithm;
extern int64_t config_agios_select_algorithm_period;
extern int32_t config_agios_select_algorithm_min_reqnumber;
extern int32_t config_agios_starting_algorithm;
extern int32_t config_waiting_time;
extern int32_t config_aioli_quantum;
extern int32_t config_mlf_quantum;
extern int64_t config_sw_size;
extern int64_t config_twins_window;
//performance module 
extern int32_t config_agios_performance_values;
