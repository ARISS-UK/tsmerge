
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
#include <getopt.h>
#include "merge.h"
#include "timing/timing.h"
#include "merger/merger.h"
#include "merger/viewer.h"
#include "merger/stats.h"
#include "merger/input_socket.h"
#include "merger/input_feed.h"
#include "merger/input_buffer.h"
#include "merger/output_socket.h"
#include "merger/output_feed.h"
#include "merger/output_log.h"
#include "merger/stations.h"

rxBuffer_t rxBuffer;

/* See main.h for this macro to define and declare the threads */
MX_DECLARE_THREADS();

void _print_version(void)
{
    printf(
        "tsmerge (tsmerge) "BUILD_VERSION" (built "BUILD_DATE")\n"
    );
}

void _print_usage(void)
{
    printf(
        "\n"
        "Usage: tsmerge [options]\n"
        "\n"
        "  -V, --version          Print version string and exit.\n"
        "\n"
    );
}

int main(int argc, char *argv[])
{
    int i;
    int c;
    int opt;

    static const struct option long_options[] = {
        { "version",     no_argument, 0, 'V' },
        { 0,             0,           0,  0  }
    };

    while((c = getopt_long(argc, argv, "Vd", long_options, &opt)) != -1)
    {
        switch(c)
        {
            case 'V': /* --version */
                _print_version();
                return 0;

            case '?':
                _print_usage();
                return 0;
        }
    }
	
    /* Set up file output */
    memset(&file_viewer, 0, sizeof(file_viewer));
    file_viewer.last_station = -1;
    file_viewer.last_counter = 0;
    file_viewer.timestamp = timestamp_ms();
	
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

