#include "utils.h"

#include <stdlib.h>
#include <string.h>

char *copy_with_null(const char *src, int len) {
    char *dest = malloc(len + 1);
    if (!dest) return NULL;
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}