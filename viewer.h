
#ifndef _VIEWER_H
#define _VIEWER_H

/* Timeout for clients (ms) */
#define _VIEWER_TIMEOUT (60 * 1000)

#define _VIEWERS_MAX 10

typedef struct {
	
	/* Socket for this viewer */
	int sock;
	
	/* The last packet sent to viewer */
	int last_station;
	uint32_t last_counter;
	
	/* Timestamp of when the last packet was sent */
	int64_t timestamp;
	
} viewer_t;

/* state for each client / viewer */
viewer_t viewers[_VIEWERS_MAX];

#endif

