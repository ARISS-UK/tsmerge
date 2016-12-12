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
#include "sim.h"

/* Set additional likelihood multiplier that station will remain in on/off state */
/* ie. 0.0 is fair, 1.0 will never change */
#define STICKINESS 0.0


#define SIM_STATIONS_NUMBER   3
_sim_station_t sim_stations[SIM_STATIONS_NUMBER];
_push_t sim_a_tspush;
_push_t sim_b_tspush;
_push_t sim_c_tspush;

int64_t global_clock;
uint8_t *input_data;
int64_t input_length;

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
		"Usage: tssim [options] INPUT\n"
		"\n"
		"  -h, --host <name>      Set the hostname to send data to. Default: localhost\n"
		"  -p, --port <number>    Set the port number to send data to. Default: 5678\n"
		"  -b, --bitrate <number> Set the desired output bitrate (accurate for 204-byte input only)\n"
		"\n"
	);
}

static uint8_t _send_packet(_sim_station_t *sta)
{
    ts_header_t ts;
    
    /* Load next packet from input */
    if(memcpy(&sta->tspush.data[0x10], &input_data[sta->counter], TS_PACKET_SIZE))
    {
        if(ts_parse_header(&ts, &sta->tspush.data[0x10]) != TS_OK)
        {
	        /* Don't transmit packets with invalid headers */
	        printf("TS_INVALID\n");
	        return 1;
        }

        /* We don't transmit NULL/padding packets */
        if(ts.pid == TS_NULL_PID) return 1;

        /* Counter (4 bytes little-endian) */
        sta->tspush.data[0x05] = (sta->tspush.output_counter & 0xFF000000) >> 24;
        sta->tspush.data[0x04] = (sta->tspush.output_counter & 0x00FF0000) >> 16;
        sta->tspush.data[0x03] = (sta->tspush.output_counter & 0x0000FF00) >>  8;
        sta->tspush.data[0x02] = (sta->tspush.output_counter & 0x000000FF) >>  0;

        send(sta->tspush.output_socket, sta->tspush.data, sizeof(sta->tspush.data), 0);

        sta->tspush.output_counter++;
        
        return 1;
    }
    else
    {
        return 0;
    }
}

static void _handle_alarm(int sig)
{
    int i;
    
    for(i=0;i<SIM_STATIONS_NUMBER;i++)
    {
        /* Skip if station is done */
        if(sim_stations[i].counter == input_length) continue;
        
        /* Check if this station is due another packet by it's latency */
        if((global_clock*TS_PACKET_SIZE) > (sim_stations[i].counter + sim_stations[i].latency_offset_ms + (rand() % sim_stations[i].latency_variance_ms)))
        {
            sim_stations[i].counter += TS_PACKET_SIZE;
            
            /* Check if it's due to drop out */
            if((global_clock % sim_stations[i].dropout_interval) < sim_stations[i].dropout_length)
            {
                /* Drop out! (no data sent) */
            }
            else
            {
                /* All good, Send a packet */
                if(!_send_packet(&sim_stations[i]))
                {
                    /* Data read error, end the station */
                    sim_stations[i].counter = input_length;
                }
            }                
            
            if(sim_stations[i].counter == input_length)
            {
                /* Station finished, close output socket */
                close(sim_stations[i].tspush.output_socket);
            }
        }
    }
    
    signal(SIGALRM, _handle_alarm);
    //printf("%ld\n",global_clock);
    global_clock++;
}

int main(int argc, char *argv[])
{
	int opt;
	int c, i;
	char *host = "localhost";
	char *port = "5678";
	int ai_family = AF_UNSPEC;
	
	int bitrate = 2023000; /* Approximate HAMTV bitrate */
    /* Assume 204-byte input packets */
	int packet_delay_us= 1000000 / (bitrate/(TS_PACKET_SIZE*8));
	
	FILE *input_fd = stdin;
	
	static const struct option long_options[] = {
		{ "host",        required_argument, 0, 'h' },
		{ "port",        required_argument, 0, 'p' },
		{ "bitrate",     required_argument, 0, 'b' },
		{ 0,             0,                 0,  0  }
	};
	
	opterr = 0;
	while((c = getopt_long(argc, argv, "h:p:b:", long_options, &opt)) != -1)
	{
		switch(c)
		{
		case 'h': /* --host <name> */
			host = optarg;
			break;
		
		case 'p': /* --port <number> */
			port = optarg;
			break;
		
		case 'b': /* --bitrate <number> */
			bitrate = atoi(optarg);
			break;
		
		case '?':
			_print_usage();
			return(0);
		}
	}
	
	/* If input file is specified */
	if(argc - optind == 1)
	{
		input_fd = fopen(argv[optind], "rb");
		if(!input_fd)
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
	
	/* Seek to the end of the file to find size */
    fseek(input_fd, 0, SEEK_END);
    input_length = ftell(input_fd);
    fseek(input_fd, 0, SEEK_SET);

    /* Allocate correctly sized buffer */
    input_data = malloc(input_length);
    
    int64_t data_ptr = 0;
    /* Read all data in, and pre-align TS packets */
    while(fread(&input_data[data_ptr], 1, TS_PACKET_SIZE, input_fd) == TS_PACKET_SIZE)
    {
        if(input_data[data_ptr] != TS_HEADER_SYNC)
        {
	        /* Re-align input to the TS sync byte */
	        uint8_t *p = memchr(&input_data[data_ptr], TS_HEADER_SYNC, TS_PACKET_SIZE);
	        if(p == NULL) return(1);
	
	        c = p - &(input_data[data_ptr]);
	        memmove(&input_data[data_ptr], p, TS_PACKET_SIZE - c);
	
	        if(fread(&input_data[data_ptr + TS_PACKET_SIZE - c], 1, c, input_fd) != c)
	        {
		        break;
	        }
	        data_ptr += c;
        }
	    data_ptr += TS_PACKET_SIZE;
	}
    /* Read all data in, and then close the file */
    //fread(input_data, input_length, 1, input_fd);
    fclose(input_fd);
    
    /* Set up sim stations */
    
    /** { callsign[10],
            latency_offset_ms, latency_variance_ms,
            dropout_interval [packets], dropout_length [packets],
            last_good [0], counter [0],
            [tspush] } **/
    sim_stations[0] = (_sim_station_t){ "SIM_A",  20,10, 3000,900, 0, sim_a_tspush };
    sim_stations[1] = (_sim_station_t){ "SIM_B",  40,20, 7000,900, 0, sim_b_tspush };
    sim_stations[2] = (_sim_station_t){ "SIM_C",  60,20, 11000,900, 0, sim_c_tspush };
    
    global_clock = 0;
    srand(time(NULL));

    /* Set up simulated stations */
    for(i=0;i<SIM_STATIONS_NUMBER;i++)
    {
	    /* Open the outgoing socket */
	    sim_stations[i].tspush.output_socket = _open_socket(host, port, ai_family);
	    if(sim_stations[i].tspush.output_socket == -1)
	    {
		    printf("Failed to resolve %s\n", host);
		    return(-1);
	    }
	
	    /* Initialise the header */
	    memset(sim_stations[i].tspush.data, 0, sizeof(sim_stations[i].tspush.data));
	
	    /* Packet ID / type */
	    sim_stations[i].tspush.data[0x00] = 0xA1;
	    sim_stations[i].tspush.data[0x01] = 0x55;
	
	    /* Station ID (10 bytes, UTF-8) */
	    if(sim_stations[i].callsign != NULL)
	    {
		    strncpy((char *) &sim_stations[i].tspush.data[0x06], sim_stations[i].callsign, 10);
	    }
	}
    
    /* On SIGALRM, run _send_packet() */
    signal(SIGALRM, _handle_alarm);
    
    /* Send a SIGALRM after packet_delay_us microseconds, and
     * at intervals of every packet_delay_us microseconds thereafter */
    ualarm (packet_delay_us, packet_delay_us);
	    
    /* Loop main function until output socket is closed by signal handler */
    uint8_t all_stations_finished;
    while(1)
    {
        all_stations_finished = 1;
        for(i=0;i<SIM_STATIONS_NUMBER;i++)
        {
            if(sim_stations[i].counter != input_length)
            {
                all_stations_finished = 0;
            }
        }
        
        if(all_stations_finished) break;
        
        sleep(10);
    }
	
	return(0);
}

