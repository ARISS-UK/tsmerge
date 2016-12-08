
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
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
#include "merger.h"

/* The maximum number of viewers */
#define _VIEWERS 10

/* Timeout for clients (ms) */
#define _VIEWER_TIMEOUT (60 * 1000)

/* Number of sockets for poll() */
#define _NFDS (_VIEWERS + 2)

/* The size of the incoming UDP buffer */
#define _BUFFER 65536

typedef struct {
	
	/* Socket for this viewer */
	int sock;
	
	/* The last packet sent to viewer */
	int last_station;
	uint32_t last_counter;
	
	/* Timestamp of when the last packet was sent */
	int64_t timestamp;
	
} viewer_t;

/* the TS merger state */
static mx_t _merger;

/* state for each client / viewer */
static viewer_t _viewers[_VIEWERS];

/* pollfd array for each client + 2 for incoming sockets */
static struct pollfd _fds[2 + _VIEWERS];

/* Returns the current unix timestamp in ms, or 0 if error */
static int64_t _timestamp_ms(void)
{
	struct timespec tp;
	
	if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
	{
		return(0);
	}
	
	return((int64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000);
}

static int _open_viewer_socket(void)
{
	int sock;
	int sarg;
	struct sockaddr_in addr;
	int r;
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return(-1);
	}
	
	/* Set the listener socket to be non-blocking */
	fcntl(sock, F_SETFL, O_NONBLOCK);
	
	sarg = 1;
	r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sarg, sizeof(int));
	if(r < 0)
	{
		perror("setsockopt");
		close(sock);
		return(-1);
	}
	
	/* Bind to TCP port 5679 */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(5679);
	
	r = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if(r < 0)
	{
		perror("bind");
		close(sock);
		return(-1);
	}
	
	r = listen(sock, 10);
	if(r < 0)
	{
		perror("listen");
		close(sock);
		return(-1);
	}
	
	return(sock);
}

static int _open_incoming_socket(void)
{
	int sock;
	struct sockaddr_in addr;
	int r;
	int optarg;
	
	/* Create the incoming socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return(-1);
	}
	
	/* Set the incoming socket to be non-blocking */
	fcntl(sock, F_SETFL, O_NONBLOCK);
	
	/* Set the RX buffer length */
	optarg = _BUFFER;
	
	r = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optarg, sizeof(optarg));
	if(r < 0)
	{
		perror("setsockopt");
		close(sock);
		return(-1);
	}
	
	/* Bind to UDP port 5678 */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(5678);
	
	r = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if(r < 0)
	{
		perror("bind");
		close(sock);
		return(-1);
	}
	
	return(sock);
}

static int _incoming_packet(struct pollfd *fds, int64_t timestamp, mx_t *merger)
{
	int i, r;
	static uint8_t data[_BUFFER];
	
	if(fds->revents != POLLIN)
	{
		fprintf(stderr, "Unexpected revents for incoming UDP socket (%d)\n", fds->revents);
		return(-1);
	}
	
	/* New UDP packet */
	r = recv(fds->fd, data, _BUFFER, 0);
	if(r < 0)
	{
		if(errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return(0);
		}
		
		fprintf(stderr, "Error receiving packet: %d\n", r);
		perror("recv");
		return(-1);
	}
	
	if(r % (0x10 + TS_PACKET_SIZE) != 0)
	{
		fprintf(stderr, "Incoming packet invalid size, expected a multiple of 204 bytes, got %d\n", r);
		return(0);
	}
	
	/* Feed in the packet(s) */
	for(i = 0; i < r; i += (0x10 + TS_PACKET_SIZE))
	{
		mx_feed(merger, timestamp, &data[i]);
	}
	
	return(0);
}

static int _accept_connection(struct pollfd *fds, int64_t timestamp, viewer_t *viewers)
{
	int i, r;
	int sock;
	struct sockaddr_in addr;
	socklen_t addr_len;
	char ipaddr[INET_ADDRSTRLEN];
	
	if(fds->revents != POLLIN)
	{
		fprintf(stderr, "Unexpected revents for incoming TCP socket (%d)\n", fds->revents);
		return(-1);
	}
	
	addr_len = sizeof(addr);
	sock = accept(fds->fd, (struct sockaddr *) &addr, &addr_len);
	if(sock < 0)
	{
		if(errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return(0);
		}
		
		perror("accept");
		return(-1);
	}
	
	ipaddr[0] = '\0';
	inet_ntop(AF_INET, &addr.sin_addr, ipaddr, INET_ADDRSTRLEN);
	
	printf("New viewer connection from %s\n", ipaddr);
	
	i = 1;
	r = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(int));
	if(r < 0)
	{
		fprintf(stderr, "%d: Error setting TCP_NODELAY on client socket\n", sock);
		perror("setsockopt");
		/* This is not a fatal error */
	}
	
	for(i = 0; i < _VIEWERS; i++)
	{
		if(viewers[i].sock > 0) continue;
		
		viewers[i].sock = sock;
		viewers[i].last_station = -1;
		viewers[i].last_counter = 0;
		viewers[i].timestamp = timestamp;
		
		_fds[2 + i].fd = sock;
		_fds[2 + i].events = POLLIN;
		
		return(0);
	}
	
	/* No free slots, disconnect */
	//r = send(sock, "BUSY\n", 5, 0);
	close(sock);
	
	return(0);
}

static void _close_connection(viewer_t *viewers, int i)
{
	printf("Closing TCP socket %d\n", i);
	
	close(viewers[i].sock);
	
	viewers[i].sock = 0;
	viewers[i].last_station = -1;
	viewers[i].last_counter = 0;
	viewers[i].timestamp = 0;
	
	_fds[2 + i].fd = -1;
	_fds[2 + i].events = 0;
}

int main(int argc, char *argv[])
{
	int i, r;
	int64_t timestamp;
	
	/* Initialise the merger */
	/* In my example file, PID 256 contains the PCR clock */
	mx_init(&_merger, 256);
	
	/* Prepare the network - ignore SIGPIPE on viewer disconnection */
	signal(SIGPIPE, SIG_IGN);
	
	/* The first two entries in the fds array are for the listening sockets */
	memset(&_fds, 0, sizeof(_fds));
	
	_fds[0].fd = _open_incoming_socket();
	_fds[0].events = POLLIN;
	
	_fds[1].fd = _open_viewer_socket();
	_fds[1].events = POLLIN;
	
	for(i = 2; i < _NFDS; i++)
	{
		_fds[i].fd = -1;
		_fds[i].events = 0;
	}
	
	/* Clear the viewers array */
	memset(&_viewers, 0, sizeof(_viewers));
	
	/* The main network loop */
	while(1)
	{
		/* Wait for network activity, or 10ms */
		i = poll(_fds, _NFDS, 10);
		if(i < 0)
		{
			perror("poll");
			break;
		}
		
		timestamp = _timestamp_ms();
		
		/* Incoming UDP packet? */
		if(_fds[0].revents != 0)
		{
			r = _incoming_packet(&_fds[0], timestamp, &_merger);
			if(r < 0) break;
		}
		
		/* Incoming client connection? */
		if(_fds[1].revents != 0)
		{
			r = _accept_connection(&_fds[1], timestamp, _viewers);
			if(r < 0) break;
		}
		
		/* Check if there is data to send to each client */
		while(mx_update(&_merger, timestamp) > 0);
		
		for(i = 0; i < _VIEWERS; i++)
		{
			mx_packet_t *p;
			
			if(_viewers[i].sock <= 0) continue;
			
			/* First test if this socket was closed by the client */
			if(_fds[2 + i].fd > 0 && _fds[2 + i].revents == POLLIN)
			{
				/* The client has sent us data, or closed the socket */
				/* Either way, close the socket on this end */
				_close_connection(_viewers, i);
				continue;
			}
			
			/* Clear the viewer entry in the fds array */
			_fds[2 + i].fd = _viewers[i].sock;
			_fds[2 + i].events = POLLIN;
			
			/* See if there is any data still to send to this viewer */
			while((p = mx_next(&_merger, _viewers[i].last_station, _viewers[i].last_counter)) != NULL)
			{
				/* Try to send the new data to the viewer */
				r = send(_viewers[i].sock, p->raw, TS_PACKET_SIZE, 0);
				if(r < 0)
				{
					if(errno == EAGAIN || errno == EWOULDBLOCK)
					{
						/* The socket is busy, try again in the next loop */
						_fds[2 + i].events |= POLLOUT;
						break;
					}
					
					/* An error has occured. Drop the connection */
					perror("send");
					_close_connection(_viewers, i);
					break;
				}
				
				if(r != TS_PACKET_SIZE)
				{
					/* TODO: I don't know if send() can return 0 or a partial amount */
					printf("Got an odd result from send(): %d\n", TS_PACKET_SIZE);
				}
				
				/* Update viewer state */
				_viewers[i].last_station = p->station;
				_viewers[i].last_counter = p->counter;
				_viewers[i].timestamp = timestamp;
			}
			
			/* Test if the client has timed out */
			if(_viewers[i].sock > 0 && timestamp - _viewers[i].timestamp > _VIEWER_TIMEOUT)
			{
				//send(_viewers[i].sock, "TIMEOUT\n", 8, 0);
				_close_connection(_viewers, i);
			}
		}
	}
	
	/* Close any open sockets */
	for(i = 0; i < _VIEWERS; i++)
	{
		if(_viewers[i].sock > 0)
		{
			close(_viewers[i].sock);
		}
	}
	
	close(_fds[1].fd);
	close(_fds[0].fd);
	
	return(0);
}

