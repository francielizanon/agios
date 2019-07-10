/*! \file agios_config.c
    \brief Configuration parameters, default values and a function to read them from a configuration file (with libconfig).
 */
#include <assert.h>
#include <libconfig.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "agios_config.h"
#include "common_functions.h"
#include "scheduling_algorithms.h"

int32_t config_agios_default_algorithm = SJF_SCHEDULER;	/**< scheduling algorithm to be used (the identifier of the scheduling algorithm) */
int32_t config_agios_max_trace_buffer_size = 1*1024*1024; /**< in bytes. A buffer is used to keep trace messages before going to the file, to avoid small writes to the disk and decrease tracing overhead. This parameter gives the size allocated for the buffer. */
int32_t config_agios_performance_values = 5; /**< for how many of the last scheduling algorithm selections should we keepperformance metrics. */
int64_t config_agios_select_algorithm_period=-1;	/**< if the scheduling algorithm is dynamic (meaning it will actually select other scheduling algorithms during the execution, this parameter defines the periodicity to change the scheduling algorithm during the execution. */
int32_t config_agios_select_algorithm_min_reqnumber=1;	/**< if the scheduling algorithm is dynamic (meaning it will actually select other scheduling algorithms during the execution, this parameter defines how many requests have to be treated during a period before a new scheduling algorithm can be selected. */
int32_t config_agios_starting_algorithm = SJF_SCHEDULER; /**< if the scheduling algorithm is dynamic (meaning it will actually select other scheduling algorithms during the execution, this is the scheduling algorithm that will be used whenever a decision cannot be made (possibly because there is not enough information */
int32_t config_aioli_quantum = 8192;			/**< in bytes, how much of a queue can be processed before going to the next one (used by aIOLi) */
int32_t config_mlf_quantum = 8192;			/**< similar to config_aioli_quantum */ 
int64_t config_sw_size = 1000000000L;			/**< the window size used for the SW scheduling algorithm */
bool config_trace_agios=false;				/**< will agios create a trace file will all requests arrivals? */
char *config_trace_agios_file_prefix=NULL; 		/**< if creating trace files, they will be named config_trace_agios_file_prefix.*.config_trace_agios_file_sufix. The value in the middle of prefix and sufix is a counter, the library will check for existing files so they are not overwritten. */
char *config_trace_agios_file_sufix=NULL;		/**< @see config_trace_agios_file_prefix */
int64_t config_twins_window=1000000L; 		/**< The amount of time TWINS will stay in one queue before moving on to the next one (in nanoseconds). The default is 1ms */
int32_t config_waiting_time = 900000;			/**< when there are no requests, the scheduler sleep using this as a timeout. It is also used by aIOLi to wait if it thinks better aggregations are possible */

/**
 * used to clean all memory allocated for the configuration parameters (at the end of the execution).
 */
void cleanup_config_parameters(void)
{
	if(config_trace_agios_file_prefix)
		free(config_trace_agios_file_prefix);
	if(config_trace_agios_file_sufix)
		free(config_trace_agios_file_sufix);
}
/**
 * simple function that receives an int and returns a bool version of it. Used while reading the parameters (because libconfig does not have a bool type).
 * @param input the input value to be converted
 * @return true if input != 0, false otherwise.
 */
bool convert_inttobool(int32_t input)
{
	if (0 == input) return false;
	else return true;
}
/** 
 * simple function that receives a message and a flag and prints the message, "YES" or "NO" depending on the flag, and then a new line.
 * @param flag the flag. If it is true we will print "YES", otherwise we'll print "NO"
 * @param message a string to be printed before the part that depends on the flag.
 */
void config_print_flag(bool flag, const char *message)
{
	if (flag) agios_just_print("%sYES.\n", message);
	else agios_just_print("%sNO.\n", message);
}
/**
 * function called during the initialization to print configuration parameters that will be used by AGIOS.
 */
void config_print(void)
{
	agios_just_print("Scheduling algorithm: %s\n", get_algorithm_name_from_index(config_agios_default_algorithm)); 
	agios_just_print("If the scheduling algorithm is dynamic, we will start with %s and keep statistics about the last %d used algorithms.\n", get_algorithm_name_from_index(config_agios_starting_algorithm), config_agios_performance_values);
	agios_just_print("Also, if the scheduling algorithm is dynamic, we will change the used scheduler every %ld ns, as long as %d requests were processed.\n",config_agios_select_algorithm_period, config_agios_select_algorithm_min_reqnumber);
	agios_just_print("If aIOLi is used, its quantum is %d.\n If MLF is used, its quanutm is %d.\n If SW is used, its window size is %ld.\n If TWINS is used, its window duration is %ld.\n", config_aioli_quantum, config_mlf_quantum, config_sw_size, config_twins_window);
	agios_just_print("The default waiting time for the AGIOS thread is %d\n", config_waiting_time);
	config_print_flag(config_trace_agios, "Will AGIOS generate trace files? ");
	if (config_trace_agios) {
		agios_just_print("\tTrace files are named %s.*.%s\n", config_trace_agios_file_prefix, config_trace_agios_file_sufix);
		agios_just_print("\tTrace file buffer has size %d bytes\n", config_agios_max_trace_buffer_size);
	} //end if tracing
}
/**
 * function used to read the configuration parameters from a configuration file. It uses libconfig to do so. 
 * @param config_file the name (with path) of the configuration file. If NULL is provided, then the function will read from DEFAULT_CONFIGFILE instead. If the default file does not exist, the default values will be used.
 * @return true or false for success.
 */
bool read_configuration_file(char *config_file)
{
	int32_t ret; /**< used to capture return values from libconfig */
	const char *ret_str; /**< used to capture return values from libconfig */
	config_t agios_config; /**< used to interact with libconfig */

	//create a configuration with libconfig
	config_init(&agios_config); 
	//read it from a file
	if ((!config_file) || (strlen(config_file) < 1)) ret = config_read_file(&agios_config, DEFAULT_CONFIGFILE);
	else ret = config_read_file(&agios_config, config_file);
 	//check if it was possible to read from a file
	if (ret != CONFIG_TRUE) { //it failed
		agios_just_print("Error reading agios config file\n%s", config_error_text(&agios_config));
		//we'll just run with default values
		return true; 
	} //end if reading from input file failed
	//if we are here we successfully read configuration parameters from the file, so we have to obtain then from libconfig and store in out variables
	/*1. library options*/
	config_lookup_bool(&agios_config, "library_options.trace", &ret);
	config_trace_agios = convert_inttobool(ret);
	config_lookup_string(&agios_config, "library_options.trace_file_prefix", &ret_str);
	config_trace_agios_file_prefix = malloc(sizeof(char)*(strlen(ret_str)+1));
	if (!config_trace_agios_file_prefix) return false;
	strcpy(config_trace_agios_file_prefix, ret_str);
	config_lookup_string(&agios_config, "library_options.trace_file_sufix", &ret_str);
	config_trace_agios_file_sufix = malloc(sizeof(char)*(strlen(ret_str)+1));
	if (!config_trace_agios_file_sufix) return false;
	strcpy(config_trace_agios_file_sufix, ret_str);
	config_lookup_string(&agios_config, "library_options.default_algorithm", &ret_str);
	if (false == get_algorithm_from_string(ret_str, &config_agios_default_algorithm)) return false;
	config_lookup_int(&agios_config, "library_options.waiting_time", &ret);
	config_waiting_time = ret;
	config_lookup_int(&agios_config, "library_options.aioli_quantum", &ret);
	config_aioli_quantum = ret;
	config_lookup_int(&agios_config, "library_options.mlf_quantum", &ret);
	config_mlf_quantum = ret;
	config_lookup_int(&agios_config, "library_options.select_algorithm_period", &ret);
	config_agios_select_algorithm_period = ret*1000000L; //convert it to ns
	config_lookup_int(&agios_config, "library_options.select_algorithm_min_reqnumber", &config_agios_select_algorithm_min_reqnumber);
	config_lookup_string(&agios_config, "library_options.starting_algorithm", &ret_str);
	if (false == get_algorithm_from_string(ret_str, &config_agios_starting_algorithm)) return false;
#if 0 //test if the starting algorithm is a dynamic one
	if((config_agios_starting_algorithm == DYN_TREE_SCHEDULER) || (config_agios_starting_algorithm == ARMED_BANDIT_SCHEDULER))
	{
		config_agios_starting_algorithm = SJF_SCHEDULER;
		agios_print("Configuration error! Starting algorithm cannot be a dynamic one. Using SJF instead");
	}
#endif
	config_lookup_int(&agios_config, "library_options.performance_values", &config_agios_performance_values);
	config_lookup_bool(&agios_config, "library_options.enable_SW", &ret);
	if (ret) enable_SW();
	config_lookup_int(&agios_config, "library_options.SW_window", &ret);
	config_sw_size = ret*1000000L; //convert to ns
	assert(config_sw_size >= 0);
	config_lookup_int(&agios_config, "library_options.twins_window", &ret);
	config_twins_window = ret*1000L; //convert us to ns
	assert(config_twins_window >= 0);
	config_lookup_int(&agios_config, "library_options.max_trace_buffer_size", &ret);
	config_agios_max_trace_buffer_size = ret*1024; //it comes in KB, we store in bytes
	//cleanup the libconfig structure
	config_destroy(&agios_config);
	config_print();
	return true;
} 
/**
 * receives a flag and a message to be printed with it. It will print the message and then yes or no depending on the flag value.
 * @param flag true or false to print yes or no
 * @param the message that will be printed BEFORE the flag value
 */
void print_flag(bool flag, char *message)
{
	agios_just_print("%s", message);
	if (flag) agios_just_print("YES\n");
	else agios_just_print("NO\n");
}


