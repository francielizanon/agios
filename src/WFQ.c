/*! \file TWINS.c
    \brief Implements the TWINS scheduling algorithm

 */ 
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agios_config.h"
#include "agios_counters.h"
#include "common_functions.h"
#include "hash.h"
#include "mylist.h"
#include "process_request.h"
#include "req_hashtable.h"
#include "req_timeline.h"
#include "scheduling_algorithms.h"


static int g_current_queue; /**< the current queue from where we are taking requests */


/**
 * function called to initialize WFQ by setting some variables.
 * @return true or false for success
 */
bool WFQ_init()
{   
    //The WFQ
    char *envvar_conf = "WFQ_CONF";
	int buf_size = 100;
	char wfq_config[buf_size];

    g_current_queue = 0; //the first id we will prioritize

    // Firstly, we check if the full path for wfq config file was set up in the environment var WFQ_CONF
	if(!getenv(envvar_conf) || snprintf(wfq_config, buf_size, "%s", getenv(envvar_conf)) >= buf_size){
        fprintf(stderr, "The environment variable %s was not found or the BUFSIZE of %d was to small .\n", envvar_conf, buf_size);
        exit(1);
    }

    //TODO: remove it (debug only)
    printf("!!!!! !!!!Reading file %s ...\n", wfq_config);
    
    // TODO: set the queues weight and debs 
    // the full path to the wfq config file is in wfq_config    


    return true;
}
/**
 * function called when stopping the use of TWINS, for now we don't have anything to clean up.
 */
void WFQ_exit()
{
}
/**
 * main function for the TWINS scheduler. It is called by the AGIOS thread to schedule some requests. It will continue to consume requests until there are no more requests or if notified by the process_requests_step2 function.
 * @return if we are returning because we were asked to stop, 0, otherwise we return the time until the end of the current window
 */
int64_t WFQ(void)
{
    bool WFQ_STOP=false; /**< the return of the process_requests_step2 function may notify us it is time to stop because of a periodic event */
    struct request_t *req; /**< used to access requests from the queues */
    int32_t hash; /**< after selecting a request to be processed, we need to find out its hash to give to the process_requests function */
    struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */
    
    PRINT_FUNCTION_NAME;
    //we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
    while (!WFQ_STOP) {
        timeline_lock();      
        //process requests!
        if (!(agios_list_empty(&(multi_timeline[g_current_queue])))) { //we can only process requests from the current app_id
            //take request from the right queue
            req = agios_list_entry(multi_timeline[g_current_queue].next, struct request_t, related);
            //remove from the queue
            agios_list_del(&req->related);
            /*send it back to the file system*/
            //we need the hash for this request's file id so we can update its stats 
            hash = get_hashtable_position(req->file_id);
            info = process_requests_step1(req, hash);
            generic_post_process(req);
            timeline_unlock();
            WFQ_STOP = process_requests_step2(info);
        } else { //if there are no requests for this queue, we return control to the AGIOS thread and it will sleep a little 
            timeline_unlock();
            break; //get out of the while 
        }
    } //end while

    g_current_queue += 1;
    //if we are here, we were asked to stop by the process_requests function, or we have no requests to the server currently being accessed
    if (WFQ_STOP) return 0;
    else return 5;
    //else return (config_twins_window - get_nanoelapsed(g_window_start));
}
