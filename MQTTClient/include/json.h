#pragma once

#include <stdlib.h>
#include "jsmn.h"

int jsoneq(const char *json, const jsmntok_t *tok, const char *s);
int tok_to_str(const char *json, const jsmntok_t *tok,char *out, size_t outsz);
int tok_to_float(const char *json, const jsmntok_t *tok, float *out);
int tokenize_json(const char *json, jsmntok_t **out_tok, int *out_n);
int tok_skip(const jsmntok_t *t, int i, int n);