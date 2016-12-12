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

typedef struct {
    /* Input file descriptor */
    FILE *input_fd;
    
    /* Output UDP Socket */
    int output_socket;
    
    /* Output Mode (MX/TS) */
    _mode_t output_mode;
    
    /* Output packet counter */
    uint32_t output_counter;
    
    /* TS Packet Data buffer */
    uint8_t data[0x10 + TS_PACKET_SIZE];
    
} _push_t;

_push_t tspush;

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

static uint8_t _send_packet(void)
{
    int c;
    ts_header_t ts;
    
    if(fread(&tspush.data[0x10], 1, TS_PACKET_SIZE, tspush.input_fd) == TS_PACKET_SIZE)
    {
        if(tspush.data[0x10] != TS_HEADER_SYNC)
        {
	        /* Re-align input to the TS sync byte */
	        uint8_t *p = memchr(&tspush.data[0x10], TS_HEADER_SYNC, TS_PACKET_SIZE);
	        if(p == NULL) return 1;
	
	        c = p - &tspush.data[0x10];
	        memmove(&tspush.data[0x10], p, TS_PACKET_SIZE - c);
	
	        if(fread(&tspush.data[0x10 + TS_PACKET_SIZE - c], 1, c, tspush.input_fd) != c)
	        {
		        return 0;
	        }
        }

        if(ts_parse_header(&ts, &tspush.data[0x10]) != TS_OK)
        {
	        /* Don't transmit packets with invalid headers */
	        printf("TS_INVALID\n");
	        return 1;
        }

        /* We don't transmit NULL/padding packets */
        if(ts.pid == TS_NULL_PID) return 1;

        /* Counter (4 bytes little-endian) */
        tspush.data[0x05] = (tspush.output_counter & 0xFF000000) >> 24;
        tspush.data[0x04] = (tspush.output_counter & 0x00FF0000) >> 16;
        tspush.data[0x03] = (tspush.output_counter & 0x0000FF00) >>  8;
        tspush.data[0x02] = (tspush.output_counter & 0x000000FF) >>  0;

        if(tspush.output_mode == MODE_MX)
        {
	        /* Send the full MX packet */
	        send(tspush.output_socket, tspush.data, sizeof(tspush.data), 0);
        }
        else if(tspush.output_mode == MODE_TS)
        {
	        /* Send just the TS packet */
	        send(tspush.output_socket, &tspush.data[0x10], TS_PACKET_SIZE, 0);
        }

        tspush.output_counter++;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

static void _handle_alarm(int sig)
{
    /* Send 1 data packet */
    if(_send_packet())
    {
        /* Successful, se re-enable alarm handler */
        signal(SIGALRM, _handle_alarm);
	}
	else
	{
	    /* Failed, likely EOF, so close sockets */
	    if(tspush.input_fd != stdin)
	    {
		    fclose(tspush.input_fd);
	    }
	    
	    close(tspush.output_socket);
	    
	    /* Main function will trigger on sock !> 0 and terminate */
	    tspush.output_socket = 0;
    }
}

int main(int argc, char *argv[])
{
	int opt;
	int c;
	char *host = "localhost";
	char *port = "5678";
	char *callsign = NULL;
	int ai_family = AF_UNSPEC;
	
	int bitrate = 0;
	int packet_delay_us;
	
	tspush.input_fd = stdin;
	tspush.output_mode = MODE_MX;
	tspush.output_counter = 0;
	
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
				tspush.output_mode = MODE_TS;
			}
			else if(strcmp(optarg, "mx") == 0)
			{
				tspush.output_mode = MODE_MX;
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
	
	/* If output mode is specified */
	if(tspush.output_mode == MODE_MX)
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
	
	/* If input file is specified */
	if(argc - optind == 1)
	{
		tspush.input_fd = fopen(argv[optind], "rb");
		if(!tspush.input_fd)
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
	tspush.output_socket = _open_socket(host, port, ai_family);
	if(tspush.output_socket == -1)
	{
		printf("Failed to resolve %s\n", host);
		return(-1);
	}
	
	/* Initialise the header */
	memset(tspush.data, 0, sizeof(tspush.data));
	
	/* Packet ID / type */
	tspush.data[0x00] = 0xA1;
	tspush.data[0x01] = 0x55;
	
	/* Station ID (10 bytes, UTF-8) */
	if(callsign != NULL)
	{
		strncpy((char *) &tspush.data[0x06], callsign, 10);
	}
	
	if(bitrate == 0)
	{
	    /* Stream the data until fails, likely due to EOF */
        while(_send_packet()) {};
	
	    /* then close sockets */
	    if(tspush.input_fd != stdin)
        {
	        fclose(tspush.input_fd);
        }
        close(tspush.output_socket);
	}
	else
	{
	    /* Assume 204-byte input packets */
	    packet_delay_us = 1000000 / (bitrate/(TS_PACKET_SIZE*8));
	    
	    /* On SIGALRM, run _send_packet() */
	    signal(SIGALRM, _handle_alarm);
	    
	    /* Send a SIGALRM after packet_delay_us microseconds, and
	     * at intervals of every packet_delay_us microseconds thereafter */
	    ualarm (packet_delay_us, packet_delay_us);
	    
	    /* Loop main function until output socket is closed by signal handler */
	    while(tspush.output_socket > 0)
	    {
	        sleep(10);
	    }
	}
	
	return(0);
}

