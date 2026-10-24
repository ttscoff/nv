/**
 * @file buffer.c
 * @brief Dynamic buffer implementation
 */

#include "apex/buffer.h"
#include <stdlib.h>
#include <string.h>

#define BUFFER_INIT_CAPACITY 256
#define BUFFER_GROWTH_FACTOR 2

void apex_buffer_init(apex_buffer *buf, size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = BUFFER_INIT_CAPACITY;
    }

    buf->data = (char *)malloc(initial_capacity);
    buf->size = 0;
    buf->capacity = initial_capacity;

    if (buf->data) {
        buf->data[0] = '\0';
    }
}

void apex_buffer_free(apex_buffer *buf) {
    if (buf && buf->data) {
        free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

void apex_buffer_clear(apex_buffer *buf) {
    buf->size = 0;
    if (buf->data) {
        buf->data[0] = '\0';
    }
}

static void apex_buffer_grow(apex_buffer *buf, size_t needed) {
    size_t new_capacity = buf->capacity;

    while (new_capacity < needed) {
        new_capacity *= BUFFER_GROWTH_FACTOR;
    }

    char *new_data = (char *)realloc(buf->data, new_capacity);
    if (new_data) {
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
}

void apex_buffer_append(apex_buffer *buf, const char *data, size_t len) {
    if (!buf || !data || len == 0) {
        return;
    }

    size_t needed = buf->size + len + 1;
    if (needed > buf->capacity) {
        apex_buffer_grow(buf, needed);
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

void apex_buffer_append_str(apex_buffer *buf, const char *str) {
    if (str) {
        apex_buffer_append(buf, str, strlen(str));
    }
}

void apex_buffer_append_char(apex_buffer *buf, char c) {
    apex_buffer_append(buf, &c, 1);
}

const char *apex_buffer_cstr(const apex_buffer *buf) {
    return buf ? buf->data : "";
}

char *apex_buffer_detach(apex_buffer *buf) {
    char *result = buf->data;
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
    return result;
}

