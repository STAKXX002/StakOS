#ifndef SH_UTIL_H
#define SH_UTIL_H

#include <stdint.h>

/*
 * Parses a non-negative integer from `s` in the given base (10 or 16).
 * For base 16, an optional "0x"/"0X" prefix is skipped. Saturates to
 * UINT32_MAX on overflow rather than wrapping — a malformed or huge
 * argument should never silently become a small number.
 */
uint32_t sh_strtoul(const char *s, int base);

#endif