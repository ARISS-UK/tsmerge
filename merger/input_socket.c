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
  struct sockaddr clientaddr;
  socklen_t clientaddr_len = sizeof(struct sockaddr);
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
  serveraddr.sin_port = htons(mx_config.input_mx_port);

  /* Bind Socket to server address */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
  { 
    fprintf(stderr, "Incoming socket failed to bind\n");
    return NULL;
  }

  mxhbresp_header_t mxhbresp_header;
  uint32_t ts_total, ts_loss;

  fprintf(stderr, "Incoming socket ready\n");
  
  /* Infinite loop, blocks until incoming packet */
  while (1)
  {   
    /* Block here until we receive a packet */
    n = recvfrom(sockfd, buf, MERGER_UDP_RX_BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientaddr_len);
    if(BRANCH_UNLIKELY(n < 0))
    {
      fprintf(stderr, "Incoming recv failed\n");
      continue;
    }

    if(BRANCH_UNLIKELY(n < 2))
    {
      // Packet not long enough to be valid
      continue;
    }

    if(BRANCH_LIKELY(buf[0] == MX_MAGIC0 && buf[1] == MX_MAGIC1 && (n % MX_PACKET_LEN) == 0))
    {
      // Packet is an MX TS packet

      timestamp = timestamp_ms();
    
      /* Feed in the packet(s) */
      rxBufferBurstPush(rxBufPtr, timestamp, buf, (uint16_t)n);
    }
    else if(BRANCH_UNLIKELY(buf[0] == MXHB_MAGIC0 && buf[1] == MXHB_MAGIC1 && n > MX_HB_LEN))
    {
      // Packet is a heartbeat

      mxhb_header_t *mxhb_header_ptr = (mxhb_header_t *)buf;

      mxhbresp_header.magic_bytes = MXHBRESP_MAGICBYTES_VALUE;

      // Don't acknowledge if actual packet length doesn't match the packet length in the header
      if(n != mxhb_header_ptr->packet_length)
      {
        continue;
      }

      if(ext_heartbeat_station(mxhb_header_ptr->callsign, mxhb_header_ptr->key, &ts_total, &ts_loss))
      {
        mxhbresp_header.auth_response = 0x01;
        mxhbresp_header.original_packet_length = mxhb_header_ptr->packet_length;
        mxhbresp_header.ts_total = ts_total;
        mxhbresp_header.ts_loss = ts_loss;
      }
      else
      {
        mxhbresp_header.auth_response = 0x00;
        mxhbresp_header.original_packet_length = mxhb_header_ptr->packet_length;
        mxhbresp_header.ts_total = 0;
        mxhbresp_header.ts_loss = 0;
      }

      sendto(sockfd, (uint8_t *)&mxhbresp_header, sizeof(mxhbresp_header), 0, &clientaddr, clientaddr_len);
    }
#if 0
    else
    {
      fprintf(stderr, "Incoming unknown magic bytes: 0x%02x, 0x%02x, length: %d / (%d, %d)\n", buf[0], buf[1], n, MX_PACKET_LEN, MX_HB_LEN);
    }
#endif
  }
}
