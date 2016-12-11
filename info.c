#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "ts.h"
#include "info.h"

static void _reset_info(ts_info_t *ts_info)
{
    ts_info->first_pcr_base = 0;
	ts_info->first_pcr_extension = 0;
	
	ts_info->last_pcr_base = 0;
	ts_info->last_pcr_extension = 0;
	
	ts_info->pcr_extension_used = 0;
	
	ts_info->packet_count = 0;
	ts_info->padding_count = 0;
	ts_info->invalid_count = 0;
	ts_info->pcr_count = 0;
}

int main(int argc, char *argv[])
{
	int c;
	FILE *f = stdin;
	uint8_t data[TS_PACKET_SIZE];
	ts_header_t ts;
	ts_info_t ts_info;
	
	_reset_info(&ts_info);
	
	/* Stream the data */
	while(fread(data, 1, TS_PACKET_SIZE, f) == TS_PACKET_SIZE)
	{
		if(data[0] != TS_HEADER_SYNC)
		{
			/* Re-align input to the TS sync byte */
			uint8_t *p = memchr(data, TS_HEADER_SYNC, TS_PACKET_SIZE);
			if(p == NULL) continue;
			
			c = p - data;
			memmove(data, p, TS_PACKET_SIZE - c);
			
			if(fread(&data[TS_PACKET_SIZE - c], 1, c, f) != c)
			{
				break;
			}
		}
		
	    ts_info.packet_count++;
		
		/* Invalid Header Packets */
		if(ts_parse_header(&ts, data) != TS_OK)
		{
			ts_info.invalid_count++;
			continue;
		}
		
		/* NULL/padding packets */
		if(ts.pid == TS_NULL_PID)
		{
		    ts_info.padding_count++;
		    continue;
		}
		
		if(ts.pcr_flag)
		{
		    ts_info.pcr_count++;
		    
		    if(ts_info.first_pcr_base == 0 && ts_info.first_pcr_extension == 0)
		    {
		        ts_info.first_pcr_base = ts.pcr_base;
		        ts_info.first_pcr_extension = ts.pcr_extension;
		    }
		    
		    ts_info.last_pcr_base = ts.pcr_base;
		    ts_info.last_pcr_extension = ts.pcr_extension;
		    
		    if(ts.pcr_extension > 0)
		    {
		        ts_info.pcr_extension_used = 1;
		    }
		}
	}
	
	if(f != stdin)
	{
		fclose(f);
	}
	
	/* Print report */
	
	printf("TS Duration: %ldms\n", (ts_info.last_pcr_base - ts_info.first_pcr_base) / 90);
	
	if(ts_info.pcr_extension_used)
	{
	    printf("TS Start PCR: %ld:%d\n", ts_info.first_pcr_base, ts_info.first_pcr_extension);
	    printf("TS End PCR  : %ld:%d\n", ts_info.last_pcr_base, ts_info.last_pcr_extension);
	}
	else
	{
	    printf("TS Start PCR: %ld\n", ts_info.first_pcr_base);
	    printf("TS End PCR  : %ld\n", ts_info.last_pcr_base);
	    printf(" (PCR Extension Value is not used in this sample)\n");
	}
	printf("Total Packets: %d\n", ts_info.packet_count);
	printf("Invalid Packets: %d\n", ts_info.invalid_count);
	printf("Padding/Null Packets: %d (%.2f%%)\n",
	        ts_info.padding_count, (double)(100*ts_info.padding_count)/ts_info.packet_count);
	printf("PCR Packets: %d (every ~%ldms)\n", ts_info.pcr_count, 
	        (ts_info.last_pcr_base - ts_info.first_pcr_base) / (90 * ts_info.pcr_count));
	
	return(0);
}
