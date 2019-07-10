/*! \file agios_counters.c
    \brief Provides functions to manipulate the request and file counters.

    These counters are kept updated during the execution, and protected with a mutex.
*/
#include "agios_counters.h"
#include "req_hashtable.h"

int32_t current_reqnb; /**< Number of queued requests */
int32_t current_filenb; /**< Number of files with queued requests */
static pthread_mutex_t current_reqnb_lock = PTHREAD_MUTEX_INITIALIZER; /**< Used to protect the request and file counters current_reqnb and current_filenb */

/**
 * function used to safely read the content of current_reqnb (using the mutex).
 */
int32_t get_current_reqnb(void)
{
	int32_t ret;
	pthread_mutex_lock(&current_reqnb_lock);
	ret = current_reqnb;
	pthread_mutex_unlock(&current_reqnb_lock);
	return ret;
}
/**
 * function used to safely increment the current_reqnb counter (using the mutex).
 */
void inc_current_reqnb()
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/** 
 * function used to safely decrement the current_reqnb counter (using the mutex). It also updates the hashtlist_reqcounter, so caller must hold mutex to the hashtable line.
 * @param hash the line of the hashtable that contains the file this request is accessing.
 */
void dec_current_reqnb(int32_t hash)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb--;
	hashlist_reqcounter[hash]--;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/** 
 * function used to safely decrement the current_reqnb counter by a certain value (using the mutex). It is tu be used instead of many calls to dec_current_reqnb(hash). It also updates the hashlist_reqcounter, so caller must hold mutex to the hashtable line.
 * @param hash the line of the hashtable that contains the file this request is accessing.
 * @param value by how much we want to decrement the current_reqnb counter.
 */
void dec_many_current_reqnb(int32_t hash, int32_t value)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_reqnb-= value;
	hashlist_reqcounter[hash]-= value;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/**
 * function used to safely increment the current_filenb counter (using the mutex).
 */
void inc_current_filenb(void)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_filenb++;
	pthread_mutex_unlock(&current_reqnb_lock);
}
/**
 * function used to safely decrement the current_filenb counter (using the mutex).
 */
void dec_current_filenb(void)
{
	pthread_mutex_lock(&current_reqnb_lock);
	current_filenb--;
	pthread_mutex_unlock(&current_reqnb_lock);
}

