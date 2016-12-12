#ifndef _SIM_H
#define _SIM_H

#define __STDC_FORMAT_MACROS

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

typedef struct {
    /* Station Callsign */
    char callsign[10];
    
    /* Output UDP Socket */
    int latency_offset_ms;
    /* + 0-latency_variance_ms random offset */
    int latency_variance_ms;
    
    /* interval in packets to drop out */
    int dropout_interval;
    /* length of dropping out in packets */
    int dropout_length;
    
    /* Station's own packet counter */
    int counter;
    
    /* tspush struct */
    _push_t tspush;
    
} _sim_station_t;
#endif
