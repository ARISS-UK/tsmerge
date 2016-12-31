#ifndef _MAIN_H
#define _MAIN_H

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <pthread.h>

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

/* Declare threads */
/** { [pthread_t], name[64], function, arg, last_cpu, last_cpu_t } **/
#define MX_THREAD_NUMBER   7
#define MX_DECLARE_THREADS() \
    mx_thread_t mx_threads[MX_THREAD_NUMBER] = { \
        { 0, "TCP TX Socket    ", merger_tx_socket, NULL,      0, 0 }, \
        { 0, "MX => TCP TX Feed", merger_tx_feed,   NULL,      0, 0 }, \
        { 0, "MX => File Feed  ", merger_file_feed, NULL,      0, 0 }, \
        { 0, "MX Loop          ", merger_mx,        NULL,      0, 0 }, \
        { 0, "UDP RX => MX Feed", merger_rx_feed,   &rxBuffer, 0, 0 }, \
        { 0, "UDP RX Socket    ", merger_rx_socket, &rxBuffer, 0, 0 }, \
        { 0, "Stats Collection ", merger_stats,     NULL,      0, 0 }  \
    };

#endif
