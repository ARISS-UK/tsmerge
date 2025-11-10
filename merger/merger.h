
#include <stdint.h>
#include <stdbool.h>
#include "../ts/ts.h"

#ifndef _MERGER_H
#define _MERGER_H

#define MIN(x,y)	((x) > (y) ? (y) : (x))
#define MAX(x,y)	((x) < (y) ? (y) : (x))

#define BRANCH_LIKELY(x)    __builtin_expect (!!(x), 1)
#define BRANCH_UNLIKELY(x)  __builtin_expect (!!(x), 0)

#include <pthread.h>
#include "../ts/ts.h"
#include "../timing/timing.h"

#define MERGER_PCR_PID          256

#define MERGER_UDP_RX_BUFSIZE       8192
#define MERGER_UDP_RX_SYSBUFSIZE    212992

#define DEFAULT_MERGER_UDP_RX_PORT      5678
#define DEFAULT_MERGER_TCP_TX_PORT      5679
#define DEFAULT_MERGER_STATS_UDP_PORT   5680

#define DEFAULT_STATIONS_JSON_FILE 		"stations.json"

/* Maximum number of stations and packets */
#define _STATIONS 16
#define _PACKETS  (UINT16_MAX+1)

/* Station timeout in milliseconds */
#define _TIMEOUT_MS 10000

/* Guard period in milliseconds */
#define _GUARD_MS 1000

/* Maximum PCR range for a segment */
#define _SEGMENT_PCR_LIMIT (0.5 * 90000) /* 500ms (90kHz clock) */

/*** MX Packet - TS Upload ***/

#define MX_MAGICBYTES_VALUE     (0x55A2)
#define MX_MAGIC0               (0xA2)
#define MX_MAGIC1               (0x55)

typedef struct {
    uint16_t magic_bytes;

    uint32_t counter;

    uint8_t snr;

    char callsign[10];
    char key[10];

} __attribute__((packed)) mx_header_t;

/* Length of MX packet */
#define MX_HEADER_LEN (sizeof(mx_header_t))
#define MX_PACKET_LEN (MX_HEADER_LEN + TS_PACKET_SIZE)

/*** MX Heartbeat - Auth and Access check ***/

#define MXHB_MAGIC0 (0xA3)
#define MXHB_MAGIC1 (0x55)

#define MX_HB_LEN (2 + 10 + 10)
// 2 - Magic Bytes
// 10 - Station ID
// 10 - Access Key

/*** MX Heartbeat Response ***/

typedef struct {
	uint8_t magic0;
	uint8_t magic1;
	uint8_t authresp;
	uint32_t ts_total;
	uint32_t ts_loss;
} mx_hbresp_t;

#define MXHBRESP_MAGIC0 (0xA4)
#define MXHBRESP_MAGIC1 (0x55)

#define MX_HBRESP_LEN (2 + 1 + 4 + 4)
// 2 - Magic Bytes
// 1 - Auth response
// 4 - Total TS received
// 4 - TS Lost

#define MXHB_MAGICBYTES_VALUE   (0x55A3)
typedef struct {
    uint16_t magic_bytes;

    char callsign[10];
    char key[10];

    int16_t packet_length;

} __attribute__((packed)) mxhb_header_t;

#define MXHBRESP_MAGICBYTES_VALUE   (0x55A4)
typedef struct {
    uint16_t magic_bytes;

    uint8_t auth_response;

    int16_t original_packet_length;

    uint32_t ts_total;
    uint32_t ts_loss;

} __attribute__((packed)) mxhbresp_header_t;

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
	/* Enabled */
	bool enabled;
	
	/* The station ID */
	char sid[10];
	
	/* The station PSK */
	char psk[10];

	/* The station location */
	float latitude;
	float longitude;
	char location[64];

	/* Calculated ISS Az/El from the station lat/lon */
	float iss_az_deg;
	float iss_el_deg;
	int64_t iss_updated;

	/* Current S/N reported by the station (0-25.5dB) */
	uint8_t sn_deci;
	int64_t sn_timestamp;
	
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
extern mx_t merger;

typedef struct {
	/* Input MX UDP port */
	uint16_t input_mx_port;

	/* Output TS TCP port */
	uint16_t output_ts_port;

	/* Output Stats UDP port (JSON) */
	uint16_t stats_udp_port;

	/* Stations config filepath */
	char *stations_filepath;
} mx_config_t;

/* the TS merger config */
extern mx_config_t mx_config;


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

int ext_heartbeat_station(char sid[10], char psk[10], uint32_t *received_ptr, uint32_t *lost_ptr);

#endif

