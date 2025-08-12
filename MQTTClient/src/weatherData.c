#include "weatherdata.h"
#include "json.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

void wd_fill_timestamp(char ts[WD_TIMESTAMP_LEN], bool utc) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tmv;
    if (utc) {
        gmtime_r(&tv.tv_sec, &tmv);
        strftime(ts, WD_TIMESTAMP_LEN, "%Y-%m-%dT%H:%M:%S", &tmv);
        snprintf(ts + strlen(ts), WD_TIMESTAMP_LEN - strlen(ts),
                 ".%03ldZ", tv.tv_usec / 1000L);
    } else {
        localtime_r(&tv.tv_sec, &tmv);
        strftime(ts, WD_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", &tmv);
        snprintf(ts + strlen(ts), WD_TIMESTAMP_LEN - strlen(ts),
                 ".%03ld%z", tv.tv_usec / 1000L);
    }
}

int parse_one_weather(weather_data_t *o, const char *js,
                             const jsmntok_t *t, int idx, int n) {
    if (idx < 0 || idx >= n || t[idx].type != JSMN_OBJECT) return -10;

    memset(o, 0, sizeof(*o));

    int j = idx + 1;
    for (int pair = 0; pair < t[idx].size; pair++) {
        if (j >= n || t[j].type != JSMN_STRING) return -11;
        const jsmntok_t *key = &t[j++];
        if (j >= n) return -12;
        const jsmntok_t *val = &t[j];

        if (jsoneq(js, key, "device_id") == 0) {
            if (tok_to_str(js, val, o->device_id, WD_DEVICE_ID_LEN)) return -13;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "temp") == 0) {
            if (tok_to_float(js, val, &o->temperature)) return -13;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "hum") == 0) {
            if (tok_to_float(js, val, &o->humidity)) return -13;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "pres") == 0) {
            if (tok_to_float(js, val, &o->pressure)) return -13;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "eco2") == 0) {
            float v; if (tok_to_float(js, val, &v)) return -13; o->eco2 = (int)v;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "tvoc") == 0) {
            float v; if (tok_to_float(js, val, &v)) return -13; o->tvoc = (int)v;
            j = tok_skip(t, j, n);
        } else if (jsoneq(js, key, "timestamp") == 0) {
            // Try to copy producer timestamp
            if (tok_to_str(js, val, o->timestamp, WD_TIMESTAMP_LEN) != 0 ||
                o->timestamp[0] == '\0') {
                // If copy failed or was empty string, fill our own
                wd_fill_timestamp(o->timestamp, false);
            }
            j = tok_skip(t, j, n);
        } else {
            // unknown field -> skip its subtree safely
            j = tok_skip(t, j, n);
        }
    }
    return j;
}

int wd_parse_weather_array_strict(weather_data_t *out, size_t max_items, const char *json, size_t *needed_out)
{
    if (!json) return -1;

    jsmntok_t *tok = NULL; int ntok = 0;
    int tr = tokenize_json(json, &tok, &ntok);
    if (tr) return -2;

    if (ntok < 1 || tok[0].type != JSMN_ARRAY) {
        free(tok);
        return -3; // root is not array
    }

    size_t total = (size_t)tok[0].size;
    if (needed_out) *needed_out = total;

    size_t written = 0;
    int i = 1;
    for (size_t elem = 0; elem < total; elem++) {
        if (i >= ntok) break;

        if (tok[i].type != JSMN_OBJECT) {
            // tolerate and skip non-object entries
            i = tok_skip(tok, i, ntok);
            continue;
        }

        if (out && written < max_items) {
            int next = parse_one_weather(&out[written], json, tok, i, ntok);
            if (next < 0) { free(tok); return next; }
            i = next;
            written++;
        } else {
            // counting-only or buffer full: skip this element
            i = tok_skip(tok, i, ntok);
        }
    }

    free(tok);
    return (int)written;
}