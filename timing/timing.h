
#ifndef _TIMING_H
#define _TIMING_H

#include <pthread.h>

int64_t timestamp_ms(void);

void sleep_ms(uint32_t _duration);

int64_t thread_timestamp(pthread_t target_thread);

#endif

