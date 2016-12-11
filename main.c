
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "main.h"
#include "timing.h"
#include "viewer.h"
#include "merger.h"
#include "merger_stats.h"
#include "merger_rx_buffer.h"
#include "merger_rx_socket.h"
#include "merger_rx_feed.h"
#include "merger_tx_socket.h"
#include "merger_tx_feed.h"

rxBuffer_t rxBuffer;

/* Declare threads */
/** { [pthread_t], name[64], function, arg, last_cpu, last_cpu_t } **/
#define MX_THREAD_NUMBER   6
mx_thread_t mx_threads[MX_THREAD_NUMBER] = {
    { 0, "TCP TX Socket    ", merger_tx_socket, NULL,      0, 0 },
    { 0, "MX => TCP TX Feed", merger_tx_feed,   NULL,      0, 0 },
    { 0, "MX Loop          ", merger_mx,        NULL,      0, 0 },
    { 0, "UDP RX => MX Feed", merger_rx_feed,   &rxBuffer, 0, 0 },
    { 0, "UDP RX Socket    ", merger_rx_socket, &rxBuffer, 0, 0 },
    { 0, "Stats Collection ", merger_stats,     NULL,      0, 0 }
};

int main(int argc, char *argv[])
{
    int i;
	
	/* Clear the viewers array */
	memset(&viewers, 0, sizeof(viewers));
	
	/* Initialise Merger */
    mx_init(&merger, MERGER_PCR_PID);
	
	/* Init UDP Rx => Mx Feed Buffer */
	rxBufferInit(&rxBuffer);
	
	/* Start all declared threads */
	for(i=0; i<MX_THREAD_NUMBER; i++)
	{
	    if(pthread_create(&mx_threads[i].thread, NULL, mx_threads[i].function, mx_threads[i].arg))
	    {
		    fprintf(stderr, "Error creating %s pthread\n", mx_threads[i].name);
		    return 1;
	    }
	    mx_threads[i].last_cpu_ts = timestamp_ms();
	    mx_threads[i].last_cpu = 0;
    }
	
	printf("tsmerger running. Go for launch.\n");
	
	sleep_ms(3*1000);
	
	while(1)
	{
	    /* Print thread CPU usage time since last sampled */
	    printf("\nThread CPU: \n");
	    int64_t timestamp_tmp, cpu_time_tmp;
	    for(i=0; i<MX_THREAD_NUMBER; i++)
	    {
	        timestamp_tmp = timestamp_ms();
	        cpu_time_tmp = thread_timestamp(mx_threads[i].thread);
	        
	        printf("%s: %.02f%%\n",
	            mx_threads[i].name,
	            100*(double)(cpu_time_tmp - mx_threads[i].last_cpu)
	                / (timestamp_tmp - mx_threads[i].last_cpu_ts)
	        );
	        
	        mx_threads[i].last_cpu_ts = timestamp_tmp;
	        mx_threads[i].last_cpu = cpu_time_tmp;
	    }
	    
	    sleep_ms(3*1000);
	}
	
	/* Close any open sockets - TODO wrap in terminate routine */ /*
	for(i = 0; i < _VIEWERS; i++)
	{
		if(_viewers[i].sock > 0)
		{
			close(_viewers[i].sock);
		}
	} */
	
	return(0);
}

