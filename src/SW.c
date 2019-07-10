/*! \file SW.c
    \brief Implementation of the SW scheduling algorithm

 */
#include <stdint.h>

#include "TO.h"

/**
 * main function for the scheduling algorithm, it simply uses the TO implementation because the only difference between them is in the inclusion of requests.
 * @return 0 (because we will never decide to sleep)
 */
int64_t SW(void)
{
	return timeorder(); // The difference between them in in the inclusion of requests, the processing is the same
}
