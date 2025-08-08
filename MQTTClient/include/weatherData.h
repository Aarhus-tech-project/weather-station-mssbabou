#pragma once

#include <stddef.h>
#include <stdbool.h>

enum { 
    WD_DEVICE_ID_LEN = 256, 
    WD_TIMESTAMP_LEN = 32 
};

typedef struct {
    char device_id[WD_DEVICE_ID_LEN];
    char timestamp[WD_TIMESTAMP_LEN];  // e.g. "2025-08-08 11:37:22+0200"
    float temperature;
    float humidity;
    float pressure;
    int   eco2;
    int   tvoc;
} WeatherData;

int wd_try_parse_weather_data(WeatherData *out, const char *json);

void wd_fill_timestamp(char ts[WD_TIMESTAMP_LEN], bool utc);