#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "merger.h"

/* This function is run on a thread, started from main()
 *
 * This function listens on the UDP port for incoming datagrams from receiver stations
 * On reception, the packet is split in MX_packets and each is added to the rxBuffer
 *  to be serviced by 'merger_rx_feed'
 */
void *merger_rx_socket(void *Buffer_void_ptr)
{
  rxBuffer_t *rxBufPtr;
  rxBufPtr = (rxBuffer_t *)Buffer_void_ptr;
  
  int sockfd; /* socket */
  struct sockaddr_in serveraddr; /* server's addr */
  uint8_t buf[MERGER_UDP_RX_BUFSIZE]; /* message buf */
  int optval; /* flag value for setsockopt */
  uint64_t timestamp;
  int n; /* message byte size */

  /* Open UDP Socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
  {
    fprintf(stderr, "Incoming socket failed to open");
  }

  /* Add REUSEADDR Flag (TODO: Check if needed) */
  optval = 1;
  n = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	if(n < 0)
	{
		fprintf(stderr, "Incoming socket failed to set SO_REUSEADDR");
		close(sockfd);
		return NULL;
	}
  
  /* Set the RX buffer length */
	optval = MERGER_UDP_RX_SYSBUFSIZE;
	n = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	if(n < 0)
	{
		fprintf(stderr, "Incoming socket failed to set buffer size");
		close(sockfd);
		return NULL;
	}

  /* Set up server address */
  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(MERGER_UDP_RX_PORT);

  /* Bind Socket to server address */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
  { 
    fprintf(stderr, "Incoming socket failed to bind\n");
    return NULL;
  }
  
  /* Infinite loop, blocks until incoming packet */
  while (1)
  {   
    /* Block here until we receive a packet */
    n = recv(sockfd, buf, MERGER_UDP_RX_BUFSIZE, 0);
    if (n < 0)
    {
      fprintf(stderr, "Incoming recv failed\n");
      continue;
    }
    
    if(n % MX_PACKET_LEN != 0)
	  {
		  fprintf(stderr, "Incoming packet invalid size, expected a multiple of 204 bytes, got %d\n", n);
		  continue;
	  }
	  
    timestamp = timestamp_ms();
    
    /* Feed in the packet(s) */
    rxBufferBurstPush(rxBufPtr, timestamp, buf, (uint16_t)n);
  }
}
