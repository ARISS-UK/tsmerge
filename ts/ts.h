/* ts.c/h - Simple DVB TS header parser                                  */
/*=======================================================================*/
/* Copyright (C)2016 Philip Heron <phil@sanslogic.co.uk>                 */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */

#ifndef _TS_H
#define _TS_H

#include <stdint.h>

#define TS_PACKET_SIZE 188
#define TS_HEADER_SYNC 0x47

#define TS_NULL_PID 0x1FFF

typedef struct {
	
	/* Standard 4-byte TS header fields (required) */
	uint8_t sync_byte;
	uint8_t transport_error_indicator;
	uint8_t payload_unit_start_indicator;
	uint8_t transport_priority;
	uint16_t pid;
	uint8_t scrambling_control;
	uint8_t adaptation_field_flag;
	uint8_t payload_flag;
	uint8_t continuity_counter;
	
	/* Adaptation field (optional, if adaptation_field_flag == 1) */
	uint8_t adaptation_field_length;
	uint8_t discontinuity_indicator;
	uint8_t random_access_indicator;
	uint8_t elementary_stream_priority_indicator;
	uint8_t pcr_flag;
	uint8_t opcr_flag;
	uint8_t splicing_point_flag;
	uint8_t transport_private_data_flag;
	uint8_t adaptation_field_extension_flag;
	
	/* Program Clock Reference (optional, if pcr_flag == 1) */
	uint64_t pcr_base;
	uint16_t pcr_extension;
	
	/* Original Program Clock Reference (optional, if opcr_flag == 1) */
	uint64_t opcr_base;
	uint16_t opcr_extension;
	
	/* Splicing point flag (optional, if splicing_point_flag == 1) */
	int8_t splicing_point;
	
	/* Transport private data (optional, if transport_private_data_flag == 1) */
	uint8_t transport_private_data_length;
	uint8_t transport_private_data_offset;
	
	/* Adaptation Extension Field (optional, if adaptation_extension_field_flag == 1) */
	uint8_t adaptation_extension_length;
	uint8_t legal_time_window_flag;
	uint8_t piecewise_rate_flag;
	uint8_t seamless_splice_flag;
	uint8_t aef_reserved;
	
	/* Legal time window (optional, if legal_time_window_flag == 1) */
	uint8_t legal_time_window_valid_flag;
	uint16_t legal_time_window_offset;
	
	/* Piecewise flag set */
	uint8_t piecewise_reserved;
	uint32_t piecewise_rate;
	
	/* Seamless splice flag set */
	uint8_t splice_type;
	/* uint64_t dts_next_access_unit -- not supported */
	
	/* Offset to payload content, in bytes */
	uint8_t payload_offset;
	
} ts_header_t;

#define TS_OK            0
#define TS_INVALID       1
#define TS_EOF           2
#define TS_OUT_OF_MEMORY 3

extern int ts_parse_header(ts_header_t *ts, uint8_t * const data);
extern void ts_dump_header(ts_header_t *ts);

#endif

