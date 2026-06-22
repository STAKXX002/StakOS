#include "sh_util.h"

uint32_t sh_strtoul(const char* s, int base) {
    uint32_t result = 0;

    /* skip optional 0x prefix */
    if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    while (*s) {
        uint32_t digit;

        if (*s >= '0' && *s <= '9')      digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;

        uint32_t max_before_mul = 0xFFFFFFFFu / (uint32_t)base;

        if (result > max_before_mul || result * (uint32_t)base > 0xFFFFFFFFu - digit) {
            result = 0xFFFFFFFFu;  /* saturate */
        } else {
            result = result * (uint32_t)base + digit;
        }

        s++;
    }
    return result;
}