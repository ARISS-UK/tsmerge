
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "main.h"
#include "timing.h"
#include "viewer.h"
#include "merger.h"
#include "merger_rx_socket.h"
#include "merger_tx_feed.h"
#include "merger_tx_socket.h"


static void _close_connection(viewer_t *viewers, int i)
{
	printf("Closing TCP socket %d\n", i);
	
	close(viewers[i].sock);
	
	viewers[i].sock = 0;
	viewers[i].last_station = -1;
	viewers[i].last_counter = 0;
	viewers[i].timestamp = 0;
}

/* This function is run on a thread, started from main()
 *
 * This function loops over the 'viewers' array,
 *  checking for when 'last_station' and 'last_count' do not match the latest selected segment
 * On this condition, the latest segment is sent to the relevant viewer,
 *  and their 'last_station' and 'last_count' data is updated.
 */
void *merger_tx_feed(void* arg)
{
  (void) arg;
	mx_packet_t *p;
  uint64_t timestamp;
  int r;
  int i;
  
  /* The main network loop */
	while(1)
	{
		timestamp = timestamp_ms();
		
		for(i = 0; i < _VIEWERS_MAX; i++)
		{
			if(viewers[i].sock <= 0) continue;
			
			p = NULL;
			
			/* See if there is any data still to send to this viewer */
			pthread_mutex_lock(&merger.lock);
			while((p = mx_next(&merger, viewers[i].last_station, viewers[i].last_counter)) != NULL)
			{
				/* Try to send the new data to the viewer */
				r = send(viewers[i].sock, p->raw, TS_PACKET_SIZE, 0);
				if(r < 0)
				{
					if(errno == EAGAIN || errno == EWOULDBLOCK)
					{
						/* The socket is busy, try again in the next loop */
						break;
					}
					
					/* An error has occured. Drop the connection */
					fprintf(stdout,"Error on sending data to connected viewer, closing connection.\n");
					_close_connection(viewers, i);
					break;
				}
				
				if(r != TS_PACKET_SIZE)
				{
					/* TODO: I don't know if send() can return 0 or a partial amount */
					printf("Got an odd result from send(): %d\n", TS_PACKET_SIZE);
				}
				
				/* Update viewer state */
				viewers[i].last_station = p->station;
				viewers[i].last_counter = p->counter;
				viewers[i].timestamp = timestamp;
			}
			pthread_mutex_unlock(&merger.lock);
			
			/* Test if the client has timed out */
			if(viewers[i].sock > 0 && timestamp - viewers[i].timestamp > _VIEWER_TIMEOUT)
			{
				//send(_viewers[i].sock, "TIMEOUT\n", 8, 0);
				_close_connection(viewers, i);
			}
		}
		
		sleep_ms(5);
	}
}
