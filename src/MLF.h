/*! \file MLF.c
    \brief Implementation of the MLF scheduling algorithm.
 */
#pragma once

#define MAX_MLF_LOCK_TRIES	2 /**< How many times we will try to acquire a lock without waiting for it. @see MLF() */

bool MLF_init();
void MLF_exit();
int64_t MLF(void);
