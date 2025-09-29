
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

#include <json-c/json.h>

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
  if(buf == NULL)
  {
    return NULL;
  }
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

static void udpstat_string(const char* statmsg)
{
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
    server.sin_port = htons(mx_config.stats_udp_port);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    length = sizeof(struct sockaddr_in);
    n = sendto(sock, statmsg, strlen(statmsg), 0, (const struct sockaddr *)&server, length);
    if(n < 0)
    {
        return;
    }
    close(sock);
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
    server.sin_port = htons(mx_config.stats_udp_port);
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
  uint32_t head, tail, loss;
  int32_t queue;
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

#define STATS_JSONSTRING_BUFLEN 4096
static void stats_merger(void)
{
  int i;
  uint8_t merger_running;

  struct json_object *stats_json_obj;
  struct json_object *stats_json_stations_array;
  struct json_object *stats_json_station_obj;

  int64_t current_timestamp_ms;

  stats_json_obj = json_object_new_object();
  json_object_object_add(stats_json_obj, "type", json_object_new_string("merger"));
  json_object_object_add(stats_json_obj, "version", json_object_new_string(BUILD_VERSION));
  json_object_object_add(stats_json_obj, "built", json_object_new_string(BUILD_DATE));
  stats_json_stations_array = json_object_new_array();
 
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

  json_object_object_add(stats_json_obj, "live", json_object_new_int(merger_running));
  json_object_object_add(stats_json_obj, "selected", json_object_new_int(merger.next_station));

  /* Sum selected count for merger contribution count */
  uint32_t selected_allstations = 0;
  char *encoded_sid, *encoded_location;
  for(i = 0; i < _STATIONS; i++)
  {
    if(merger.station[i].enabled == 0)
    {
      continue;
    }
    selected_allstations += merger.station[i].selected;
  }

  current_timestamp_ms = timestamp_ms();
  
  for(i = 0; i < _STATIONS; i++)
  {
    if(merger.station[i].enabled == 0)
    {
      continue;
    }

    encoded_sid = url_encode(merger.station[i].sid);
    encoded_location = url_encode(merger.station[i].location);

    stats_json_station_obj = json_object_new_object();

    json_object_object_add(stats_json_station_obj, "id", json_object_new_int(i));
    json_object_object_add(stats_json_station_obj, "enabled", json_object_new_int(merger.station[i].enabled));
    json_object_object_add(stats_json_station_obj, "callsign", json_object_new_string(encoded_sid));
    json_object_object_add(stats_json_station_obj, "latitude", json_object_new_double(merger.station[i].latitude));
    json_object_object_add(stats_json_station_obj, "longitude", json_object_new_double(merger.station[i].longitude));
    json_object_object_add(stats_json_station_obj, "location", json_object_new_string(encoded_location));
    json_object_object_add(stats_json_station_obj, "last_updated", json_object_new_int64(merger.station[i].timestamp));
    json_object_object_add(stats_json_station_obj, "received", json_object_new_int(merger.station[i].received));
    json_object_object_add(stats_json_station_obj, "received_sum", json_object_new_int(merger.station[i].received_sum));
    json_object_object_add(stats_json_station_obj, "selected", json_object_new_int(merger.station[i].selected));
    json_object_object_add(stats_json_station_obj, "selected_percent", json_object_new_int((int)(100 * ((double)merger.station[i].selected / (double)selected_allstations))));
    json_object_object_add(stats_json_station_obj, "selected_sum", json_object_new_int(merger.station[i].selected_sum));
    json_object_object_add(stats_json_station_obj, "lost_sum", json_object_new_int(merger.station[i].latest - (merger.station[i].counter_initial + merger.station[i].received_sum)));
    
    if(merger.station[i].sn_timestamp > (current_timestamp_ms - 2000))
    {
      json_object_object_add(stats_json_station_obj, "s/n", json_object_new_double(((double)merger.station[i].sn_deci)/10.0));
    }
    else
    {
      json_object_object_add(stats_json_station_obj, "s/n", json_object_new_double(0.0));
    }

    json_object_array_add(stats_json_stations_array, stats_json_station_obj);

    free(encoded_sid);
    free(encoded_location);

    merger.station[i].received = 0;
    merger.station[i].selected = 0;
  }
  pthread_mutex_unlock(&merger.lock);

  json_object_object_add(stats_json_obj, "stations", stats_json_stations_array);

  udpstat_string(json_object_to_json_string(stats_json_obj));

  /* Free JSON object tree */
  json_object_put(stats_json_obj);
}

static void stats_threads(void)
{
  int i;
  int64_t timestamp_tmp, cpu_time_tmp;

  struct json_object *stats_json_obj;
  struct json_object *stats_json_threads_array;
  struct json_object *stats_json_thread_obj;

  stats_json_obj = json_object_new_object();
  json_object_object_add(stats_json_obj, "type", json_object_new_string("threads"));
  stats_json_threads_array = json_object_new_array();
  
  /* Print thread CPU usage time since last sampled */
  for(i=0; i<MX_THREAD_NUMBER; i++)
  {
    timestamp_tmp = timestamp_ms();
    cpu_time_tmp = thread_timestamp(mx_threads[i].thread);
    
    if((timestamp_tmp - mx_threads[i].last_cpu_ts) > 0)
    {
      stats_json_thread_obj = json_object_new_object();
      json_object_object_add(stats_json_thread_obj, "id", json_object_new_int(i));
      json_object_object_add(stats_json_thread_obj, "name", json_object_new_string(mx_threads[i].name));
      json_object_object_add(stats_json_thread_obj, "cpu_percent", json_object_new_double(100*(double)(cpu_time_tmp - mx_threads[i].last_cpu) / (timestamp_tmp - mx_threads[i].last_cpu_ts)));
      json_object_array_add(stats_json_threads_array, stats_json_thread_obj);
    }
    
    mx_threads[i].last_cpu_ts = timestamp_tmp;
    mx_threads[i].last_cpu = cpu_time_tmp;
  }
  
  json_object_object_add(stats_json_obj, "threads", stats_json_threads_array);

  udpstat_string(json_object_to_json_string(stats_json_obj));

  /* Free JSON object tree */
  json_object_put(stats_json_obj);
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
    /** ~2Hz */
 
    /* Merger internal Stats */
    stats_merger();
    
    if(i % 2 == 0)
    {
      /** ~1Hz **/
      
      /* Server threads stats */
      stats_threads();
    
      /* UDP Rx Circular Buffer */
      stats_rxCircularBuffer();
    }
    
    i++;
    sleep_ms(500);
  }
}
