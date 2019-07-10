/*! \file common_functions.c
    \brief Miscellaneous functions used everywhere in AGIOS source code.
 */
#include <assert.h>

#include "common_functions.h"

#define TIMESPEC_DIFF(t1, t2)  ((t2.tv_nsec - t1.tv_nsec) + ((t2.tv_sec - t1.tv_sec)*1000000000L)) /**< used to measure the time difference between two struct timespec */

/** 
 * function that returns the minimum of two values.
 * @param t1 and t2 the values.
 * @return min(t1,t2).
 */
int64_t agios_min( int64_t t1,  int64_t t2)
{
	if (t1 < t2) return t1;
	else return t2;
}
/**
 * function that returns the maximum of two values.
 * @param t1 and t2 the values.
 * @return max(t1, t2).
 */
int64_t agios_max( int64_t t1, int64_t t2)
{
	if (t1 > t2) return t1;
	else return t2;
}
/**
 * function that gives the index containing the maximum value of an array of only two values.
 * @param count an array of two ints.
 * @return 0 or 1 corresponding to the index of maximum value.
 */
int32_t get_index_of_max_from_two(int32_t *count)
{
	if (count[0] >= count[1]) return 0;
	else return 1;
}
/**
 * takes a struct timespec and tells you how many nanoseconds passed since the timespec was obtained.
 * @param t1 a filled struct timespec
 * @return elapsed nanoseconds since t1
 */
int64_t get_nanoelapsed(struct timespec t1)
{
	struct timespec t2;
	agios_gettime(&t2);
	return TIMESPEC_DIFF(t1,t2);
}
/**
 * translates a struct timespec to a int64_t (in nanoseconds)
 * @param t the filled struct timespec.
 * @return t in nanoseconds.
 */
int64_t get_timespec2long(struct timespec t)
{
	return (t.tv_sec*1000000000L + t.tv_nsec);
}
/**
 * traslates an int64_t (in nanoseconds) to struct timespec.
 * @param t a time in nanoseconds.
 * @param ret to be filled here, it will receive t 
 */
void get_long2timespec(int64_t t, struct timespec *ret)
{
	ret->tv_sec = t / 1000000000L;
	ret->tv_nsec = t % 1000000000L;
}
/**
 * does the same as get_nanoelapsed, but takes as parameter a int64_t instead of a struct timespec
 * @param t1 a timestamp in nanoseconds.
 * @return elapsed time since t1.
 */
int64_t get_nanoelapsed_long(int64_t t1)
{
	struct timespec t2;
	agios_gettime(&t2);
	return (get_timespec2long(t2) - t1);
}
/**
 * convert ns to s.
 * @param t1 time in nanoseconds.
 * @return t1 in seconds. 
 */
double get_ns2s(int64_t t1)
{
	double ret = ((double)t1) / 1000.0;
	ret = ret / 1000.0;
	return ret / 1000.0;
}
/** 
 * update a iterativelly calculated average
 * @param avg the current average value. 
 * @param value is the new observed value that needs to be integrated into the average.
 * @param count the index of the current update (it will be 1 the first time, always updated before calling this).
 * @return the new average value. */
int64_t update_iterative_average(int64_t avg, int64_t value, int64_t count)
{
	assert(count > 0);
	if (count == 1) return value; //this is the first value, we don't have an average yet.
	else return avg + ((value - avg)/count);
}

