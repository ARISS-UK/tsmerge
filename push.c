/* tspush - Relay TS data to tsmerger                                    */
/*=======================================================================*/
/* Copyright (C)2016 Philip Heron <phil@sanslogic.co.uk>                 */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include "ts.h"

typedef enum {
	MODE_MX,
	MODE_TS,
} _mode_t;

static int _open_socket(char *host, char *port, int ai_family)
{
	int r;
	int sock;
	struct addrinfo hints;
	struct addrinfo *re, *rp;
	char s[INET6_ADDRSTRLEN];
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = ai_family;
	hints.ai_socktype = SOCK_DGRAM;
	
	r = getaddrinfo(host, port, &hints, &re);
	if(r != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
		return(-1);
	}
	
	/* Try IPv6 first */
	for(sock = -1, rp = re; sock == -1 && rp != NULL; rp = rp->ai_next)
	{
		if(rp->ai_addr->sa_family != AF_INET6) continue;
		
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) rp->ai_addr)->sin6_addr), s, INET6_ADDRSTRLEN);
		printf("Sending to [%s]:%d\n", s, ntohs((((struct sockaddr_in6 *) rp->ai_addr)->sin6_port)));
		
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock == -1)
		{
			perror("socket");
			continue;
		}
		
		if(connect(sock, rp->ai_addr, rp->ai_addrlen) == -1)
		{
			perror("connect");
			close(sock);
			sock = -1;
		}
	}
	
	/* Try IPv4 next */
	for(rp = re; sock == -1 && rp != NULL; rp = rp->ai_next)
	{
		if(rp->ai_addr->sa_family != AF_INET) continue;
		
		inet_ntop(AF_INET, &(((struct sockaddr_in *) rp->ai_addr)->sin_addr), s, INET6_ADDRSTRLEN);
		printf("Sending to %s:%d\n", s, ntohs((((struct sockaddr_in *) rp->ai_addr)->sin_port)));
		
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock == -1)
		{
			perror("socket");
			continue;
		}
		
		if(connect(sock, rp->ai_addr, rp->ai_addrlen) == -1)
		{
			perror("connect");
			close(sock);
			sock = -1;
		}
	}
	
	freeaddrinfo(re);
	
	return(sock);
}

void _print_usage(void)
{
	printf(
		"\n"
		"Usage: tspush [options] INPUT\n"
		"\n"
		"  -h, --host <name>      Set the hostname to send data to. Default: localhost\n"
		"  -p, --port <number>    Set the port number to send data to. Default: 5678\n"
		"  -4, --ipv4             Force IPv4 only.\n"
		"  -6, --ipv6             Force IPv6 only.\n"
		"  -m, --mode <ts|mx>     Send raw TS packets, or MX packets for TS merger.\n"
		"                         Default: mx\n"
		"  -c, --callsign <id>    Set the station callsign, up to 10 characters.\n"
		"                         Required for mx mode. Not used by ts mode.\n"
		"  -b, --bitrate <number> Set the desired output bitrate (accurate for 204-byte input only)\n"
		"\n"
	);
}


int sock;
uint8_t data[0x10 + TS_PACKET_SIZE];
_mode_t mode;
FILE *f;
int r;
int c;
ts_header_t ts;
uint32_t counter;

static uint8_t stream_finished;
static void _send_packet(int sig)
{
    /* Send 1 data packet */
	if(fread(&data[0x10], 1, TS_PACKET_SIZE, f) == TS_PACKET_SIZE)
	{
        /* Re-enable signal handler */
        signal(SIGALRM, _send_packet);
        
        if(data[0x10] != TS_HEADER_SYNC)
	    {
		    /* Re-align input to the TS sync byte */
		    uint8_t *p = memchr(&data[0x10], TS_HEADER_SYNC, TS_PACKET_SIZE);
		    if(p == NULL) return;
		
		    c = p - &data[0x10];
		    memmove(&data[0x10], p, TS_PACKET_SIZE - c);
		
		    if(fread(&data[0x10 + TS_PACKET_SIZE - c], 1, c, f) != c)
		    {
			    return;
		    }
	    }
	
	    if(ts_parse_header(&ts, &data[0x10]) != TS_OK)
	    {
		    /* Don't transmit packets with invalid headers */
		    printf("TS_INVALID\n");
		    return;
	    }
	
	    /* We don't transmit NULL/padding packets */
	    if(ts.pid == TS_NULL_PID) return;
	
	    /* Counter (4 bytes little-endian) */
	    data[0x05] = (counter & 0xFF000000) >> 24;
	    data[0x04] = (counter & 0x00FF0000) >> 16;
	    data[0x03] = (counter & 0x0000FF00) >>  8;
	    data[0x02] = (counter & 0x000000FF) >>  0;
	
	    if(mode == MODE_MX)
	    {
		    /* Send the full MX packet */
		    send(sock, data, sizeof(data), 0);
	    }
	    else if(mode == MODE_TS)
	    {
		    /* Send just the TS packet */
		    send(sock, &data[0x10], TS_PACKET_SIZE, 0);
	    }
	
	    counter++;
	}
	else
	{
        /* Signal main process to exit */
	    stream_finished = 1;
    }
}

static void _stream_data(void)
{
    /* Stream the data */
	while(fread(&data[0x10], 1, TS_PACKET_SIZE, f) == TS_PACKET_SIZE)
	{
       if(data[0x10] != TS_HEADER_SYNC)
	    {
		    /* Re-align input to the TS sync byte */
		    uint8_t *p = memchr(&data[0x10], TS_HEADER_SYNC, TS_PACKET_SIZE);
		    if(p == NULL) continue;
		
		    c = p - &data[0x10];
		    memmove(&data[0x10], p, TS_PACKET_SIZE - c);
		
		    if(fread(&data[0x10 + TS_PACKET_SIZE - c], 1, c, f) != c)
		    {
			    break;
		    }
	    }
	
	    if(ts_parse_header(&ts, &data[0x10]) != TS_OK)
	    {
		    /* Don't transmit packets with invalid headers */
		    printf("TS_INVALID\n");
		    continue;
	    }
	
	    /* We don't transmit NULL/padding packets */
	    if(ts.pid == TS_NULL_PID) continue;
	
	    /* Counter (4 bytes little-endian) */
	    data[0x05] = (counter & 0xFF000000) >> 24;
	    data[0x04] = (counter & 0x00FF0000) >> 16;
	    data[0x03] = (counter & 0x0000FF00) >>  8;
	    data[0x02] = (counter & 0x000000FF) >>  0;
	
	    if(mode == MODE_MX)
	    {
		    /* Send the full MX packet */
		    send(sock, data, sizeof(data), 0);
	    }
	    else if(mode == MODE_TS)
	    {
		    /* Send just the TS packet */
		    send(sock, &data[0x10], TS_PACKET_SIZE, 0);
	    }
	
	    counter++;
	}
}

int main(int argc, char *argv[])
{
	int opt;
	char *host = "localhost";
	char *port = "5678";
	char *callsign = NULL;
	int bitrate = 0;
	int packet_delay_us;
	int ai_family = AF_UNSPEC;
	
	counter = 0;
	mode = MODE_MX;
	f = stdin;
	
	static const struct option long_options[] = {
		{ "host",        required_argument, 0, 'h' },
		{ "port",        required_argument, 0, 'p' },
		{ "ipv6",        no_argument,       0, '6' },
		{ "ipv4",        no_argument,       0, '4' },
		{ "callsign",    required_argument, 0, 'c' },
		{ "mode",        required_argument, 0, 'm' },
		{ "bitrate",     required_argument, 0, 'b' },
		{ 0,             0,                 0,  0  }
	};
	
	opterr = 0;
	while((c = getopt_long(argc, argv, "h:p:64c:b:", long_options, &opt)) != -1)
	{
		switch(c)
		{
		case 'h': /* --host <name> */
			host = optarg;
			break;
		
		case 'p': /* --port <number> */
			port = optarg;
			break;
		
		case '6': /* --ipv6 */
			ai_family = AF_INET6;
			break;
		
		case '4': /* --ipv4 */
			ai_family = AF_INET;
			break;
		
		case 'm': /* --mode <ts|mx> */
			if(strcmp(optarg, "ts") == 0)
			{
				mode = MODE_TS;
			}
			else if(strcmp(optarg, "mx") == 0)
			{
				mode = MODE_MX;
			}
			else
			{
				printf("Error: Unrecognised mode '%s'\n", optarg);
				_print_usage();
				return(-1);
			}
			
			break;
		
		case 'c': /* --callsign <id> */
			callsign = optarg;
			break;
		
		case 'b': /* --bitrate <number> */
			bitrate = atoi(optarg);
			break;
		
		case '?':
			_print_usage();
			return(0);
		}
	}
	
	if(mode == MODE_MX)
	{
		if(callsign == NULL)
		{
			printf("Error: A callsign is required in mx mode\n");
			_print_usage();
			return(-1);
		}
		else if(strlen(callsign) > 10)
		{
			printf("Error: Callsign cannot be longer than 10 characters\n");
			_print_usage();
			return(-1);
		}
	}
	
	if(argc - optind == 1)
	{
		f = fopen(argv[optind], "rb");
		if(!f)
		{
			perror("fopen");
			return(-1);
		}
        }
	else if(argc - optind > 1)
	{
		printf("Error: More than one input file specified\n");
		_print_usage();
		return(0);
	}
	
	/* Open the outgoing socket */
	sock = _open_socket(host, port, ai_family);
	if(sock == -1)
	{
		printf("Failed to resolve %s\n", host);
		return(-1);
	}
	
	/* Initialise the header */
	memset(data, 0, sizeof(data));
	
	/* Packet ID / type */
	data[0x00] = 0xA1;
	data[0x01] = 0x55;
	
	/* Station ID (10 bytes, UTF-8) */
	if(callsign != NULL)
	{
		strncpy((char *) &data[0x06], callsign, 10);
	}
	
	if(bitrate == 0)
	{
	    /* Default mode, stream it as we get it */
	    _stream_data();
	}
	else
	{
	    /* Assume 204-byte input packets */
	    packet_delay_us = 1000000 / (bitrate/(TS_PACKET_SIZE*8));
	    
	    /* On SIGALRM, run _send_packet() */
	    stream_finished = 0;
	    signal(SIGALRM, _send_packet);
	    
	    /* Send a SIGALRM after packet_delay_us microseconds, and
	     * at intervals of every packet_delay_us microseconds thereafter */
	    ualarm (packet_delay_us, packet_delay_us);
	    
	    while(!stream_finished)
	    {
	        sleep(10);
	    }
	}
	
	if(f != stdin)
	{
		fclose(f);
	}
	
	close(sock);
	
	return(0);
}

