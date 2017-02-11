
#include <stdint.h>
#include "../ts/ts.h"

#ifndef _MERGER_H
#define _MERGER_H

#include <pthread.h>
#include "../ts/ts.h"
#include "../timing/timing.h"

#define MERGER_PCR_PID          256

#define MERGER_UDP_RX_PORT          5678
#define MERGER_UDP_RX_BUFSIZE       2048
#define MERGER_UDP_RX_SYSBUFSIZE    212992

#define MERGER_TCP_TX_PORT      5679

/* Maximum number of stations and packets */
#define _STATIONS 6
#define _PACKETS  UINT16_MAX

/* Station timeout in milliseconds */
#define _TIMEOUT_MS 10000

/* Guard period in milliseconds */
#define _GUARD_MS 1000

/* Length of MX packet */
#define MX_PACKET_LEN (0x10 + TS_PACKET_SIZE)

/* Packet structure: (little endian)
 *
 * uint16_t type    = Packet type identifier, always 0x55A1
 * uint32_t counter = Packet counter, starting with 0 and incremented by 1
 * char station[10] = Station callsign (eg. "MI0VIM-15"). Unused bytes set to 0x00.
*/

typedef struct {
	
	/* The station number */
	int station;
	
	/* The station packet counter */
	uint32_t counter;
	
	/* The receive time of the packet (in ms) */
	int64_t timestamp;
	
	/* Packet error flag, 0 == No Error, 1 = Error (header not populated) */
	int error;
	
	/* Parsed header */
	ts_header_t header;
	
	/* Processed flag */
	//int processed;
	
	/* A copy of the raw TS packet */
	uint8_t raw[TS_PACKET_SIZE];
	
	/* Branch information */
	int next_station;
	uint32_t next_counter;
	
} mx_packet_t;

typedef struct {
	
	/* The station ID */
	char sid[10];
	
	/* The current position in the stream */
	uint32_t current;
	
	/* The latest position counter value from this station */
	uint32_t latest;
	
	/* Timestamp when the station connected */
	int64_t connected;
	
	/* Counter when the station connected */
	uint32_t counter_initial;
	
	/* Total packets received (to allow net loss calculation) */
	uint32_t received;
	uint32_t received_sum;
	
	/* Number of times the station has been selected as best */
	uint32_t selected;
        uint32_t selected_sum;
	
	/* The receive time of the last packet (in ms) */
	int64_t timestamp;
	
	/* The current segment (if left == right, no segment) */
	uint32_t left;
	uint32_t right;
	
	/* The station packet buffer */
	mx_packet_t packet[_PACKETS];
	
} mx_station_t;

typedef struct {
	
	/* The PID to use for the PCR clock */
	uint16_t pcr_pid;
	
	/* The current timestamp */
	int64_t timestamp;
	
	/* Pointer to the latest packet */
	int next_station;
	uint32_t next_counter;
	
	/* The station array */
	mx_station_t station[_STATIONS];
	
	/* Big Fat Lock TODO: Don't do this */
	pthread_mutex_t lock;
	
} mx_t;

/* the TS merger state */
mx_t merger;

#include "viewer.h"
#include "input_socket.h"
#include "input_buffer.h"
#include "input_feed.h"
#include "output_socket.h"
#include "output_feed.h"
#include "output_log.h"

void *merger_mx(void*);

extern void mx_init(mx_t *s, uint16_t pcr_pid);
extern void mx_feed(mx_t *s, int64_t timestamp, uint8_t *data);
extern int mx_update(mx_t *s, int64_t timestamp);
extern mx_packet_t *mx_next(mx_t *s, int last_station, uint32_t last_counter);

#endif

