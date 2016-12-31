#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <time.h>
#include "ts.h"
#include "info.h"

void _print_version(void)
{
	printf(
		"tsinfo (tsmerge) "BUILD_VERSION" (built "BUILD_DATE")\n"
	);
}

void _print_usage(void)
{
	printf(
		"\n"
		"Usage: cat <ts file> | tsinfo [options]\n"
		"\n"
		"  -V, --version          Print version string and exit.\n"
		"  -d, --dump             Dump CSV of TS Packet PCR & Continuity Counter Data.\n"
		"\n"
	);
}

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
	
	int opt;
	
	FILE *fp;
        int csvEnabled = 0;
	char csvText[128];
        char csvFilename[64];
	uint8_t csvTextSize;
        time_t now;
        char stime[20];

	static const struct option long_options[] = {
		{ "version",     no_argument, 0, 'V' },
		{ "dump",        no_argument, 0, 'd' },
		{ 0,             0,           0,  0  }
	};

	while((c = getopt_long(argc, argv, "Vd", long_options, &opt)) != -1)
	{
		switch(c)
		{
		case 'V': /* --version */
                        _print_version();
			exit(0);
			break;
		
		case 'd': /* --dump */
			csvEnabled = 1;
			break;
		
		case '?':
			_print_usage();
			return(0);
		}
	}

	_reset_info(&ts_info);

	if(csvEnabled == 1)
	{
		time(&now);
		strftime(stime, sizeof stime, "%F_%TZ", gmtime(&now));
		snprintf(csvFilename,63,"tsinfo-%s.csv",stime);

		printf("Dumping TS description CSV to: %s\n", csvFilename);

		fp = fopen(csvFilename, "a+");
	}

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

		if(csvEnabled == 1)
		{
			/* Create text [packet counter, packet pcr base, packet pcr extension, packet pid, packet continuity counter] */
			csvTextSize = snprintf(csvText, 127, "%"PRIu32",%"PRIu64",%"PRIu16",%"PRIu16",%"PRIu8"\n",
				ts_info.packet_count,
				ts.pcr_base,
				ts.pcr_extension,
                	        ts.pid,
				ts.continuity_counter
			);
			(void)fwrite(csvText, csvTextSize, 1, fp);
		}
	}
	
	if(csvEnabled == 1)
	{
		fclose(fp);
	}

	if(f != stdin)
	{
		fclose(f);
	}
	
	/* Print report */
	
	printf("TS Duration: %ldms\n", (ts_info.last_pcr_base - ts_info.first_pcr_base) / 90);
	printf("TS Bitrate: ~%.03fMbps\n", (double)(ts_info.packet_count*204*8)/((ts_info.last_pcr_base - ts_info.first_pcr_base) / 90000)/(1000*1000));
	
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
