/*! \file trace.c
    \brief Trace Module, that creates a trace file during execution with information about all requests arrivals.
 */
#pragma once

#include "agios_request.h"

void agios_trace_add_request(struct request_t *req);
bool init_trace_module(void);
void cleanup_agios_trace(void);
void close_agios_trace();

