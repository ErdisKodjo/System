#include "../include/afros_utils.h"
#include <stdio.h>

/**
 * @file utils.c
 * @brief Implementation of common utilities for AfriOS.
 */

uint32_t afros_hash(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void afros_hex_dump(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}
