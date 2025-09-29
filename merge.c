
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
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
#include <signal.h>

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

mx_config_t mx_config = {
    .input_mx_port = DEFAULT_MERGER_UDP_RX_PORT,
    .output_ts_port = DEFAULT_MERGER_TCP_TX_PORT,
    .stats_udp_port = DEFAULT_MERGER_STATS_UDP_PORT,
    .stations_filepath = DEFAULT_STATIONS_JSON_FILE
};

/* See main.h for this macro to define and declare the threads */
MX_DECLARE_THREADS();

static bool app_exit = false;
void sigint_handler(int sig)
{
  (void)sig;
  app_exit = true;
}

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
        "  -i, --input_mx_port    Input UDP MX port number. (default: %d)\n"
        "  -o, --output_ts_port   Output TCP TS port number. (default: %d)\n"
        "  -s, --stats_udp_port   Output UDP JSON stats number. (default: %d)\n"
        "  -f, --stations_file    Stations JSON config file. (default: %s)\n"
        "\n",
        DEFAULT_MERGER_UDP_RX_PORT,
        DEFAULT_MERGER_TCP_TX_PORT,
        DEFAULT_MERGER_STATS_UDP_PORT,
        DEFAULT_STATIONS_JSON_FILE
    );
}

int main(int argc, char *argv[])
{
    int i;
    int c;
    int opt;

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    static const struct option long_options[] = {
        { "version",         no_argument,       NULL, 'V'  },
        { "input_mx_port",   required_argument, NULL, 'i'  },
        { "output_ts_port",  required_argument, NULL, 'o'  },
        { "stats_udp_port",  required_argument, NULL, 's'  },
        { "stations_file",   required_argument, NULL, 'f'  },
        { 0,                 0,                 NULL, 0    }
    };

    while((c = getopt_long(argc, argv, "Vi:o:s:f:", long_options, &opt)) != -1)
    {
        switch(c)
        {
            case 'V': /* --version */
                _print_version();
                return 0;

            case 'i': /* --input_mx_port */
                mx_config.input_mx_port = strtoul(optarg, NULL, 10);
                break;

            case 'o': /* --output_ts_port */
                mx_config.output_ts_port = strtoul(optarg, NULL, 10);
                break;

            case 's': /* --stats_udp_port */
                mx_config.stats_udp_port = strtoul(optarg, NULL, 10);
                break;

            case 'f': /* --stations_file */
                mx_config.stations_filepath = strdup(optarg);
                break;

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
        pthread_setname_np(mx_threads[i].thread, mx_threads[i].name);
        mx_threads[i].last_cpu_ts = timestamp_ms();
        mx_threads[i].last_cpu = 0;
    }
	
    printf("tsmerger running. Go for launch.\n");
	
    while(!app_exit)
    {
        sleep_ms(100);
    }

    printf("\nReceived SIGTERM/INT, exiting..\n");
    app_exit = true;

    /* Interrupt all declared threads */
    for(i=0; i<MX_THREAD_NUMBER; i++)
    {
        pthread_kill(mx_threads[i].thread, SIGINT);
    //    fprintf(stderr, " - Killed %s\n", mx_threads[i].name);
    }
    
    /* Wait for all declared threads */
    //for(i=0; i<MX_THREAD_NUMBER; i++)
    //{
    //    fprintf(stderr, " - Waiting for %s\n", mx_threads[i].name);
    //    pthread_join(mx_threads[i].thread, NULL);
    //}
    //printf("All threads caught.\n");
	
    return(0);
}

