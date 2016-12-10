
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
#include "merger_rx.h"
#include "merger_tx.h"
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

void *merger_tx(void* arg)
{
  (void) arg;
  uint64_t timestamp;
  int r;
  int i;
  
  /* The main network loop */
	while(1)
	{		
		timestamp = timestamp_ms();
		
		for(i = 0; i < _VIEWERS_MAX; i++)
		{
			mx_packet_t *p;
			
			if(viewers[i].sock <= 0) continue;
			
			//printf("Viewer counter: %d: %d\n",i,viewers[i].last_counter);
			
			/* See if there is any data still to send to this viewer */
			pthread_mutex_lock(&merger.lock);
			while((p = mx_next(&merger, viewers[i].last_station, viewers[i].last_counter)) != NULL)
			{
			  //printf("TX Data content: 0x%02X%02X%02X%02X\n",
        //  p->raw[0], p->raw[1], p->raw[2], p->raw[3]);
			  //printf("New data for viewer, count now: %d: %d\n",i,viewers[i].last_counter);
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
