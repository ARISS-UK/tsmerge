
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include "../merge.h" /* TODO: Rework this to not require revese dependency */
#include "merger.h"

#define MERGER_STATS_UDP_PORT   5680

extern rxBuffer_t rxBuffer;
extern mx_thread_t mx_threads[];

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
static char *url_encode(char *str) {
  char *pstr = str, *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

static void udpstat(char* fmt, ...)
{
    char statmsg[4096];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(statmsg,4096,fmt, arg);
    va_end(arg);

    int sock, n;
    socklen_t length;
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
    {
        return;
    }
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(MERGER_STATS_UDP_PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    length = sizeof(struct sockaddr_in);
    n = sendto(sock, statmsg, strlen(statmsg), 0, (const struct sockaddr *)&server, length);
    if(n < 0)
    {
        return;
    }
    close(sock);
}

static void stats_rxCircularBuffer(void)
{
  int head, tail, queue, loss;
  /* rxBuffer functions are internally locked */

  head = rxBufferHead(&rxBuffer);
  tail = rxBufferTail(&rxBuffer);
  loss = rxBufferLoss(&rxBuffer);

  queue = head - tail;
  if(queue < 0)
  {
    queue = queue + 2048;
  }
  
  udpstat("{\"type\":\"udprxbuffer\",\"queue\":%d,\"loss\":%d}",
    queue,
    loss
  );
}

static void stats_resetLogTimestamp(void)
{
  time_t now;
  char stime[20];

  if(file_viewer.tsenabled || file_viewer.csvenabled)
  {
    /* Generate timestamp string */
    time(&now);
    strftime(stime, sizeof stime, "%F_%TZ", gmtime(&now));
  }
  if(file_viewer.tsenabled)
  {
    snprintf(file_viewer.tsfilename,63,"logs/merged-%s.ts",stime);
  }
  if(file_viewer.csvenabled)
  {
    snprintf(file_viewer.csvfilename,63,"logs/merged-%s.csv",stime);
  }
}

typedef struct {
  int station;
  uint32_t counter;
} stats_merger_t;

stats_merger_t previous_merger_state; 

static void stats_merger(void)
{
  int i, j;
  char tmpString[4096];
 
  uint8_t merger_running;
 
  pthread_mutex_lock(&merger.lock);
  
  merger_running = 0;
  if(merger.next_station >= 0)
  {
    if(merger.next_station != previous_merger_state.station || merger.next_counter != previous_merger_state.counter)
    {
      merger_running = 1;
      previous_merger_state.station = merger.next_station;
      previous_merger_state.counter = merger.next_counter;
    }
  }

  if(merger_running == 0)
  {
    stats_resetLogTimestamp();
  }
  
  tmpString[0] = '\0';
  sprintf(tmpString,"{\"type\":\"merger\",\"version\":\""BUILD_VERSION"\",\"built\":\""BUILD_DATE"\",\"live\":%d,\"selected\":%d,\"stations\":[",
    merger_running,
    merger.next_station
  );

  /* Sum selected count for merger contribution count */
  uint32_t selected_allstations = 0;
  char *encoded_sid;
  for(i = 0; i < _STATIONS; i++)
  {
    selected_allstations += merger.station[i].selected;
  }
  
  for(i = j = 0; i < _STATIONS; i++)
  {
    if(j!=0)
    {
      /* Comma seperation */
      sprintf(tmpString,"%s,",tmpString);
    }
    j++;

    encoded_sid = url_encode(merger.station[i].sid);
    
    sprintf(tmpString,"%s{\"id\":%d,\"callsign\":\"%s\",\"last_updated\":%ld,\"received\":%d,\"received_sum\":%d,\"selected\":%d,\"selected_percent\":%d,\"selected_sum\":%d,\"lost_sum\":%d}",
      tmpString,
      i,
      encoded_sid,
      merger.station[i].timestamp,
      merger.station[i].received,
      merger.station[i].received_sum,
      merger.station[i].selected,
      (int)(100 * ((double)merger.station[i].selected / (double)selected_allstations)),
      merger.station[i].selected_sum,
      merger.station[i].latest - (merger.station[i].counter_initial + merger.station[i].received_sum)
    );
    free(encoded_sid);

    merger.station[i].received = 0;
    merger.station[i].selected = 0;
  }
  pthread_mutex_unlock(&merger.lock);
  
  udpstat("%s]}",tmpString);
}

static void stats_threads(void)
{
  int i;
  int64_t timestamp_tmp, cpu_time_tmp;
  char tmpString[1024];
  
  tmpString[0] = '\0';
  sprintf(tmpString,"{\"type\":\"threads\",\"threads\":[");
  
  /* Print thread CPU usage time since last sampled */
  for(i=0; i<MX_THREAD_NUMBER; i++)
  {
    timestamp_tmp = timestamp_ms();
    cpu_time_tmp = thread_timestamp(mx_threads[i].thread);
    
    if((timestamp_tmp - mx_threads[i].last_cpu_ts) > 0)
    {
        if(i!=0)
        {
          /* Comma seperation */
          sprintf(tmpString,"%s,",tmpString);
        }
        
        sprintf(tmpString,"%s{\"id\":%d,\"name\":\"%s\",\"cpu_percent\":%.2f}",
          tmpString,
          i,
          mx_threads[i].name,
          100*(double)(cpu_time_tmp - mx_threads[i].last_cpu)
                / (timestamp_tmp - mx_threads[i].last_cpu_ts)
        );
    }
    
    mx_threads[i].last_cpu_ts = timestamp_tmp;
    mx_threads[i].last_cpu = cpu_time_tmp;
  }
  
  udpstat("%s]}",tmpString);
}

/* This function is run on a thread, started from main()
 *
 * This function collects stats and pushes them out as JSON over a local UDP socket 
 */
void *merger_stats(void* arg)
{
  (void) arg;
  int i = 0;

  /* Enable file logging */
  file_viewer.tsenabled = 1;
  file_viewer.csvenabled = 1;
  stats_resetLogTimestamp();
  
  while(1)
  {
    /** ~5Hz */
 
    /* Merger internal Stats */
    stats_merger();
    
    if(i % 2 == 0)
    {
      /** ~2.5Hz **/
      
      /* Server threads stats */
      stats_threads();
    
      /* UDP Rx Circular Buffer */
      stats_rxCircularBuffer();
    }
    
    i++;
    sleep_ms(200);
  }
}
