#ifndef AFROS_UTILS_H
#define AFROS_UTILS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file afros_utils.h
 * @brief Common utility functions for AfriOS.
 */

uint32_t afros_hash(const char *str);
void afros_hex_dump(const uint8_t *data, size_t size);

#endif
