/*! \file trace.c
    \brief Trace Module, that creates a trace file during execution with information about all requests arrivals.

    Trace files are named according as prefix.N.sufix with prefix and sufix given as configuration parameters and N being a counter. In the initialization of this module, it tries to open trace files with increasing values to N until finding an unused one that will be used as the new trace file. A buffer is used to write information before sending it to disk, in order to avoid generating a large number of small requests to the local storage (and also to minimize the interference with the scheduled requests in case they are also to the local storage). The length of this buffer is also a configuration parameter.
    @see agios_config.c
 */
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "agios.h"
#include "agios_config.h"
#include "agios_request.h"
#include "common_functions.h"


static FILE *agios_tracefile_fd; /**< the current trace file*/
static pthread_mutex_t agios_trace_mutex = PTHREAD_MUTEX_INITIALIZER; /**< since multiple threads call the Trace Module's functions, this mutex makes sure only one tries to access the trace file at a time. The thread calling the functions MUST NOT lock it, the function itself handles it. */
static struct timespec agios_trace_t0; /**< time measured at initialization (all traced times are relative to this one). */
static char *agios_tracefile_buffer=NULL; /**< a buffer avoids generating many I/O operations to the trace file, which is stored in the local file system. This way we also minimize interference with the scheduled requests (in case they are also to the local file system. */ 
static int32_t agios_tracefile_buffer_size=0; /**< occupancy of the buffer. Used to control when to flush it. */
static char *aux_buf = NULL; /**< this smaller buffer is used by the functions to write a line at a time to the main buffer. We keep it global to avoid having to allocate it multiple times (it is allocated during initialization). */
static int32_t aux_buf_size = 300*sizeof(char); /**< the size for the smaller buffer. This is hardcoded. Shame!*/

/**
 * flushes the buffer to the tracefile and resets it. The caller must have the trace lock.
 */
void agios_tracefile_flush_buffer(void)
{
	/*write it*/
	if (fprintf(agios_tracefile_fd, "%s", agios_tracefile_buffer) < agios_tracefile_buffer_size) {
		agios_print("PANIC! Could not write trace buffer to trace file!\n");
	}
	fflush(agios_tracefile_fd);
	/*reset the bufer*/
	agios_tracefile_buffer_size=0;
}
/**
 * write the contents of aux_buf (the smaller buffer) to the main buffer. The user must have the trace lock.
 */
void agios_trace_write_to_buffer(void)
{
	int32_t size = strlen(aux_buf);
	if ((agios_tracefile_buffer_size + size) >= config_agios_max_trace_buffer_size) {
		agios_tracefile_flush_buffer();
	}
	snprintf(agios_tracefile_buffer+agios_tracefile_buffer_size, size+1, "%s", aux_buf);
	agios_tracefile_buffer_size += size;
	aux_buf[0]='\0';
}
/**
 * write information about a request to the aux_buf (the smaller buffer).
 * @param req the request.
 */
void agios_trace_print_request(struct request_t *req)
{
	int32_t index = strlen(aux_buf);
	if (req->type == RT_READ) snprintf(aux_buf+index, aux_buf_size - index, "%s\tR\t%ld\t%ld\n", req->file_id, req->offset, req->len);
	else snprintf(aux_buf+index, aux_buf_size - index, "%s\tW\t%ld\t%ld\n", req->file_id, req->offset, req->len);	
}
/**
 * called by agios_add_request when a new request was added to the library. It will trace its arrival. The caller must NOT hold the trace mutex.
 * @param req the newly arrived request. Its arrival_time must be filled BEFORE calling this function.
 */
void agios_trace_add_request(struct request_t *req)
{
	pthread_mutex_lock(&agios_trace_mutex);
	snprintf(aux_buf, aux_buf_size, "%ld\t", (req->arrival_time - get_timespec2long(agios_trace_t0)));
	agios_trace_print_request(req);
	agios_trace_write_to_buffer();
	pthread_mutex_unlock(&agios_trace_mutex);
}
/**
 * function called at the beginning of the execution. It checks for existing trace files given the prefix and sufix in the configuration parameters. Then it creates and opens the next one. It also allocates the buffers used to write to the trace file. The caller must NOT hold the trace mutex.
 * @return true or false for success. 
 */
bool init_trace_module(void)
{
	char filename[256]; /**< the filename of the tracefile that will be opened. */	
	int32_t agios_trace_counter=0; /**< used to search for the next tracefile to be created. */ 
	bool ret = true; /**< used to return false in case of errors. */

	pthread_mutex_lock(&agios_trace_mutex);
	agios_gettime(&agios_trace_t0);
	/*we have to find out how many trace files are there*/
	do {
		if (agios_trace_counter > 0) fclose(agios_tracefile_fd);
		agios_trace_counter++;
		sprintf(filename, "%s.%d.%s", config_trace_agios_file_prefix, agios_trace_counter, config_trace_agios_file_sufix);	
		agios_tracefile_fd = fopen(filename, "r");
	} while(agios_tracefile_fd);
	/*create and open the new trace file*/
	agios_tracefile_fd = fopen(filename,  "w+");
	if (!agios_tracefile_fd) {
		ret = false; //we will return an error but we can't leave right now because we need to unlock the mutex first. 
	} else {
		/*prepare the buffer*/
		if (agios_tracefile_buffer) agios_tracefile_buffer_size=0; //we already have a buffer, just have to reset it
		else agios_tracefile_buffer = (char *)malloc(config_agios_max_trace_buffer_size); 
		if (!agios_tracefile_buffer) ret = false;
		else {
			if (!aux_buf) {
				aux_buf = (char *)malloc(aux_buf_size); 
				if (!aux_buf) {
					agios_print("PANIC! Could not allocate memory for trace file buffer!\n");
					ret = false;
				}
			}
		}
	}
	pthread_mutex_unlock(&agios_trace_mutex);
	return ret;
}
/**
 * called at the end of the execution to free allocated memory. 
 */
void cleanup_agios_trace(void)
{
	if (agios_tracefile_buffer) free(agios_tracefile_buffer);
	if (aux_buf) free(aux_buf);
}
/**
 * closes the current trace after flushing the buffer to it. The caller must NOT hold the mutex. 
 */
void close_agios_trace()
{
	pthread_mutex_lock(&agios_trace_mutex);
	agios_tracefile_flush_buffer();
	fclose(agios_tracefile_fd);
	pthread_mutex_unlock(&agios_trace_mutex);
}
