
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
#include "merger_file_feed.h"

rxBuffer_t rxBuffer;

/* See main.h for this macro to define and declare the threads */
MX_DECLARE_THREADS();

int main(int argc, char *argv[])
{
    int i;
	
	time_t now;
    char stime[20];
    
	/* Set up file output */
	memset(&file_viewer, 0, sizeof(file_viewer));
	file_viewer.enabled = 1;

    /* Generate timestamp string */
    time(&now);
    strftime(stime, sizeof stime, "%FT%TZ", gmtime(&now));
	
	/* Construct merged filename output */
	snprintf(file_viewer.filename,63,"merged-%s.ts",stime);
	printf("Saving merged TS to: %s\n",file_viewer.filename);
	
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
	
	while(1)
	{
	    sleep_ms(1000);
	}
	
	return(0);
}

