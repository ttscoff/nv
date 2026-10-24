#ifndef APEX_BEAR_IMAGE_ATTRS_H
#define APEX_BEAR_IMAGE_ATTRS_H

#include <stdbool.h>
#include <stddef.h>

#define APEX_BEAR_IMAGE_ATTR_CAPACITY 7

typedef struct {
    char *key;
    char *value;
} apex_bear_image_attr;

typedef struct {
    apex_bear_image_attr items[APEX_BEAR_IMAGE_ATTR_CAPACITY];
    size_t count;
} apex_bear_image_attrs;

bool apex_parse_bear_image_comment(
    const char *comment_start,
    const char *line_end,
    const char **comment_end,
    apex_bear_image_attrs *attrs);

void apex_free_bear_image_attrs(apex_bear_image_attrs *attrs);

#endif
