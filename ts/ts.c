/* ts.c/h - Simple DVB TS header parser                                  */
/*=======================================================================*/
/* Copyright (C)2016 Philip Heron <phil@sanslogic.co.uk>                 */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ts.h"

int ts_parse_header(ts_header_t *ts, uint8_t * const data)
{
	memset(ts, 0, sizeof(ts_header_t));
	
	/* All packets must begin with TS_HEADER_SYNC / 0x47) */
	if(data[0] != TS_HEADER_SYNC)
	{
		/* Not a TS packet */
		return(TS_INVALID);
	}
	
	/* Standard 4-byte TS header fields (required) */
	ts->sync_byte                    = data[0];
	ts->transport_error_indicator    = (data[1] & 0x80) >> 7;
	ts->payload_unit_start_indicator = (data[1] & 0x40) >> 6;
	ts->transport_priority           = (data[1] & 0x20) >> 5;
	ts->pid                          = ((data[1] & 0x1F) << 8) | data[2];
	ts->scrambling_control           = (data[3] & 0xC0) >> 6;
	ts->adaptation_field_flag        = (data[3] & 0x20) >> 5;
	ts->payload_flag                 = (data[3] & 0x10) >> 4;
	ts->continuity_counter           = data[3] & 0x0F;
	ts->payload_offset               = 4;
	
	/* Adaptation field (optional) */
	if(ts->adaptation_field_flag == 1)
	{
		uint8_t *pdata;
		uint8_t length;
		
		/* Setup pointer to adaptation field data */
		pdata = &data[ts->payload_offset];
		
		ts->adaptation_field_length = pdata[0];
		
		if(ts->adaptation_field_length == 0)
		{
			/* An empty adaptation field is valid */
			ts->adaptation_field_flag = 0;
		}
		else if(ts->adaptation_field_length > 183)
		{
			/* Field can't be longer than 183 bytes */
			return(TS_INVALID);
		}
		else
		{
			ts->discontinuity_indicator              = (pdata[1] & 0x80) >> 7;
			ts->random_access_indicator              = (pdata[1] & 0x40) >> 6;
			ts->elementary_stream_priority_indicator = (pdata[1] & 0x20) >> 5;
			ts->pcr_flag                             = (pdata[1] & 0x10) >> 4;
			ts->opcr_flag                            = (pdata[1] & 0x08) >> 3;
			ts->splicing_point_flag                  = (pdata[1] & 0x04) >> 2;
			ts->transport_private_data_flag          = (pdata[1] & 0x02) >> 1;
			ts->adaptation_field_extension_flag      = pdata[1] & 0x01;
			
			pdata += 2;
			length = 1;
			
			if(ts->pcr_flag)
			{
				ts->pcr_base = ((uint64_t) pdata[0] << 25)
				             | ((uint64_t) pdata[1] << 17)
				             | ((uint64_t) pdata[2] << 9)
				             | ((uint64_t) pdata[3] << 1)
				             | ((uint64_t) (pdata[4] & 0x80) >> 7);
				
				ts->pcr_extension = ((pdata[4] & 0x01) << 8) | pdata[5];
				
				if(ts->pcr_extension >= 300)
				{
					/* The 27MHz clock part should never reach 300 */
					return(TS_INVALID);
				}
				
				pdata += 6;
				length += 6;
			}
			
			/* Untested */
			if(ts->opcr_flag)
			{
				ts->opcr_base = ((uint64_t) pdata[0] << 25)
				              | ((uint64_t) pdata[1] << 17)
				              | ((uint64_t) pdata[2] << 9)
				              | ((uint64_t) pdata[3] << 1)
				              | ((uint64_t) (pdata[4] & 0x80) >> 7);
				
				ts->opcr_extension = ((pdata[4] & 0x01) << 8) | pdata[5];
				
				if(ts->pcr_extension >= 300)
				{
					/* The 27MHz clock part should never reach 300 */
					return(TS_INVALID);
				}
				
				pdata += 6;
				length += 6;
			}
			
			/* Untested */
			if(ts->splicing_point_flag)
			{
				ts->splicing_point = (int8_t) *pdata;
				
				pdata++;
				length++;
			}
			
			/* Untested */
			if(ts->transport_private_data_flag)
			{
				ts->transport_private_data_length = *pdata;
				ts->transport_private_data_offset = 4 + length + 1;
				
				pdata += 1 + ts->transport_private_data_length;
				length += 1 + ts->transport_private_data_length;
			}
			
			/* Untested */
			if(ts->adaptation_field_extension_flag == 1)
			{
				ts->adaptation_extension_length = pdata[0];
				ts->legal_time_window_flag      = (pdata[1] & 0x80) >> 7;
				ts->piecewise_rate_flag         = (pdata[1] & 0x40) >> 6;
				ts->seamless_splice_flag        = (pdata[1] & 0x20) >> 5;
				ts->aef_reserved                = pdata[1] & 0x1F;
				
				pdata += 2;
				length += 2;
				
				/* Untested */
				if(ts->legal_time_window_flag == 1)
				{
					ts->legal_time_window_valid_flag = (pdata[0] & 0x80) >> 7;
					ts->legal_time_window_offset = ((pdata[0] & 0x7F) << 8) | pdata[1];
					
					pdata += 2;
					length += 2;
				}
				
				/* Untested */
				if(ts->piecewise_rate_flag == 1)
				{
					ts->piecewise_reserved = (pdata[0] & 0xC0) >> 6;
					ts->piecewise_rate     = ((pdata[0] & 0x3F) << 16)
					                       | (pdata[1] << 8)
					                       | pdata[2];
					
					pdata += 3;
					length += 3;
				}
				
				/* Untested */
				if(ts->seamless_splice_flag == 1)
				{
					ts->splice_type          = (pdata[0] & 0xF0) >> 4;
					/* ts->dts_next_access_unit -- unsupported */
					
					pdata += 5;
					length += 5;
				}
			}
			
			if(length > ts->adaptation_field_length)
			{
				/* Adaptation field is larger than advertised */
				return(TS_INVALID);
			}
		}
		
		ts->payload_offset += 1 + ts->adaptation_field_length;
	}
	
	return(TS_OK);
}

void ts_dump_header(ts_header_t *ts)
{
	printf("TS: Sync 0x%02X TEI %d PUSI %d TP %i PID %4d SC %2d AFF %d PF %d CC %2d\n",
		ts->sync_byte,
		ts->transport_error_indicator,
		ts->payload_unit_start_indicator,
		ts->transport_priority,
		ts->pid,
		ts->scrambling_control,
		ts->adaptation_field_flag,
		ts->payload_flag,
		ts->continuity_counter
	);
	
	if(ts->adaptation_field_flag == 1)
	{
		printf("    AFL %3d DI %d RAI %d ESPI %d PCR %d OPCR %d SPF %d TPDF %d AFEF %d\n",
			ts->adaptation_field_length,
			ts->discontinuity_indicator,
			ts->random_access_indicator,
			ts->elementary_stream_priority_indicator,
			ts->pcr_flag,
			ts->opcr_flag,
			ts->splicing_point_flag,
			ts->transport_private_data_flag,
			ts->adaptation_field_extension_flag
		);
		
		if(ts->pcr_flag)
		{
			printf("    PCR %ld:%d\n", ts->pcr_base, ts->pcr_extension);
		}
		
		if(ts->opcr_flag)
		{
			printf("    OPCR %ld:%d\n", ts->opcr_base, ts->opcr_extension);
		}
		
		if(ts->splicing_point_flag)
		{
			printf("    SP %d\n", ts->splicing_point);
		}
		
		if(ts->transport_private_data_flag)
		{
			printf("    TPDL %d\n", ts->transport_private_data_length);
		}
		
		if(ts->adaptation_field_extension_flag == 1)
		{
			printf("    AFEL %3d LTWF %d PRF %d SPF %d Reserved %d\n",
				ts->adaptation_extension_length,
				ts->legal_time_window_flag,
				ts->piecewise_rate_flag,
				ts->seamless_splice_flag,
				ts->aef_reserved
			);
			
			if(ts->legal_time_window_flag == 1)
			{
				printf("    LTWVF %d LTWO %d\n",
					ts->legal_time_window_valid_flag,
					ts->legal_time_window_offset
				);
			}
			
			if(ts->piecewise_rate_flag == 1)
			{
				printf("    P Reserved %d PR %d\n",
					ts->piecewise_reserved,
					ts->piecewise_rate
				);
			}
			
			if(ts->seamless_splice_flag == 1)
			{
				printf("    ST %d DTSNAU unsupported\n", ts->splice_type);
			}
		}
	}
}

