#include "json.h"
#include <string.h>

int jsoneq(const char *json, const jsmntok_t *tok, const char *s) {
    int len = tok->end - tok->start;
    return tok->type == JSMN_STRING &&
           (int)strlen(s) == len &&
           strncmp(json + tok->start, s, len) == 0 ? 0 : -1;
}

int tok_to_str(const char *json, const jsmntok_t *tok,
               char *out, size_t outsz) {
    int len = tok->end - tok->start;
    if (len <= 0 || (size_t)len >= outsz) return -1;
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
    return 0;
}

int tok_to_float(const char *json, const jsmntok_t *tok, float *out) {
    char buf[32];
    int len = tok->end - tok->start;
    if (len <= 0 || len >= (int)sizeof(buf)) return -1;
    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';
    char *end = NULL;
    *out = strtof(buf, &end);
    return (end && *end == '\0') ? 0 : -1;
}

int tokenize_json(const char *json, jsmntok_t **out_tok, int *out_n) {
    jsmn_parser p;
    int cap = 256; // start size; doubles on demand
    jsmntok_t *buf = NULL;

    for (;;) {
        jsmntok_t *nbuf = (jsmntok_t *)realloc(buf, cap * sizeof(jsmntok_t));
        if (!nbuf) { free(buf); return -1; }
        buf = nbuf;

        jsmn_init(&p);
        int r = jsmn_parse(&p, json, (int)strlen(json), buf, cap);
        if (r >= 0) {
            *out_tok = buf;
            *out_n = r;
            return 0;
        }
        if (r == JSMN_ERROR_NOMEM) { cap *= 2; continue; }

        free(buf);
        return -2; 
    }
}

int tok_skip(const jsmntok_t *t, int i, int n) {
    if (i < 0 || i >= n) return i + 1;
    int j = i + 1;
    if (t[i].type == JSMN_ARRAY || t[i].type == JSMN_OBJECT) {
        for (int k = 0; k < t[i].size; k++) j = tok_skip(t, j, n);
    }
    return j;
}