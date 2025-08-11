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
} weather_data_t;

int wd_parse_weather_array_strict(weather_data_t *out, size_t max_items, const char *json, size_t *needed_out);

void wd_fill_timestamp(char ts[WD_TIMESTAMP_LEN], bool utc);