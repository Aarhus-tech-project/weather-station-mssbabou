#include "weatherdata.h"
#include "jsmn.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

static int jsoneq(const char *json, const jsmntok_t *tok, const char *s) {
    int len = tok->end - tok->start;
    return tok->type == JSMN_STRING &&
           (int)strlen(s) == len &&
           strncmp(json + tok->start, s, len) == 0 ? 0 : -1;
}

static int tok_to_str(const char *json, const jsmntok_t *tok,
                      char *out, size_t outsz) {
    int len = tok->end - tok->start;
    if (len <= 0 || (size_t)len >= outsz) return -1;
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
    return 0;
}

static int tok_to_float(const char *json, const jsmntok_t *tok, float *out) {
    char buf[32];
    int len = tok->end - tok->start;
    if (len <= 0 || len >= (int)sizeof(buf)) return -1;
    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';
    char *end = NULL;
    *out = strtof(buf, &end);
    return (end && *end == '\0') ? 0 : -1;
}

void wd_fill_timestamp(char ts[WD_TIMESTAMP_LEN], bool utc) {
    time_t now = time(NULL);
    struct tm tmv;
    if (utc) {
        gmtime_r(&now, &tmv);
        strftime(ts, WD_TIMESTAMP_LEN, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    } else {
        localtime_r(&now, &tmv);
        strftime(ts, WD_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S%z", &tmv);
    }
}

int wd_try_parse_weather_data(WeatherData *out, const char *json) {
    if (!out || !json) return -1;

    jsmn_parser p;
    jsmntok_t tok[64];
    jsmn_init(&p);
    int n = jsmn_parse(&p, json, strlen(json), tok, 64);
    if (n < 1 || tok[0].type != JSMN_OBJECT) return -2;

    // defaults
    memset(out, 0, sizeof(*out));

    for (int i = 1; i < n; i++) {
        if (jsoneq(json, &tok[i], "device_id") == 0) {
            if (tok_to_str(json, &tok[i+1], out->device_id, WD_DEVICE_ID_LEN)) return -3; i++;
        } else if (jsoneq(json, &tok[i], "temp") == 0) {
            if (tok_to_float(json, &tok[i+1], &out->temperature)) return -3; i++;
        } else if (jsoneq(json, &tok[i], "hum") == 0) {
            if (tok_to_float(json, &tok[i+1], &out->humidity)) return -3; i++;
        } else if (jsoneq(json, &tok[i], "pres") == 0) {
            if (tok_to_float(json, &tok[i+1], &out->pressure)) return -3; i++;
        } else if (jsoneq(json, &tok[i], "eco2") == 0) {
            float v; if (tok_to_float(json, &tok[i+1], &v)) return -3; out->eco2 = (int)v; i++;
        } else if (jsoneq(json, &tok[i], "tvoc") == 0) {
            float v; if (tok_to_float(json, &tok[i+1], &v)) return -3; out->tvoc = (int)v; i++;
        }
    }

    wd_fill_timestamp(out->timestamp, false);
    return 0;
}