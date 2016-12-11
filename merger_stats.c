
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "main.h"
#include "timing.h"
#include "viewer.h"
#include "merger.h"
#include "merger_stats.h"
#include "merger_rx_buffer.h"
#include "merger_rx_socket.h"
#include "merger_tx_feed.h"
#include "merger_tx_socket.h"

#define MERGER_STATS_UDP_PORT   5680

extern rxBuffer_t rxBuffer;
extern mx_thread_t mx_threads[];

static void udpstat(char* fmt, ...)
{
    char statmsg[1024];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(statmsg,1023,fmt, arg);
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
  /* rxBuffer functions are internally locked */
  udpstat("{\"type\":\"udprxbuffer\",\"head\":%d,\"tail\":%d,\"loss\":%d}",
    rxBufferHead(&rxBuffer),
    rxBufferTail(&rxBuffer),
    rxBufferLoss(&rxBuffer)
    );
}

static void stats_stations(void)
{
  int i, j;
  int64_t timestamp;
  char tmpString[1024];
  
  tmpString[0] = '\0';
  sprintf(tmpString,"{\"type\":\"stations\",\"stations\":[");
  timestamp = timestamp_ms();
  
  pthread_mutex_lock(&merger.lock);
  for(i = j = 0; i < _STATIONS; i++)
  {
    /* Only active if last packet within the last 60s */
    if(merger.station[i].timestamp < (timestamp - 60 * 1000))
    {
      continue;
    }
    
    if(j!=0)
    {
      /* Comma seperation */
      sprintf(tmpString,"%s,",tmpString);
    }
    j++;
    
    sprintf(tmpString,"%s{\"id\":%d,\"callsign\":\"%s\",\"connected\":%ld,\"counter_initial\":%d,\"timestamp\":%ld,\"latest\":%d,\"total_received\":%d,\"selected\":%d,\"lost\":%d}",
      tmpString,
      i,
      merger.station[i].sid,
      merger.station[i].connected,
      merger.station[i].counter_initial,
      merger.station[i].timestamp,
      merger.station[i].latest,
      merger.station[i].total_received,
      merger.station[i].selected,
      (merger.station[i].latest + 1 - (merger.station[i].counter_initial + merger.station[i].total_received))
    );
  }
  pthread_mutex_unlock(&merger.lock);
  
  udpstat("%s]}",tmpString);
}

static void stats_selection(void)
{
  int next_station_id;
  char next_station_name[10];
  
  pthread_mutex_lock(&merger.lock);
  if(merger.next_station >= 0)
  {
    next_station_id = merger.next_station;
    strncpy(next_station_name, merger.station[next_station_id].sid, 10);
    
    pthread_mutex_unlock(&merger.lock);
    
    udpstat("{\"type\":\"selection\",\"id\":%d,\"callsign\":\"%s\"}",
      next_station_id,
      next_station_name
      );
  }
  else
  {
    pthread_mutex_unlock(&merger.lock);
  }
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
    
    if(i!=0)
    {
      /* Comma seperation */
      sprintf(tmpString,"%s,",tmpString);
    }
    
    sprintf(tmpString,"%s{\"id\":%d,\"name\":\"%s\",\"cpu_percent\":%f}",
      tmpString,
      i,
      mx_threads[i].name,
      100*(double)(cpu_time_tmp - mx_threads[i].last_cpu)
            / (timestamp_tmp - mx_threads[i].last_cpu_ts)
    );
    
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
  
  while(1)
  {
    /* UDP Rx Circular Buffer */
    stats_rxCircularBuffer();
    
    /* RX per-station Stats */
    stats_stations();
    
    /* Output contribution stats */
    stats_selection();
    
    /* Server threads stats */
    stats_threads();
    
    sleep_ms(100);
  }
}
