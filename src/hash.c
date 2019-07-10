/*! \file hash.c
    \brief Implementation of the get_hashtable_position function, used to select a line of the hashtable according to a file handle.
*/
#include <string.h>

#include "hash.h"
#include "req_hashtable.h"

/**
 * function used by get_hashtable_position to calculate a hash according to a number.
 * @param val a 64-bit value.
 * @return a 64-bit hash value.
 */
int64_t calculate_hash(int64_t val)
{
        int64_t hash = val;
        int64_t n = hash;
        n <<= 18;
        hash -= n;
        n <<= 33;
        hash -= n;
        n <<= 3;
        hash += n;
        n <<= 3;
        hash -= n;
        n <<= 4;
        hash += n;
        n <<= 2;
        hash += n;
        /* High bits are more random, so use them. */
        return hash >> (64 - AGIOS_HASH_SHIFT);
}
/**
 * function that returns a line of the hashtable where to put information about a file handle.
 * @param file_handle a string handle for the file.
 * @return an index between 0 and AGIOS_HASH_ENTRIES.
 */
int32_t get_hashtable_position(const char *file_handle)
{
	//makes a number using the sum of all characters of the name of the file
	int64_t sum=0;	
	for(int32_t i=0; i< strlen(file_handle); i++)
		sum += file_handle[i];
	//calculates the hash and transforms it into a position of the hashtable
	sum = calculate_hash(sum);
	if (sum < 0) sum = -sum;
	return sum % AGIOS_HASH_ENTRIES;
}
