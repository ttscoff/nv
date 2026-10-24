#include "bear_image_attrs.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define APEX_BEAR_JSON_MAX_DEPTH 32

static void skip_json_ws(const char **cursor, const char *end) {
    while (*cursor < end &&
           (**cursor == ' ' || **cursor == '\t' ||
            **cursor == '\n' || **cursor == '\r')) {
        (*cursor)++;
    }
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_hex_quad(const char **cursor, const char *end,
                           uint32_t *value) {
    if ((size_t)(end - *cursor) < 4) {
        return false;
    }

    uint32_t result = 0;
    for (unsigned i = 0; i < 4; i++) {
        int digit = hex_value((*cursor)[i]);
        if (digit < 0) {
            return false;
        }
        result = (result << 4) | (uint32_t)digit;
    }
    *cursor += 4;
    *value = result;
    return true;
}

static bool append_utf8(char *output, size_t *length, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        output[(*length)++] = (char)codepoint;
    } else if (codepoint <= 0x7ff) {
        output[(*length)++] = (char)(0xc0 | (codepoint >> 6));
        output[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        output[(*length)++] = (char)(0xe0 | (codepoint >> 12));
        output[(*length)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0x10ffff) {
        output[(*length)++] = (char)(0xf0 | (codepoint >> 18));
        output[(*length)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        output[(*length)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[(*length)++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        return false;
    }
    return true;
}

static bool parse_json_string(const char **cursor, const char *end,
                              char **value) {
    if (*cursor >= end || **cursor != '"') {
        return false;
    }

    const char *input = ++(*cursor);
    char *output = malloc((size_t)(end - input) + 1);
    if (!output) {
        return false;
    }
    size_t length = 0;

    while (*cursor < end) {
        unsigned char c = (unsigned char)*(*cursor)++;
        if (c == '"') {
            output[length] = '\0';
            *value = output;
            return true;
        }
        if (c < 0x20) {
            free(output);
            return false;
        }
        if (c != '\\') {
            output[length++] = (char)c;
            continue;
        }
        if (*cursor >= end) {
            free(output);
            return false;
        }

        c = (unsigned char)*(*cursor)++;
        switch (c) {
        case '"':
        case '\\':
        case '/':
            output[length++] = (char)c;
            break;
        case 'b':
            output[length++] = '\b';
            break;
        case 'f':
            output[length++] = '\f';
            break;
        case 'n':
            output[length++] = '\n';
            break;
        case 'r':
            output[length++] = '\r';
            break;
        case 't':
            output[length++] = '\t';
            break;
        case 'u': {
            uint32_t codepoint;
            if (!parse_hex_quad(cursor, end, &codepoint)) {
                free(output);
                return false;
            }
            if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                if ((size_t)(end - *cursor) < 6 ||
                    (*cursor)[0] != '\\' || (*cursor)[1] != 'u') {
                    free(output);
                    return false;
                }
                *cursor += 2;
                uint32_t low;
                if (!parse_hex_quad(cursor, end, &low) ||
                    low < 0xdc00 || low > 0xdfff) {
                    free(output);
                    return false;
                }
                codepoint = 0x10000 +
                    ((codepoint - 0xd800) << 10) + (low - 0xdc00);
            } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                free(output);
                return false;
            }
            if (!append_utf8(output, &length, codepoint)) {
                free(output);
                return false;
            }
            break;
        }
        default:
            free(output);
            return false;
        }
    }

    free(output);
    return false;
}

static bool parse_json_number(const char **cursor, const char *end,
                              char **value) {
    const char *start = *cursor;
    const char *p = start;

    if (p < end && *p == '-') {
        p++;
    }
    if (p >= end) {
        return false;
    }
    if (*p == '0') {
        p++;
        if (p < end && isdigit((unsigned char)*p)) {
            return false;
        }
    } else if (*p >= '1' && *p <= '9') {
        do {
            p++;
        } while (p < end && isdigit((unsigned char)*p));
    } else {
        return false;
    }

    if (p < end && *p == '.') {
        p++;
        if (p >= end || !isdigit((unsigned char)*p)) {
            return false;
        }
        do {
            p++;
        } while (p < end && isdigit((unsigned char)*p));
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) {
            p++;
        }
        if (p >= end || !isdigit((unsigned char)*p)) {
            return false;
        }
        do {
            p++;
        } while (p < end && isdigit((unsigned char)*p));
    }

    size_t length = (size_t)(p - start);
    char *copy = malloc(length + 1);
    if (!copy) {
        return false;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    *cursor = p;
    *value = copy;
    return true;
}

static bool skip_json_value(const char **cursor, const char *end,
                            unsigned depth) {
    skip_json_ws(cursor, end);
    if (*cursor >= end) {
        return false;
    }

    if (**cursor == '"') {
        char *string = NULL;
        bool ok = parse_json_string(cursor, end, &string);
        free(string);
        return ok;
    }
    if (**cursor == '-' || isdigit((unsigned char)**cursor)) {
        char *number = NULL;
        bool ok = parse_json_number(cursor, end, &number);
        free(number);
        return ok;
    }
    if ((size_t)(end - *cursor) >= 4 &&
        memcmp(*cursor, "true", 4) == 0) {
        *cursor += 4;
        return true;
    }
    if ((size_t)(end - *cursor) >= 5 &&
        memcmp(*cursor, "false", 5) == 0) {
        *cursor += 5;
        return true;
    }
    if ((size_t)(end - *cursor) >= 4 &&
        memcmp(*cursor, "null", 4) == 0) {
        *cursor += 4;
        return true;
    }
    if (**cursor != '[' && **cursor != '{') {
        return false;
    }
    if (depth >= APEX_BEAR_JSON_MAX_DEPTH) {
        return false;
    }

    char open = *(*cursor)++;
    char close = open == '[' ? ']' : '}';
    skip_json_ws(cursor, end);
    if (*cursor < end && **cursor == close) {
        (*cursor)++;
        return true;
    }

    for (;;) {
        if (open == '{') {
            char *key = NULL;
            if (!parse_json_string(cursor, end, &key)) {
                return false;
            }
            free(key);
            skip_json_ws(cursor, end);
            if (*cursor >= end || *(*cursor)++ != ':') {
                return false;
            }
        }
        if (!skip_json_value(cursor, end, depth + 1)) {
            return false;
        }
        skip_json_ws(cursor, end);
        if (*cursor >= end) {
            return false;
        }
        if (**cursor == close) {
            (*cursor)++;
            return true;
        }
        if (*(*cursor)++ != ',') {
            return false;
        }
        skip_json_ws(cursor, end);
        if (*cursor >= end || **cursor == close) {
            return false;
        }
    }
}

static bool is_allowed_key(const char *key) {
    static const char *allowed[] = {
        "width", "height", "style", "class", "id", "rel", "title"
    };
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
        if (strcmp(key, allowed[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool key_accepts_number(const char *key) {
    return strcmp(key, "width") == 0 || strcmp(key, "height") == 0;
}

static bool set_attr(apex_bear_image_attrs *attrs, char *key, char *value) {
    for (size_t i = 0; i < attrs->count; i++) {
        if (strcmp(attrs->items[i].key, key) == 0) {
            free(attrs->items[i].value);
            attrs->items[i].value = value;
            free(key);
            return true;
        }
    }
    if (attrs->count >= APEX_BEAR_IMAGE_ATTR_CAPACITY) {
        free(key);
        free(value);
        return false;
    }
    attrs->items[attrs->count].key = key;
    attrs->items[attrs->count].value = value;
    attrs->count++;
    return true;
}

void apex_free_bear_image_attrs(apex_bear_image_attrs *attrs) {
    if (!attrs) {
        return;
    }
    for (size_t i = 0; i < attrs->count; i++) {
        free(attrs->items[i].key);
        free(attrs->items[i].value);
    }
    memset(attrs, 0, sizeof(*attrs));
}

bool apex_parse_bear_image_comment(
    const char *comment_start,
    const char *line_end,
    const char **comment_end,
    apex_bear_image_attrs *attrs) {
    if (!attrs) {
        return false;
    }
    /* Release any previous result so reusing the struct cannot leak. */
    apex_free_bear_image_attrs(attrs);

    if (!comment_start || !line_end || !comment_end ||
        line_end < comment_start || (size_t)(line_end - comment_start) < 7 ||
        memcmp(comment_start, "<!--", 4) != 0) {
        return false;
    }

    const char *body_end = NULL;
    for (const char *p = comment_start + 4; p + 2 < line_end; p++) {
        if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
            body_end = p;
            break;
        }
    }
    if (!body_end) {
        return false;
    }

    const char *cursor = comment_start + 4;
    skip_json_ws(&cursor, body_end);
    if (cursor >= body_end || *cursor != '{') {
        return false;
    }
    cursor++;
    skip_json_ws(&cursor, body_end);

    if (cursor < body_end && *cursor != '}') {
        for (;;) {
            char *key = NULL;
            char *value = NULL;
            if (!parse_json_string(&cursor, body_end, &key)) {
                goto fail;
            }
            skip_json_ws(&cursor, body_end);
            if (cursor >= body_end || *cursor++ != ':') {
                free(key);
                goto fail;
            }
            skip_json_ws(&cursor, body_end);

            bool allowed = is_allowed_key(key);
            bool store = false;
            if (cursor < body_end && *cursor == '"') {
                if (!parse_json_string(&cursor, body_end, &value)) {
                    free(key);
                    goto fail;
                }
                store = allowed;
            } else if (cursor < body_end &&
                       (*cursor == '-' ||
                        isdigit((unsigned char)*cursor))) {
                if (!parse_json_number(&cursor, body_end, &value)) {
                    free(key);
                    goto fail;
                }
                store = allowed && key_accepts_number(key);
            } else {
                if (!skip_json_value(&cursor, body_end, 0)) {
                    free(key);
                    goto fail;
                }
            }

            if (store) {
                if (!set_attr(attrs, key, value)) {
                    goto fail;
                }
            } else {
                free(key);
                free(value);
            }

            skip_json_ws(&cursor, body_end);
            if (cursor >= body_end) {
                goto fail;
            }
            if (*cursor == '}') {
                break;
            }
            if (*cursor++ != ',') {
                goto fail;
            }
            skip_json_ws(&cursor, body_end);
            if (cursor >= body_end || *cursor == '}') {
                goto fail;
            }
        }
    }

    if (cursor >= body_end || *cursor++ != '}') {
        goto fail;
    }
    skip_json_ws(&cursor, body_end);
    if (cursor != body_end) {
        goto fail;
    }

    *comment_end = body_end + 3;
    return true;

fail:
    apex_free_bear_image_attrs(attrs);
    return false;
}
