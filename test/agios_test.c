#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <agios.h>

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
};
struct request_info_t *requests; /**< the list containing ALL requests generated in this test */
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
	return 0;
}
void * test_process(int64_t req_id)
{
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
	}
	free(lastoffset);
}

int main (int argc, char **argv)
{
	int64_t elapsed;
	pthread_t *threads;
	int64_t *thread_index;
	struct timespec start_time, end_time;

	/*get arguments*/
	retrieve_arguments_and_generate_requests(argc, argv);
	/*start AGIOS*/
	if (!agios_init(test_process, NULL, "/tmp/agios.conf", g_queue_ids)) {
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
	for (int32_t i = 0; i < g_thread_nb; i++) pthread_join(threads[i], NULL);
	for (int32_t i = 0; i < g_generated_reqnb; i++) pthread_join(processing_threads[i], NULL);
	//TODO free other stuff?
	free(threads);
	free(thread_index);
	free(requests);
	free(processing_threads);
	return 0;
}
