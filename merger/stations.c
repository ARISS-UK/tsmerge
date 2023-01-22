#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "merger.h"
#include "stations.h"
#include <json-c/json.h>

#define STATIONS_JSON_FILE "stations.json"

static volatile bool reload_pending = false;

static void _sighup_handler (int sig)
{
    (void) sig;

    reload_pending = true;
}

void *merger_stations(void* arg)
{
    (void) arg;

    // Set up signal handler to catch reload
    struct sigaction new_action;
    
    new_action.sa_handler = &_sighup_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction(SIGHUP, &new_action, NULL);

    while(1)
    {
        if(reload_pending)
        {
            reload_pending = false;
            _reload_stations(&merger);
        }

        sleep_ms(100);
    }
}

static int load_stations_file(char **result) 
{ 
	int size = 0;
	FILE *f = fopen(STATIONS_JSON_FILE, "rb");
	if (f == NULL) 
	{ 
		*result = NULL;
		return -1; // -1 means file opening fail 
	} 
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f)) 
	{ 
		free(*result);
		return -2; // -2 means file reading fail 
	} 
	fclose(f);
	(*result)[size] = 0;
	return size;
}

void _reload_stations(mx_t *s)
{
    uint32_t i;
    char *stations_json_string;
    json_object *stations_json_object;
    json_object *station_json_object;
    struct json_object *field_object;

    if(load_stations_file(&stations_json_string) <= 0)
    {
        // Error loading file
        return;
    }
    stations_json_object = json_tokener_parse(stations_json_string);
    free(stations_json_string);
    if(json_type_array != json_object_get_type(stations_json_object))
    {
        // Error loading json array
        return;
    }

    pthread_mutex_lock(&merger.lock);

    for(i = 0; i < _STATIONS; i++)
    {
        memset(&s->station[i], 0, sizeof(mx_station_t));
    }

    for(i = 0; i < json_object_array_length(stations_json_object); i++)
    {
        station_json_object = json_object_array_get_idx(stations_json_object, i);
	json_object_object_get_ex(station_json_object, "enabled", &field_object);

        if(json_object_get_type(field_object) == json_type_boolean
            && json_object_get_boolean(field_object))
        {
            json_object_object_get_ex(station_json_object, "sid", &field_object);
            if(json_object_get_type(field_object) != json_type_string)
                continue;
            strncpy(s->station[i].sid, (char *)json_object_get_string(field_object), 10);

            json_object_object_get_ex(station_json_object, "psk", &field_object);
            if(json_object_get_type(field_object) != json_type_string)
                strncpy(s->station[i].psk, s->station[i].sid, 10);
            else
                strncpy(s->station[i].psk, (char *)json_object_get_string(field_object), 10);

            json_object_object_get_ex(station_json_object, "latitude", &field_object);
            if(json_object_get_type(field_object) != json_type_double)
                continue;
            s->station[i].latitude = json_object_get_double(field_object);

            json_object_object_get_ex(station_json_object, "longitude", &field_object);
            if(json_object_get_type(field_object) != json_type_double)
                continue;
            s->station[i].longitude = json_object_get_double(field_object);

            json_object_object_get_ex(station_json_object, "location", &field_object);
            if(json_object_get_type(field_object) != json_type_string)
                continue;
            strncpy(s->station[i].location, (char *)json_object_get_string(field_object), 64);
            
            s->station[i].enabled = true;
        }
        json_object_put(station_json_object);
    }

    pthread_mutex_unlock(&merger.lock);

    json_object_put(stations_json_object);
}
