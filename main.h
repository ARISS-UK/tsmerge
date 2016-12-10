#ifndef _MAIN_H
#define _MAIN_H

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <pthread.h>

#define MERGER_UDP_RX_PORT      5678
#define MERGER_UDP_RX_BUFSIZE   65535

typedef struct mx_thread_t {
	
	/* pthread instance */
	pthread_t thread;
	
	/* Thread name */
	char* name;
	
	/* Target function */
	void* function;
	
	/* Target function argument */
	void* arg;
	
	/* Last cpu timestamp */
	int64_t last_cpu;
	
	/* System timestamp of last cpu timestamp */
	int64_t last_cpu_ts;
	
} mx_thread_t;

#endif
