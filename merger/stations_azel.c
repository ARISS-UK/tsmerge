#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#include "libpredict/predict.h"

#include "merger.h"

typedef char tle_two_t[2][70];

static bool tle_load(char *tle_filename, char **tle_lines_ptr)
{
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  fp = fopen(tle_filename, "r");
  if (fp == NULL)
  {
    fprintf(stderr, "\nError opening TLE file!");
    return false;
  }

  int n = 0;
  while ((read = getline(&line, &len, fp)) != -1)
  {
    if(read > 50)
    {
      tle_lines_ptr[n] = malloc(read);
      strncpy(tle_lines_ptr[n], line, MIN(read, 150-1));
      n++;
      if(n > 1)
      {
        break;
      }
    }
  }

  fclose(fp);
  if (line)
    free(line);
  
  if(n != 2)
  {
    fprintf(stderr,"\nError reading TLE from file!");
    return false;
  }

  return true;
}

void *merger_azels(void *arg)
{
  (void) arg;

  predict_orbital_elements_t iss_tle;
  struct predict_position iss_orbit;
  predict_observer_t obs;
  struct predict_observation iss_observation;

  // Near-space (period <225 minutes)
  struct predict_sgp4 sgp;
  // Deep-space (period >225 minutes)
  struct predict_sdp4 sdp;

  char tle_lines[2][150];
  char *tle[2];
  
  tle[0] = &tle_lines[0][0];
  tle[1] = &tle_lines[1][0];

  int64_t curr_time_ms = 0;
  predict_julian_date_t curr_time;

  while(1)
  {
    if(!tle_load("/home/_ariss/web-live/iss.txt", tle))
    {
      printf("Error loading TLE!\n");
      sleep_ms(100);
      continue;
    }
    /*printf("TLE:\n  %s  %s",
      tle[0],
      tle[1]
    );*/

    //printf("Parsing ISS TLE..         ");
    if(!predict_parse_tle(&iss_tle, tle[0], tle[1], &sgp, &sdp))
    {
      printf("Error parsing TLE!\n");
      sleep_ms(100);
      continue;
    }
    /*snprintf(
      app_state_ptr->iss_tle_epoch,
      sizeof(app_state_ptr->iss_tle_epoch)-1,
      "20%01d.%2.2f",
      iss_tle.epoch_year,
      iss_tle.epoch_day
    );
    printf("OK (Epoch: %s)\n",
      app_state_ptr->iss_tle_epoch
    );*/

    curr_time_ms = timestamp_ms();
    curr_time = julian_from_timestamp_ms(curr_time_ms);

    for(int i = 0; i < _STATIONS; i++)
    {
      /* Skip inactive stations */
      if(merger.station[i].sid[0] == '\0') continue;

      predict_create_observer(&obs, "Self", PREDICT_DEG2RAD(merger.station[i].latitude), PREDICT_DEG2RAD(merger.station[i].longitude), 10);

      if(predict_orbit(&iss_tle, &iss_orbit, curr_time) >= 0)
      {
        predict_observe_orbit(&obs, &iss_orbit, &iss_observation);

        merger.station[i].iss_az_deg = PREDICT_RAD2DEG(iss_observation.azimuth);
        merger.station[i].iss_el_deg = PREDICT_RAD2DEG(iss_observation.elevation);
        merger.station[i].iss_updated = curr_time_ms;
      }
    }

    sleep_ms(100);
  }

  pthread_exit(NULL);
}
