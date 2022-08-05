#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <agios.h>
#include <scheduling_algorithms.h>

int32_t g_processed_reqnb=0; /**< the number of requests already processed and released rfom agios */
pthread_mutex_t g_processed_reqnb_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_processed_reqnb_cond=PTHREAD_COND_INITIALIZER;
int32_t g_generated_reqnb; /**< the total number of generated requests */
int32_t g_reqnb_perthread; /**< the number pf requests generated per thread */
int32_t g_thread_nb; /**< number of thread */
int32_t g_queue_ids; /**< number of possible ids provided with agios_add_request to identify different servers or applications to SW and TWINS */

struct request_info_t {
	char fileid[100];
	int32_t len;
	int64_t offset;
	int32_t type;
	int32_t process_time;
	int32_t time_before;
	int32_t queue_id;

    struct timespec start_time;
    struct timespec end_time;
    struct request_info_t * next;
};

struct executed_t{
    struct request_info_t *head;
    struct request_info_t *tail;
};

struct request_info_t *requests; /**< the list containing ALL requests generated in this test */

//TODO: add comment
struct executed_t * executed;

pthread_barrier_t test_start;
pthread_t *processing_threads;

void inc_processed_reqnb()
{
	pthread_mutex_lock(&g_processed_reqnb_mutex);
	g_processed_reqnb++;
	if(g_processed_reqnb >= g_generated_reqnb)
		pthread_cond_signal(&g_processed_reqnb_cond);
	pthread_mutex_unlock(&g_processed_reqnb_mutex);
}

void * process_thr(void *arg)
{
	struct request_info_t *req = (struct request_info_t *)arg;
	struct timespec timeout;

	timeout.tv_sec = req->process_time / 1000000000L;
	timeout.tv_nsec = req->process_time % 1000000000L;
	nanosleep(&timeout, NULL);
	if (!agios_release_request(req->fileid, req->type, req->len, req->offset)) {
		printf("PANIC! release request failed!\n");
	}
	inc_processed_reqnb();
    //TODO: Add the request to a linked list

	return 0;
}
void * test_process(int64_t req_id)
{
     //add the request to the executed list
    struct request_info_t *req = &requests[req_id];


    clock_gettime(CLOCK_MONOTONIC, &req->end_time);

    //executed linked list
    if(executed->head == NULL) executed->head = req;
    else executed->tail->next = req;
    executed->tail = req;

	//create a thread to process this request (so AGIOS does not have to wait for us). Another solution (possibly better depending on the user) would be to have a producer-consumer set up where here we put requests into a ready queue and a fixed number of threads consume them.
	int32_t ret = pthread_create(&(processing_threads[req_id]), NULL, process_thr, (void *)&requests[req_id]);		
	if (ret != 0) {
		printf("PANIC! Could not create processing thread for request %ld\n", req_id);
		inc_processed_reqnb(); //so the program can end
	}
	return 0;
}
/**
 * thread that will generate tons of requests to AGIOS
 */
void *test_thr(void *arg)
{
	int32_t me = *((int64_t *) arg);
	int32_t start_i = me * g_reqnb_perthread;
	struct timespec timeout;

	/*wait for the start signal*/
	pthread_barrier_wait(&test_start);
	//generate all requests and give them to agios
	for(int32_t i = start_i; i < start_i + g_reqnb_perthread; i++) {
		/*wait a while before generating the next one*/
		timeout.tv_sec = requests[i].time_before / 1000000000L;
		timeout.tv_nsec = requests[i].time_before % 1000000000L;
		nanosleep(&timeout, NULL);

        clock_gettime(CLOCK_MONOTONIC, &requests[i].start_time);


        /*give a request to AGIOS*/
		if(!agios_add_request(requests[i].fileid, requests[i].type, requests[i].offset, requests[i].len, i, requests[i].queue_id)) {
			printf("PANIC! Agios_add_request failed!\n");
		}
	}
	return 0;
}
/**
 * read the arguments given to the program from the command line and create the list of requests to be issued by the threads
 */
void retrieve_arguments_and_generate_requests(int argc, char **argv)
{
	int32_t req_size;
	int64_t *lastoffset;
	int32_t time_between;
	int32_t filenb;
	int32_t this_thread;
	int32_t sequential_prob;
	int32_t process_time;
	int64_t seed;
	int32_t this_fileid;
	int64_t draw;

	if (argc < 9) {
		printf("Usage: ./%s <number of threads> <number of files> <number of requests per thread> <number of servers/apps> <probability of sequential access (percent)> <requests' size in bytes> <time between requests in ns> <time to process requests in ns> <random seed (optional)>\n", argv[0]);
		exit(1);
	}
	g_thread_nb=atoi(argv[1]);
	assert(g_thread_nb > 0);
	filenb = atoi(argv[2]);
	assert((filenb > 0) && (filenb <= g_thread_nb));
	g_reqnb_perthread = atoi(argv[3]);
	assert(g_reqnb_perthread > 0);
	g_queue_ids = atoi(argv[4]);
	assert(g_queue_ids > 0);
	sequential_prob = atoi(argv[5]);
	assert((sequential_prob >= 0) && (sequential_prob <= 100));
	g_generated_reqnb = g_reqnb_perthread * g_thread_nb;
	req_size = atoi(argv[6]);
	time_between = atoi(argv[7]);
	process_time = atoi(argv[8]);
	if (argc == 10) seed = atoi(argv[9]);
	else seed = rand();
	printf("Generating %d threads to access %d files. Each one of them will issue %d requests, with %d percent change of being sequential and represented by %d different server/application identifiers, of %d bytes every up to %dns. Requests take up to %dns to be processed. The used random seed is %ld \n", 
		g_thread_nb, 
		filenb,
		g_reqnb_perthread, 
		sequential_prob,
		g_queue_ids,
		req_size, 
		time_between,
		process_time,
		seed);
	/* generate a list of requests */
	srand(seed);
	requests = (struct request_info_t *)malloc(sizeof(struct request_info_t)*g_generated_reqnb);
	lastoffset = (int64_t *) malloc(sizeof(int64_t)*filenb);
	if ((!requests) || (!lastoffset)) {
		printf("Could not allocate memory\n");
		exit(1);
	}
	for (int32_t i = 0; i < filenb; i++) lastoffset[i] = 0;
	for (int32_t i = 0; i < g_generated_reqnb; i++) {
		this_thread = i / g_reqnb_perthread;
		this_fileid = this_thread % filenb;
		sprintf(requests[i].fileid, "arquivo.%d.out", this_fileid);
		requests[i].len = req_size;
		draw = rand() % 100;
		if (draw < sequential_prob) requests[i].offset = lastoffset[this_fileid]+req_size;
		else requests[i].offset = rand() % 2000000000;
		lastoffset[this_fileid] = requests[i].offset;
		requests[i].type = rand() % 2;
		requests[i].process_time = rand() % process_time;
		requests[i].time_before = rand() % time_between;
		requests[i].queue_id = rand() % g_queue_ids;
        requests[i].next = NULL;
	}
	free(lastoffset);
}

/* test_priorities
 *
 *
 * to test the share of the bandwidth of each set over a certain window
 * this function receives an array test_results of size nbr_sets and fills the array
 * with the share for the corresponding set over the window
 *
 * Input: #sets, pointer to the first request of the window, the size of the window and the array to store
 * the test results*/
void test_priorities(char * output_file, int32_t nbr_sets, int32_t window_size, bool verbose_flag){

    FILE * output = fopen(output_file, "w"); //"w" for write

    double test_results[nbr_sets];


    // writing the header
    for(int i = 0; i < nbr_sets; i++){
        if(i == nbr_sets - 1) fprintf(output,"set_%d\n", i+1);
        else fprintf(output, "set_%d,", i+1);
    }

    struct request_info_t * current_req  = executed->head;


    while(current_req != NULL){ //going through all requests

        // Cleaning or starting the test_results array
        for(int i = 0; i < nbr_sets; i++)
            test_results[i] = 0;

        struct request_info_t *ptr = current_req;
        int total_size = 0; //total nbr of bytes to calculate the shares

        //calculating the sum of the bytes of each set within the window
        int window_counter = window_size;
        while(ptr != NULL && window_counter > 0){
            test_results[ptr->queue_id] += ptr->len;
            //count the number of requests per set in the window

            total_size += ptr->len;
            ptr = ptr->next;
            window_counter -= 1;
        }

        //calculating the share of the set and writing it in the csv file
        for(int i = 0; i < nbr_sets; i++) {
            test_results[i] = test_results[i] / total_size;
            if(i == nbr_sets-1)
                fprintf(output,"%lf\n", test_results[i]);
            else
                fprintf(output, "%lf,", test_results[i]);
        }

        //display results
        if(verbose_flag) {
            for (int i = 0; i < nbr_sets; i++) {
                printf("Set %d: %lf\n", i + 1, test_results[i]);
            }
            puts("\n");
        }

        current_req = current_req->next;

    }

    fclose(output);
}

int main (int argc, char **argv)
{
	int64_t elapsed;
	pthread_t *threads;
	int64_t *thread_index;
	struct timespec start_time, end_time;

	char *envvar_conf = "AGIOS_CONF";
	int buf_size = 100;
	char file_config[buf_size];

	if(!getenv(envvar_conf) || snprintf(file_config, buf_size, "%s", getenv(envvar_conf)) >= buf_size){
        fprintf(stderr, "The environment variable %s was not found or the BUFSIZE of %d was to small .\n", envvar_conf, buf_size);
        exit(1);
    }

    // allocating the executed list structure
    executed = (struct executed_t * ) malloc(sizeof(struct executed_t));
    executed->head = NULL;
    executed->tail = NULL;
	
	// commit test
	/*get arguments*/
	retrieve_arguments_and_generate_requests(argc, argv);
	/*start AGIOS*/
	if (!agios_init(test_process, NULL, file_config, g_queue_ids)) {
		printf("PANIC! Could not initialize AGIOS!\n");
		exit(1);
	}
	// allocate the vector of requet-processing threads
	processing_threads = (pthread_t *)malloc(sizeof(pthread_t)*g_generated_reqnb);
	/*generate the request-issuing threads*/
	thread_index = (int64_t *)malloc(sizeof(int64_t)*g_thread_nb);
	if (!thread_index) {
		printf("PANIC! Could not allocate memory\n");
	}
	for (int32_t i = 0; i < g_thread_nb; i++) thread_index[i] = (int64_t) i;
	pthread_barrier_init(&test_start, NULL, g_thread_nb+1);
	threads = (pthread_t *)malloc(sizeof(pthread_t)*(g_thread_nb));
	for (int32_t i=0; i< g_thread_nb; i++) {
		int32_t ret = pthread_create(&(threads[i]), NULL, test_thr, (void *)&thread_index[i]);		
		if (ret != 0) {
			printf("PANIC! Unable to create thread %d!\n", i);
			free(threads);
			exit(1);
		}
	}
	/*start timestamp*/
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	/*give the start signal to the threads*/
	pthread_barrier_wait(&test_start);
	/*wait until all requests have been processed*/
	pthread_mutex_lock(&g_processed_reqnb_mutex);
	while (g_processed_reqnb < g_generated_reqnb) pthread_cond_wait(&g_processed_reqnb_cond, &g_processed_reqnb_mutex);
	pthread_mutex_unlock(&g_processed_reqnb_mutex);
	/*end timestamp*/
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	/*calculate and print the throughput*/
	elapsed = ((end_time.tv_nsec - start_time.tv_nsec) + ((end_time.tv_sec - start_time.tv_sec)*1000000000L));
	printf("It took %ldns to generate and schedule %d requests. The thoughput was of %f requests/s\n", elapsed, g_generated_reqnb, ((double) (g_generated_reqnb) / (double) elapsed)*1000000000L);	
	//end agios, wait for the end of all threads, free stuff


	agios_exit();

    // Agios has finished, now let's compute some execution metrics

    // WFQ: starting test_priorities heuristic to verify if the WFQ heuristic kept the right bandwidth proportions
    //test_priorities("test_priorities.csv",g_queue_ids, 30, true);

    // WFQ: gerenating a CSV file with the timestamps of the requests
    char buffer[1024];
    sprintf(buffer, "output_WFQ_%d.csv", (int)g_thread_nb);
    FILE * output = fopen(buffer, "w"); //TODO: other name
    // us argv[1]
    fprintf(output, "request_id,queue_id,start_time,end_time,elapsed\n");
    // Generate the csv going through all the requests
    for(int32_t i = 0; i < g_generated_reqnb; i++){
        fprintf(output, "%d,%d,%ld,%ld,%ld\n",
                i, // request_id
                requests[i].queue_id,          //queue_id
                requests[i].start_time.tv_nsec, //request start_time
                requests[i].end_time.tv_nsec,   //request end_time
                requests[i].end_time.tv_nsec - requests[i].start_time.tv_nsec); //elapsed time

    }


	for (int32_t i = 0; i < g_thread_nb; i++) pthread_join(threads[i], NULL);
	for (int32_t i = 0; i < g_generated_reqnb; i++) pthread_join(processing_threads[i], NULL);
	//TODO free other stuff?
	free(threads);
	free(thread_index);
	free(requests);
	free(processing_threads);
	return 0;
}
