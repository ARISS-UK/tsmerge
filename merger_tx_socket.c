#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
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

/* This function is run on a thread, started from main()
 *
 * This function listens on the TCP port for incoming connections from viewers
 * On connection, the viewer's socket file descriptor is added to the 'viewers' array
 *  to be serviced by 'merger_tx_feed'
 */
void *merger_tx_socket(void* arg)
{
  (void) arg;
  int parentsock; /* parent socket */
  int childsock; /* child socket */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  char clientip[INET_ADDRSTRLEN]; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  int i; /* Looper var */
  int viewerstored; /* Flag indication of successful viewer storage */
  
  clientlen = sizeof(clientaddr);
  
  /* Prepare the network - ignore SIGPIPE on viewer disconnection */
	signal(SIGPIPE, SIG_IGN);

  /* Open TCP Socket */
  parentsock = socket(AF_INET, SOCK_STREAM, 0);
  if (parentsock < 0)
  {
    fprintf(stderr,"ERROR opening socket");
  }

  /* Add REUSEADDR Flag (TODO: Check if needed) */
  optval = 1;
  setsockopt(parentsock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  /* Set up server address */
  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(MERGER_TCP_TX_PORT);

  /* Bind Socket to server address */
  if (bind(parentsock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
  { 
    fprintf(stderr, "Output socket failed to bind\n");
  }

  /* Listen, and allow 5 requests in queue */
  if (listen(parentsock, 5) < 0)
  {
    fprintf(stderr, "Output socket failed to listen");
  }

  /* Infinite loop, blocks until incoming connection */
  while (1)
  {
    /* Block here until we receive an incoming connection */
    childsock = accept(parentsock, (struct sockaddr *) &clientaddr, &clientlen);
    if (childsock < 0)
    {
      fprintf(stderr, "Output socket failed to accept connection");
    }
    
    /* Process IP address of connection client */
    clientip[0] = '\0';
    inet_ntop(AF_INET, &clientaddr.sin_addr, clientip, INET_ADDRSTRLEN);
    printf("New output connection from %s\n", clientip);


    /* Attempt to set TCP_NODELAY on output socket */
    i = 1;
    n = setsockopt(childsock, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int));
    if(n < 0)
    {
      fprintf(stderr, "%d: Error setting TCP_NODELAY on output socket\n", childsock);
      perror("setsockopt");
      /* This is not a fatal error */
    }
    
    /* Add Client Socket to Viewer array */
    viewerstored = 0;
    for(i = 0; i < _VIEWERS_MAX; i++)
	  {
		  if(viewers[i].sock > 0) continue;
		
		  viewers[i].sock = childsock;
		  viewers[i].last_station = -1;
		  viewers[i].last_counter = 0;
		  viewers[i].timestamp = timestamp_ms();
		
		  viewerstored = 1;
		  break;
	  }
	
	  if(!viewerstored)
	  {
	    /* No free slots, disconnect */
	    //n = send(sock, "BUSY\n", 5, 0);
      close(childsock);
    }
  }
}
