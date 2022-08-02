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
struct wfq_weights_t * wfq_weights; /**< An array that keeps the weight and the debt of each queue */

struct wfq_weights_t {
    int32_t weight;
    int32_t debt;

};

/**
 * function called to initialize WFQ by setting some variables.
 * @return true or false for success
 */
bool WFQ_init(char *setup) {
    //The WFQ
    char *envvar_conf = "WFQ_CONF";
    int buf_size = 100;
    char wfq_config_file[buf_size];

    // Firstly, we check if the full path for wfq config file was set up in the environment var WFQ_CONF
    if (!getenv(envvar_conf))
    {
        agios_print("WFQ Error: The environment variable %s was not found.\n", envvar_conf);
        return false;
    }
    if( snprintf(wfq_config_file, buf_size, "%s", getenv(envvar_conf)) >= buf_size)
    {
        agios_print("WFQ Error: The BUFSIZE of %d was to small.\n", buf_size);
        return false;
    }

    //Secondly, we set the queues weight and debs
    // the weights of each queue is read from the wfq.conf.
    FILE *setup_file = fopen(wfq_config_file, "r");
    if(!setup_file)
    {
        agios_print("WFQ Error: Error opening WFQ config file %s.\n", wfq_config_file);
        return false;
    }

    wfq_weights  = (struct wfq_weights_t *) malloc(multi_timeline_size * sizeof(struct wfq_weights_t));

    for (int i = 0; i < multi_timeline_size; i++)
    { //fscanf to get the weights from the setup file
        fscanf(setup_file, "%d ", &(wfq_weights[i].weight));
        wfq_weights[i].debt = 0;
    }

    fclose(setup_file);


    return true;
}

/**
 * function called when stopping the use of TWINS, for now we don't have anything to clean up.
 */
void WFQ_exit() { //free

    free(wfq_weights);
}

/**
 * main function for the TWINS scheduler. It is called by the AGIOS thread to schedule some requests. It will continue to consume requests until there are no more requests or if notified by the process_requests_step2 function.
 * @return if we are returning because we were asked to stop, 0, otherwise we return the time until the end of the current window
 */
int64_t WFQ(void) {
    bool WFQ_STOP = false; /**< the return of the process_requests_step2 function may notify us it is time to stop because of a periodic event */
    struct request_t *req; /**< used to access requests from the queues */
    int32_t hash; /**< after selecting a request to be processed, we need to find out its hash to give to the process_requests function */
    struct processing_info_t *info; /**< the struct with information about requests to be processed, filled by process_requests_step1 and given as parameter to process_requests_step2 */

    PRINT_FUNCTION_NAME;
    //we are not locking the current_reqnb_mutex, so we could be using outdated information. We have chosen to do this for performance reasons
    while (!WFQ_STOP) {
        timeline_lock();
        //process requests!
        if (!(agios_list_empty(
                &(multi_timeline[g_current_queue])))) { //we can only process requests from the current app_id
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

    g_current_queue = (g_current_queue + 1) %  multi_timeline_size;

    //if we are here, we were asked to stop by the process_requests function, or we have no requests to the server currently being accessed
    if (WFQ_STOP) return 0;
    else return 5;
    //else return (config_twins_window - get_nanoelapsed(g_window_start));
}
