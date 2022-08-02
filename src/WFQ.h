/*! \file WFQ.h
    \brief Headers for the implementation of the WFQ scheduling algorithm

 */
#pragma once

struct wfq_weights_t;

bool WFQ_init();
int64_t WFQ(void);
void WFQ_exit();

