
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "merger.h"
#include "stations.h"

#include <stdlib.h> /* testing only */

static mx_packet_t *_get_packet(mx_t *s, int station, uint32_t counter)
{
	mx_packet_t *p;
	
	/* Check for a valid station */
	if(station < 0 || station >= _STATIONS) return(NULL);
	if(s->station[station].sid[0] == '\0') return(NULL);
	if(s->station[station].timestamp <= s->timestamp - _TIMEOUT_MS) return(NULL);
	
	/* Fetch a pointer to the target packet */
	p = &s->station[station].packet[counter & 0xFFFF];
	
	/* Is it stale? */
	if(p->station != station || p->counter != counter) return(NULL);
	
	/* Return the pointer */
	return(p);
}

static mx_packet_t *_next_pcr(mx_t *s, int station, uint32_t counter)
{
	mx_station_t *st;
	mx_packet_t *p;
	
	/* Increments the counter until we find the first
	 * packet in the designated PID with a PCR timestamp.
	 * Returns a pointer to the packet on success. */
	
	st = &s->station[station];
	
	for(; counter != st->latest + 1; counter++)
	{
		/* Test for a valid packet */
		p = &st->packet[counter & 0xFFFF];
		
		/* Packet must exist */
		if(p->station != station || p->counter != counter) continue;
		
		/* Packet header must have been parsed */
		if(p->error != TS_OK) continue;
		
		/* Packet must be a part of the designated
		 * PID and have a timestamp */
		if(p->header.pid != s->pcr_pid ||
		   p->header.pcr_flag == 0) continue;
		
		/* Packet must be outside the guard period */
		if(p->timestamp >= s->timestamp - _GUARD_MS) continue;
		
		/* Found one */
		return(p);
	}
	
	return(NULL);
}

static mx_packet_t *_next_segment(mx_t *s, int station, mx_packet_t **r)
{
	mx_station_t *st;
	mx_packet_t *left, *right;
	
	/* Convenience pointer */
	st = &s->station[station];
	
	left = _get_packet(s, station, st->right);
	
	if(st->left == st->right || left == NULL)
	{
		/* No segment is currently set. Search for the first usable packet */
		left = _next_pcr(s, station, st->current);
		if(left == NULL) return(NULL);
	}
	
	/* Scan for the right hand side of the new segment */
	right = _next_pcr(s, station, left->counter + 1);
	if(right == NULL) return(NULL);
	
	/* A new segment was found. Update the station data */
	st->left = left->counter;
	st->right = right->counter;
	
	/* Advance the current stream position */
	st->current = right->counter + 1;
	
	/* Set a pointer to the right hand packet */
	if(r != NULL) *r = right;
	
	/* Return a pointer to the left hand packet */
	return(left);
}

static void _reset_station(mx_t *s, int id, uint32_t counter)
{
	/* The memset 0 creates a false positive 'valid' packet for station 0 */
	s->station[id].packet[0].counter = 1;
	
	/* Set the stream positions */
	s->station[id].current = counter;
	s->station[id].latest = counter;
	
	/* Set connection information */
	s->station[id].connected = timestamp_ms();
	s->station[id].counter_initial = counter;
	s->station[id].received = 0;
	s->station[id].received_sum = 0;
	s->station[id].selected = 0;
	s->station[id].selected_sum = 0;
}

static int _auth_station(mx_t *s, char psk[10])
{
	int i;
	
	/* Search for the station ID, return index if found or -1 */
	for(i = 0; i < _STATIONS; i++)
	{
		if((s->station[i].enabled == 1) && (strncmp(s->station[i].psk, psk, strlen(s->station[i].psk)) == 0))
		{
			/* Found a matching station */
			return(i);
		}
	}
	
	return(-1);
}

void mx_init(mx_t *s, uint16_t pcr_pid)
{
	memset(s, 0, sizeof(mx_t));
	
	s->pcr_pid = pcr_pid;
	s->next_station = -1;
}

void mx_feed(mx_t *s, int64_t timestamp, uint8_t *data)
{
	int i;
	int32_t d;
	uint32_t counter;
	mx_packet_t *p;
	
	/* Update the global timestamp */
	s->timestamp = timestamp;
	
	/* Packet ID (2 bytes) */
	if(data[0x00] != 0xA1 || data[0x01] != 0x55)
	{
		/* Invalid header. Ignore this packet */
		return;
	}
	
	/* Lookup the station number */
	i = _auth_station(s, (char *) &data[0x06]);
	
	if(i < 0)
	{
		//printf("Unrecognised PSK: %.10s\n", (char *) &data[0x06]);
		return;
	}
	
	/* Counter (4 bytes little-endian) */
	counter = (uint32_t) data[0x05] << 24
	        | (uint32_t) data[0x04] << 16
	        | (uint32_t) data[0x03] <<  8
	        | (uint32_t) data[0x02] <<  0;
	
	/* Existing station, ensure this new packet
	 * has a counter within the expected bounds */
	d = (int32_t) counter - (int32_t) s->station[i].current;
	
	if(s->station[i].connected==0 || d < -0xFFFF || d > 0xFFFF)
	{
		/* Never connected, or the counter is too far out */
		printf("Station %d counter reset\n", i);
		_reset_station(s, i, counter);
	}
	else if(d <= 0)
	{
		/* The current stream position has already moved past
		 * this packet, it's too late to process it */
		printf("Dropping late packet for station %d\n", i);
		return;
	}
	
	
	
	/* Get a pointer to where the packet should go */
	p = &s->station[i].packet[counter & 0xFFFF];
	
	/* Do we already have this packet? If so, ignore it */
	if(p->station == i && p->counter == counter)
	{
		printf("Duplicate packet received from station %d\n", i);
		return;
	}
	
	/* Insert the packet into memory */
	memset(p, 0, sizeof(mx_packet_t));
	p->station   = i;
	p->counter   = counter;
	p->timestamp = timestamp;
	
	memcpy(p->raw, &data[0x10], TS_PACKET_SIZE);
	p->error = ts_parse_header(&p->header, p->raw);
	
	p->next_station = -1;
	p->next_counter = 0;
	
	//printf("%d: ", counter);
	//if(p->error != TS_INVALID) ts_dump_header(&p->header);
	//else printf("TS_INVALID\n");
	
	/* Update the station data */
	d = (int32_t) counter - (int32_t) s->station[i].latest;
	if(d > 0)
	{
		s->station[i].latest = counter;
	}
	
	s->station[i].timestamp = timestamp;
	s->station[i].received++;
	s->station[i].received_sum++;
}

int mx_update(mx_t *s, int64_t timestamp)
{
	int i;
	mx_packet_t *o, *l, *r, *p;
	uint64_t pcr, best_pcr;
	uint32_t counter;
	int best_station;
        uint64_t timer;
        timer = timestamp_ms();
	FILE *debug_fp;
	time_t debug_now;
	char debug_buf[sizeof "2011-10-08T07:07:09Z"];

	debug_fp = fopen("/srv/tsmerge/logs/next_segment_log.txt","a+");

	time(&debug_now);
	strftime(debug_buf, sizeof debug_buf, "%FT%TZ", gmtime(&debug_now));
	fprintf(debug_fp, "%s: Begun MX Update\n", debug_buf);

	/* Update the global timestamp */
	s->timestamp = timestamp;
	
	/* Fetch the timestamp of the last packet sent, or 0 */
	o = _get_packet(s, s->next_station, s->next_counter);
	pcr = (o != NULL ? o->header.pcr_base : 0);
	
	best_station = -1;
	best_pcr = 0;
	
	/* For each station, process the segments until we find one that
	 * begins at or after the timestamp 'pcr' */
	for(i = 0; i < _STATIONS; i++)
	{
		/* Skip inactive stations */
		if(s->station[i].sid[0] == '\0') continue;
		if(s->station[i].timestamp <= s->timestamp - _TIMEOUT_MS) continue;
		
		while((p = _next_segment(s, i, &r)) != NULL)
		{
			fprintf(debug_fp, "next_segment: station: %d, p->header.pcr_base: %ld, r->header.pcr_base: %ld\n",i, p->header.pcr_base, r->header.pcr_base);
			/* Skip past segments with weird or invalid PCR timings */
			/* TODO: This won't handle clock roll-over well */
			if(p->header.pcr_base >= r->header.pcr_base) continue;
			if(r->header.pcr_base - p->header.pcr_base > _SEGMENT_PCR_LIMIT) continue;
			
			/* Stop when we are at or ahead of the last sent segment */
			if(p->header.pcr_base >= pcr) break;
		}
		
		/* Didn't find a newer segment? */
		if(p == NULL)
                {
                    fprintf(debug_fp, "next_segment: station %d returned NULL\n", i);
                    continue;
                }
		
		/* Track which station is offering the "best" segment to use next */
		if(best_station == -1 || p->header.pcr_base < best_pcr)
		{
			best_station = i;
			best_pcr = p->header.pcr_base;
		}
		else if(p->header.pcr_base == best_pcr)
		{
			/* TODO: Use segment and station score to decide
			 * if we should switch to this station next */
		}
	}
	
	if(best_station == -1)
	{
		fprintf(debug_fp, "MX update finished, no valid station, took %ld ms\n", (timestamp_ms() - timer));
		fclose(debug_fp);
		return(0);
	}
	
	/* Link the packets inside the segment */
	l = NULL;
	for(counter = s->station[best_station].left; counter != s->station[best_station].right + 1; counter++)
	{
		p = _get_packet(s, best_station, counter);
		if(p == NULL) continue;
		
		/* Link the last packet to this one */
		if(l != NULL)
		{
			l->next_station = p->station;
			l->next_counter = p->counter;
		}
		
		l = p;
	}
	
	/* Link the previous segment to this one */
	if(o != NULL)
	{
		p = _get_packet(s, best_station, s->station[best_station].left);
		
		o->next_station = p->station;
		o->next_counter = (pcr == best_pcr ? p->next_counter : p->counter);
	}
	
	/* Update pointer for new stations */
	s->next_station = best_station;
	s->next_counter = s->station[s->next_station].right;
	
	/* Update station stats */
	s->station[best_station].selected++;
	s->station[best_station].selected_sum++;
	
	//printf("s->next_station = %d\n", s->next_station);
	//printf("s->next_counter = %d\n", s->next_counter);
	
	fprintf(debug_fp, "MX update finished, best station: %d, took %ld ms\n", best_station, (timestamp_ms() - timer));
	fclose(debug_fp);

	return(1);
}

void _setup_stations(mx_t *s)
{
  pthread_mutex_lock(&merger.lock);

  /* Zero out struct */
  memset(&s->station[0], 0, sizeof(mx_station_t));
  #if defined(STATION_0_ENABLED) && defined(STATION_0_SID) && defined(STATION_0_PSK) 
    /* Set station-specific fields */
    s->station[0].enabled = STATION_0_ENABLED;
    strncpy(s->station[0].sid, STATION_0_SID, 10);
    strncpy(s->station[0].psk, STATION_0_PSK, 10);
  #endif

  /* Zero out struct */
  memset(&s->station[1], 0, sizeof(mx_station_t));
  #if defined(STATION_1_ENABLED) && defined(STATION_1_SID) && defined(STATION_1_PSK)
    /* Set station-specific fields */
    s->station[1].enabled = STATION_1_ENABLED;
    strncpy(s->station[1].sid, STATION_1_SID, 10);
    strncpy(s->station[1].psk, STATION_1_PSK, 10);
  #endif

  /* Zero out struct */
  memset(&s->station[2], 0, sizeof(mx_station_t));
  #if defined(STATION_2_ENABLED) && defined(STATION_2_SID) && defined(STATION_2_PSK)
    /* Set station-specific fields */
    s->station[2].enabled = STATION_2_ENABLED;
    strncpy(s->station[2].sid, STATION_2_SID, 10);
    strncpy(s->station[2].psk, STATION_2_PSK, 10);
  #endif

  /* Zero out struct */
  memset(&s->station[3], 0, sizeof(mx_station_t));
  #if defined(STATION_3_ENABLED) && defined(STATION_3_SID) && defined(STATION_3_PSK)
    /* Set station-specific fields */
    s->station[3].enabled = STATION_3_ENABLED;
    strncpy(s->station[3].sid, STATION_3_SID, 10);
    strncpy(s->station[3].psk, STATION_3_PSK, 10);
  #endif

  /* Zero out struct */
  memset(&s->station[4], 0, sizeof(mx_station_t));
  #if defined(STATION_4_ENABLED) && defined(STATION_4_SID) && defined(STATION_4_PSK)
		/* Set station-specific fields */
		s->station[4].enabled = STATION_4_ENABLED;
		strncpy(s->station[4].sid, STATION_4_SID, 10);
		strncpy(s->station[4].psk, STATION_4_PSK, 10);
	#endif

  /* Zero out struct */
  memset(&s->station[5], 0, sizeof(mx_station_t));
  #if defined(STATION_5_ENABLED) && defined(STATION_5_SID) && defined(STATION_5_PSK)
		/* Set station-specific fields */
		s->station[5].enabled = STATION_5_ENABLED;
		strncpy(s->station[5].sid, STATION_5_SID, 10);
		strncpy(s->station[5].psk, STATION_5_PSK, 10);
	#endif

  /* Zero out struct */
  memset(&s->station[6], 0, sizeof(mx_station_t));
  #if defined(STATION_6_ENABLED) && defined(STATION_6_SID) && defined(STATION_6_PSK)
		/* Set station-specific fields */
		s->station[6].enabled = STATION_6_ENABLED;
		strncpy(s->station[6].sid, STATION_6_SID, 10);
		strncpy(s->station[6].psk, STATION_6_PSK, 10);
  #endif

  /* Zero out struct */
  memset(&s->station[7], 0, sizeof(mx_station_t));
  #if defined(STATION_7_ENABLED) && defined(STATION_7_SID) && defined(STATION_7_PSK)
		/* Set station-specific fields */
		s->station[7].enabled = STATION_7_ENABLED;
		strncpy(s->station[7].sid, STATION_7_SID, 10);
		strncpy(s->station[7].psk, STATION_7_PSK, 10);
  #endif

  pthread_mutex_unlock(&merger.lock);
}

void *merger_mx(void* arg)
{
    (void) arg;
	uint64_t timestamp;
    
    /* TODO: Trigger mx_update() via a signal or condition var (only),
     * when mx_feed() adds a new PCR packet for a station (ie. completing a new segment) */
    /* Passing the station id as well would remove the need to loop over all stations
     * as it would only need to compare the current versus the new one,
     * for being either newer or less lossy. */

    _setup_stations(&merger);
    
    while(1)
    {
        /* Process any pending data */
        pthread_mutex_lock(&merger.lock);
        
        timestamp = timestamp_ms();
        while(mx_update(&merger, timestamp) > 0);
        
        pthread_mutex_unlock(&merger.lock);
        
        sleep_ms(5);
    }
}

mx_packet_t *mx_next(mx_t *s, int last_station, uint32_t last_counter)
{
	mx_packet_t *p = _get_packet(s, last_station, last_counter);
	
	/* Fetch a pointer to the next packet in the chain, or if no
	 * previous valid packet use the latest segment in s */
	if(p == NULL) p = _get_packet(s, s->next_station, s->next_counter);
	else          p = _get_packet(s, p->next_station, p->next_counter);
	
	return(p);
}

