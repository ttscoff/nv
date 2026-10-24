/**
 * Kramdown IAL (Inline Attribute Lists) Implementation
 */

#include "ial.h"
#include "bear_image_attrs.h"
#include "table.h"  /* For CMARK_NODE_TABLE */
#include "apex/apex.h"  /* For apex_mode_t */
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

static int apex_ial_ptrdiff_to_int(ptrdiff_t v) {
    if (v <= 0) return 0;
    if (v > INT_MAX) return INT_MAX;
    return (int)v;
}

static int apex_ial_size_to_int(size_t v) {
    if (v > (size_t)INT_MAX) return INT_MAX;
    return (int)v;
}

/**
 * Free attributes structure
 */
void apex_free_attributes(apex_attributes *attrs) {
    if (!attrs) return;

    free(attrs->id);

    for (int i = 0; i < attrs->class_count; i++) {
        free(attrs->classes[i]);
    }
    free(attrs->classes);

    for (int i = 0; i < attrs->attr_count; i++) {
        free(attrs->keys[i]);
        free(attrs->values[i]);
    }
    free(attrs->keys);
    free(attrs->values);

    free(attrs);
}

/**
 * Free ALD list
 */
void apex_free_alds(ald_entry *alds) {
    while (alds) {
        ald_entry *next = alds->next;
        free(alds->name);
        apex_free_attributes(alds->attrs);
        free(alds);
        alds = next;
    }
}

/**
 * Create empty attributes structure
 */
static apex_attributes *create_attributes(void) {
    apex_attributes *attrs = calloc(1, sizeof(apex_attributes));
    return attrs;
}

/**
 * Add class to attributes
 */
static void add_class(apex_attributes *attrs, const char *class_name) {
    if (!attrs || !class_name) return;

    attrs->classes = realloc(attrs->classes, sizeof(char*) * (attrs->class_count + 1));
    attrs->classes[attrs->class_count++] = strdup(class_name);
}

/**
 * Add key-value attribute
 */
static void add_attribute(apex_attributes *attrs, const char *key, const char *value) {
    if (!attrs || !key) return;

    attrs->keys = realloc(attrs->keys, sizeof(char*) * (attrs->attr_count + 1));
    attrs->values = realloc(attrs->values, sizeof(char*) * (attrs->attr_count + 1));
    attrs->keys[attrs->attr_count] = strdup(key);
    attrs->values[attrs->attr_count] = value ? strdup(value) : strdup("");
    attrs->attr_count++;
}

/**
 * Parse IAL/ALD content
 * Format: #id .class .class2 key="value" key2='value2'
 */
apex_attributes *parse_ial_content(const char *content, int len) {
    apex_attributes *attrs = create_attributes();
    if (!attrs) return NULL;

    char buffer[2048];
    if (len >= (int)sizeof(buffer)) len = (int)sizeof(buffer) - 1;
    memcpy(buffer, content, len);
    buffer[len] = '\0';

    char *p = buffer;
    while (*p) {
        /* Skip whitespace */
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Check for ID (#id) */
        if (*p == '#') {
            p++;
            char *id_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '}') p++;
            if (p > id_start) {
                char saved = *p;
                *p = '\0';
                if (attrs->id) free(attrs->id);
                attrs->id = strdup(id_start);
                *p = saved;
            }
            continue;
        }

        /* Check for class (.class) */
        if (*p == '.') {
            p++;
            char *class_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '#' && *p != '}') p++;
            if (p > class_start) {
                char saved = *p;
                *p = '\0';
                add_class(attrs, class_start);
                *p = saved;
            }
            continue;
        }

        /* Check for key="value" or key='value' */
        char *key_start = p;
        while (*p && *p != '=' && *p != ' ' && *p != '\t' && *p != '}') p++;

        if (*p == '=') {
            /* Found key=value */
            char saved = *p;
            *p = '\0';
            char *key = strdup(key_start);
            *p = saved;
            p++; /* Skip = */

            /* Parse value (could be quoted - handle both straight and curly quotes) */
            char *value = NULL;
            bool is_curly_left = ((unsigned char)*p == 0xE2 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x9C);  /* " */
            bool is_curly_right = ((unsigned char)*p == 0xE2 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x9D);  /* " */
            bool is_straight_quote = (*p == '"' || *p == '\'');

            if (is_straight_quote || is_curly_left || is_curly_right) {
                bool is_curly = (is_curly_left || is_curly_right);
                char *value_start;

                if (is_curly) {
                    /* Skip UTF-8 curly quote (3 bytes) */
                    p += 3;
                    value_start = p;

                    /* Find closing curly quote (either left or right) */
                    char *value_end = p;
                    while (*value_end) {
                        if ((unsigned char)*value_end == 0xE2 && (unsigned char)value_end[1] == 0x80 &&
                            ((unsigned char)value_end[2] == 0x9C || (unsigned char)value_end[2] == 0x9D)) {
                            break;  /* Found closing curly quote */
                        }
                        value_end++;
                    }
                    if ((unsigned char)*value_end == 0xE2) {
                        /* Extract value (content between curly quotes, excluding quotes) */
                        size_t value_len = value_end - value_start;
                        value = malloc(value_len + 1);
                        if (value) {
                            memcpy(value, value_start, value_len);
                            value[value_len] = '\0';
                        }
                        p = value_end + 3;  /* Skip closing curly quote */
                    }
                } else {
                    /* Straight quote */
                    char quote = *p++;
                    value_start = p;
                    while (*p && *p != quote) {
                        if (*p == '\\' && *(p+1)) p++; /* Skip escaped char */
                        p++;
                    }
                    if (*p == quote) {
                        *p = '\0';
                        value = strdup(value_start);
                        *p = quote;
                        p++;
                    }
                }
            } else {
                /* Unquoted value */
                char *value_start = p;
                while (*p && !isspace((unsigned char)*p) && *p != '}') p++;
                char saved_val = *p;
                *p = '\0';
                value = strdup(value_start);
                *p = saved_val;
            }

            add_attribute(attrs, key, value);
            free(key);
            free(value);
            continue;
        }

        /* Check for bare @2x/@3x (retina srcset markers) */
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == '@' && key_start[1] == '2' && key_start[2] == 'x') {
            add_attribute(attrs, "data-srcset-2x", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == '@' && key_start[1] == '3' && key_start[2] == 'x') {
            add_attribute(attrs, "data-srcset-3x", "1");
            continue;
        }

        /* Check for bare webp/avif (picture srcset format markers) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'w' && key_start[1] == 'e' && key_start[2] == 'b' && key_start[3] == 'p') {
            add_attribute(attrs, "data-srcset-webp", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'a' && key_start[1] == 'v' && key_start[2] == 'i' && key_start[3] == 'f') {
            add_attribute(attrs, "data-srcset-avif", "1");
            continue;
        }

        /* Check for bare video format markers (webm, ogg, mp4, mov, m4v) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'w' && key_start[1] == 'e' && key_start[2] == 'b' && key_start[3] == 'm') {
            add_attribute(attrs, "data-video-webm", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'o' && key_start[1] == 'g' && key_start[2] == 'g') {
            add_attribute(attrs, "data-video-ogg", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == 'p' && key_start[2] == '4') {
            add_attribute(attrs, "data-video-mp4", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == 'o' && key_start[2] == 'v') {
            add_attribute(attrs, "data-video-mov", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == '4' && key_start[2] == 'v') {
            add_attribute(attrs, "data-video-m4v", "1");
            continue;
        }

        /* Check for bare auto (discover formats from filesystem) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'a' && key_start[1] == 'u' && key_start[2] == 't' && key_start[3] == 'o') {
            add_attribute(attrs, "data-apex-auto", "1");
            continue;
        }

        /* Unknown token, skip */
        p++;
    }

    return attrs;
}

/**
 * Check if line is an ALD
 * Pattern: {:ref-name: attributes}
 */
static bool is_ald_line(const char *line, char **ref_name, apex_attributes **attrs) {
    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Leanpub index syntax {i: term} is not an ALD */
    if (p[0] == '{' && p[1] == 'i' && p[2] == ':') return false;

    /* Check for {: */
    if (p[0] != '{' || p[1] != ':') return false;
    p += 2;

    /* Extract reference name */
    const char *name_start = p;
    while (*p && *p != ':' && *p != '}') p++;

    if (*p != ':') return false; /* Not an ALD, maybe regular IAL */

    /* Found ALD */
    int name_len = apex_ial_ptrdiff_to_int(p - name_start);
    if (name_len <= 0) return false;

    *ref_name = malloc(name_len + 1);
    memcpy(*ref_name, name_start, name_len);
    (*ref_name)[name_len] = '\0';

    p++; /* Skip second : */

    /* Find closing } */
    const char *content_start = p;
    const char *close = strchr(p, '}');
    if (!close) {
        free(*ref_name);
        return false;
    }

    /* Parse attributes */
    *attrs = parse_ial_content(content_start, apex_ial_ptrdiff_to_int(close - content_start));

    return true;
}

/**
 * Extract ALDs from text
 */
ald_entry *apex_extract_alds(char **text_ptr) {
    if (!text_ptr || !*text_ptr) return NULL;

    char *text = *text_ptr;
    ald_entry *alds = NULL;
    ald_entry **tail = &alds;

    char *line_start = text;
    char *line_end;

    char *output = malloc(strlen(text) + 1);
    char *output_write = output;

    if (!output) return NULL;

    while ((line_end = strchr(line_start, '\n')) != NULL || *line_start) {
        if (!line_end) line_end = line_start + strlen(line_start);

        size_t line_len = (size_t)(line_end - line_start);
        char *line = malloc(line_len + 1);
        if (!line) {
            apex_free_alds(alds);
            free(output);
            return NULL;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        /* Check if this is an ALD */
        char *ref_name = NULL;
        apex_attributes *attrs = NULL;

        if (is_ald_line(line, &ref_name, &attrs)) {
            /* Found ALD - store it */
            ald_entry *entry = malloc(sizeof(ald_entry));
            if (entry) {
                entry->name = ref_name;
                entry->attrs = attrs;
                entry->next = NULL;

                *tail = entry;
                tail = &entry->next;
            }

            /* Skip this line in output */
            free(line);
            line_start = *line_end ? line_end + 1 : line_end;
            continue;
        }

        /* Not an ALD, copy line to output */
        memcpy(output_write, line_start, line_len);
        output_write += line_len;
        free(line);
        if (*line_end) {
            *output_write++ = '\n';
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    *output_write = '\0';

    /* Use the output buffer as the new text */
    size_t output_len = strlen(output);
    if (output_len <= strlen(*text_ptr)) {
        strcpy(*text_ptr, output);
    } else {
        /* Output is larger, need to reallocate */
        free(*text_ptr);
        *text_ptr = strdup(output);
    }
    free(output);

    return alds;
}

/**
 * Find ALD by name
 */
static apex_attributes *find_ald(ald_entry *alds, const char *name) {
    for (ald_entry *entry = alds; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            return entry->attrs;
        }
    }
    return NULL;
}

/**
 * Check if an attribute key already exists in the attributes structure
 * Returns the index if found, or -1 if not found
 */
static int find_attribute_index(apex_attributes *attrs, const char *key) {
    if (!attrs || !key) return -1;
    for (int i = 0; i < attrs->attr_count; i++) {
        if (attrs->keys[i] && strcmp(attrs->keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Merge attributes (for ALD references)
 * Base attributes are copied first, then override attributes are applied.
 * Override attributes replace base attributes with the same key/ID.
 * Classes are appended (duplicates allowed, HTML will handle them).
 */
static apex_attributes *merge_attributes(apex_attributes *base, apex_attributes *override) {
    apex_attributes *merged = create_attributes();
    if (!merged) return base;

    /* Copy base attributes */
    if (base) {
        if (base->id) merged->id = strdup(base->id);
        for (int i = 0; i < base->class_count; i++) {
            add_class(merged, base->classes[i]);
        }
        for (int i = 0; i < base->attr_count; i++) {
            add_attribute(merged, base->keys[i], base->values[i]);
        }
    }

    /* Override with new attributes */
    if (override) {
        /* Override ID if present */
        if (override->id) {
            free(merged->id);
            merged->id = strdup(override->id);
        }
        /* Append classes (allow duplicates) */
        for (int i = 0; i < override->class_count; i++) {
            add_class(merged, override->classes[i]);
        }
        /* Override key-value attributes (replace if key exists, otherwise add) */
        for (int i = 0; i < override->attr_count; i++) {
            int existing_idx = find_attribute_index(merged, override->keys[i]);
            if (existing_idx >= 0) {
                /* Replace existing attribute */
                free(merged->values[existing_idx]);
                merged->values[existing_idx] = strdup(override->values[i]);
            } else {
                /* Add new attribute */
                add_attribute(merged, override->keys[i], override->values[i]);
            }
        }
    }

    return merged;
}

static apex_attributes *copy_attributes(const apex_attributes *source) {
    apex_attributes *copy = create_attributes();
    if (!copy) return NULL;

    if (source) {
        if (source->id) copy->id = strdup(source->id);
        for (int i = 0; i < source->class_count; i++) {
            add_class(copy, source->classes[i]);
        }
        for (int i = 0; i < source->attr_count; i++) {
            add_attribute(copy, source->keys[i], source->values[i]);
        }
    }

    return copy;
}

static char *escape_bear_attribute_value(const char *value) {
    size_t length = 0;
    for (const char *p = value; *p; p++) {
        switch (*p) {
        case '&':
            length += strlen("&amp;");
            break;
        case '"':
            length += strlen("&quot;");
            break;
        case '<':
            length += strlen("&lt;");
            break;
        case '>':
            length += strlen("&gt;");
            break;
        default:
            length++;
            break;
        }
    }

    char *escaped = malloc(length + 1);
    if (!escaped) return NULL;

    char *write = escaped;
    for (const char *p = value; *p; p++) {
        const char *replacement = NULL;
        switch (*p) {
        case '&':
            replacement = "&amp;";
            break;
        case '"':
            replacement = "&quot;";
            break;
        case '<':
            replacement = "&lt;";
            break;
        case '>':
            replacement = "&gt;";
            break;
        default:
            *write++ = *p;
            break;
        }
        if (replacement) {
            size_t replacement_length = strlen(replacement);
            memcpy(write, replacement, replacement_length);
            write += replacement_length;
        }
    }
    *write = '\0';
    return escaped;
}

static apex_attributes *attributes_from_bear(
    const apex_bear_image_attrs *bear) {
    apex_attributes *attrs = create_attributes();
    if (!attrs) return NULL;

    for (size_t i = 0; i < bear->count; i++) {
        /*
         * Width/height classification is unchanged by escaping: all encoded
         * characters are non-numeric in the decoded value as well.
         */
        char *escaped = escape_bear_attribute_value(bear->items[i].value);
        if (escaped) {
            add_attribute(attrs, bear->items[i].key, escaped);
            free(escaped);
        }
    }
    return attrs;
}

static apex_attributes *parse_adjacent_bear_attrs(
    const char *after_construct,
    const char *line_end,
    const char **comment_start,
    const char **comment_end) {
    *comment_start = NULL;
    *comment_end = NULL;

    const char *candidate = after_construct;
    while (candidate < line_end &&
           (*candidate == ' ' || *candidate == '\t')) {
        candidate++;
    }

    apex_bear_image_attrs bear = {0};
    if (!apex_parse_bear_image_comment(
            candidate, line_end, comment_end, &bear)) {
        return NULL;
    }

    apex_attributes *attrs = attributes_from_bear(&bear);
    apex_free_bear_image_attrs(&bear);
    if (!attrs) {
        *comment_end = NULL;
        return NULL;
    }

    *comment_start = candidate;
    return attrs;
}

/**
 * Check if text ends with IAL pattern
 * Pattern: {: attributes} or {:.class} or {: ref-name} or {: ref-name .class #id}
 */
static bool extract_ial_from_text(const char *text, apex_attributes **attrs_out, ald_entry *alds) {
    if (!text) return false;

    /* Find { from the end - support both {: ...} and {#id .class} formats */
    const char *ial_start = strrchr(text, '{');
    if (!ial_start) return false;

    /* Check if it's a valid IAL format: {: or {# or {. */
    char second_char = ial_start[1];
    /* Leanpub index syntax {i: term} is not IAL */
    if (second_char == 'i' && ial_start[2] == ':') return false;
    if (second_char != ':' && second_char != '#' && second_char != '.') return false;

    /* Find closing } */
    const char *ial_end = strchr(ial_start, '}');
    if (!ial_end) return false;

    /* Check if this is at the end (only whitespace after) */
    const char *p_check = ial_end + 1;
    while (*p_check && isspace((unsigned char)*p_check)) p_check++;
    if (*p_check) return false; /* Not at end */

    /* Parse IAL content */
    /* For {: format, skip {: (2 chars); for {# or {. format, skip { (1 char) */
    const char *content_start = (second_char == ':') ? ial_start + 2 : ial_start + 1;
    int content_len = apex_ial_ptrdiff_to_int(ial_end - content_start);

    if (content_len <= 0) {
        *attrs_out = NULL;
        return false;
    }

    /* Copy content to work with */
    char buffer[2048];
    if (content_len >= (int)sizeof(buffer)) content_len = (int)sizeof(buffer) - 1;
    memcpy(buffer, content_start, content_len);
    buffer[content_len] = '\0';

    char *p = buffer;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) {
        *attrs_out = NULL;
        return false;
    }

    /* Extract first token */
    char *token_start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '#' && *p != '.' && *p != '=') p++;

    apex_attributes *ald_attrs = NULL;
    const char *remaining_content = NULL;
    int remaining_len = 0;

    if (p > token_start) {
        /* Check if first token is a simple word (ALD reference) */
        char saved = *p;
        *p = '\0';

        bool is_ref = true;
        for (char *c = token_start; *c; c++) {
            if (*c == '#' || *c == '.' || *c == '=') {
                is_ref = false;
                break;
            }
        }

        *p = saved; /* Restore */

        if (is_ref && *token_start) {
            /* Trim the token */
            char *trimmed = token_start;
            while (isspace((unsigned char)*trimmed)) trimmed++;
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && isspace((unsigned char)*end)) *end-- = '\0';

            /* Look up ALD */
            if (*trimmed) {
                ald_attrs = find_ald(alds, trimmed);
            }
        }
    }

    /* If we found an ALD, check for remaining content */
    if (ald_attrs) {
        /* Skip whitespace after token */
        while (*p && isspace((unsigned char)*p)) p++;

        if (*p) {
            /* There's remaining content - parse it as additional attributes */
            remaining_content = content_start + (p - buffer);
            remaining_len = content_len - apex_ial_ptrdiff_to_int(p - buffer);
        }
    }

    /* Parse additional attributes if any */
    apex_attributes *additional_attrs = NULL;
    if (remaining_content && remaining_len > 0) {
        additional_attrs = parse_ial_content(remaining_content, remaining_len);
    }

    /* Merge ALD with additional attributes */
    if (ald_attrs) {
        *attrs_out = merge_attributes(ald_attrs, additional_attrs);
        if (additional_attrs) {
            apex_free_attributes(additional_attrs);
        }
        return true;
    }

    /* No ALD found, parse as regular IAL */
    *attrs_out = parse_ial_content(content_start, content_len);
    return *attrs_out != NULL;
}

/**
 * Apply attributes to HTML tag
 * Helper function to generate attribute string
 */
char *attributes_to_html(apex_attributes *attrs) {
    if (!attrs) return strdup("");

    char buffer[4096];
    char *p = buffer;
    size_t remaining = sizeof(buffer);

    #define APPEND(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(p, str, len); \
            p += len; \
        remaining -= len; \
        } \
    } while(0)

    bool first_attr = true;

    /* Add ID */
    if (attrs->id) {
        char id_str[512];
        if (first_attr) {
            snprintf(id_str, sizeof(id_str), "id=\"%s\"", attrs->id);
            first_attr = false;
        } else {
            snprintf(id_str, sizeof(id_str), " id=\"%s\"", attrs->id);
        }
        APPEND(id_str);
    }

    /* Add classes */
    if (attrs->class_count > 0) {
        if (first_attr) {
            APPEND("class=\"");
            first_attr = false;
        } else {
            APPEND(" class=\"");
        }
        for (int i = 0; i < attrs->class_count; i++) {
            if (i > 0) APPEND(" ");
            APPEND(attrs->classes[i]);
        }
        APPEND("\"");
    }

    /* Check for existing style attribute to merge with */
    const char *existing_style = NULL;
    for (int i = 0; i < attrs->attr_count; i++) {
        if (strcmp(attrs->keys[i], "style") == 0) {
            existing_style = attrs->values[i];
            break;
        }
    }

    /* Build style string for width/height that need to be in style */
    char style_buffer[1024] = {0};
    bool has_style = false;

    /* Start with existing style if present */
    if (existing_style && *existing_style) {
        strncpy(style_buffer, existing_style, sizeof(style_buffer) - 1);
        style_buffer[sizeof(style_buffer) - 1] = '\0';
        has_style = true;
    }

    /* Process width and height attributes */
    for (int i = 0; i < attrs->attr_count; i++) {
        const char *key = attrs->keys[i];
        const char *val = attrs->values[i];

        /* Skip style attribute - we'll handle it separately */
        if (strcmp(key, "style") == 0) {
            continue;
        }

        if (strcmp(key, "width") == 0 || strcmp(key, "height") == 0) {
            size_t val_len = strlen(val);
            bool is_px = (val_len >= 2 && val[val_len - 2] == 'p' && val[val_len - 1] == 'x');
            bool is_percent = (val_len >= 1 && val[val_len - 1] == '%');
            bool is_integer = true;
            bool is_decimal_px = false;

            /* Check if it's a bare integer or integer pixel value */
            if (is_px) {
                /* Check if the part before 'px' is a pure integer (no decimal) */
                for (size_t j = 0; j < val_len - 2; j++) {
                    if (val[j] == '.' || val[j] == ',') {
                        is_decimal_px = true;
                        is_integer = false;
                        break;
                    }
                    if (!isdigit((unsigned char)val[j])) {
                        is_integer = false;
                        break;
                    }
                }
            } else if (is_percent) {
                is_integer = false;
            } else {
                /* Check if all characters are digits */
                for (const char *c = val; *c; c++) {
                    if (!isdigit((unsigned char)*c)) {
                        is_integer = false;
                        break;
                    }
                }
            }

            if (is_px && !is_decimal_px && is_integer) {
                /* Convert integer Xpx to integer X for width/height attributes */
                char int_val[64];
                memcpy(int_val, val, val_len - 2);
                int_val[val_len - 2] = '\0';

                char attr_str[1024];
                if (first_attr) {
                    snprintf(attr_str, sizeof(attr_str), "%s=\"%s\"", key, int_val);
                    first_attr = false;
                } else {
                    snprintf(attr_str, sizeof(attr_str), " %s=\"%s\"", key, int_val);
                }
                APPEND(attr_str);
            } else if (is_integer && !is_px && !is_percent) {
                /* Bare integer - use as width/height attribute */
                char attr_str[1024];
                if (first_attr) {
                    snprintf(attr_str, sizeof(attr_str), "%s=\"%s\"", key, val);
                    first_attr = false;
                } else {
                    snprintf(attr_str, sizeof(attr_str), " %s=\"%s\"", key, val);
                }
                APPEND(attr_str);
            } else {
                /* Percentage, decimal pixel, or other non-integer - add to style */
                if (has_style) {
                    strcat(style_buffer, "; ");
                }
                strcat(style_buffer, key);
                strcat(style_buffer, ": ");
                strcat(style_buffer, val);
                has_style = true;
            }
        }
    }

    /* Add style attribute if we have width/height in style or existing style */
    if (has_style) {
        char style_str[1024];
        if (first_attr) {
            snprintf(style_str, sizeof(style_str), "style=\"%s\"", style_buffer);
            first_attr = false;
        } else {
            snprintf(style_str, sizeof(style_str), " style=\"%s\"", style_buffer);
        }
        APPEND(style_str);
    }

    /* Add other attributes (excluding width/height/style and internal srcset markers) */
    for (int i = 0; i < attrs->attr_count; i++) {
        const char *key = attrs->keys[i];
        const char *val = attrs->values[i];

        /* Skip width, height, style - we already processed them */
        if (strcmp(key, "width") == 0 || strcmp(key, "height") == 0 || strcmp(key, "style") == 0) {
            continue;
        }
        /* Skip internal @2x/@3x markers - used by attributes_to_html_for_image to emit srcset */
        if (strcmp(key, "data-srcset-2x") == 0 ||
            strcmp(key, "data-srcset-3x") == 0) {
            continue;
        }
        /* Skip picture/video format markers - used for picture/video element generation */
        if (strcmp(key, "data-srcset-webp") == 0 ||
            strcmp(key, "data-srcset-avif") == 0 ||
            strcmp(key, "data-video-webm") == 0 ||
            strcmp(key, "data-video-ogg") == 0 ||
            strcmp(key, "data-video-mp4") == 0 ||
            strcmp(key, "data-video-mov") == 0 ||
            strcmp(key, "data-video-m4v") == 0 ||
            strcmp(key, "data-apex-auto") == 0) {
            continue;
        }

        char attr_str[1024];
        if (first_attr) {
            snprintf(attr_str, sizeof(attr_str), "%s=\"%s\"", key, val);
            first_attr = false;
        } else {
            snprintf(attr_str, sizeof(attr_str), " %s=\"%s\"", key, val);
        }
        APPEND(attr_str);
    }

    #undef APPEND

    *p = '\0';
    return strdup(buffer);
}

/**
 * Check if a paragraph contains IAL (possibly with other content)
 * Returns true if IAL found, extracts attributes, and modifies text to remove IAL
 */
/**
 * Extract IAL from a PURE IAL paragraph (only contains "{: ...}")
 * This is ONLY for next-line block IAL that applies to the previous element.
 * Does NOT handle inline paragraph IAL - that's not supported in standard Kramdown.
 *
 * Example:
 *   Paragraph text.
 *
 *   {: #id .class}     <-- This is a pure IAL paragraph
 */
static bool extract_ial_from_paragraph(cmark_node *para, apex_attributes **attrs_out, ald_entry *alds) {
    if (cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) return false;

    /* Must have one text node, optionally followed by softbreak/linebreak (so "{: .lead}\n" is recognized) */
    cmark_node *text_node = cmark_node_first_child(para);
    if (!text_node) return false;
    if (cmark_node_get_type(text_node) != CMARK_NODE_TEXT) return false;
    cmark_node *after_text = cmark_node_next(text_node);
    if (after_text) {
        cmark_node_type after_type = cmark_node_get_type(after_text);
        if (after_type != CMARK_NODE_SOFTBREAK && after_type != CMARK_NODE_LINEBREAK) return false;
        if (cmark_node_next(after_text) != NULL) return false;
    }

    const char *text = cmark_node_get_literal(text_node);
    if (!text) return false;

    /* Trim leading whitespace */
    while (isspace((unsigned char)*text)) text++;
    if (*text == '\0') return false;

    /* Must start with {: or {# or {. */
    if (text[0] != '{') return false;
    char second_char = text[1];
    if (second_char != ':' && second_char != '#' && second_char != '.') return false;

    /* Find closing } */
    /* For {: format, skip {: (2 chars); for {# or {. format, skip { (1 char) */
    const char *close = (second_char == ':') ? strchr(text + 2, '}') : strchr(text + 1, '}');
    if (!close) return false;

    /* If there is non-whitespace after }, use only the IAL prefix (e.g. "{: .lead}\n</div>" -> extract "{: .lead}") */
    const char *after = close + 1;
    while (*after && isspace((unsigned char)*after)) after++;
    if (*after && *after != '\n') {
        /* Trailing content (e.g. </div>) - extract IAL from prefix only */
        size_t prefix_len = (size_t)(close + 1 - text);
        char *prefix = malloc(prefix_len + 1);
        if (!prefix) return false;
        memcpy(prefix, text, prefix_len);
        prefix[prefix_len] = '\0';
        bool ok = extract_ial_from_text(prefix, attrs_out, alds);
        free(prefix);
        return ok;
    }

    /* This is a pure IAL paragraph - extract attributes */
    return extract_ial_from_text(text, attrs_out, alds);
}

/* Drop a softbreak or hard break that only separated a removed IAL. */
static void ial_drop_break_before(cmark_node *node) {
    cmark_node *prev = cmark_node_previous(node);
    if (!prev) return;
    cmark_node_type prev_type = cmark_node_get_type(prev);
    if (prev_type == CMARK_NODE_SOFTBREAK || prev_type == CMARK_NODE_LINEBREAK) {
        cmark_node_unlink(prev);
        cmark_node_free(prev);
    }
}

/**
 * Handle span-level IAL (inline elements with attributes)
 * Example: [Link](url){: .class} or ![Image](img){: #id}
 *
 * The IAL applies to the immediately preceding inline element (link, image, emphasis, etc.)
 * IALs can appear inline within paragraphs, not just at the end.
 * This function processes IALs recursively to handle nested inline elements.
 */
static bool process_span_ial_in_container(cmark_node *container, ald_entry *alds) {
    cmark_node_type container_type = cmark_node_get_type(container);
    /* Only process paragraphs and inline elements that can contain other inline elements */
    if (container_type != CMARK_NODE_PARAGRAPH &&
        container_type != CMARK_NODE_STRONG &&
        container_type != CMARK_NODE_EMPH &&
        container_type != CMARK_NODE_LINK) {
        return false;
    }

    bool found_ial = false;

    /* Process all text nodes in the container to find IALs */
    /* Save next sibling before processing in case we need to unlink the node */
    for (cmark_node *child = cmark_node_first_child(container); child; ) {
        cmark_node *next = cmark_node_next(child);  /* Save next before potential modification */

        if (cmark_node_get_type(child) != CMARK_NODE_TEXT) {
            child = next;
            continue;
        }

        const char *text = cmark_node_get_literal(child);
        if (!text) {
            child = next;
            continue;
        }

        /* Look for IAL pattern: {: ... } or {#id .class} at the start (after optional whitespace) or at the end */
        const char *text_ptr = text;
        const char *ial_start = NULL;
        char second_char = 0;

        /* First, try at the start (after optional whitespace) */
        const char *start_ptr = text;
        while (*start_ptr && isspace((unsigned char)*start_ptr)) {
            start_ptr++;
        }

        if (start_ptr[0] == '{') {
            char sc = start_ptr[1];
            if (sc == ':' || sc == '#' || sc == '.') {
                ial_start = start_ptr;
                text_ptr = start_ptr;
                second_char = sc;
            }
        }

        /* If not found at start, try at the end (for inline IALs after elements) */
        if (!ial_start) {
            const char *end_ptr = strrchr(text, '{');
            if (end_ptr) {
                char sc = end_ptr[1];
                if (sc == ':' || sc == '#' || sc == '.') {
                    /* Check if this is at the end (only whitespace after closing brace) */
                    const char *close = strchr(end_ptr, '}');
                    if (close) {
                        const char *after = close + 1;
                        while (*after && isspace((unsigned char)*after)) after++;
                        if (!*after) {
                            /* IAL is at the end of the text node */
                            ial_start = end_ptr;
                            text_ptr = text; /* Keep original text_ptr for prefix calculation */
                            second_char = sc;
                        }
                    }
                }
            }
        }

        if (!ial_start) {
            child = next;
            continue;
        }

        const char *close = strchr(ial_start, '}');
        if (!close) {
            child = next;
            continue;
        }

        /* Extract attributes from IAL - we need a version that doesn't require it to be at end */
        apex_attributes *attrs = NULL;

        /* Parse IAL content directly (since we know it's valid IAL syntax) */
        /* For {: format, skip {: (2 chars); for {# or {. format, skip { (1 char) */
        const char *content_start = (second_char == ':') ? ial_start + 2 : ial_start + 1;
        int content_len = apex_ial_ptrdiff_to_int(close - content_start);

        if (content_len <= 0) {
            child = next;
            continue;
        }

        /* Copy content to work with */
        char buffer[2048];
        if (content_len >= (int)sizeof(buffer)) content_len = (int)sizeof(buffer) - 1;
        memcpy(buffer, content_start, content_len);
        buffer[content_len] = '\0';

        char *p = buffer;

        /* Skip leading whitespace */
        while (isspace((unsigned char)*p)) p++;
        if (!*p) {
            child = next;
            continue;
        }

        /* Extract first token */
        char *token_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '#' && *p != '.' && *p != '=') p++;

        apex_attributes *ald_attrs = NULL;
        const char *remaining_content = NULL;
        int remaining_len = 0;

        if (p > token_start) {
            /* Check if first token is a simple word (ALD reference) */
            char saved = *p;
            *p = '\0';

            bool is_ref = true;
            for (char *c = token_start; *c; c++) {
                if (*c == '#' || *c == '.' || *c == '=') {
                    is_ref = false;
                    break;
                }
            }

            *p = saved; /* Restore */

            if (is_ref && *token_start) {
                /* Trim the token */
                char *trimmed = token_start;
                while (isspace((unsigned char)*trimmed)) trimmed++;
                char *end = trimmed + strlen(trimmed) - 1;
                while (end > trimmed && isspace((unsigned char)*end)) *end-- = '\0';

                /* Look up ALD */
                if (*trimmed) {
                    ald_attrs = find_ald(alds, trimmed);
                }
            }
        }

        /* If we found an ALD, check for remaining content */
        if (ald_attrs) {
            /* Skip whitespace after token */
            while (*p && isspace((unsigned char)*p)) p++;

            if (*p) {
                /* There's remaining content - parse it as additional attributes */
                remaining_content = content_start + (p - buffer);
                remaining_len = content_len - apex_ial_ptrdiff_to_int(p - buffer);
            }
        }

        /* Parse additional attributes if any */
        apex_attributes *additional_attrs = NULL;
        if (remaining_content && remaining_len > 0) {
            additional_attrs = parse_ial_content(remaining_content, remaining_len);
        }

        /* Merge ALD with additional attributes */
        if (ald_attrs) {
            attrs = merge_attributes(ald_attrs, additional_attrs);
            if (additional_attrs) {
                apex_free_attributes(additional_attrs);
            }
        } else {
            /* No ALD found, parse as regular IAL */
            attrs = parse_ial_content(content_start, content_len);
        }

        if (!attrs) {
            child = next;
            continue;
        }

        /* Find the inline element immediately before this text node */
        cmark_node *target = NULL;
        cmark_node *prev = cmark_node_previous(child);

        /* Skip over any text nodes to find the actual inline element */
        while (prev) {
            cmark_node_type prev_type = cmark_node_get_type(prev);
            if (prev_type == CMARK_NODE_LINK ||
                prev_type == CMARK_NODE_IMAGE ||
                prev_type == CMARK_NODE_EMPH ||
                prev_type == CMARK_NODE_STRONG ||
                prev_type == CMARK_NODE_CODE) {
                target = prev;
                break;
            }
            /* If it's a text node, continue walking backwards */
            if (prev_type == CMARK_NODE_TEXT) {
                prev = cmark_node_previous(prev);
                continue;
            }
            /* If it's some other node type, stop - IAL can't apply to it */
            break;
        }

        if (!target) {
            /* No inline element found - if this is a paragraph and there is content before this IAL (so not a pure IAL-only paragraph), apply to the paragraph (block IAL without blank line).
             * Content before IAL can be: a previous sibling (e.g. text + softbreak + IAL text node), or text before IAL in this node (e.g. single node "Text\n{: .lead }"). */
            bool has_preceding_content = (cmark_node_previous(child) != NULL) || (ial_start > text);
            if (container_type == CMARK_NODE_PARAGRAPH && has_preceding_content) {
                /* IAL was at end of text - apply to this paragraph (e.g. "Text\n{: .lead }" on one block).
                 * Inside a blockquote, "> {: .class}" follows the paragraph, so the class stays on that paragraph.
                 * On the list marker line, the IAL belongs to the item. A later paragraph inside the item keeps its own class. */
                cmark_node *attr_node = container;
                cmark_node *parent = cmark_node_parent(container);
                if (parent && cmark_node_get_type(parent) == CMARK_NODE_ITEM &&
                    cmark_node_first_child(parent) == container) {
                    attr_node = parent;
                }
                char *attr_str = attributes_to_html(attrs);
                cmark_node_set_user_data(attr_node, attr_str);
                apex_free_attributes(attrs);

                /* Remove the IAL from the text node */
                size_t prefix_len = ial_start - text;
                const char *suffix = close + 1;
                size_t suffix_len = strlen(suffix);
                size_t new_len = prefix_len + suffix_len;
                char *new_text = NULL;

                if (new_len > 0) {
                    new_text = malloc(new_len + 1);
                    if (new_text) {
                        if (prefix_len > 0) memcpy(new_text, text, prefix_len);
                        if (suffix_len > 0)
                            strcpy(new_text + prefix_len, suffix);
                        else
                            new_text[prefix_len] = '\0';
                        if (prefix_len > 0 && suffix_len == 0) {
                            char *end = new_text + prefix_len - 1;
                            while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';
                        }
                        if (strlen(new_text) == 0) {
                            ial_drop_break_before(child);
                            cmark_node_unlink(child);
                            cmark_node_free(child);
                            free(new_text);
                            new_text = NULL;
                        } else {
                            cmark_node_set_literal(child, new_text);
                        }
                    }
                } else {
                    ial_drop_break_before(child);
                    cmark_node_unlink(child);
                    cmark_node_free(child);
                }
                if (new_text) free(new_text);
                found_ial = true;
            } else {
                apex_free_attributes(attrs);
            }
            child = next;
            continue;
        }

        /* Verify that the target is actually within this container (could be nested) */
        /* Walk up from target to see if we reach container */
        cmark_node *target_parent = cmark_node_parent(target);
        bool target_in_container = false;
        while (target_parent) {
            if (target_parent == container) {
                target_in_container = true;
                break;
            }
            /* If we reach a non-inline element, stop */
            cmark_node_type parent_type = cmark_node_get_type(target_parent);
            if (parent_type != CMARK_NODE_STRONG &&
                parent_type != CMARK_NODE_EMPH &&
                parent_type != CMARK_NODE_LINK &&
                parent_type != CMARK_NODE_PARAGRAPH) {
                break;
            }
            target_parent = cmark_node_parent(target_parent);
        }

        if (!target_in_container) {
            apex_free_attributes(attrs);
            child = next;
            continue;
        }

        /* Apply attributes to the target inline element */
        char *attr_str = attributes_to_html(attrs);
        cmark_node_set_user_data(target, attr_str);
        if (getenv("APEX_DEBUG_PIPELINE") && cmark_node_get_type(target) == CMARK_NODE_LINK) {
            fprintf(stderr, "[APEX_DEBUG] IAL applied to link (attrs: %.80s%s)\n",
                    attr_str ? attr_str : "(null)", attr_str && strlen(attr_str) > 80 ? "..." : "");
        }
        apex_free_attributes(attrs);

        /* Remove the IAL from the text node, preserving any text before/after it */
        size_t prefix_len;
        if (ial_start == start_ptr) {
            /* IAL was at the start - prefix is just whitespace before IAL */
            prefix_len = text_ptr - text;
        } else {
            /* IAL was at the end - prefix is everything before the IAL */
            prefix_len = ial_start - text;
        }
        const char *suffix = close + 1;  /* Text after IAL closing brace */

        /* Build new text: prefix (if any) + suffix (if any) */
        size_t suffix_len = strlen(suffix);
        size_t new_len = prefix_len + suffix_len;
        char *new_text = NULL;

        if (new_len > 0) {
            new_text = malloc(new_len + 1);
            if (new_text) {
                /* Copy prefix (leading whitespace before IAL) */
                if (prefix_len > 0) {
                    memcpy(new_text, text, prefix_len);
                }
                /* Copy suffix (text after IAL) */
                if (suffix_len > 0) {
                    strcpy(new_text + prefix_len, suffix);
                } else {
                    new_text[prefix_len] = '\0';
                }

                /* Trim trailing whitespace from prefix */
                if (prefix_len > 0 && suffix_len == 0) {
                    char *end = new_text + prefix_len - 1;
                    while (end >= new_text && isspace((unsigned char)*end)) {
                        *end-- = '\0';
                    }
                }

                /* Remove node if empty, otherwise update it */
                if (strlen(new_text) == 0) {
                    cmark_node_unlink(child);
                    cmark_node_free(child);
                    free(new_text);
                    new_text = NULL;
                    /* Don't update child here - use saved 'next' */
                } else {
                    cmark_node_set_literal(child, new_text);
                }
            }
        } else {
            /* No prefix and no suffix - remove the node */
            cmark_node_unlink(child);
            cmark_node_free(child);
            /* Don't update child here - use saved 'next' */
        }

        if (new_text) {
            free(new_text);
        }

        found_ial = true;
        /* Continue with next sibling (may be NULL if we unlinked) */
        child = next;
    }

    /* Recursively process inline elements that can contain other inline elements */
    /* Use a separate loop to avoid modifying the container while iterating */
    for (cmark_node *inline_child = cmark_node_first_child(container); inline_child; inline_child = cmark_node_next(inline_child)) {
        cmark_node_type child_type = cmark_node_get_type(inline_child);
        if (child_type == CMARK_NODE_STRONG ||
            child_type == CMARK_NODE_EMPH ||
            child_type == CMARK_NODE_LINK) {
            if (process_span_ial_in_container(inline_child, alds)) {
                found_ial = true;
            }
        }
    }

    return found_ial;
}

/**
 * Handle span-level IAL for paragraphs (wrapper for recursive function)
 */
static bool process_span_ial(cmark_node *para, ald_entry *alds) {
    if (cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) return false;
    return process_span_ial_in_container(para, alds);
}

/**
 * Extract IAL from heading text (inline syntax: ## Heading {: #id})
 * Headings may have multiple inline children (e.g. when "&" creates HTML_INLINE),
 * so we must check all children, not just the first.
 */
static bool extract_ial_from_heading(cmark_node *heading, apex_attributes **attrs_out, ald_entry *alds) {
    if (cmark_node_get_type(heading) != CMARK_NODE_HEADING) return false;

    /* Find the text node that contains the IAL - walk all children since "&" etc.
       can split content across multiple nodes (e.g. TEXT + HTML_INLINE + TEXT) */
    cmark_node *ial_node = NULL;
    const char *ial_start = NULL;

    for (cmark_node *child = cmark_node_first_child(heading); child; child = cmark_node_next(child)) {
        if (cmark_node_get_type(child) != CMARK_NODE_TEXT) continue;

        const char *text = cmark_node_get_literal(child);
        if (!text) continue;

        const char *brace = strrchr(text, '{');
        if (!brace) continue;

        char second_char = brace[1];
        if (second_char != ':' && second_char != '#' && second_char != '.') continue;

        const char *close = strchr(brace, '}');
        if (!close) continue;

        const char *after = close + 1;
        while (*after && isspace((unsigned char)*after)) after++;
        if (*after) continue;

        /* Found valid IAL - prefer the rightmost (last) one */
        ial_node = child;
        ial_start = brace;
    }

    if (!ial_node || !ial_start) return false;

    const char *text = cmark_node_get_literal(ial_node);
    if (!text) return false;

    /* Extract attributes */
    if (!extract_ial_from_text(ial_start, attrs_out, alds)) {
        return false;
    }

    /* Remove IAL from heading text */
    size_t prefix_len = ial_start - text;

    char *new_text = malloc(prefix_len + 1);
    if (!new_text) return false;

    if (prefix_len > 0) {
        memcpy(new_text, text, prefix_len);
        new_text[prefix_len] = '\0';

        /* Trim trailing whitespace */
        char *end = new_text + prefix_len - 1;
        while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';
    } else {
        /* This node was only IAL - leave empty string */
        new_text[0] = '\0';
    }

    cmark_node_set_literal(ial_node, new_text);
    free(new_text);
    return true;
}

/**
 * Check if a paragraph is ONLY an IAL (should be removed entirely).
 * Allows optional trailing softbreak/linebreak so "{: .lead}\n" is recognized when the parser adds a linebreak node.
 */
static bool is_pure_ial_paragraph(cmark_node *para) {
    if (cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) {
        return false;
    }

    cmark_node *text_node = cmark_node_first_child(para);
    if (!text_node) {
        return false;
    }
    if (cmark_node_get_type(text_node) != CMARK_NODE_TEXT) {
        return false;
    }
    cmark_node *after_text = cmark_node_next(text_node);
    if (after_text) {
        cmark_node_type after_type = cmark_node_get_type(after_text);
        if (after_type != CMARK_NODE_SOFTBREAK && after_type != CMARK_NODE_LINEBREAK) {
            return false;
        }
        if (cmark_node_next(after_text) != NULL) {
            return false;
        }
    }

    const char *text = cmark_node_get_literal(text_node);
    if (!text) {
        return false;
    }

    /* Trim leading whitespace */
    while (isspace((unsigned char)*text)) text++;

    /* Find end of text, trimming trailing whitespace including newlines */
    const char *text_end = text + strlen(text);
    while (text_end > text && isspace((unsigned char)*(text_end - 1))) {
        text_end--;
    }
    size_t text_len = text_end - text;

    /* Check if it's ONLY {: ... } or {#id .class} */
    if (text_len == 0 || text[0] != '{') return false;
    if (text_len < 2) return false;
    char second_char = text[1];
    if (second_char != ':' && second_char != '#' && second_char != '.') {
        return false;
    }

    /* For {: format, skip {: (2 chars); for {# or {. format, skip { (1 char) */
    const char *search_start = (second_char == ':') ? text + 2 : text + 1;
    if (search_start >= text_end) {
        return false;
    }
    const char *close = strchr(search_start, '}');
    if (!close) {
        return false;
    }
    if (close >= text_end) {
        return false;
    }

    /* Allow optional trailing content (e.g. "{: .lead}\n</div>" when IAL is followed by HTML in same block); still treat as pure IAL for previous block */
    return true;
}

/**
 * Process IAL for a single node
 * Check if node has inline IAL or if next sibling is IAL paragraph
 */
/**
 * Process IAL for a node
 * Returns the node to free (if any), or NULL
 * Caller must free the returned node after iteration is complete
 */
/* Copy attrs without one key="value" attribute. */
static char *ial_attrs_without(const char *attrs, const char *name) {
    size_t nlen = strlen(name);
    char *out = malloc(strlen(attrs) + 1);
    if (!out) return NULL;
    char *write = out;
    const char *p = attrs;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        bool drop = strncmp(p, name, nlen) == 0 && p[nlen] == '=' &&
                    (p[nlen + 1] == '"' || p[nlen + 1] == '\'');
        if (drop) {
            char quote = p[nlen + 1];
            p += nlen + 2;
            while (*p && *p != quote) p++;
            if (*p == quote) p++;
            continue;
        }
        while (*p && !isspace((unsigned char)*p)) p++;
        if (write != out) *write++ = ' ';
        memcpy(write, start, (size_t)(p - start));
        write += p - start;
    }
    *write = '\0';
    return out;
}

/* True when an HTML block is only a closing tag (and comments), so a
 * following IAL still belongs to the block before it. */
static bool ial_html_is_closer_only(cmark_node *node) {
    const char *html = cmark_node_get_literal(node);
    if (!html) return false;
    for (const char *p = html; *p; p++) {
        if (*p == '<' && p[1] && p[1] != '/' && p[1] != '!' && p[1] != '?') {
            return false;
        }
    }
    return true;
}

/* Insert IAL attributes into the first opening tag of an HTML block.
 * An existing class is extended instead of adding a second class attribute. */
static char *ial_html_with_attrs(const char *html, const char *attrs) {
    if (!html || !attrs || !*attrs) return NULL;

    const char *tag = NULL;
    for (const char *p = html; *p; p++) {
        if (*p == '<' && p[1] && p[1] != '/' && p[1] != '!' && p[1] != '?') {
            tag = p;
            break;
        }
    }
    if (!tag) return NULL;
    const char *gt = strchr(tag, '>');
    if (!gt) return NULL;

    const char *class_at = NULL;
    const char *class_val = NULL;
    const char *class_end = NULL;
    for (const char *p = tag + 1; p + 7 < gt; p++) {
        if ((p == tag + 1 || isspace((unsigned char)p[-1])) && strncmp(p, "class=\"", 7) == 0) {
            class_at = p;
            class_val = p + 7;
            class_end = memchr(class_val, '"', (size_t)(gt - class_val));
            break;
        }
    }

    const char *ial_class = NULL;
    const char *ial_class_end = NULL;
    const char *ial_at = strstr(attrs, "class=\"");
    if (ial_at && (ial_at == attrs || isspace((unsigned char)ial_at[-1]))) {
        ial_class = ial_at + 7;
        ial_class_end = strchr(ial_class, '"');
        if (!ial_class_end) ial_class = NULL;
    }

    char *extra = NULL;
    char *merged_class = NULL;
    if (class_at && class_end && ial_class) {
        size_t old_len = (size_t)(class_end - class_val);
        size_t add_len = (size_t)(ial_class_end - ial_class);
        merged_class = malloc(old_len + add_len + 2);
        if (!merged_class) return NULL;
        memcpy(merged_class, class_val, old_len);
        merged_class[old_len] = ' ';
        memcpy(merged_class + old_len + 1, ial_class, add_len);
        merged_class[old_len + 1 + add_len] = '\0';
        extra = ial_attrs_without(attrs, "class");
    } else {
        extra = strdup(attrs);
    }
    if (!extra) {
        free(merged_class);
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t extra_len = strlen(extra);
    size_t merged_len = merged_class ? strlen(merged_class) : 0;
    char *out = malloc(html_len + extra_len + merged_len + 2);
    if (!out) {
        free(extra);
        free(merged_class);
        return NULL;
    }
    char *write = out;
    if (merged_class && class_val && class_end) {
        memcpy(write, html, (size_t)(class_val - html));
        write += class_val - html;
        memcpy(write, merged_class, merged_len);
        write += merged_len;
        memcpy(write, class_end, (size_t)(gt - class_end));
        write += gt - class_end;
    } else {
        memcpy(write, html, (size_t)(gt - html));
        write += gt - html;
    }
    if (extra_len > 0) {
        *write++ = ' ';
        memcpy(write, extra, extra_len);
        write += extra_len;
    }
    memcpy(write, gt, html_len - (size_t)(gt - html) + 1);

    free(extra);
    free(merged_class);
    return out;
}

static cmark_node *process_node_ial(cmark_node *node, ald_entry *alds) {
    if (!node) return NULL;

    cmark_node_type type = cmark_node_get_type(node);

    /* Handle heading with inline IAL (## Heading {: #id}) */
    if (type == CMARK_NODE_HEADING) {
        apex_attributes *attrs = NULL;
        bool extracted = extract_ial_from_heading(node, &attrs, alds);
        if (extracted) {
            /* Store attributes in heading */
            char *attr_str = attributes_to_html(attrs);

            /* Merge with existing user_data if present */
            char *existing = (char *)cmark_node_get_user_data(node);
            if (existing) {
                /* Append to existing */
                char *combined = malloc(strlen(existing) + strlen(attr_str) + 1);
                if (combined) {
                    strcpy(combined, existing);
                    strcat(combined, attr_str);
                    cmark_node_set_user_data(node, combined);
                    free(attr_str);
                } else {
                    cmark_node_set_user_data(node, attr_str);
                }
            } else {
                cmark_node_set_user_data(node, attr_str);
            }

            apex_free_attributes(attrs);
            return NULL;  /* No node to free */
        }
        /* If no inline IAL, fall through to check for next-line IAL */
    }

    /* Handle span-level IAL (links, images, emphasis, etc. with inline attributes) */
    if (type == CMARK_NODE_PARAGRAPH) {
        if (process_span_ial(node, alds)) {
            return NULL;  /* Span IAL processed, no node to free */
        }
        /* No span IAL found, fall through to check for next-line IAL */
    }

    /* Any block can take an IAL on the following line. */
    if (type != CMARK_NODE_HEADING &&
        type != CMARK_NODE_PARAGRAPH &&
        type != CMARK_NODE_BLOCK_QUOTE &&
        type != CMARK_NODE_CODE_BLOCK &&
        type != CMARK_NODE_LIST &&
        type != CMARK_NODE_ITEM &&
        type != CMARK_NODE_TABLE &&
        type != CMARK_NODE_THEMATIC_BREAK &&
        type != CMARK_NODE_HTML_BLOCK) {
        return NULL;  /* No node to free */
    }


    /* A pure IAL paragraph is the marker, not a block that takes another IAL. */
    if (type == CMARK_NODE_PARAGRAPH && is_pure_ial_paragraph(node)) {
        return NULL;
    }

    /* Look at the next sibling for an IAL paragraph. A closing-tag HTML
     * block can sit between a paragraph and its IAL. */
    cmark_node *next = cmark_node_next(node);
    if (!next) {
        return NULL;  /* No node to free */
    }

    while (next && cmark_node_get_type(next) == CMARK_NODE_HTML_BLOCK &&
           ial_html_is_closer_only(next)) {
        next = cmark_node_next(next);
    }

    if (!next || cmark_node_get_type(next) != CMARK_NODE_PARAGRAPH) {
        return NULL;  /* No node to free */
    }

    /* Check if it's a pure IAL paragraph */
    if (is_pure_ial_paragraph(next)) {
        apex_attributes *attrs = NULL;
        if (extract_ial_from_paragraph(next, &attrs, alds)) {
            char *attr_str = attributes_to_html(attrs);
            apex_free_attributes(attrs);
            if (type == CMARK_NODE_HTML_BLOCK) {
                const char *literal = cmark_node_get_literal(node);
                char *merged = ial_html_with_attrs(literal, attr_str);
                free(attr_str);
                if (!merged) return NULL;
                cmark_node_set_literal(node, merged);
                free(merged);
            } else {
                cmark_node_set_user_data(node, attr_str);
            }

            /* Return node to be unlinked and freed after iteration completes */
            /* Don't unlink here - that invalidates the iterator */
            return next;  /* Return node to unlink and free after iteration */
        }
    }

    return NULL;  /* No node to free */
}

/**
 * Process IAL in AST
 */
void apex_process_ial_in_tree(cmark_node *node, ald_entry *alds) {
    if (!node) return;

    /* Collect nodes to unlink and free after iteration to avoid use-after-free */
    cmark_node **nodes_to_free = NULL;
    size_t free_count = 0;
    size_t free_capacity = 0;

    /* First pass: process IAL and collect nodes to remove */
    cmark_iter *iter = cmark_iter_new(node);
    cmark_event_type ev_type;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);

        /* Only process on ENTER events */
        if (ev_type == CMARK_EVENT_ENTER) {
            /* Process node and collect any nodes that need to be freed */
            cmark_node *node_to_free = process_node_ial(cur, alds);
            if (node_to_free) {
                bool already_queued = false;
                for (size_t i = 0; i < free_count; i++) {
                    if (nodes_to_free[i] == node_to_free) {
                        already_queued = true;
                        break;
                    }
                }
                if (already_queued) continue;
                /* Expand array if needed */
                if (free_count >= free_capacity) {
                    free_capacity = free_capacity ? free_capacity * 2 : 16;
                    cmark_node **new_array = realloc(nodes_to_free, free_capacity * sizeof(cmark_node*));
                    if (new_array) {
                        nodes_to_free = new_array;
                    } else {
                        /* If realloc fails, unlink and free immediately */
                        cmark_node_unlink(node_to_free);
                        cmark_node_free(node_to_free);
                        continue;
                    }
                }
                nodes_to_free[free_count++] = node_to_free;
            }
        }
    }

    cmark_iter_free(iter);

    /* Second pass: unlink and free collected nodes after iteration is complete */
    for (size_t i = 0; i < free_count; i++) {
        cmark_node_unlink(nodes_to_free[i]);
        cmark_node_free(nodes_to_free[i]);
    }
    free(nodes_to_free);
}

/**
 * Check if a line is a pure IAL (starts with {: or {# or {. and ends with })
 */
static bool is_ial_line(const char *line, size_t len) {
    const char *p = line;
    const char *end = line + len;

    /* Skip leading whitespace */
    while (p < end && isspace((unsigned char)*p)) p++;

    /* Must start with {: or {# or {. */
    if (p + 2 > end || p[0] != '{') return false;
    /* Leanpub index syntax {i: term} is not IAL */
    if (p[1] == 'i' && p[2] == ':') return false;
    char second_char = p[1];
    if (second_char != ':' && second_char != '#' && second_char != '.') return false;

    /* Find closing } */
    /* For {: format, skip {: (2 chars); for {# or {. format, skip { (1 char) */
    const char *search_start = (second_char == ':') ? p + 2 : p + 1;
    const char *close = memchr(search_start, '}', end - search_start);
    if (!close) return false;

    /* Check nothing substantial after the } */
    const char *after = close + 1;
    while (after < end && isspace((unsigned char)*after)) after++;

    return (after >= end);
}

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static int ial_looks_like_list_marker(const char *p) {
    if (!*p) return 0;
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return 0;
}

/** True if line is an indented code block (4+ spaces or tab at start). */
static bool ial_line_is_indented_code_block(const char *line, size_t len) {
    if (len == 0) return false;
    if (line[0] == '\t')
        return !ial_looks_like_list_marker(line + 1);
    if (len < 4 || line[0] != ' ' || line[1] != ' ' || line[2] != ' ' || line[3] != ' ')
        return false;
    const char *content = line + 4;
    while (content < line + len && *content == ' ') content++;
    return content < line + len && !ial_looks_like_list_marker(content);
}

/** True if line is a fenced code fence (``` or ~~~) with up to 3 leading spaces. */
static bool ial_line_is_fence(const char *line, size_t len) {
    const char *p = line;
    const char *end = line + len;
    int spaces = 0;
    while (p < end && *p == ' ' && spaces < 3) {
        p++;
        spaces++;
    }
    if (p + 2 >= end) return false;
    if (p[0] == '`' && p[1] == '`' && p[2] == '`') return true;
    if (p[0] == '~' && p[1] == '~' && p[2] == '~') return true;
    return false;
}

/**
 * Preprocess text to separate IAL markers from preceding content.
 * Kramdown allows IAL on the line immediately following content,
 * but cmark-gfm treats that as part of the same paragraph.
 * This inserts blank lines before IAL markers.
 */
char *apex_preprocess_ial(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    /* Worst case: we add a newline before every line */
    size_t capacity = text_len * 2 + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;

    char *out = output;
    const char *p = text;
    bool prev_line_was_content = false;
    bool prev_line_was_blank = true;  /* Start as if there was a blank line before */
    bool in_fenced_code = false;
    bool in_indented_code = false;

    while (*p) {
        /* Find end of current line */
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) {
            line_end = p + strlen(p);
        }

        size_t line_len = line_end - line_start;

        /* Check if this line is blank */
        bool is_blank = true;
        for (size_t i = 0; i < line_len; i++) {
            if (!isspace((unsigned char)line_start[i])) {
                is_blank = false;
                break;
            }
        }

        /* Check if this line is an IAL */
        bool is_ial = is_ial_line(line_start, line_len);

        if (ial_line_is_fence(line_start, line_len)) {
            in_fenced_code = !in_fenced_code;
        }
        if (!in_fenced_code) {
            if (ial_line_is_indented_code_block(line_start, line_len)) {
                in_indented_code = true;
            } else if (!is_blank) {
                in_indented_code = false;
            }
        }
        bool in_code = in_fenced_code || in_indented_code;

        /* Special case: Kramdown-style TOC marker "{:toc ...}".
         *
         * In Kramdown/Jekyll, a pure IAL paragraph containing only "{:toc}"
         * (optionally with additional parameters) is replaced with a
         * generated table of contents. We map this syntax to Apex's
         * existing TOC marker "<!--TOC ...-->" so it is handled by the
         * TOC extension.
         */
        bool handled_toc_marker = false;
        if (is_ial && !in_code) {
            const char *q = line_start;
            const char *end = line_start + line_len;

            /* Skip leading whitespace */
            while (q < end && isspace((unsigned char)*q)) q++;

            /* Must start with "{:" */
            if (q + 2 <= end && q[0] == '{' && q[1] == ':') {
                q += 2;

                /* Find closing '}' */
                const char *close = memchr(q, '}', (size_t)(end - q));
                if (close) {
                    /* Extract and trim inner content */
                    const char *inner_start = q;
                    const char *inner_end = close;
                    while (inner_start < inner_end && isspace((unsigned char)*inner_start)) {
                        inner_start++;
                    }
                    while (inner_end > inner_start && isspace((unsigned char)inner_end[-1])) {
                        inner_end--;
                    }

                    if (inner_start < inner_end) {
                        /* Check for leading "toc" (case-insensitive) */
                        const char *toc_start = inner_start;
                        const char *toc_end = toc_start + 3;
                        if ((size_t)(inner_end - inner_start) >= 3 &&
                            (toc_start[0] == 't' || toc_start[0] == 'T') &&
                            (toc_start[1] == 'o' || toc_start[1] == 'O') &&
                            (toc_start[2] == 'c' || toc_start[2] == 'C') &&
                            (toc_end == inner_end || isspace((unsigned char)*toc_end))) {
                            /* Everything after "toc" (including any whitespace) is
                             * treated as TOC options and passed through to the
                             * marker. This allows syntax like "{:toc max=3 min=2}". */
                            const char *options_start = toc_end;
                            while (options_start < inner_end &&
                                   isspace((unsigned char)*options_start)) {
                                options_start++;
                            }

                            /* If this is a TOC marker, optionally insert a blank
                             * line before it to keep block structure consistent
                             * with normal IAL handling. */
                            if (prev_line_was_content && !prev_line_was_blank) {
                                *out++ = '\n';
                            }

                            /* Build "<!--TOC [options]-->" */
                            const char *marker_prefix = "<!--TOC";
                            size_t prefix_len = strlen(marker_prefix);
                            memcpy(out, marker_prefix, prefix_len);
                            out += prefix_len;

                            if (options_start < inner_end) {
                                *out++ = ' ';
                                size_t opts_len = (size_t)(inner_end - options_start);
                                memcpy(out, options_start, opts_len);
                                out += opts_len;
                            }

                            *out++ = '-';
                            *out++ = '-';
                            *out++ = '>';

                            /* Preserve original newline if present */
                            if (*line_end == '\n') {
                                *out++ = '\n';
                            }

                            handled_toc_marker = true;
                        }
                    }
                }
            }
        }

        if (!handled_toc_marker) {
            /* If this is an IAL and previous line was content (not blank, not IAL),
             * insert a blank line before it */
            if (is_ial && prev_line_was_content && !prev_line_was_blank) {
                *out++ = '\n';
            }

            /* Copy the line */
            memcpy(out, line_start, line_len);
            out += line_len;

            /* Copy the newline if present */
            if (*line_end == '\n') {
                *out++ = '\n';
            }
        }

        /* Advance input pointer */
        p = (*line_end == '\n') ? line_end + 1 : line_end;

        /* Track state for next iteration */
        prev_line_was_blank = is_blank;
        prev_line_was_content = !is_blank && !is_ial;
    }

    *out = '\0';
    return output;
}

/**
 * URL encode a string (percent encoding)
 * Only encodes unsafe characters (space, control chars, non-ASCII, etc.)
 * Preserves valid URL characters like /, :, ?, #, etc.
 * Returns newly allocated string, caller must free
 */
static char *url_encode(const char *url) {
    if (!url) return NULL;

    /* Calculate size needed (worst case: 3 chars per byte) */
    size_t len = strlen(url);
    size_t capacity = len * 3 + 1;
    char *encoded = malloc(capacity);
    if (!encoded) return NULL;

    char *out = encoded;
    for (const char *p = url; *p; p++) {
        unsigned char c = (unsigned char)*p;
        /* Unreserved characters (always safe): A-Z, a-z, 0-9, -, _, ., ~ */
        /* Reserved characters that are safe in URL paths: /, :, ?, #, [, ], @, !, $, &, ', (, ), *, +, ,, ;, = */
        /* Also preserve % if it's part of already-encoded content */
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' ||
            c == '/' || c == ':' || c == '?' || c == '#' ||
            c == '[' || c == ']' || c == '@' || c == '!' ||
            c == '$' || c == '&' || c == '\'' || c == '(' ||
            c == ')' || c == '*' || c == '+' || c == ',' ||
            c == ';' || c == '=' || c == '%') {
            *out++ = c;
        } else {
            /* Encode unsafe characters: space, control chars, non-ASCII, etc. */
            snprintf(out, 4, "%%%02X", c);
            out += 3;
        }
    }
    *out = '\0';
    return encoded;
}

/**
 * Parse attributes from a string (similar to parse_ial_content but for image attributes)
 * Handles: width=300 style="float:left" "title"
 */
static apex_attributes *parse_image_attributes(const char *attr_str, int len) {
    apex_attributes *attrs = create_attributes();
    if (!attrs) return NULL;

    if (len <= 0 || !attr_str) return attrs;

    char buffer[2048];
    if (len >= (int)sizeof(buffer)) len = (int)sizeof(buffer) - 1;
    memcpy(buffer, attr_str, len);
    buffer[len] = '\0';

    char *p = buffer;
    while (*p) {
        /* Skip whitespace */
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Check for quoted title at the end ("title" or 'title') */
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            char *title_start = p;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p+1)) p++; /* Skip escaped char */
                p++;
            }
            if (*p == quote) {
                *p = '\0';
                add_attribute(attrs, "title", title_start);
                *p = quote;
                p++;
            }
            continue;
        }

        /* Check for key=value */
        char *key_start = p;
        while (*p && *p != '=' && !isspace((unsigned char)*p)) p++;

        if (*p == '=') {
            /* Found key=value */
            char saved = *p;
            *p = '\0';
            char *key = strdup(key_start);
            *p = saved;
            p++; /* Skip = */

            /* Parse value (could be quoted or unquoted) */
            char *value = NULL;
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                char *value_start = p;
                while (*p && *p != quote) {
                    if (*p == '\\' && *(p+1)) p++; /* Skip escaped char */
                    p++;
                }
                if (*p == quote) {
                    *p = '\0';
                    value = strdup(value_start);
                    *p = quote;
                    p++;
                }
            } else {
                /* Unquoted value */
                char *value_start = p;
                while (*p && !isspace((unsigned char)*p)) p++;
                char saved_val = *p;
                *p = '\0';
                value = strdup(value_start);
                *p = saved_val;
            }

            if (value) {
                add_attribute(attrs, key, value);
                free(value);
            }
            free(key);
            continue;
        }

        /* Check for bare @2x/@3x (retina srcset markers) */
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == '@' && key_start[1] == '2' && key_start[2] == 'x') {
            add_attribute(attrs, "data-srcset-2x", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == '@' && key_start[1] == '3' && key_start[2] == 'x') {
            add_attribute(attrs, "data-srcset-3x", "1");
            continue;
        }

        /* Check for bare webp/avif (picture srcset format markers) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'w' && key_start[1] == 'e' && key_start[2] == 'b' && key_start[3] == 'p') {
            add_attribute(attrs, "data-srcset-webp", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'a' && key_start[1] == 'v' && key_start[2] == 'i' && key_start[3] == 'f') {
            add_attribute(attrs, "data-srcset-avif", "1");
            continue;
        }

        /* Check for bare video format markers (webm, ogg, mp4, mov, m4v) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'w' && key_start[1] == 'e' && key_start[2] == 'b' && key_start[3] == 'm') {
            add_attribute(attrs, "data-video-webm", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'o' && key_start[1] == 'g' && key_start[2] == 'g') {
            add_attribute(attrs, "data-video-ogg", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == 'p' && key_start[2] == '4') {
            add_attribute(attrs, "data-video-mp4", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == 'o' && key_start[2] == 'v') {
            add_attribute(attrs, "data-video-mov", "1");
            continue;
        }
        if (p > key_start && (size_t)(p - key_start) == 3 &&
            key_start[0] == 'm' && key_start[1] == '4' && key_start[2] == 'v') {
            add_attribute(attrs, "data-video-m4v", "1");
            continue;
        }

        /* Check for bare auto (discover formats from filesystem) */
        if (p > key_start && (size_t)(p - key_start) == 4 &&
            key_start[0] == 'a' && key_start[1] == 'u' && key_start[2] == 't' && key_start[3] == 'o') {
            add_attribute(attrs, "data-apex-auto", "1");
            continue;
        }

        /* Unknown token, skip */
        p++;
    }

    return attrs;
}

/**
 * Build the @2x version of a URL.
 *
 * Rules:
 * - Never modify the domain portion of a URL (everything up to the first '/' after the scheme).
 * - Only insert "@2x" before a file-style extension in the path, e.g.:
 *     "img/icon.png" -> "img/icon@2x.png"
 *     "https://host/img/icon.png?x=1" -> "https://host/img/icon@2x.png?x=1"
 * - If there is no '.' in the path segment (before query/fragment), we skip srcset and
 *   return NULL rather than trying to synthesize a bogus "@2x" URL.
 *
 * Caller must free the returned string when non-NULL.
 */
static char *url_with_2x_suffix(const char *url) {
    if (!url || !*url) return NULL;

    /* Skip scheme (e.g. "http://", "https://") so we don't treat dots in the scheme. */
    const char *p = strstr(url, "://");
    if (p) {
        p += 3;  /* move past "://" */
    } else {
        p = url;
    }

    /* Find first '/' after scheme to separate domain from path. */
    const char *first_slash = strchr(p, '/');

    /* Path search should start at first_slash (if any), otherwise at url. */
    const char *path_start = first_slash ? first_slash : url;

    /* Identify end of path segment before query ('?') or fragment ('#'). */
    const char *qmark = strchr(path_start, '?');
    const char *hash  = strchr(path_start, '#');
    const char *path_end = NULL;
    if (qmark && hash) {
        path_end = (qmark < hash) ? qmark : hash;
    } else if (qmark) {
        path_end = qmark;
    } else if (hash) {
        path_end = hash;
    } else {
        path_end = url + strlen(url);
    }

    /* Search for last '.' in the path portion only (do not look in the domain). */
    const char *scan_start = path_start;
    const char *scan_end   = path_end;
    const char *last_dot   = NULL;
    for (const char *c = scan_start; c < scan_end; c++) {
        if (*c == '.') {
            last_dot = c;
        }
    }

    /* If there is no '.' in the path (i.e. no obvious extension), skip @2x. */
    if (!last_dot) {
        return NULL;
    }

    /* Build: prefix (up to dot) + "@2x" + suffix (from dot to end of URL). */
    size_t prefix_len = (size_t)(last_dot - url);
    size_t suffix_len = strlen(last_dot);
    char *out = malloc(prefix_len + 3 + suffix_len + 1); /* 3 for "@2x" */
    if (!out) return NULL;

    memcpy(out, url, prefix_len);
    memcpy(out + prefix_len, "@2x", 3);
    memcpy(out + prefix_len + 3, last_dot, suffix_len + 1); /* include NUL */
    return out;
}

/**
 * Replace the file extension in a URL with a new extension.
 * Uses same path logic as url_with_2x_suffix. Caller must free.
 * e.g. url_with_extension("img/icon.png?x=1", "webp") -> "img/icon.webp?x=1"
 */
static char *url_with_extension(const char *url, const char *new_ext) {
    if (!url || !*url || !new_ext) return NULL;

    const char *p = strstr(url, "://");
    if (p) p += 3;
    else p = url;

    const char *first_slash = strchr(p, '/');
    const char *path_start = first_slash ? first_slash : url;
    const char *qmark = strchr(path_start, '?');
    const char *hash  = strchr(path_start, '#');
    const char *path_end = (qmark && hash) ? ((qmark < hash) ? qmark : hash) :
                           qmark ? qmark : hash ? hash : url + strlen(url);

    const char *last_dot = NULL;
    for (const char *c = path_start; c < path_end; c++) {
        if (*c == '.') last_dot = c;
    }
    if (!last_dot) return NULL;

    size_t prefix_len = (size_t)(last_dot - url);
    size_t ext_len = strlen(new_ext);
    size_t tail_len = strlen(path_end);  /* from ? or # to end, or 0 */
    char *out = malloc(prefix_len + 1 + ext_len + tail_len + 1);
    if (!out) return NULL;

    memcpy(out, url, prefix_len);
    out[prefix_len] = '.';
    memcpy(out + prefix_len + 1, new_ext, ext_len + 1);
    if (tail_len > 0) {
        memcpy(out + prefix_len + 1 + ext_len, path_end, tail_len + 1);
    }
    return out;
}

/**
 * Check if URL has a video extension (whitelist: mp4, mov, webm, ogg, ogv, m4v)
 */
static bool is_video_url(const char *url) {
    if (!url || !*url) return false;
    const char *path_end = strchr(url, '?');
    if (!path_end) path_end = strchr(url, '#');
    if (!path_end) path_end = url + strlen(url);

    const char *last_dot = NULL;
    for (const char *c = url; c < path_end; c++) {
        if (*c == '.') last_dot = c;
    }
    if (!last_dot || last_dot <= url) return false;
    const char *ext = last_dot + 1;
    size_t ext_len = (size_t)(path_end - ext);
    if (ext_len == 0) return false;

    if (ext_len == 3 && strncasecmp(ext, "mp4", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "mov", 3) == 0) return true;
    if (ext_len == 4 && strncasecmp(ext, "webm", 4) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "ogg", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "ogv", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "m4v", 3) == 0) return true;
    return false;
}

/**
 * Check if attributes contain the @2x/@3x srcset markers
 */
static bool attrs_have_srcset_2x(apex_attributes *attrs) {
    if (!attrs) return false;
    return find_attribute_index(attrs, "data-srcset-2x") >= 0;
}

static bool attrs_have_srcset_3x(apex_attributes *attrs) {
    if (!attrs) return false;
    return find_attribute_index(attrs, "data-srcset-3x") >= 0;
}

static bool attrs_have_srcset_webp(apex_attributes *attrs) {
    if (!attrs) return false;
    return find_attribute_index(attrs, "data-srcset-webp") >= 0;
}

static bool attrs_have_srcset_avif(apex_attributes *attrs) {
    if (!attrs) return false;
    return find_attribute_index(attrs, "data-srcset-avif") >= 0;
}

static bool attrs_have_video_format(apex_attributes *attrs, const char *fmt) {
    if (!attrs) return false;
    char key[32];
    snprintf(key, sizeof(key), "data-video-%s", fmt);
    return find_attribute_index(attrs, key) >= 0;
}

/**
 * Build the @3x version of a URL.
 * Uses the same domain-safe rules as url_with_2x_suffix.
 */
static char *url_with_3x_suffix(const char *url) {
    if (!url || !*url) return NULL;

    /* Skip scheme (e.g. "http://", "https://") so we don't treat dots in the scheme. */
    const char *p = strstr(url, "://");
    if (p) {
        p += 3;  /* move past "://" */
    } else {
        p = url;
    }

    /* Find first '/' after scheme to separate domain from path. */
    const char *first_slash = strchr(p, '/');

    /* Path search should start at first_slash (if any), otherwise at url. */
    const char *path_start = first_slash ? first_slash : url;

    /* Identify end of path segment before query ('?') or fragment ('#'). */
    const char *qmark = strchr(path_start, '?');
    const char *hash  = strchr(path_start, '#');
    const char *path_end = NULL;
    if (qmark && hash) {
        path_end = (qmark < hash) ? qmark : hash;
    } else if (qmark) {
        path_end = qmark;
    } else if (hash) {
        path_end = hash;
    } else {
        path_end = url + strlen(url);
    }

    /* Search for last '.' in the path portion only (do not look in the domain). */
    const char *scan_start = path_start;
    const char *scan_end   = path_end;
    const char *last_dot   = NULL;
    for (const char *c = scan_start; c < scan_end; c++) {
        if (*c == '.') {
            last_dot = c;
        }
    }

    if (!last_dot) {
        return NULL;
    }

    size_t prefix_len = (size_t)(last_dot - url);
    size_t suffix_len = strlen(last_dot);
    char *out = malloc(prefix_len + 3 + suffix_len + 1); /* 3 for "@3x" */
    if (!out) return NULL;

    memcpy(out, url, prefix_len);
    memcpy(out + prefix_len, "@3x", 3);
    memcpy(out + prefix_len + 3, last_dot, suffix_len + 1); /* include NUL */
    return out;
}

/**
 * Build picture srcset string for a format (e.g. webp: "base.webp 1x, base@2x.webp 2x").
 * Uses base url and optional @2x/@3x. Caller must free.
 */
static char *build_picture_srcset(const char *url, const char *ext, bool want_2x, bool want_3x) {
    if (!url) return NULL;
    char *base = url_with_extension(url, ext);
    if (!base) return NULL;

    char *url_2x = NULL, *url_3x = NULL;
    if (want_2x) {
        char *base_2x = url_with_2x_suffix(url);
        if (base_2x) {
            url_2x = url_with_extension(base_2x, ext);
            free(base_2x);
        }
    }
    if (want_3x) {
        char *base_3x = url_with_3x_suffix(url);
        if (base_3x) {
            url_3x = url_with_extension(base_3x, ext);
            free(base_3x);
        }
    }

    size_t len = strlen(base) + 32;
    if (url_2x) len += strlen(url_2x) + 16;
    if (url_3x) len += strlen(url_3x) + 16;

    char *out = malloc(len);
    if (!out) {
        free(base);
        free(url_2x);
        free(url_3x);
        return NULL;
    }

    char *p = out;
    p += snprintf(p, len, "%s 1x", base);
    if (url_2x) p += snprintf(p, len - (size_t)(p - out), ", %s 2x", url_2x);
    if (url_3x) p += snprintf(p, len - (size_t)(p - out), ", %s 3x", url_3x);

    free(base);
    free(url_2x);
    free(url_3x);
    return out;
}

/**
 * Convert image attributes to HTML string, including srcset when @2x/@3x is present.
 * When data-srcset-2x/data-srcset-3x are in attrs, emits srcset="url 1x, url@2x 2x[, url@3x 3x]"
 * and omits the internal markers from the output attributes.
 * For video URLs, emits data-apex-replace-video with format markers for renderer replacement.
 * For webp/avif, emits data-apex-picture-* for renderer to wrap in <picture>.
 * Caller must free the returned string.
 */
static char *attributes_to_html_for_image(const char *url, apex_attributes *attrs) {
    if (!attrs) return strdup("");

    bool have_auto = (find_attribute_index(attrs, "data-apex-auto") >= 0);
    bool have_2x = attrs_have_srcset_2x(attrs);
    bool have_3x = attrs_have_srcset_3x(attrs);
    bool want_2x = have_2x || have_3x;
    bool want_3x = have_3x;

    /* Auto: emit marker for html_renderer to discover formats from filesystem */
    if (have_auto && url) {
        char *base = attributes_to_html(attrs);
        size_t base_len = base && *base ? strlen(base) : 0;
        size_t need = base_len + 64;
        char *result = malloc(need);
        if (result) {
            char *p = result;
            if (base_len > 0) {
                memcpy(p, base, base_len + 1);
                p += base_len;
            }
            p += sprintf(p, " data-apex-replace-auto=1");
            free(base);
            return result;
        }
        free(base);
    }

    /* Video URL: emit replacement markers for renderer to output <video> */
    if (url && is_video_url(url)) {
        char *base = attributes_to_html(attrs);
        size_t base_len = base && *base ? strlen(base) : 0;
        size_t need = base_len + 128;
        char *result = malloc(need);
        if (!result) {
            free(base);
            return strdup("");
        }
        char *p = result;
        if (base_len > 0) {
            memcpy(p, base, base_len + 1);
            p += base_len;
        }
        p += sprintf(p, " data-apex-replace-video=1");
        if (attrs_have_video_format(attrs, "webm")) p += sprintf(p, " data-apex-video-webm=1");
        if (attrs_have_video_format(attrs, "ogg")) p += sprintf(p, " data-apex-video-ogg=1");
        if (attrs_have_video_format(attrs, "mp4")) p += sprintf(p, " data-apex-video-mp4=1");
        if (attrs_have_video_format(attrs, "mov")) p += sprintf(p, " data-apex-video-mov=1");
        if (attrs_have_video_format(attrs, "m4v")) p += sprintf(p, " data-apex-video-m4v=1");
        free(base);
        return result;
    }

    /* Picture (webp/avif): emit data-apex-picture-* for renderer to wrap in <picture> */
    bool have_webp = attrs_have_srcset_webp(attrs);
    bool have_avif = attrs_have_srcset_avif(attrs);
    if ((have_webp || have_avif) && url) {
        char *webp_srcset = have_webp ? build_picture_srcset(url, "webp", want_2x, want_3x) : NULL;
        char *avif_srcset = have_avif ? build_picture_srcset(url, "avif", want_2x, want_3x) : NULL;

        char *base = attributes_to_html(attrs);
        size_t need = (base ? strlen(base) : 0) + 64;
        if (webp_srcset) need += strlen(webp_srcset) + 32;
        if (avif_srcset) need += strlen(avif_srcset) + 32;

        char *result = malloc(need);
        if (result) {
            char *p = result;
            if (base && *base) p += sprintf(p, "%s", base);
            if (webp_srcset) {
                p += sprintf(p, " data-apex-replace-picture=1 data-apex-picture-webp=\"%s\"", webp_srcset);
            }
            if (avif_srcset) {
                p += sprintf(p, " data-apex-picture-avif=\"%s\"", avif_srcset);
            }
            if (!webp_srcset && avif_srcset) {
                p += sprintf(p, " data-apex-replace-picture=1");
            }
            free(webp_srcset);
            free(avif_srcset);
        }
        free(base);
        if (result) return result;
    }

    char *url_2x = (want_2x && url) ? url_with_2x_suffix(url) : NULL;
    char *url_3x = (want_3x && url) ? url_with_3x_suffix(url) : NULL;

    char *base_attrs = attributes_to_html(attrs);
    if (!base_attrs) {
        free(url_2x);
        free(url_3x);
        return strdup("");
    }

    /* If no valid srcset needed, return base_attrs (attributes_to_html already omits data-srcset-*). */
    if (!want_2x || !url_2x) {
        free(url_2x);
        free(url_3x);
        return base_attrs;
    }

    /* Build srcset string. */
    size_t len = strlen(url) + strlen(url_2x) + 32;
    if (want_3x && url_3x) {
        len += strlen(url_3x) + 16;
    }
    char *srcset_attr = malloc(len);
    if (!srcset_attr) {
        free(base_attrs);
        free(url_2x);
        free(url_3x);
        return base_attrs;
    }

    if (want_3x && url_3x) {
        /* url 1x, url@2x 2x, url@3x 3x */
        snprintf(srcset_attr, len, " srcset=\"%s 1x, %s 2x, %s 3x\"", url, url_2x, url_3x);
    } else {
        /* url 1x, url@2x 2x */
        snprintf(srcset_attr, len, " srcset=\"%s 1x, %s 2x\"", url, url_2x);
    }

    free(url_2x);
    free(url_3x);

    /* Prepend srcset to base_attrs */
    size_t base_len = strlen(base_attrs);
    while (base_len > 0 && (base_attrs[base_len - 1] == ' ' || base_attrs[base_len - 1] == '\t')) {
        base_attrs[--base_len] = '\0';
    }
    size_t srcset_attr_len = strlen(srcset_attr);
    char *result = malloc(srcset_attr_len + (base_len ? base_len + 2 : 0) + 1);
    if (!result) {
        free(srcset_attr);
        return base_attrs;
    }
    char *w = result;
    memcpy(w, srcset_attr, srcset_attr_len);
    w += srcset_attr_len;
    if (base_len > 0) {
        *w++ = ' ';
        memcpy(w, base_attrs, base_len + 1);
    } else {
        *w = '\0';
    }
    free(srcset_attr);
    free(base_attrs);
    return result;
}

/**
 * Create a new image attribute entry (always creates a new entry, doesn't reuse)
 */
static image_attr_entry *create_image_attr_entry(image_attr_entry **list, const char *url, int index) {
    if (!list || !url) return NULL;

    /* Create new entry - always create new, don't reuse by URL */
    image_attr_entry *entry = calloc(1, sizeof(image_attr_entry));
    if (entry) {
        entry->url = strdup(url);
        entry->attrs = create_attributes();
        entry->index = index;
        entry->ref_name = NULL;
        entry->next = *list;
        *list = entry;
    }
    return entry;
}

/**
 * Create image attribute entry with reference name (for reference-style definitions)
 */
static image_attr_entry *create_image_attr_entry_with_ref(image_attr_entry **list, const char *url, const char *ref_name) {
    if (!list || !url) return NULL;

    image_attr_entry *entry = calloc(1, sizeof(image_attr_entry));
    if (entry) {
        entry->url = strdup(url);
        entry->attrs = create_attributes();
        entry->index = -1;
        entry->ref_name = ref_name ? strdup(ref_name) : NULL;
        entry->next = *list;
        *list = entry;
    }
    return entry;
}

/**
 * Free image attribute list
 */
void apex_free_image_attributes(image_attr_entry *img_attrs) {
    while (img_attrs) {
        image_attr_entry *next = img_attrs->next;
        free(img_attrs->url);
        free(img_attrs->ref_name);
        apex_free_attributes(img_attrs->attrs);
        free(img_attrs);
        img_attrs = next;
    }
}

/**
 * Convert attributes to inline image format (key=value, for use inside parentheses)
 * This converts IAL attributes (ID, classes) to key=value format that parse_image_attributes can understand
 */
static char *attributes_to_inline_format(apex_attributes *attrs) {
    if (!attrs || (!attrs->id && attrs->class_count == 0 && attrs->attr_count == 0)) {
        return strdup("");
    }

    char buffer[4096];
    char *p = buffer;
    size_t remaining = sizeof(buffer);

    #define APPEND_INLINE(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(p, str, len); \
            p += len; \
            remaining -= len; \
        } \
    } while(0)

    bool first = true;

    /* Add ID as id="value" */
    if (attrs->id) {
        char id_str[512];
        snprintf(id_str, sizeof(id_str), "%sid=\"%s\"", first ? "" : " ", attrs->id);
        first = false;
        APPEND_INLINE(id_str);
    }

    /* Add classes as class="class1 class2" */
    if (attrs->class_count > 0) {
        char class_str[1024];
        char *class_p = class_str;
        size_t class_remaining = sizeof(class_str);
        snprintf(class_p, class_remaining, "%sclass=\"", first ? "" : " ");
        class_p += strlen(class_p);
        class_remaining -= strlen(class_p);

        for (int i = 0; i < attrs->class_count; i++) {
            if (i > 0 && class_remaining > 0) {
                *class_p++ = ' ';
                class_remaining--;
            }
            size_t class_len = strlen(attrs->classes[i]);
            if (class_len < class_remaining) {
                memcpy(class_p, attrs->classes[i], class_len);
                class_p += class_len;
                class_remaining -= class_len;
            }
        }
        if (class_remaining > 0) {
            *class_p++ = '"';
            class_remaining--;
        }
        *class_p = '\0';
        first = false;
        APPEND_INLINE(class_str);
    }

    /* Add other attributes - format as key=value or key="value" */
    for (int i = 0; i < attrs->attr_count; i++) {
        char attr_str[1024];
        const char *val = attrs->values[i];
        /* Quote value if it contains spaces, semicolons, or special characters */
        bool need_quotes = (strchr(val, ' ') != NULL || strchr(val, ';') != NULL || strchr(val, '"') != NULL);
        if (need_quotes) {
            snprintf(attr_str, sizeof(attr_str), "%s%s=\"%s\"", first ? "" : " ", attrs->keys[i], val);
        } else {
            snprintf(attr_str, sizeof(attr_str), "%s%s=%s", first ? "" : " ", attrs->keys[i], val);
        }
        first = false;
        APPEND_INLINE(attr_str);
    }

    #undef APPEND_INLINE

    *p = '\0';
    return strdup(buffer);
}

/**
 * Convert attributes back to markdown attribute string (for expanding reference-style images)
 * Uses inline format (key=value) for compatibility with parse_image_attributes
 */
static char *attributes_to_markdown(apex_attributes *attrs) {
    return attributes_to_inline_format(attrs);
}

static bool reference_id_matches(
    const char *ref_id, const char *text, size_t text_len);

/**
 * Find image attribute entry by reference name
 */
static image_attr_entry *find_image_attr_by_ref(image_attr_entry *list, const char *ref_name) {
    for (image_attr_entry *entry = list; entry; entry = entry->next) {
        if (entry->ref_name && ref_name &&
            reference_id_matches(
                entry->ref_name, ref_name, strlen(ref_name))) {
            return entry;
        }
    }
    return NULL;
}

/**
 * Check if text starting at 'p' looks like the start of attributes (key=value pattern)
 */
static bool looks_like_attribute_start(const char *p, const char *end) {
    if (!p || p >= end) return false;

    /* Skip whitespace */
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end) return false;

    /* Look for key= pattern (attribute name followed by =) */
    const char *key_start = p;
    while (p < end && *p != '=' && *p != ' ' && *p != '\t' && *p != ')') {
        p++;
    }

    if (p < end && *p == '=' && p > key_start) {
        /* Found key= pattern - this looks like attributes */
        return true;
    }

    /* Also check for quoted title at the end ("title" or 'title') */
    if (p < end && (*p == '"' || *p == '\'')) {
        return true;
    }

    return false;
}

/**
 * Check if URL has a protocol (scheme followed by ://). Such URLs are assumed
 * already percent-encoded; we do not encode them. Scheme = letter then *( letter / digit / "+" / "-" / "." ) per URI spec.
 */
static bool has_protocol(const char *url) {
    if (!url || !*url) return false;
    const char *p = url;
    if (!(*p >= 'a' && *p <= 'z') && !(*p >= 'A' && *p <= 'Z'))
        return false;
    p++;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
           *p == '+' || *p == '-' || *p == '.')
        p++;
    return (p[0] == ':' && p[1] == '/' && p[2] == '/');
}

/**
 * Known attribute names (for splitting URL from attributes; avoids splitting on query params).
 */
/**
 * Check if attributes look like image-specific (width, height, style, class, id, rel, data-srcset-2x, data-srcset-3x).
 * Used to decide whether a reference definition with attributes should be treated as image ref.
 */
static bool attrs_are_image_specific(apex_attributes *attrs) {
    if (!attrs) return false;
    if (attrs->id || (attrs->class_count > 0)) return true;
    for (int i = 0; i < attrs->attr_count; i++) {
        const char *key = attrs->keys[i];
        if (strcmp(key, "width") == 0 || strcmp(key, "height") == 0 ||
            strcmp(key, "style") == 0 || strcmp(key, "class") == 0 ||
            strcmp(key, "id") == 0 || strcmp(key, "rel") == 0 ||
            strcmp(key, "data-srcset-2x") == 0 ||
            strcmp(key, "data-srcset-3x") == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Check if text at 'p' (after optional space) looks like the start of attributes:
 * a key with no spaces followed immediately by '=' (\w+=), or a quoted title (" or ').
 * Used to split URL from attributes without treating URL query params as attributes.
 */
static bool looks_like_attr_key_equals(const char *p, const char *end) {
    if (!p || p >= end) return false;
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end) return false;
    /* Quoted title at end */
    if (*p == '"' || *p == '\'') return true;
    /* key= : one or more word chars (letter, digit, underscore) then = */
    const char *key_start = p;
    while (p < end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                       (*p >= '0' && *p <= '9') || *p == '_'))
        p++;
    return (p > key_start && p < end && *p == '=');
}

static const char *ial_next_line(const char *p) {
    while (*p && *p != '\n' && *p != '\r') p++;
    if (*p == '\r' && p[1] == '\n') return p + 2;
    return *p ? p + 1 : p;
}

/* Length of a ``` or ~~~ fence run opening the line (after indentation and
 * blockquote markers), or 0 if the line is not a fence. */
static size_t ial_fence_run(const char *line, char *fence_char, const char **after_run) {
    const char *p = line;
    while (*p == ' ' || *p == '\t' || *p == '>') p++;
    if (*p != '`' && *p != '~') return 0;
    char c = *p;
    const char *run = p;
    while (*p == c) p++;
    if (p - run < 3) return 0;
    *fence_char = c;
    *after_run = p;
    return (size_t)(p - run);
}

/**
 * Number of bytes at pos that are code (a closed fenced block starting at
 * this line, a code span, or an unmatched backtick run) and must be copied
 * through untouched. Returns 0 when pos does not start code.
 */
static size_t ial_code_region_len(const char *base, const char *pos) {
    bool line_start = (pos == base || pos[-1] == '\n' || pos[-1] == '\r');
    if (line_start) {
        char fence_char;
        const char *after;
        size_t open_len = ial_fence_run(pos, &fence_char, &after);
        if (open_len) {
            bool valid = true;
            if (fence_char == '`') {
                for (const char *q = after; *q && *q != '\n' && *q != '\r'; q++) {
                    if (*q == '`') { valid = false; break; }
                }
            }
            if (valid) {
                const char *line = ial_next_line(pos);
                while (*line) {
                    char close_char;
                    const char *close_after;
                    size_t close_len = ial_fence_run(line, &close_char, &close_after);
                    if (close_len >= open_len && close_char == fence_char) {
                        while (*close_after == ' ' || *close_after == '\t') close_after++;
                        if (!*close_after || *close_after == '\n' || *close_after == '\r') {
                            return (size_t)(ial_next_line(line) - pos);
                        }
                    }
                    line = ial_next_line(line);
                }
                /* Unclosed fence: protect only the opening line */
                return (size_t)(ial_next_line(pos) - pos);
            }
        }
    }

    if (*pos != '`') return 0;

    size_t backslashes = 0;
    for (const char *b = pos; b > base && b[-1] == '\\'; b--) backslashes++;
    if (backslashes % 2) return 1;

    const char *p = pos;
    while (*p == '`') p++;
    size_t open_len = (size_t)(p - pos);

    /* Code spans may wrap lines but end at a blank line */
    while (*p) {
        if (*p == '\n' || *p == '\r') {
            const char *next = ial_next_line(p);
            const char *q = next;
            while (*q == ' ' || *q == '\t' || *q == '>') q++;
            if (!*q || *q == '\n' || *q == '\r') break;
            p = next;
            continue;
        }
        if (*p == '`') {
            const char *run = p;
            while (*p == '`') p++;
            if ((size_t)(p - run) == open_len) return (size_t)(p - pos);
            continue;
        }
        p++;
    }
    return open_len;
}

/* A reference definition label must open its line, after at most three
 * spaces of indentation (blockquote markers allowed). */
static bool ial_at_definition_start(const char *base, const char *pos) {
    const char *p = pos;
    for (;;) {
        int spaces = 0;
        while (p > base && p[-1] == ' ') {
            p--;
            spaces++;
        }
        if (spaces > 3) return false;
        if (p == base || p[-1] == '\n' || p[-1] == '\r') return true;
        if (p[-1] != '>') return false;
        p--;
    }
}

static bool ial_append_verbatim(char **buf, char **write, size_t *remaining,
                                const char *src, size_t len) {
    if (len >= *remaining) {
        size_t written = (size_t)(*write - *buf);
        size_t new_cap = (written + len + 1) * 2;
        char *grown = realloc(*buf, new_cap);
        if (!grown) return false;
        *buf = grown;
        *write = grown + written;
        *remaining = new_cap - written - 1;
    }
    memcpy(*write, src, len);
    *write += len;
    *remaining -= len;
    return true;
}

/**
 * Preprocess markdown to extract image attributes and URL-encode all link URLs
 */
/* Closing ')' of a link or image destination.
 * Quotes hide parentheses, and unquoted '(' must be balanced, so a title
 * like "Output options (Markdown)" does not end the destination early. */
static const char *ial_destination_closer(const char *start) {
    int depth = 1;
    bool in_quote = false;
    char quote = 0;
    const char *p = start;

    if (!start) return NULL;
    while (*p && *p != '\n' && *p != '\r') {
        if (in_quote) {
            if (*p == '\\' && p[1]) {
                p += 2;
                continue;
            }
            if (*p == quote) in_quote = false;
            p++;
            continue;
        }
        if (*p == '"' || *p == '\'') {
            in_quote = true;
            quote = *p++;
            continue;
        }
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            depth--;
            if (depth == 0) return p;
        }
        p++;
    }
    return NULL;
}

char *apex_preprocess_image_attributes(const char *text, image_attr_entry **img_attrs, apex_mode_t mode) {
    if (!text) return NULL;

    if (getenv("APEX_DEBUG_PIPELINE")) {
        size_t len = strlen(text);
        fprintf(stderr, "[APEX_DEBUG] preprocess_image_attributes in (len=%zu): %.300s%s\n",
                len, text, len > 300 ? "..." : "");
    }

    /* Check if we should do URL encoding */
    bool do_url_encoding = apex_mode_is_unified_family(mode) ||
                            mode == APEX_MODE_MULTIMARKDOWN ||
                            mode == APEX_MODE_KRAMDOWN;

    /* Check if we should process image attributes. */
    bool do_image_attrs = apex_mode_is_unified_family(mode) ||
                           mode == APEX_MODE_MULTIMARKDOWN ||
                           mode == APEX_MODE_GFM ||
                           mode == APEX_MODE_COMMONMARK;

    if (!do_url_encoding && !do_image_attrs) {
        /* Nothing to do */
        return NULL;
    }
    size_t text_len = strlen(text);
    size_t capacity = text_len * 3 + 1; /* Extra space for URL encoding expansion */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;
    image_attr_entry *local_img_attrs = NULL;
    /* Track position of images in document so inline attributes
     * are applied only to the specific image that declared them.
     * This must count all inline images, including those without
     * any attributes, to keep it in sync with the AST walk in
     * apex_apply_image_attributes.
     */
    int image_index = 0;

    while (*read) {
        size_t code_len = ial_code_region_len(text, read);
        if (code_len) {
            if (!ial_append_verbatim(&output, &write, &remaining, read, code_len)) {
                free(output);
                apex_free_image_attributes(local_img_attrs);
                return NULL;
            }
            read += code_len;
            continue;
        }

        /* Look for inline images: ![alt](url attributes) */
        if (*read == '!' && read[1] == '[') {
            const char *img_start = read;
            const char *check_pos = read + 2; /* After ![ */

            /* Find closing ] for alt text */
            const char *alt_end = strchr(check_pos, ']');
            if (alt_end && alt_end[1] == '(') {
                /* This is an inline image ![alt](url) - process it */
                const char *url_start = alt_end + 2; /* After ]( */
                const char *p = url_start;
                const char *url_end = NULL;
                const char *attr_start = NULL;
                const char *paren_end = NULL;

                /* Find the closing paren first. Parentheses inside a quoted
                 * title, and balanced parentheses in the URL, do not count. */
                paren_end = ial_destination_closer(url_start);
                if (!paren_end) {
                    /* Malformed - skip just the ! and continue */
                    read = img_start + 1;
                    continue;
                }

                /* Scan forward from url_start.
                 *
                 * For images (do_image_attrs == true) we treat everything after the
                 * first whitespace as an "attribute string" and hand it to
                 * parse_image_attributes(). That function understands both quoted
                 * titles ("title" or 'title') and key=value pairs (width=200),
                 * so constructs like:
                 *
                 *   ![alt](url "Title" width=200)
                 *   ![alt](url width=200)
                 *
                 * are all parsed correctly:
                 *   - "Title" -> title attribute
                 *   - width=200 -> width attribute
                 *
                 * We then remove that attribute string from the markdown we pass
                 * to cmark, leaving only the URL inside the parentheses so the
                 * core parser still sees a valid inline image.
                 *
                 * In modes without image attributes, we keep the original
                 * behavior and try to detect a standard Markdown title so that
                 * cmark can parse it.
                 */
                p = url_start;
                while (p < paren_end) {
                    if (*p == ' ' || *p == '\t') {
                        /* Found a space - check what follows */
                        const char *after_space = p;
                        while (after_space < paren_end && (*after_space == ' ' || *after_space == '\t')) after_space++;

                        if (after_space < paren_end) {
                            /* @2x/@3x: always split so we don't encode into URL */
                            if ((size_t)(paren_end - after_space) >= 3 &&
                                after_space[0] == '@' &&
                                ((after_space[1] == '2' && after_space[2] == 'x') ||
                                 (after_space[1] == '3' && after_space[2] == 'x')) &&
                                (after_space + 3 >= paren_end || isspace((unsigned char)after_space[3]))) {
                                attr_start = after_space;
                                url_end = p;
                                break;
                            }
                            if (do_image_attrs) {
                                /* For images with MMD6-style parentheses titles,
                                 * keep using the core parser's title handling:
                                 *
                                 *   ![Image](image.png (Parentheses title))
                                 *
                                 * In this case we should NOT treat the tail as
                                 * attributes, otherwise we lose the title and
                                 * leave a stray closing parenthesis in output.
                                 */
                                if (*after_space == '(') {
                                    url_end = p;  /* Let cmark handle the title */
                                    break;
                                }

                                /* For quoted titles only (no other attributes),
                                 * let cmark handle the title so it appears on the
                                 * img tag for caption logic and tooltips.
                                 * Must check BEFORE looks_like_attr_key_equals,
                                 * which returns true for '"' and would treat
                                 * the title as attributes.
                                 */
                                if (*after_space == '"' || *after_space == '\'') {
                                    char qc = *after_space;
                                    const char *tail = after_space + 1;
                                    while (tail < paren_end && *tail != qc) tail++;
                                    if (tail < paren_end) tail++; /* skip closing quote */
                                    while (tail < paren_end && (*tail == ' ' || *tail == '\t')) tail++;
                                    if (tail == paren_end) {
                                        url_end = p;  /* Let cmark handle the title */
                                        break;
                                    }
                                }

                                /* For everything else, treat the tail as an
                                 * attribute string (including quoted title).
                                 */
                                attr_start = after_space;
                                url_end = p;
                                break;
                            } else {
                                /* No image attributes: preserve standard Markdown
                                 * title parsing so cmark can handle it.
                                 */
                                /* Check if it's a quoted title */
                                if (*after_space == '"' || *after_space == '\'') {
                                    /* Found a title - URL ends before this space */
                                    url_end = p;
                                    break;
                                }

                                /* Check if it's a parentheses title: space followed by '(' */
                                if (*after_space == '(') {
                                    /* This is a title in parentheses - URL ends before the space */
                                    url_end = p;
                                    break;
                                }
                            }
                        }
                    }
                    p++;
                }

                /* If no title or attributes found, URL goes to closing paren */
                if (!url_end) {
                    url_end = paren_end;
                }

                /* If image attributes disabled and we didn't split on known attributes, treat everything as URL */
                if (!do_image_attrs && !attr_start) {
                    attr_start = NULL;
                    url_end = paren_end;
                }

                if (url_end && url_end > url_start) {
                    /* Extract URL */
                    size_t url_len = url_end - url_start;
                    char *url = malloc(url_len + 1);
                    if (url) {
                        memcpy(url, url_start, url_len);
                        url[url_len] = '\0';

                        /* Extract attributes if present (from do_image_attrs split or known-attribute split) */
                        apex_attributes *attrs = NULL;
                        if (attr_start && attr_start < paren_end) {
                            size_t attr_len = paren_end - attr_start;
                            attrs = parse_image_attributes(attr_start, apex_ial_size_to_int(attr_len));
                        }

                        /* Check for IAL syntax after closing paren: {#id .class} or { width=50% } */
                        const char *ial_end_pos = NULL;
                        const char *after_paren = paren_end + 1;
                        /* Skip whitespace before checking for IAL, but keep original position for skipping */
                        const char *check_pos = after_paren;
                        while (check_pos[0] == ' ' || check_pos[0] == '\t') check_pos++;
                        if (do_image_attrs && check_pos[0] == '{') {
                            /* Find the closing brace */
                            const char *ial_end = strchr(check_pos + 1, '}');
                            if (ial_end) {
                                char second_char = check_pos[1];
                                /* Check if it's a valid IAL format: {: or {# or {. or { (with space/attributes) */
                                bool is_ial = false;
                                const char *content_start = NULL;

                                if (second_char == ':' || second_char == '#' || second_char == '.') {
                                    /* Kramdown/Pandoc IAL format: {: or {# or {. */
                                    is_ial = true;
                                    content_start = (second_char == ':') ? check_pos + 2 : check_pos + 1;
                                } else if (second_char == ' ' || second_char == '\t' ||
                                          (second_char >= 'a' && second_char <= 'z') ||
                                          (second_char >= 'A' && second_char <= 'Z')) {
                                    /* Pandoc-style: { width=50% } or {key=val} */
                                    is_ial = true;
                                    content_start = check_pos + 1;
                                }

                                if (is_ial && content_start) {
                                    int content_len = apex_ial_ptrdiff_to_int(ial_end - content_start);
                                    if (content_len > 0) {
                                        /* Try parsing as IAL first (handles #id .class key=val) */
                                        apex_attributes *ial_attrs = parse_ial_content(content_start, content_len);
                                        if (!ial_attrs || (ial_attrs->attr_count == 0 && !ial_attrs->id && ial_attrs->class_count == 0)) {
                                            /* If IAL parsing didn't work, try as image attributes (handles width=50%) */
                                            if (ial_attrs) apex_free_attributes(ial_attrs);
                                            ial_attrs = parse_image_attributes(content_start, content_len);
                                        }

                                        if (ial_attrs && (ial_attrs->attr_count > 0 || ial_attrs->id || ial_attrs->class_count > 0)) {
                                            /* Merge IAL attributes with existing attributes */
                                            if (attrs) {
                                                apex_attributes *merged = merge_attributes(attrs, ial_attrs);
                                                apex_free_attributes(attrs);
                                                apex_free_attributes(ial_attrs);
                                                attrs = merged;
                                            } else {
                                                attrs = ial_attrs;
                                            }
                                        } else if (ial_attrs) {
                                            apex_free_attributes(ial_attrs);
                                        }
                                        /* Always skip IAL syntax even if parsing failed, to prevent it appearing in output */
                                        ial_end_pos = ial_end;
                                    } else {
                                        /* Empty IAL - still skip it */
                                        ial_end_pos = ial_end;
                                    }
                                } else {
                                    /* Not a valid IAL format, but if it looks like one (starts with { and ends with }), skip it anyway */
                                    /* This handles edge cases where parsing might fail but we still want to skip the syntax */
                                    ial_end_pos = ial_end;
                                }
                            }
                        }

                        /*
                         * Bear stores image metadata in an adjacent HTML
                         * comment. Parse and merge it, but leave the source
                         * bytes untouched so cmark preserves the comment.
                         * Gated on do_image_attrs so modes that only enter
                         * the preprocessor for URL encoding (Kramdown) do
                         * not pick up Bear attributes.
                         */
                        if (do_image_attrs) {
                            const char *after_attributes =
                                ial_end_pos ? ial_end_pos + 1 : after_paren;
                            const char *line_end = after_attributes;
                            while (*line_end &&
                                   *line_end != '\n' && *line_end != '\r') {
                                line_end++;
                            }
                            const char *bear_comment_start = NULL;
                            const char *bear_comment_end = NULL;
                            apex_attributes *bear_attrs =
                                parse_adjacent_bear_attrs(
                                    after_attributes,
                                    line_end,
                                    &bear_comment_start,
                                    &bear_comment_end);
                            if (bear_attrs) {
                                if (attrs) {
                                    apex_attributes *merged =
                                        merge_attributes(attrs, bear_attrs);
                                    apex_free_attributes(attrs);
                                    apex_free_attributes(bear_attrs);
                                    attrs = merged;
                                } else {
                                    attrs = bear_attrs;
                                }
                            }
                        }

                        /* URL ending in .* means auto-discover formats (same as auto attribute) */
                        bool url_is_wildcard = (url_len >= 2 && url[url_len - 2] == '.' && url[url_len - 1] == '*');
                        if (url_is_wildcard && do_image_attrs) {
                            if (!attrs) attrs = create_attributes();
                            if (attrs) add_attribute(attrs, "data-apex-auto", "1");
                        }

                        /* URL encode the URL only when enabled and URL has no known protocol (http/https/file/x-marked) */
                        bool skip_encode = has_protocol(url);
                        char *encoded_url = (do_url_encoding && !skip_encode) ? url_encode(url) : strdup(url);
                        if (encoded_url) {
                            /* Store attributes with URL - create entry when we have attrs, or when URL is a video (needs replacement), or when URL is wildcard (.*) */
                            image_attr_entry *entry = NULL;
                            if (attrs || is_video_url(url) || url_is_wildcard) {
                                /* Use the running image_index so attributes are
                                 * bound to the correct inline image position,
                                 * even when some images have no attributes.
                                 */
                                entry = create_image_attr_entry(&local_img_attrs, encoded_url, image_index);
                                if (entry && attrs) {
                                    /* Copy attributes (don't merge) */
                                    for (int i = 0; i < attrs->attr_count; i++) {
                                        add_attribute(entry->attrs, attrs->keys[i], attrs->values[i]);
                                    }
                                    if (attrs->id) {
                                        entry->attrs->id = strdup(attrs->id);
                                    }
                                    for (int i = 0; i < attrs->class_count; i++) {
                                        add_class(entry->attrs, attrs->classes[i]);
                                    }
                                }
                            }

                            /* Write the image syntax up to URL */
                            size_t prefix_len = url_start - img_start;
                            if (prefix_len < remaining) {
                                memcpy(write, img_start, prefix_len);
                                write += prefix_len;
                                remaining -= prefix_len;
                            }

                            /* Write encoded URL */
                            size_t encoded_len = strlen(encoded_url);
                            if (encoded_len < remaining) {
                                memcpy(write, encoded_url, encoded_len);
                                write += encoded_len;
                                remaining -= encoded_len;
                            } else {
                                /* Buffer too small, expand */
                                size_t written = write - output;
                                capacity = (written + encoded_len + 1) * 2;
                                char *new_output = realloc(output, capacity);
                                if (!new_output) {
                                    free(output);
                                    free(url);
                                    free(encoded_url);
                                    if (attrs) apex_free_attributes(attrs);
                                    apex_free_image_attributes(local_img_attrs);
                                    return NULL;
                                }
                                output = new_output;
                                write = output + written;
                                remaining = capacity - written;
                                memcpy(write, encoded_url, encoded_len);
                                write += encoded_len;
                                remaining -= encoded_len;
                            }

                            /* Write the rest: when we split URL from attributes (do_image_attrs or known-attribute),
                             * omit the attribute tail so cmark only sees ![alt](url). Otherwise preserve tail for cmark.
                             */
                            const char *rest_start = url_end;
                            const char *rest_end = paren_end;
                            if (attr_start && attr_start < paren_end) {
                                /* Drop everything from attr_start onward */
                                rest_end = attr_start;
                            }

                            while (rest_start < rest_end) {
                                if (remaining > 0) {
                                    *write++ = *rest_start++;
                                    remaining--;
                                } else {
                                    /* Buffer too small, need to expand */
                                    size_t written = write - output;
                                    size_t rest_len = paren_end - rest_start;
                                    capacity = (written + rest_len + 1) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(output);
                                        free(url);
                                        free(encoded_url);
                                        if (attrs) apex_free_attributes(attrs);
                                        apex_free_image_attributes(local_img_attrs);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }
                            }

                            /* Write closing ) */
                            if (remaining > 0 && paren_end && *paren_end == ')') {
                                *write++ = ')';
                                remaining--;

                                /* Skip IAL if it was found and processed */
                                if (ial_end_pos) {
                                    /* Skip from after_paren (which includes any whitespace before IAL) to after the closing brace */
                                    /* ial_end_pos points to the closing '}', so we skip to after it */
                                    read = ial_end_pos + 1;
                                } else {
                                    read = paren_end + 1;
                                }
                            } else {
                                /* No closing paren found, but still need to skip IAL if present */
                                if (ial_end_pos) {
                                    read = ial_end_pos + 1;
                                } else {
                                    read = paren_end;
                                }
                            }

                            /* We successfully processed an inline image (with or
                             * without attributes). Bump image_index so subsequent
                             * images get distinct positions that match the AST.
                             */
                            image_index++;

                            free(encoded_url);
                            if (attrs) apex_free_attributes(attrs);
                        }
                        free(url);
                        continue;
                    }
                } else {
                    /* Malformed inline image - skip just the ! and continue */
                    read = img_start + 1;
                    continue;
                }
            } else {
                /* Not an inline image - might be reference-style ![ref][id] */
                /* Pass through unchanged by copying the ![ */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                    if (remaining > 0 && *read == '[') {
                        *write++ = *read++;
                        remaining--;
                    }
                } else {
                    read++;
                }
                continue;
            }
        }

        /* Look for reference-style link definitions: [ref]: url attributes */
        /* Process to URL-encode URLs and extract image attributes if present */
        if (*read == '[' && ial_at_definition_start(text, read)) {
            const char *ref_start = read;
            const char *ref_end = strchr(ref_start, ']');
            if (ref_end && ref_end[1] == ':' && (ref_end[2] == ' ' || ref_end[2] == '\t')) {
                /* Found [ref]: */
                const char *url_start = ref_end + 2;
                /* Skip whitespace */
                while (*url_start && (*url_start == ' ' || *url_start == '\t')) url_start++;

                const char *p = url_start;
                const char *url_end = NULL;
                const char *attr_start = NULL;

                /* Find the end of the line first */
                const char *line_end = p;
                while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
                const char *physical_line_end = line_end;
                const char *bear_comment_start = NULL;
                const char *bear_comment_end = NULL;
                const char *bear_output_start = NULL;
                apex_attributes *bear_attrs = NULL;

                /* Pre-scan for a candidate Bear comment so title/attribute
                 * parsing below does not consume it. Adjacency to the true
                 * end of the definition is verified after that parsing.
                 */
                if (do_image_attrs) {
                    const char *bear_candidate = strstr(url_start, "<!--");
                    while (bear_candidate &&
                           bear_candidate < physical_line_end) {
                        bear_attrs = parse_adjacent_bear_attrs(
                            bear_candidate,
                            physical_line_end,
                            &bear_comment_start,
                            &bear_comment_end);
                        if (bear_attrs) break;
                        bear_candidate = strstr(bear_candidate + 1, "<!--");
                    }
                    if (bear_attrs) {
                        bear_output_start = bear_comment_start;
                        while (bear_output_start > url_start &&
                               (bear_output_start[-1] == ' ' ||
                                bear_output_start[-1] == '\t')) {
                            bear_output_start--;
                        }
                        line_end = bear_output_start;
                    }
                }

                /* For reference definitions, we need to check for:
                 * 1. Image attributes (if do_image_attrs): key=value pattern
                 * 2. Title: quoted string ("title" or 'title')
                 * We should stop at whichever comes first (attributes or title)
                 */
                p = url_start;
                while (p < line_end) {
                    if (*p == ' ' || *p == '\t') {
                        const char *after_space = p;
                        while (after_space < line_end && (*after_space == ' ' || *after_space == '\t')) after_space++;

                        if (after_space < line_end) {
                            /* Space + key= or bare @2x/@3x: split so attributes are applied (regardless of do_image_attrs) */
                            if (looks_like_attr_key_equals(after_space, line_end)) {
                                attr_start = after_space;
                                url_end = p;
                                break;
                            }
                            /* Bare @2x/@3x (retina srcset) - not \w+= but we treat as attribute */
                            if ((size_t)(line_end - after_space) >= 3 &&
                                after_space[0] == '@' &&
                                ((after_space[1] == '2' && after_space[2] == 'x') ||
                                 (after_space[1] == '3' && after_space[2] == 'x')) &&
                                (after_space + 3 >= line_end || isspace((unsigned char)after_space[3]))) {
                                attr_start = after_space;
                                url_end = p;
                                break;
                            }
                            /* Check if it's a quoted title */
                            if (*after_space == '"' || *after_space == '\'') {
                                /* Found a title - URL ends before this space */
                                url_end = p;
                                break;
                            }

                            /* Check if it's a parentheses title: space followed by '(' */
                            if (*after_space == '(') {
                                /* This is a title in parentheses - URL ends before the space */
                                url_end = p;
                                break;
                            }

                            /* Check if it's attributes (for images) */
                            /* Check for IAL syntax: { */
                            if (do_image_attrs && *after_space == '{') {
                                /* Found IAL syntax - URL ends before this space */
                                attr_start = after_space;
                                url_end = p;
                                break;
                            }
                            /* Check for key= pattern (inline attributes) */
                            if (do_image_attrs && looks_like_attribute_start(after_space, line_end)) {
                                /* This looks like attributes */
                                attr_start = after_space;
                                url_end = p;
                                break;
                            }
                        }
                    }
                    p++;
                }

                if (!url_end) {
                    url_end = line_end; /* URL ends at newline (no title or attributes found) */
                }

                if (url_end > url_start) {
                    /* Extract URL */
                    size_t url_len = url_end - url_start;
                    char *url = malloc(url_len + 1);
                    if (url) {
                        memcpy(url, url_start, url_len);
                        url[url_len] = '\0';

                        /* Extract attributes if present (from do_image_attrs or known-attribute split) */
                        apex_attributes *attrs = NULL;
                        const char *title_end = NULL;
                        char *title_text = NULL;
                        bool found_ial = false;
                        if (attr_start) {
                            const char *attr_end = p;
                            while (attr_end < line_end && *attr_end != '\n' && *attr_end != '\r') attr_end++;
                            size_t attr_len = attr_end - attr_start;
                            attrs = parse_image_attributes(attr_start, apex_ial_size_to_int(attr_len));
                            title_end = attr_end;
                        } else if (url_end < line_end) {
                            /* Check if there's a title after the URL */
                            const char *after_url = url_end;
                            while (after_url < line_end && (*after_url == ' ' || *after_url == '\t')) after_url++;

                            /* Check for quoted title */
                            if (after_url < line_end && (*after_url == '"' || *after_url == '\'')) {
                                char quote = *after_url;
                                const char *title_start = after_url + 1;
                                const char *title_close = title_start;
                                while (title_close < line_end && *title_close != quote) {
                                    if (*title_close == '\\' && title_close + 1 < line_end) title_close++;
                                    title_close++;
                                }
                                if (title_close < line_end && *title_close == quote) {
                                    /* Extract title text */
                                    size_t title_len = title_close - title_start;
                                    if (title_len > 0) {
                                        title_text = malloc(title_len + 1);
                                        if (title_text) {
                                            memcpy(title_text, title_start, title_len);
                                            title_text[title_len] = '\0';
                                        }
                                    }
                                    title_end = title_close + 1;
                                }
                            }

                            /* Check for IAL after title: {#id .class} or { width=50% } */
                            if (title_end && do_image_attrs && title_end < line_end) {
                                const char *after_title = title_end;
                                while (after_title < line_end && (*after_title == ' ' || *after_title == '\t')) after_title++;

                                if (after_title < line_end && *after_title == '{') {
                                    /* Find the closing brace */
                                    const char *ial_end = strchr(after_title + 1, '}');
                                    if (ial_end && ial_end <= line_end) {
                                        char second_char = after_title[1];
                                        /* Check if it's a valid IAL format: {: or {# or {. or { (with space/attributes) */
                                        bool is_ial = false;
                                        const char *content_start = NULL;

                                        if (second_char == ':' || second_char == '#' || second_char == '.') {
                                            /* Kramdown/Pandoc IAL format: {: or {# or {. */
                                            is_ial = true;
                                            content_start = (second_char == ':') ? after_title + 2 : after_title + 1;
                                        } else if (second_char == ' ' || second_char == '\t' ||
                                                  (second_char >= 'a' && second_char <= 'z') ||
                                                  (second_char >= 'A' && second_char <= 'Z')) {
                                            /* Pandoc-style: { width=50% } or {key=val} */
                                            is_ial = true;
                                            content_start = after_title + 1;
                                        }

                                        if (is_ial && content_start) {
                                            int content_len = apex_ial_ptrdiff_to_int(ial_end - content_start);
                                            if (content_len > 0) {
                                                /* Try parsing as IAL first (handles #id .class key=val) */
                                                apex_attributes *ial_attrs = parse_ial_content(content_start, content_len);
                                                if (!ial_attrs || (ial_attrs->attr_count == 0 && !ial_attrs->id && ial_attrs->class_count == 0)) {
                                                    /* If IAL parsing didn't work, try as image attributes (handles width=50%) */
                                                    if (ial_attrs) apex_free_attributes(ial_attrs);
                                                    ial_attrs = parse_image_attributes(content_start, content_len);
                                                }

                                                if (ial_attrs && (ial_attrs->attr_count > 0 || ial_attrs->id || ial_attrs->class_count > 0)) {
                                                    /* Create or merge with existing attributes */
                                                    if (!attrs) {
                                                        attrs = ial_attrs;
                                                    } else {
                                                        apex_attributes *merged = merge_attributes(attrs, ial_attrs);
                                                        apex_free_attributes(attrs);
                                                        apex_free_attributes(ial_attrs);
                                                        attrs = merged;
                                                    }
                                                    title_end = ial_end + 1; /* Update end position to skip IAL */
                                                    found_ial = true;
                                                } else if (ial_attrs) {
                                                    apex_free_attributes(ial_attrs);
                                                    title_end = ial_end + 1;
                                                    found_ial = true;
                                                } else {
                                                    /* Even if parsing failed, skip the IAL syntax to prevent it appearing in output */
                                                    title_end = ial_end + 1;
                                                    found_ial = true;
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            /* Check for MultiMarkdown-style attributes after title:
                             * [id]: url "Title" class=center width=300
                             * These are parsed with the same image-attribute parser used for inline images.
                             */
                            if (title_end && do_image_attrs && title_end < line_end) {
                                const char *after_title_attrs = title_end;
                                while (after_title_attrs < line_end &&
                                       (*after_title_attrs == ' ' || *after_title_attrs == '\t')) {
                                    after_title_attrs++;
                                }

                                /* If there's remaining content and it doesn't start an IAL block,
                                 * treat it as a sequence of key=value attributes (MultiMarkdown style).
                                 */
                                if (after_title_attrs < line_end && *after_title_attrs != '{') {
                                    const char *attr_end = line_end;
                                    size_t attr_len = attr_end - after_title_attrs;
                                    apex_attributes *mmd_attrs = parse_image_attributes(after_title_attrs, (int)attr_len);

                                    if (mmd_attrs &&
                                        (mmd_attrs->attr_count > 0 || mmd_attrs->id || mmd_attrs->class_count > 0)) {
                                        if (!attrs) {
                                            attrs = mmd_attrs;
                                        } else {
                                            apex_attributes *merged = merge_attributes(attrs, mmd_attrs);
                                            apex_free_attributes(attrs);
                                            apex_free_attributes(mmd_attrs);
                                            attrs = merged;
                                        }
                                        /* We consumed the rest of the line as attributes */
                                        title_end = line_end;
                                        found_ial = true;
                                    } else if (mmd_attrs) {
                                        apex_free_attributes(mmd_attrs);
                                    }
                                }
                            }

                            /* Also check for IAL directly after URL if no title was found */
                            if (!title_end && do_image_attrs && url_end < line_end) {
                                const char *after_url = url_end;
                                while (after_url < line_end && (*after_url == ' ' || *after_url == '\t')) after_url++;

                                if (after_url < line_end && *after_url == '{') {
                                    /* Find the closing brace */
                                    const char *ial_end = strchr(after_url + 1, '}');
                                    if (ial_end && ial_end <= line_end) {
                                        char second_char = after_url[1];
                                        bool is_ial = false;
                                        const char *content_start = NULL;

                                        if (second_char == ':' || second_char == '#' || second_char == '.') {
                                            is_ial = true;
                                            content_start = (second_char == ':') ? after_url + 2 : after_url + 1;
                                        } else if (second_char == ' ' || second_char == '\t' ||
                                                  (second_char >= 'a' && second_char <= 'z') ||
                                                  (second_char >= 'A' && second_char <= 'Z')) {
                                            is_ial = true;
                                            content_start = after_url + 1;
                                        }

                                        if (is_ial && content_start) {
                                            int content_len = apex_ial_ptrdiff_to_int(ial_end - content_start);
                                            if (content_len > 0) {
                                                apex_attributes *ial_attrs = parse_ial_content(content_start, content_len);
                                                if (!ial_attrs || (ial_attrs->attr_count == 0 && !ial_attrs->id && ial_attrs->class_count == 0)) {
                                                    if (ial_attrs) apex_free_attributes(ial_attrs);
                                                    ial_attrs = parse_image_attributes(content_start, content_len);
                                                }

                                                if (ial_attrs && (ial_attrs->attr_count > 0 || ial_attrs->id || ial_attrs->class_count > 0)) {
                                                    attrs = ial_attrs;
                                                    title_end = ial_end + 1;
                                                    found_ial = true;
                                                } else if (ial_attrs) {
                                                    apex_free_attributes(ial_attrs);
                                                    title_end = ial_end + 1;
                                                    found_ial = true;
                                                } else {
                                                    title_end = ial_end + 1;
                                                    found_ial = true;
                                                }
                                            } else {
                                                title_end = ial_end + 1;
                                            }
                                        } else {
                                            title_end = ial_end + 1;
                                        }
                                    }
                                }
                            }
                        }

                        if (bear_attrs) {
                            /* Enforce adjacency: re-scan from the true end of
                             * the URL, title, IAL, or MMD attributes so only
                             * spaces or tabs may precede the Bear comment.
                             * The URL itself ends at its first whitespace,
                             * matching cmark link destinations.
                             */
                            const char *after_construct;
                            if (title_end) {
                                after_construct = title_end;
                            } else {
                                after_construct = url_start;
                                while (after_construct < url_end &&
                                       *after_construct != ' ' &&
                                       *after_construct != '\t') {
                                    after_construct++;
                                }
                            }
                            const char *adjacent_start = NULL;
                            const char *adjacent_end = NULL;
                            apex_attributes *adjacent_attrs =
                                parse_adjacent_bear_attrs(
                                    after_construct,
                                    physical_line_end,
                                    &adjacent_start,
                                    &adjacent_end);
                            apex_free_attributes(bear_attrs);
                            bear_attrs = adjacent_attrs;
                            if (bear_attrs) {
                                bear_comment_start = adjacent_start;
                                bear_comment_end = adjacent_end;
                                bear_output_start = adjacent_start;
                                while (bear_output_start > after_construct &&
                                       (bear_output_start[-1] == ' ' ||
                                        bear_output_start[-1] == '\t')) {
                                    bear_output_start--;
                                }
                            } else {
                                bear_comment_start = NULL;
                                bear_comment_end = NULL;
                                bear_output_start = NULL;
                            }
                        }

                        if (bear_attrs) {
                            if (attrs) {
                                apex_attributes *merged =
                                    merge_attributes(attrs, bear_attrs);
                                apex_free_attributes(attrs);
                                apex_free_attributes(bear_attrs);
                                attrs = merged;
                            } else {
                                attrs = bear_attrs;
                            }
                            bear_attrs = NULL;
                        }

                        /* Extract reference name */
                        size_t ref_name_len = ref_end - ref_start - 1; /* Exclude [ and ] */
                        char *ref_name = malloc(ref_name_len + 1);
                        if (ref_name) {
                            memcpy(ref_name, ref_start + 1, ref_name_len);
                            ref_name[ref_name_len] = '\0';
                            /* Trim whitespace from reference name */
                            char *p = ref_name;
                            while (*p && isspace((unsigned char)*p)) p++;
                            if (p > ref_name) {
                                memmove(ref_name, p, strlen(p) + 1);
                            }
                            p = ref_name + strlen(ref_name) - 1;
                            while (p >= ref_name && isspace((unsigned char)*p)) {
                                *p = '\0';
                                p--;
                            }
                        }

                        /* Detect footnote-style reference: [^id]: ... */
                        bool is_footnote_ref = false;
                        if (ref_name && ref_name_len > 0) {
                            const char *name_p = ref_name;
                            while (*name_p == ' ' || *name_p == '\t') name_p++;
                            if (*name_p == '^') {
                                is_footnote_ref = true;
                            }
                        }

                        /* Footnote definitions should not have their "URL" (the footnote text)
                         * percent-encoded. Copy the line as-is and skip URL encoding.
                         */
                        if (is_footnote_ref) {
                            /* Write the entire definition line unchanged */
                            size_t line_len = physical_line_end - ref_start;
                            if (*physical_line_end == '\n') {
                                line_len++; /* Include newline */
                            }

                            if (line_len > remaining) {
                                size_t written = write - output;
                                size_t new_capacity = (written + line_len + 1) * 2;
                                char *new_output = realloc(output, new_capacity);
                                if (!new_output) {
                                    free(output);
                                    free(url);
                                    free(ref_name);
                                    if (attrs) apex_free_attributes(attrs);
                                    apex_free_image_attributes(local_img_attrs);
                                    return NULL;
                                }
                                output = new_output;
                                write = output + written;
                                remaining = new_capacity - written;
                            }

                            memcpy(write, ref_start, line_len);
                            write += line_len;
                            remaining -= line_len;

                            /* Advance read past this line (including newline if present) */
                            const char *next = physical_line_end;
                            if (*next == '\n') {
                                next++;
                            } else if (*next == '\r') {
                                if (next[1] == '\n') {
                                    next += 2;
                                } else {
                                    next++;
                                }
                            }
                            read = next;

                            free(url);
                            free(ref_name);
                            if (title_text) free(title_text);
                            if (attrs) apex_free_attributes(attrs);
                            continue;
                        }

                        /* URL encode the URL only when enabled and URL has no known protocol */
                        bool skip_encode_ref = has_protocol(url);
                        char *encoded_url = (do_url_encoding && !skip_encode_ref) ? url_encode(url) : strdup(url);
                        if (encoded_url) {
                            bool has_bear_attrs = (bear_comment_start != NULL);
                            bool has_image_attrs = (attrs != NULL && attrs_are_image_specific(attrs));
                            /* If has image-specific attributes (width, height, etc.), store with reference name.
                             * Don't create entry for link refs with only a title (e.g. [ref]: url "title"). */
                            if (ref_name) {
                                if (has_bear_attrs || has_image_attrs ||
                                    (do_image_attrs && found_ial)) {
                                    image_attr_entry *entry = create_image_attr_entry_with_ref(&local_img_attrs, encoded_url, ref_name);
                                    if (entry) {
                                        if (attrs) {
                                            /* Copy attributes (don't merge) */
                                            for (int i = 0; i < attrs->attr_count; i++) {
                                                add_attribute(entry->attrs, attrs->keys[i], attrs->values[i]);
                                            }
                                            if (attrs->id) {
                                                entry->attrs->id = strdup(attrs->id);
                                            }
                                            for (int i = 0; i < attrs->class_count; i++) {
                                                add_class(entry->attrs, attrs->classes[i]);
                                            }
                                        }
                                        /* Add the markdown title unless Bear
                                         * metadata already supplied one. */
                                        if (title_text &&
                                            find_attribute_index(
                                                entry->attrs, "title") < 0) {
                                            add_attribute(entry->attrs, "title", title_text);
                                        }
                                        /* Entry created - will be used for expansion */
                                    }
                                }
                            }
                            /* If this reference definition has image-specific attributes, remove it so we expand and apply attrs */
                            bool created_entry =
                                (ref_name &&
                                 (has_bear_attrs || has_image_attrs ||
                                  (do_image_attrs && found_ial)));
                            bool should_remove = created_entry;

                            if (should_remove) {
                                /* Reference definitions with attributes are removed from output (like ALDs) */
                                /* Preserve Bear metadata as one standalone raw HTML comment. */
                                bool wrote_bear_comment = false;
                                if (bear_output_start && bear_comment_end) {
                                    size_t comment_len =
                                        (size_t)(bear_comment_end - bear_output_start);
                                    if (comment_len + 2 > remaining) {
                                        size_t written = write - output;
                                        size_t new_capacity =
                                            (written + comment_len + 3) * 2;
                                        char *new_output =
                                            realloc(output, new_capacity);
                                        if (!new_output) {
                                            free(output);
                                            free(url);
                                            free(encoded_url);
                                            free(ref_name);
                                            if (title_text) free(title_text);
                                            if (attrs) apex_free_attributes(attrs);
                                            apex_free_image_attributes(local_img_attrs);
                                            return NULL;
                                        }
                                        output = new_output;
                                        write = output + written;
                                        remaining = new_capacity - written;
                                    }
                                    memcpy(write, bear_output_start, comment_len);
                                    write += comment_len;
                                    remaining -= comment_len;
                                    wrote_bear_comment = true;
                                }
                                free(ref_name);
                                if (title_text) free(title_text);
                                /* A preserved comment keeps the original line
                                 * ending so the next block stays separate.
                                 * Otherwise the whole line is removed.
                                 */
                                const char *p = physical_line_end;
                                if (*p == '\n') {
                                    if (wrote_bear_comment && remaining > 0) {
                                        *write++ = '\n';
                                        remaining--;
                                    }
                                    p++;
                                } else if (*p == '\r') {
                                    if (p[1] == '\n') {
                                        if (wrote_bear_comment &&
                                            remaining >= 2) {
                                            *write++ = '\r';
                                            *write++ = '\n';
                                            remaining -= 2;
                                        }
                                        p += 2;
                                    } else {
                                        if (wrote_bear_comment &&
                                            remaining > 0) {
                                            *write++ = '\r';
                                            remaining--;
                                        }
                                        p++;
                                    }
                                }
                                read = p;
                            } else {
                                /* Write back the reference definition with encoded URL (so cmark can resolve it) */
                                /* Write the reference up to URL */
                                size_t prefix_len = url_start - ref_start;
                                if (prefix_len < remaining) {
                                    memcpy(write, ref_start, prefix_len);
                                    write += prefix_len;
                                    remaining -= prefix_len;
                                }

                                /* Write encoded URL */
                                size_t encoded_len = strlen(encoded_url);
                                if (encoded_len < remaining) {
                                    memcpy(write, encoded_url, encoded_len);
                                    write += encoded_len;
                                    remaining -= encoded_len;
                                } else {
                                    size_t written = write - output;
                                    capacity = (written + encoded_len + 1) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(output);
                                        free(url);
                                        free(encoded_url);
                                        free(ref_name);
                                        if (attrs) apex_free_attributes(attrs);
                                        apex_free_image_attributes(local_img_attrs);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                    memcpy(write, encoded_url, encoded_len);
                                    write += encoded_len;
                                    remaining -= encoded_len;
                                }

                                /* Write the rest (title if present, but skip IAL if it was processed) */
                                const char *rest_end = title_end ? title_end : line_end;
                                const char *rest_start = url_end;
                                while (rest_start < rest_end) {
                                    if (remaining > 0) {
                                        *write++ = *rest_start++;
                                        remaining--;
                                    } else {
                                        size_t written = write - output;
                                        size_t rest_len = rest_end - rest_start;
                                        capacity = (written + rest_len + 1) * 2;
                                        char *new_output = realloc(output, capacity);
                                        if (!new_output) {
                                            free(output);
                                            free(url);
                                            free(encoded_url);
                                            free(ref_name);
                                            if (attrs) apex_free_attributes(attrs);
                                            apex_free_image_attributes(local_img_attrs);
                                            return NULL;
                                        }
                                        output = new_output;
                                        write = output + written;
                                        remaining = capacity - written;
                                    }
                                }

                                /* Advance read past the processed portion so
                                 * any remaining line text (for example a
                                 * rejected comment) is copied by the main
                                 * loop. */
                                const char *p = title_end ? title_end : line_end;

                                /* Write newline */
                                if (*p == '\n' && remaining > 0) {
                                    *write++ = *p++;
                                    remaining--;
                                } else if (*p == '\n') {
                                    p++;
                                } else if (*p == '\r') {
                                    if (p[1] == '\n' && remaining >= 2) {
                                        *write++ = *p++;
                                        *write++ = *p++;
                                        remaining -= 2;
                                    } else if (remaining > 0) {
                                        *write++ = *p++;
                                        remaining--;
                                    } else {
                                        p++;
                                    }
                                }

                                read = p;
                                free(ref_name);
                            }
                            free(encoded_url);
                            if (attrs) apex_free_attributes(attrs);
                        }
                        free(url);
                        continue;
                    }
                }

                /* URL extraction failed or was empty; release any pre-scanned
                 * Bear attributes that were never merged. */
                if (bear_attrs) {
                    apex_free_attributes(bear_attrs);
                    bear_attrs = NULL;
                }
            }
        }

        /* Look for regular links: [text](url) or [text](url "title") - URL encode only the URL */
        /* Skip when link text starts with "![": that's [![image]...], process as image when we hit '!' */
        if (*read == '[' && (read == text || read[-1] != '!')) {
            const char *link_start = read;
            const char *link_text_end = strchr(link_start, ']');
            if (link_text_end && link_text_end[1] == '(' &&
                !(link_text_end > link_start + 2 && link_start[1] == '!' && link_start[2] == '[')) {
                /* Found [text]\( and not [![image]...] */
                const char *url_start = link_text_end + 2; /* After ]( */
                const char *p = url_start;
                const char *url_end = NULL;
                const char *paren_end = NULL;

                /* Find the closing paren first. A quoted title may contain
                 * parentheses; stop at a newline when the destination never closes. */
                paren_end = ial_destination_closer(url_start);
                if (!paren_end) {
                    p = url_start;
                    while (*p && *p != '\n' && *p != '\r') p++;
                    paren_end = p;
                }

                /* Scan forward looking for titles: "title", 'title', or (title) */
                /* Key insight: (title) has a space before the '(', while URL parentheses don't */
                p = url_start;
                while (p < paren_end) {
                    if (*p == ' ' || *p == '\t') {
                        /* Found a space - check what follows */
                        const char *after_space = p;
                        while (after_space < paren_end && (*after_space == ' ' || *after_space == '\t')) after_space++;

                        if (after_space < paren_end) {
                            /* Check if it's a quoted title */
                            if (*after_space == '"' || *after_space == '\'') {
                                /* Found a title - URL ends before this space */
                                url_end = p;
                                break;
                            }

                            /* Check if it's a parentheses title: space followed by '(' */
                            if (*after_space == '(') {
                                /* This is a title in parentheses - URL ends before the space */
                                url_end = p;
                                break;
                            }
                        }
                    }
                    p++;
                }

                /* If no title found, URL goes to closing paren */
                if (!url_end) {
                    url_end = paren_end;
                }

                if (url_end > url_start) {
                    /* Extract URL */
                    size_t url_len = url_end - url_start;
                    char *url = malloc(url_len + 1);
                    if (url) {
                        memcpy(url, url_start, url_len);
                        url[url_len] = '\0';

                        /* URL encode (if enabled) */
                        char *encoded_url = do_url_encoding ? url_encode(url) : strdup(url);
                        if (encoded_url) {
                            /* Write link prefix */
                            size_t prefix_len = url_start - link_start;
                            if (prefix_len < remaining) {
                                memcpy(write, link_start, prefix_len);
                                write += prefix_len;
                                remaining -= prefix_len;
                            }

                            /* Write encoded URL */
                            size_t encoded_len = strlen(encoded_url);
                            if (encoded_len < remaining) {
                                memcpy(write, encoded_url, encoded_len);
                                write += encoded_len;
                                remaining -= encoded_len;
                            } else {
                                size_t written = write - output;
                                capacity = (written + encoded_len + 1) * 2;
                                char *new_output = realloc(output, capacity);
                                if (!new_output) {
                                    free(output);
                                    free(url);
                                    free(encoded_url);
                                    apex_free_image_attributes(local_img_attrs);
                                    return NULL;
                                }
                                output = new_output;
                                write = output + written;
                                remaining = capacity - written;
                                memcpy(write, encoded_url, encoded_len);
                                write += encoded_len;
                                remaining -= encoded_len;
                            }

                            /* Write the rest (title if present) */
                            const char *rest_start = url_end;
                            while (rest_start < paren_end) {
                                if (remaining > 0) {
                                    *write++ = *rest_start++;
                                    remaining--;
                                } else {
                                    /* Buffer too small, need to expand */
                                    size_t written = write - output;
                                    size_t rest_len = paren_end - rest_start;
                                    capacity = (written + rest_len + 1) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(output);
                                        free(url);
                                        free(encoded_url);
                                        apex_free_image_attributes(local_img_attrs);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }
                            }

                            /* Write closing paren */
                            if (remaining > 0 && paren_end && *paren_end == ')') {
                                *write++ = ')';
                                remaining--;
                                read = paren_end + 1;
                            } else if (paren_end) {
                                read = paren_end;
                            } else {
                                read = p;
                            }
                            free(encoded_url);
                        }
                        free(url);
                        continue;
                    }
                }
            }
        }

        /* Regular character - copy as-is */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            /* Expand buffer */
            size_t written = write - output;
            capacity = (written + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                apex_free_image_attributes(local_img_attrs);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';

    if (getenv("APEX_DEBUG_PIPELINE") && local_img_attrs) {
        fprintf(stderr, "[APEX_DEBUG] image_attr_entries:\n");
        for (image_attr_entry *e = local_img_attrs; e; e = e->next) {
            bool has_2x = attrs_have_srcset_2x(e->attrs);
            fprintf(stderr,
                    "  - url=\"%s\" index=%d ref=\"%s\" has_2x=%s\n",
                    e->url ? e->url : "(null)",
                    e->index,
                    e->ref_name ? e->ref_name : "",
                    has_2x ? "yes" : "no");
        }
    }

    /* Second pass: expand reference-style images that have attributes */
    /* We need to expand ![ref][img1] to ![ref](url attributes) for definitions with attributes */
    /* Run when we have ref_name entries (from do_image_attrs or known-attribute split on ref defs) */
    bool has_ref_entries = false;
    for (image_attr_entry *e = local_img_attrs; e; e = e->next) {
        if (e->ref_name) { has_ref_entries = true; break; }
    }
    if ((do_image_attrs && local_img_attrs) || has_ref_entries) {
        char *expanded_output = malloc(strlen(output) * 3 + 1);
        if (expanded_output) {
            const char *read2 = output;
            char *write2 = expanded_output;
            size_t remaining2 = strlen(output) * 3;
            bool made_expansions = false;

            while (*read2) {
                size_t code_len2 = ial_code_region_len(output, read2);
                if (code_len2) {
                    if (!ial_append_verbatim(&expanded_output, &write2, &remaining2, read2, code_len2)) {
                        free(expanded_output);
                        free(output);
                        apex_free_image_attributes(local_img_attrs);
                        return NULL;
                    }
                    read2 += code_len2;
                    continue;
                }

                /* Look for full, collapsed, and shortcut reference images. */
                if (*read2 == '!' && read2[1] == '[') {
                    const char *img_start = read2;
                    read2 += 2; /* Skip ![ */

                    /* Find closing ] for alt text */
                    const char *alt_end = strchr(read2, ']');
                    if (alt_end && alt_end[1] != '(') {
                        const char *ref_start = read2;
                        const char *ref_end = alt_end;
                        const char *image_end = alt_end;
                        bool is_explicit_reference = false;
                        if (alt_end[1] == '[') {
                            const char *explicit_ref_start = alt_end + 2;
                            const char *explicit_ref_end =
                                strchr(explicit_ref_start, ']');
                            if (!explicit_ref_end) {
                                ref_end = NULL;
                            } else if (explicit_ref_end > explicit_ref_start) {
                                ref_start = explicit_ref_start;
                                ref_end = explicit_ref_end;
                                image_end = explicit_ref_end;
                                is_explicit_reference = true;
                            } else {
                                image_end = explicit_ref_end;
                            }
                        }

                        if (ref_end) {
                            size_t ref_name_len =
                                (size_t)(ref_end - ref_start);
                            char *ref_name = malloc(ref_name_len + 1);
                            if (ref_name) {
                                memcpy(ref_name, ref_start, ref_name_len);
                                ref_name[ref_name_len] = '\0';
                                /* Trim whitespace from reference name */
                                char *p = ref_name;
                                while (*p && isspace((unsigned char)*p)) p++;
                                if (p > ref_name) {
                                    memmove(ref_name, p, strlen(p) + 1);
                                }
                                p = ref_name + strlen(ref_name) - 1;
                                while (p >= ref_name && isspace((unsigned char)*p)) {
                                    *p = '\0';
                                    p--;
                                }

                                apex_attributes *local_attrs = NULL;
                                if (is_explicit_reference) {
                                    const char *after_use = image_end + 1;
                                    const char *line_end = after_use;
                                    while (*line_end && *line_end != '\n' &&
                                           *line_end != '\r') {
                                        line_end++;
                                    }
                                    const char *comment_start = NULL;
                                    const char *comment_end = NULL;
                                    local_attrs = parse_adjacent_bear_attrs(
                                        after_use,
                                        line_end,
                                        &comment_start,
                                        &comment_end);
                                }

                                /* Look up if this reference has attributes */
                                image_attr_entry *def_entry =
                                    find_image_attr_by_ref(
                                        local_img_attrs, ref_name);
                                if (def_entry && def_entry->url) {
                                    apex_attributes *effective_attrs =
                                        copy_attributes(def_entry->attrs);
                                    if (effective_attrs && local_attrs) {
                                        for (int i = 0;
                                             i < local_attrs->attr_count;
                                             i++) {
                                            int existing_idx =
                                                find_attribute_index(
                                                    effective_attrs,
                                                    local_attrs->keys[i]);
                                            if (existing_idx >= 0) {
                                                free(effective_attrs
                                                         ->values[existing_idx]);
                                                effective_attrs
                                                    ->values[existing_idx] =
                                                    strdup(
                                                        local_attrs->values[i]);
                                            } else {
                                                add_attribute(
                                                    effective_attrs,
                                                    local_attrs->keys[i],
                                                    local_attrs->values[i]);
                                            }
                                        }
                                    }
                                    if (getenv("APEX_DEBUG_PIPELINE")) {
                                        fprintf(stderr, "[APEX_DEBUG] expand ref [%s] -> inline image\n", ref_name);
                                    }
                                    /* Expand to inline image: ![alt](url attributes) */
                                    /* Extract alt text */
                                    size_t alt_len = alt_end - read2;

                                    /* Convert attributes to markdown format */
                                    char *attr_str =
                                        effective_attrs
                                            ? attributes_to_markdown(
                                                  effective_attrs)
                                            : NULL;

                                    /* Build expanded image: ![alt](url attributes) */
                                    /* Need space for: ![ + alt + ]( + url + space + attr_str + ) */
                                    size_t url_len = strlen(def_entry->url);
                                    size_t attr_len = attr_str ? strlen(attr_str) : 0;
                                    size_t needed = 4 + alt_len + url_len + (attr_len > 0 ? attr_len + 1 : 0) + 1;

                                    /* Check if we need to expand buffer */
                                    if (remaining2 < needed) {
                                        size_t written = write2 - expanded_output;
                                        size_t new_cap = (written + needed + 1) * 2;
                                        char *new_expanded = realloc(expanded_output, new_cap);
                                        if (new_expanded) {
                                            expanded_output = new_expanded;
                                            write2 = expanded_output + written;
                                            remaining2 = new_cap - written;
                                        }
                                    }

                                    if (remaining2 >= needed) {
                                        made_expansions = true;

                                        /* Write ![alt](url */
                                        *write2++ = '!';
                                        *write2++ = '[';
                                        remaining2 -= 2;
                                        memcpy(write2, read2, alt_len);
                                        write2 += alt_len;
                                        remaining2 -= alt_len;
                                        *write2++ = ']';
                                        *write2++ = '(';
                                        remaining2 -= 2;

                                        /* Write URL */
                                        memcpy(write2, def_entry->url, url_len);
                                        write2 += url_len;
                                        remaining2 -= url_len;

                                        /* Write attributes if present (inside parentheses for inline format) */
                                        if (attr_str && *attr_str) {
                                            *write2++ = ' ';
                                            remaining2--;
                                            memcpy(write2, attr_str, attr_len);
                                            write2 += attr_len;
                                            remaining2 -= attr_len;
                                        }

                                        *write2++ = ')';
                                        remaining2--;

                                        read2 = image_end + 1;
                                        free(ref_name);
                                        free(attr_str);
                                        apex_free_attributes(local_attrs);
                                        apex_free_attributes(effective_attrs);
                                        continue;
                                    }
                                    free(attr_str);
                                    apex_free_attributes(effective_attrs);
                                }
                                apex_free_attributes(local_attrs);
                                free(ref_name);
                            }
                        }
                    }
                    /* Not a reference-style image with attributes, or expansion failed - copy as-is */
                    read2 = img_start;
                }

                /* Copy character */
                if (remaining2 > 0) {
                    *write2++ = *read2++;
                    remaining2--;
                } else {
                    read2++;
                }
            }

            *write2 = '\0';
            free(output);
            output = expanded_output;

            /* If we made expansions, we need to extract attributes from the expanded inline images */
            /* Process the expanded output to extract attributes from newly created inline images */
            if (made_expansions) {
                /* Create a temporary buffer to process expanded inline images */
                const char *proc_read = output;
                char *proc_output = malloc(strlen(output) * 2 + 1);
                if (proc_output) {
                    char *proc_write = proc_output;
                    size_t proc_remaining = strlen(output) * 2;

                    while (*proc_read) {
                        size_t code_len3 = ial_code_region_len(output, proc_read);
                        if (code_len3) {
                            if (!ial_append_verbatim(&proc_output, &proc_write, &proc_remaining, proc_read, code_len3)) {
                                free(proc_output);
                                proc_output = NULL;
                                break;
                            }
                            proc_read += code_len3;
                            continue;
                        }

                        /* Look for inline images that were just expanded */
                        if (*proc_read == '!' && proc_read[1] == '[') {
                            const char *img_start = proc_read;
                            const char *check_pos = proc_read + 2; /* After ![ */

                            /* Find closing ] for alt text */
                            const char *alt_end = strchr(check_pos, ']');
                            if (alt_end && alt_end[1] == '(') {
                                const char *url_start = alt_end + 2;
                                const char *p = url_start;
                                const char *url_end = NULL;
                                const char *attr_start = NULL;
                                const char *paren_end = NULL;

                                /* Find closing paren, skipping parentheses inside quotes. */
                                paren_end = ial_destination_closer(url_start);
                                if (paren_end) {
                                    p = url_start;

                                    /* Look for attributes */
                                    while (p < paren_end) {
                                        if (*p == ' ' || *p == '\t') {
                                            const char *after_space = p;
                                            while (after_space < paren_end && (*after_space == ' ' || *after_space == '\t')) after_space++;
                                            if (after_space < paren_end && looks_like_attribute_start(after_space, paren_end)) {
                                                attr_start = after_space;
                                                url_end = p;
                                                break;
                                            }
                                        }
                                        p++;
                                    }

                                    if (!url_end) url_end = paren_end;

                                    /* Only process images with attributes (these are the expanded reference-style images) */
                                    /* Images without attributes were already processed in the first pass */
                                    if (url_end > url_start && attr_start) {
                                        /* Extract URL and attributes */
                                        size_t url_len = url_end - url_start;
                                        char *url = malloc(url_len + 1);
                                        char *encoded_url = NULL;
                                        apex_attributes *attrs = NULL;

                                        if (url) {
                                            memcpy(url, url_start, url_len);
                                            url[url_len] = '\0';

                                            size_t attr_len = paren_end - attr_start;
                                            attrs = parse_image_attributes(attr_start, apex_ial_size_to_int(attr_len));

                                            /* URL is already encoded from expansion, so use as-is */
                                            encoded_url = strdup(url);
                                            if (encoded_url && attrs) {
                                                /* Count existing entries to get index */
                                                int img_index = 0;
                                                for (image_attr_entry *e = local_img_attrs; e; e = e->next) {
                                                    if (e->index >= 0) img_index++;
                                                }

                                                image_attr_entry *entry = create_image_attr_entry(&local_img_attrs, encoded_url, img_index);
                                                if (entry) {
                                                    /* Copy attributes */
                                                    for (int i = 0; i < attrs->attr_count; i++) {
                                                        add_attribute(entry->attrs, attrs->keys[i], attrs->values[i]);
                                                    }
                                                    if (attrs->id) {
                                                        entry->attrs->id = strdup(attrs->id);
                                                    }
                                                    for (int i = 0; i < attrs->class_count; i++) {
                                                        add_class(entry->attrs, attrs->classes[i]);
                                                    }
                                                }
                                            }

                                            /* Write the processed image: ![alt](encoded_url) - attributes removed */
                                            size_t prefix_len = url_start - img_start; /* Includes ![alt]( */
                                            if (prefix_len < proc_remaining) {
                                                memcpy(proc_write, img_start, prefix_len);
                                                proc_write += prefix_len;
                                                proc_remaining -= prefix_len;
                                            } else {
                                                /* Buffer too small - skip this image */
                                                if (attrs) apex_free_attributes(attrs);
                                                free(encoded_url);
                                                free(url);
                                                proc_read = paren_end + 1;
                                                continue;
                                            }

                                            /* Write encoded URL (already encoded, so write as-is) */
                                            size_t encoded_len = strlen(encoded_url ? encoded_url : url);
                                            if (encoded_len < proc_remaining) {
                                                memcpy(proc_write, encoded_url ? encoded_url : url, encoded_len);
                                                proc_write += encoded_len;
                                                proc_remaining -= encoded_len;
                                            } else {
                                                if (attrs) apex_free_attributes(attrs);
                                                free(encoded_url);
                                                free(url);
                                                proc_read = paren_end + 1;
                                                continue;
                                            }

                                            /* Write closing paren */
                                            if (proc_remaining > 0) {
                                                *proc_write++ = ')';
                                                proc_remaining--;
                                            }

                                            /* Cleanup */
                                            if (attrs) apex_free_attributes(attrs);
                                            free(encoded_url);
                                            free(url);

                                            /* Advance past the processed image */
                                            proc_read = paren_end + 1;
                                            continue;
                                        }
                                    }
                                }
                            }
                            /* Not a valid inline image, or no attributes (already processed in first pass) - fall through to copy */
                        }

                        /* Copy character - this handles regular text and inline images without attributes */
                        if (proc_remaining > 0) {
                            *proc_write++ = *proc_read++;
                            proc_remaining--;
                        } else {
                            proc_read++;
                        }
                    }

                    if (proc_output) {
                        *proc_write = '\0';
                        free(output);
                        output = proc_output;
                    }
                }
            }
        }
    }

    /* Return the image attributes list */
    *img_attrs = local_img_attrs;

    if (getenv("APEX_DEBUG_PIPELINE") && output) {
        size_t len = strlen(output);
        fprintf(stderr, "[APEX_DEBUG] preprocess_image_attributes out (len=%zu): %.300s%s\n",
                len, output, len > 300 ? "..." : "");
    }

    return output;
}

/**
 * Apply image attributes to image nodes in AST
 * Uses two matching strategies:
 * 1. First tries to match by index (position) for inline images
 * 2. Then tries to match by URL for reference-style images
 * This ensures inline images with same URL get different attributes,
 * while reference-style images share attributes from their definition.
 */
void apex_apply_image_attributes(cmark_node *document, image_attr_entry *img_attrs) {
    if (!document || !img_attrs) return;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type event;

    /* Preprocessing assigns index 0, 1, 2... to inline images only (ref-style get -1).
     * Use inline_image_position to match so that same-URL images (e.g. webp vs avif)
     * get correct attrs, while ref-style images are matched by URL. */
    int inline_image_position = 0;

    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (event == CMARK_EVENT_ENTER && cmark_node_get_type(node) == CMARK_NODE_IMAGE) {
            const char *url = cmark_node_get_url(node);
            image_attr_entry *matching = NULL;

            /* First, try inline entry with index == inline_image_position and URL match. */
            for (image_attr_entry *e = img_attrs; e; e = e->next) {
                if (e->index == inline_image_position && e->url && url && strcmp(e->url, url) == 0 && e->attrs) {
                    matching = e;
                    e->index = -2; /* mark as used */
                    inline_image_position++;
                    break;
                }
            }

            /* If no inline match, try reference-style entries (index == -1) by URL. */
            if (!matching && url) {
                for (image_attr_entry *e = img_attrs; e; e = e->next) {
                    if (e->index == -1 && e->url && strcmp(e->url, url) == 0 && e->attrs) {
                        matching = e;
                        break;
                    }
                }
            }

            if (matching && matching->attrs) {
                char *attr_str = attributes_to_html_for_image(url, matching->attrs);
                if (attr_str) {
                    char *existing = (char *)cmark_node_get_user_data(node);
                    if (existing) {
                        char *combined = malloc(strlen(existing) + strlen(attr_str) + 2);
                        if (combined) {
                            strcpy(combined, existing);
                            strcat(combined, " ");
                            strcat(combined, attr_str);
                            cmark_node_set_user_data(node, combined);
                            free(attr_str);
                        } else {
                            cmark_node_set_user_data(node, attr_str);
                        }
                    } else {
                        cmark_node_set_user_data(node, attr_str);
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
}

/**
 * Extract reference link definition IDs from text
 * Returns a hash set (simple array) of reference IDs
 * Caller must free the returned array
 */
static char **extract_reference_link_ids(const char *text, size_t *count) {
    if (!text || !count) return NULL;

    *count = 0;
    size_t capacity = 16;
    char **ids = malloc(capacity * sizeof(char*));
    if (!ids) return NULL;

    const char *p = text;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        /* Skip leading whitespace */
        const char *content_start = line_start;
        while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
            content_start++;
        }

        /* Check if this is a reference link definition: [id]: URL */
        if (content_start < line_end && *content_start == '[') {
            const char *id_end = strchr(content_start + 1, ']');
            if (id_end && id_end < line_end && id_end[1] == ':') {
                /* Extract the ID from this definition */
                size_t def_id_len = id_end - (content_start + 1);
                if (def_id_len > 0) {
                    char *def_id = malloc(def_id_len + 1);
                    if (def_id) {
                        memcpy(def_id, content_start + 1, def_id_len);
                        def_id[def_id_len] = '\0';

                        /* Check if we already have this ID */
                        bool found = false;
                        for (size_t i = 0; i < *count; i++) {
                            if (strcmp(ids[i], def_id) == 0) {
                                found = true;
                                free(def_id);
                                break;
                            }
                        }

                        if (!found) {
                            /* Add to array */
                            if (*count >= capacity) {
                                capacity *= 2;
                                char **new_ids = realloc(ids, capacity * sizeof(char*));
                                if (!new_ids) {
                                    free(def_id);
                                    break;
                                }
                                ids = new_ids;
                            }
                            ids[*count] = def_id;
                            (*count)++;
                        }
                    }
                }
            }
        }

        p = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    return ids;
}

/**
 * Check if a reference ID matches the given text (case-insensitive, whitespace normalized)
 */
static bool reference_id_matches(const char *ref_id, const char *text, size_t text_len) {
    if (!ref_id || !text) return false;

    const char *p = ref_id;
    const char *t = text;
    size_t remaining = text_len;

    /* Skip leading whitespace in both */
    while (*p && isspace((unsigned char)*p)) p++;
    while (remaining > 0 && isspace((unsigned char)*t)) {
        t++;
        remaining--;
    }

    /* Compare case-insensitively, collapsing each whitespace run. */
    while (*p && remaining > 0) {
        bool p_space = isspace((unsigned char)*p);
        bool t_space = isspace((unsigned char)*t);
        if (p_space || t_space) {
            if (!p_space || !t_space) return false;
            while (*p && isspace((unsigned char)*p)) p++;
            while (remaining > 0 && isspace((unsigned char)*t)) {
                t++;
                remaining--;
            }
            continue;
        }
        if (tolower((unsigned char)*p) != tolower((unsigned char)*t)) {
            return false;
        }
        p++;
        t++;
        remaining--;
    }

    /* Skip trailing whitespace in both */
    while (*p && isspace((unsigned char)*p)) p++;
    while (remaining > 0 && isspace((unsigned char)*t)) {
        t++;
        remaining--;
    }

    /* Both should be at end */
    return (*p == '\0' && remaining == 0);
}

/**
 * Preprocess bracketed spans [text]{IAL}
 * Converts [text]{IAL} to <span markdown="span" ...>text</span> if [text] is not a reference link
 */
char *apex_preprocess_bracketed_spans(const char *text) {
    if (!text) return NULL;

    /* First, extract all reference link definition IDs */
    size_t ref_count = 0;
    char **ref_ids = extract_reference_link_ids(text, &ref_count);

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2;  /* Worst case: every char becomes part of HTML */
    char *output = malloc(output_capacity);
    if (!output) {
        /* Free ref_ids */
        if (ref_ids) {
            for (size_t i = 0; i < ref_count; i++) {
                free(ref_ids[i]);
            }
            free(ref_ids);
        }
        return NULL;
    }

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;
    bool in_code_block = false;
    bool in_inline_code = false;
    int code_block_backticks = 0;

    while (*read) {
        /* Skip code blocks and inline code */
        if (!in_code_block && !in_inline_code && *read == '`') {
            int backtick_count = 1;
            const char *p = read + 1;
            while (*p == '`') {
                backtick_count++;
                p++;
            }
            if (backtick_count >= 3) {
                /* Code block */
                in_code_block = !in_code_block;
                code_block_backticks = backtick_count;
            } else {
                /* Inline code */
                in_inline_code = !in_inline_code;
            }
        } else if (in_code_block && *read == '`') {
            int backtick_count = 1;
            const char *p = read + 1;
            while (*p == '`') {
                backtick_count++;
                p++;
            }
            if (backtick_count >= code_block_backticks) {
                in_code_block = false;
                code_block_backticks = 0;
            }
        } else if (in_inline_code && *read == '`') {
            in_inline_code = false;
        }

        /* Only process if not in code */
        if (!in_code_block && !in_inline_code && *read == '[') {
            const char *bracket_start = read;
            /* Find matching closing bracket by counting nested brackets */
            const char *bracket_end = NULL;
            int bracket_depth = 1;
            const char *p = bracket_start + 1;

            while (*p && bracket_depth > 0) {
                if (*p == '[') {
                    bracket_depth++;
                } else if (*p == ']') {
                    bracket_depth--;
                    if (bracket_depth == 0) {
                        bracket_end = p;
                        break;
                    }
                }
                p++;
            }

            if (bracket_end) {
                /* Check if this is followed by {IAL} */
                const char *after_bracket = bracket_end + 1;
                /* Skip whitespace */
                while (*after_bracket && (*after_bracket == ' ' || *after_bracket == '\t')) {
                    after_bracket++;
                }

                if (*after_bracket == '{') {
                    /* Found potential {IAL} - check if it's a valid IAL */
                    const char *ial_start = after_bracket;
                    const char *ial_end = strchr(ial_start + 1, '}');

                    if (ial_end) {
                        /* Extract text inside brackets */
                        size_t text_len = bracket_end - (bracket_start + 1);
                        char *bracket_text = malloc(text_len + 1);
                        if (bracket_text) {
                            memcpy(bracket_text, bracket_start + 1, text_len);
                            bracket_text[text_len] = '\0';

                            /* Check if this matches a reference link definition */
                            bool is_reference_link = false;
                            for (size_t i = 0; i < ref_count; i++) {
                                if (reference_id_matches(ref_ids[i], bracket_text, text_len)) {
                                    is_reference_link = true;
                                    break;
                                }
                            }

                            if (!is_reference_link) {
                                /* This is a bracketed span - convert to <span> */
                                /* Parse IAL attributes */
                                size_t ial_len = ial_end - (ial_start + 1);
                                apex_attributes *attrs = parse_ial_content(ial_start + 1, apex_ial_size_to_int(ial_len));

                                if (attrs) {
                                    /* Build span tag with attributes */
                                    char *attr_str = attributes_to_html(attrs);
                                    if (attr_str) {
                                        /* Decide whether this span actually needs markdown=\"span\"
                                         * Only enable markdown-in-HTML for bracketed spans whose
                                         * inner text contains inline markdown syntax (emphasis,
                                         * links, code, etc.). This prevents simple spans like
                                         * [-]{.taskmarker} from being reparsed as block lists. */
                                        bool needs_markdown_span = false;
                                        for (size_t i = 0; i < text_len; i++) {
                                            char ch = bracket_text[i];
                                            if (ch == '*' || ch == '_' || ch == '`' ||
                                                ch == '[' || ch == '!' || ch == '#') {
                                                needs_markdown_span = true;
                                                break;
                                            }
                                        }

                                        /* Calculate space needed.
                                         * Worst case assumes we include markdown=\"span\" plus attributes. */
                                        size_t span_open_len = 20 + strlen(attr_str) + strlen(bracket_text) + 10; /* <span markdown="span" ...>text</span> */
                                        if (remaining < span_open_len) {
                                            size_t written = write - output;
                                            output_capacity = (written + span_open_len + 1) * 2;
                                            char *new_output = realloc(output, output_capacity);
                                            if (!new_output) {
                                                free(bracket_text);
                                                free(attr_str);
                                                apex_free_attributes(attrs);
                                                goto cleanup;
                                            }
                                            output = new_output;
                                            write = output + written;
                                            remaining = output_capacity - written;
                                        }

                                        int written;
                                        if (needs_markdown_span) {
                                            /* Write <span markdown="span" ...> for spans that
                                             * genuinely need inline markdown processing. */
                                            if (attr_str && attr_str[0]) {
                                                written = snprintf(write, remaining, "<span markdown=\"span\" %s>", attr_str);
                                            } else {
                                                written = snprintf(write, remaining, "<span markdown=\"span\">");
                                            }
                                        } else {
                                            /* For simple text-only spans, omit markdown=\"span\"
                                             * so that content like a lone '-' is not reparsed
                                             * as a list item by the markdown-in-HTML pipeline. */
                                            if (attr_str && attr_str[0]) {
                                                written = snprintf(write, remaining, "<span %s>", attr_str);
                                            } else {
                                                written = snprintf(write, remaining, "<span>");
                                            }
                                        }

                                        if (written > 0 && (size_t)written < remaining) {
                                            write += written;
                                            remaining -= written;
                                        }

                                        /* Write the text content */
                                        size_t text_written = strlen(bracket_text);
                                        if (text_written < remaining) {
                                            memcpy(write, bracket_text, text_written);
                                            write += text_written;
                                            remaining -= text_written;
                                        }

                                        /* Write </span> */
                                        if (remaining >= 7) {
                                            memcpy(write, "</span>", 7);
                                            write += 7;
                                            remaining -= 7;
                                        }

                                        free(attr_str);
                                        read = ial_end + 1;  /* Skip past the IAL */
                                        free(bracket_text);
                                        apex_free_attributes(attrs);
                                        continue;
                                    }
                                    apex_free_attributes(attrs);
                                }
                                free(bracket_text);
                            } else {
                                free(bracket_text);
                            }
                        }
                    }
                }
            }
        }

        /* Copy character as-is */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            size_t written = write - output;
            output_capacity = (written + 1) * 2;
            char *new_output = realloc(output, output_capacity);
            if (!new_output) {
                goto cleanup;
            }
            output = new_output;
            write = output + written;
            remaining = output_capacity - written;
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';

cleanup:
    /* Free ref_ids */
    if (ref_ids) {
        for (size_t i = 0; i < ref_count; i++) {
            free(ref_ids[i]);
        }
        free(ref_ids);
    }

    /* Check if we made any changes */
    if (strcmp(output, text) == 0) {
        free(output);
        return NULL;  /* No changes */
    }

    return output;
}

