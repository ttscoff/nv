/**
 * @file utf8.c
 * @brief UTF-8 utility functions
 */

#include <stddef.h>
#include <stdbool.h>

/**
 * Check if byte is valid UTF-8 start byte
 */
bool apex_utf8_is_valid_start(unsigned char byte) {
    return (byte & 0x80) == 0 ||
           (byte & 0xE0) == 0xC0 ||
           (byte & 0xF0) == 0xE0 ||
           (byte & 0xF8) == 0xF0;
}

/**
 * Get length of UTF-8 character from first byte
 */
int apex_utf8_char_length(unsigned char byte) {
    if ((byte & 0x80) == 0) return 1;
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 0; /* Invalid */
}

/**
 * Validate UTF-8 string
 */
bool apex_utf8_validate(const char *str, size_t len) {
    size_t i = 0;

    while (i < len) {
        unsigned char byte = (unsigned char)str[i];
        int char_len = apex_utf8_char_length(byte);

        if (char_len == 0 || i + char_len > len) {
            return false;
        }

        /* Check continuation bytes */
        for (int j = 1; j < char_len; j++) {
            if ((str[i + j] & 0xC0) != 0x80) {
                return false;
            }
        }

        i += char_len;
    }

    return true;
}

