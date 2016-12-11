
#ifndef _TSINFO_H
#define _TSINFO_H

typedef struct {

    uint64_t first_pcr_base;
	uint16_t first_pcr_extension;
	
	uint64_t last_pcr_base;
	uint16_t last_pcr_extension;
	
	uint8_t pcr_extension_used;
	
	uint32_t packet_count;
	uint32_t padding_count;
	uint32_t invalid_count;
	uint32_t pcr_count;
	
} ts_info_t;

#endif
