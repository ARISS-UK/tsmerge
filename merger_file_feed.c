
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include "main.h"
#include "timing.h"
#include "viewer.h"
#include "merger.h"
#include "merger_rx_socket.h"
#include "merger_tx_feed.h"
#include "merger_tx_socket.h"

/* This function is run on a thread, started from main()
 *
 * This function loops over the 'viewers' array,
 *  checking for when 'last_station' and 'last_count' do not match the latest selected segment
 * On this condition, the latest segment is sent to the relevant viewer,
 *  and their 'last_station' and 'last_count' data is updated.
 */
void *merger_file_feed(void* arg)
{
  (void) arg;
  mx_packet_t *p;
  uint64_t timestamp;
  int r;
  
  /* The main network loop */
	while(1)
	{	
		if(file_viewer.enabled==1)
		{
	    p = NULL;
	    timestamp = timestamp_ms();
		    
	    /* See if there is any data still to send to this viewer */
			pthread_mutex_lock(&merger.lock);
			while((p = mx_next(&merger, file_viewer.last_station, file_viewer.last_counter)) != NULL)
			{
		    FILE *f = fopen(file_viewer.filename, "a+");
				/* Try to send the new data to the viewer */
				r = fwrite(p->raw, TS_PACKET_SIZE, 1, f);
				if(r < 0)
				{
					/* An error has occured. */
					fprintf(stdout,"Error saving output to file: %d\n", r);
					/* Disable file output */
					file_viewer.enabled = 0;
					break;
				}
				fclose(f);
				
				/* Update viewer state */
				file_viewer.last_station = p->station;
				file_viewer.last_counter = p->counter;
				file_viewer.timestamp = timestamp;
			}
			pthread_mutex_unlock(&merger.lock);
    }
		
		sleep_ms(5);
	}
}
