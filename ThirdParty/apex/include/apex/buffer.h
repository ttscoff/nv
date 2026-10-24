/**
 * @file buffer.h
 * @brief Dynamic string buffer for efficient string building
 */

#ifndef APEX_BUFFER_H
#define APEX_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

/**
 * Dynamic buffer structure
 */
typedef struct {
    char *data;           /**< Buffer data */
    size_t size;          /**< Current size */
    size_t capacity;      /**< Allocated capacity */
} apex_buffer;

/**
 * Initialize a buffer
 *
 * @param buf Buffer to initialize
 * @param initial_capacity Initial capacity
 */
void apex_buffer_init(apex_buffer *buf, size_t initial_capacity);

/**
 * Free buffer resources
 *
 * @param buf Buffer to free
 */
void apex_buffer_free(apex_buffer *buf);

/**
 * Clear buffer contents
 *
 * @param buf Buffer to clear
 */
void apex_buffer_clear(apex_buffer *buf);

/**
 * Append string to buffer
 *
 * @param buf Buffer
 * @param data String to append
 * @param len Length of string
 */
void apex_buffer_append(apex_buffer *buf, const char *data, size_t len);

/**
 * Append null-terminated string to buffer
 *
 * @param buf Buffer
 * @param str String to append
 */
void apex_buffer_append_str(apex_buffer *buf, const char *str);

/**
 * Append single character to buffer
 *
 * @param buf Buffer
 * @param c Character to append
 */
void apex_buffer_append_char(apex_buffer *buf, char c);

/**
 * Get buffer contents as string
 *
 * @param buf Buffer
 * @return Null-terminated string (do not free)
 */
const char *apex_buffer_cstr(const apex_buffer *buf);

/**
 * Detach buffer data (caller must free)
 *
 * @param buf Buffer
 * @return Buffer data (must be freed with free())
 */
char *apex_buffer_detach(apex_buffer *buf);

#ifdef __cplusplus
}
#endif

#endif /* APEX_BUFFER_H */

