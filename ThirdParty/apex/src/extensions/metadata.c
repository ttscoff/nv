/**
 * Metadata Extension for Apex
 * Implementation (Simplified version - metadata handled via preprocessing)
 */

#include "../../include/apex/apex.h"
#include "metadata.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <regex.h>

#ifdef APEX_HAVE_LIBYAML
#include <yaml.h>
#endif
#ifdef _POSIX_C_SOURCE
/* strptime should be available if POSIX is defined */
#elif defined(__APPLE__) || defined(__linux__)
/* strptime is available on macOS and most Linux systems */
#define HAVE_STRPTIME 1
#elif defined(_GNU_SOURCE)
/* GNU extension */
#define HAVE_STRPTIME 1
#endif

/* For now, we'll handle metadata as a preprocessing step rather than a block type
 * This is simpler and matches how MultiMarkdown actually works */

/* Node type for metadata blocks */
/* Note: APEX_NODE_METADATA is defined as an enum in parser.h, not as a variable */

/* Transform structures */
typedef struct apex_transform {
    char *name;
    char *options;  /* NULL if no options */
    struct apex_transform *next;
} apex_transform;

/* Dynamic array for string arrays (used by split/join/slice) */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} apex_string_array;

/**
 * Free metadata items list
 */
void apex_free_metadata(apex_metadata_item *metadata) {
    while (metadata) {
        apex_metadata_item *next = metadata->next;
        free(metadata->key);
        free(metadata->value);
        free(metadata);
        metadata = next;
    }
}

/**
 * Trim whitespace from both ends of a string (in-place)
 */
static char *trim_whitespace(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

/**
 * Add a metadata item to the list
 */
static void add_metadata_item(apex_metadata_item **list, const char *key, const char *value) {
    if (!key || !value) return;  /* Don't add items with NULL key or value */

    apex_metadata_item *item = malloc(sizeof(apex_metadata_item));
    if (!item) return;

    item->key = strdup(key);
    item->value = strdup(value);

    /* If strdup failed, free the item and don't add it */
    if (!item->key || !item->value) {
        free(item->key);
        free(item->value);
        free(item);
        return;
    }

    item->next = *list;
    *list = item;
}

/* Append code point cp as UTF-8 at out[*len], keeping room for the NUL. */
static void yaml_append_utf8(char *out, size_t out_size, size_t *len, unsigned long cp) {
    char buf[4];
    size_t n;
    if (cp < 0x80) {
        buf[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    } else {
        return;
    }
    if (*len + n >= out_size) return;
    memcpy(out + *len, buf, n);
    *len += n;
}

/* Parse exactly `digits` hex characters at p into *cp. */
static bool yaml_parse_hex(const char *p, int digits, unsigned long *cp) {
    unsigned long v = 0;
    for (int i = 0; i < digits; i++) {
        char c = p[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = (v << 4) | (unsigned long)d;
    }
    *cp = v;
    return true;
}

/* True if s is empty, whitespace, or a whitespace-led # comment. */
static bool yaml_rest_is_ignorable(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return *s == '\0' || *s == '#';
}

/**
 * Unquote a single-line YAML scalar for the fallback parser, matching libyaml:
 * single-quoted '' becomes ', double-quoted backslash escapes are decoded, and
 * plain scalars are copied unchanged. A quoted scalar may be followed by a
 * # comment. Output is truncated to fit out_size.
 */
static void yaml_unquote_scalar(const char *raw, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!raw) return;

    size_t len = 0;
    char quote = raw[0];

    if (quote == '\'') {
        const char *p = raw + 1;
        while (*p) {
            if (*p == '\'') {
                if (p[1] == '\'') {
                    if (len + 1 < out_size) out[len++] = '\'';
                    p += 2;
                    continue;
                }
                if (yaml_rest_is_ignorable(p + 1)) {
                    out[len] = '\0';
                    return;
                }
                break;
            }
            if (len + 1 < out_size) out[len++] = *p;
            p++;
        }
    } else if (quote == '"') {
        const char *p = raw + 1;
        while (*p) {
            if (*p == '"') {
                if (yaml_rest_is_ignorable(p + 1)) {
                    out[len] = '\0';
                    return;
                }
                break;
            }
            if (*p != '\\' || !p[1]) {
                if (len + 1 < out_size) out[len++] = *p;
                p++;
                continue;
            }

            char esc = p[1];
            unsigned long cp = 0;
            int hex_digits = 0;
            char simple = 0;
            switch (esc) {
                case '0': simple = '\0'; break;
                case 'a': simple = '\a'; break;
                case 'b': simple = '\b'; break;
                case 't': case '\t': simple = '\t'; break;
                case 'n': simple = '\n'; break;
                case 'v': simple = '\v'; break;
                case 'f': simple = '\f'; break;
                case 'r': simple = '\r'; break;
                case 'e': simple = '\x1B'; break;
                case ' ': simple = ' '; break;
                case '"': simple = '"'; break;
                case '/': simple = '/'; break;
                case '\\': simple = '\\'; break;
                case 'N': cp = 0x85; break;
                case '_': cp = 0xA0; break;
                case 'L': cp = 0x2028; break;
                case 'P': cp = 0x2029; break;
                case 'x': hex_digits = 2; break;
                case 'u': hex_digits = 4; break;
                case 'U': hex_digits = 8; break;
                default:
                    /* Unknown escape: keep it literally */
                    if (len + 2 < out_size) {
                        out[len++] = '\\';
                        out[len++] = esc;
                    }
                    p += 2;
                    continue;
            }

            if (hex_digits) {
                if (!yaml_parse_hex(p + 2, hex_digits, &cp)) {
                    if (len + 2 < out_size) {
                        out[len++] = '\\';
                        out[len++] = esc;
                    }
                    p += 2;
                    continue;
                }
                yaml_append_utf8(out, out_size, &len, cp);
                p += 2 + hex_digits;
            } else if (cp) {
                yaml_append_utf8(out, out_size, &len, cp);
                p += 2;
            } else {
                /* \0 would end the C string; drop it rather than truncate */
                if (simple != '\0' && len + 1 < out_size) out[len++] = simple;
                p += 2;
            }
        }
    }

    /* Plain scalar, or a quoted one that is unterminated or has trailing text */
    strncpy(out, raw, out_size - 1);
    out[out_size - 1] = '\0';
}

#ifdef APEX_HAVE_LIBYAML
/* Recursively convert libyaml node to flat metadata items */
static void yaml_node_to_flat_items(yaml_document_t *doc, yaml_node_t *node,
                                    const char *prefix, apex_metadata_item **items) {
    if (!node) return;

    switch (node->type) {
        case YAML_SCALAR_NODE: {
            char *value = (char *)node->data.scalar.value;
            size_t value_len = node->data.scalar.length;
            char *value_str = malloc(value_len + 1);
            if (value_str) {
                memcpy(value_str, value, value_len);
                value_str[value_len] = '\0';
                if (prefix && *prefix) {
                    add_metadata_item(items, prefix, value_str);
                }
                free(value_str);
            }
            break;
        }
        case YAML_SEQUENCE_NODE: {
            /* For arrays, normalize to comma-separated string for backward compatibility with transforms */
            /* Collect all scalar values and join with ", " */
            size_t scalar_count = 0;
            char **scalar_values = NULL;
            size_t scalar_cap = 0;

            yaml_node_item_t *item = node->data.sequence.items.start;
            while (item < node->data.sequence.items.top) {
                yaml_node_t *child = yaml_document_get_node(doc, *item);
                if (child && child->type == YAML_SCALAR_NODE) {
                    if (scalar_count == scalar_cap) {
                        size_t new_cap = scalar_cap ? scalar_cap * 2 : 8;
                        char **tmp = realloc(scalar_values, new_cap * sizeof(char *));
                        if (!tmp) {
                            for (size_t i = 0; i < scalar_count; i++) free(scalar_values[i]);
                            free(scalar_values);
                            break;
                        }
                        scalar_values = tmp;
                        scalar_cap = new_cap;
                    }
                    char *val = (char *)child->data.scalar.value;
                    size_t val_len = child->data.scalar.length;
                    scalar_values[scalar_count] = malloc(val_len + 1);
                    if (scalar_values[scalar_count]) {
                        memcpy(scalar_values[scalar_count], val, val_len);
                        scalar_values[scalar_count][val_len] = '\0';
                        scalar_count++;
                    }
                } else if (child && child->type != YAML_SCALAR_NODE) {
                    /* Non-scalar in array - fall back to indexed keys */
                    for (size_t i = 0; i < scalar_count; i++) free(scalar_values[i]);
                    free(scalar_values);
                    scalar_count = 0;
                    scalar_values = NULL;
                    scalar_cap = 0;

                    /* Use indexed approach for complex arrays */
                    int idx = 0;
                    yaml_node_item_t *it = node->data.sequence.items.start;
                    while (it < node->data.sequence.items.top) {
                        char idx_key[512];
                        if (prefix && *prefix) {
                            snprintf(idx_key, sizeof(idx_key), "%s.%d", prefix, idx);
                        } else {
                            snprintf(idx_key, sizeof(idx_key), "%d", idx);
                        }
                        yaml_node_t *ch = yaml_document_get_node(doc, *it);
                        if (ch) {
                            yaml_node_to_flat_items(doc, ch, idx_key, items);
                        }
                        it++;
                        idx++;
                    }
                    break;
                }
                item++;
            }

            /* If we collected scalars, join them with ", " */
            if (scalar_count > 0 && scalar_values) {
                size_t total_len = 0;
                for (size_t i = 0; i < scalar_count; i++) {
                    total_len += strlen(scalar_values[i]);
                    if (i < scalar_count - 1) total_len += 2; /* ", " */
                }
                char *joined = malloc(total_len + 1);
                if (joined && prefix && *prefix) {
                    char *p = joined;
                    for (size_t i = 0; i < scalar_count; i++) {
                        size_t len = strlen(scalar_values[i]);
                        memcpy(p, scalar_values[i], len);
                        p += len;
                        if (i < scalar_count - 1) {
                            *p++ = ',';
                            *p++ = ' ';
                        }
                    }
                    *p = '\0';
                    add_metadata_item(items, prefix, joined);
                    free(joined);
                }
                for (size_t i = 0; i < scalar_count; i++) free(scalar_values[i]);
                free(scalar_values);
            }
            break;
        }
        case YAML_MAPPING_NODE: {
            yaml_node_pair_t *pair = node->data.mapping.pairs.start;
            while (pair < node->data.mapping.pairs.top) {
                yaml_node_t *key_node = yaml_document_get_node(doc, pair->key);
                yaml_node_t *value_node = yaml_document_get_node(doc, pair->value);

                if (key_node && key_node->type == YAML_SCALAR_NODE && value_node) {
                    char *key = (char *)key_node->data.scalar.value;
                    size_t key_len = key_node->data.scalar.length;
                    char *key_str = malloc(key_len + 1);
                    if (key_str) {
                        memcpy(key_str, key, key_len);
                        key_str[key_len] = '\0';

                        char full_key[512];
                        if (prefix && *prefix) {
                            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key_str);
                        } else {
                            strncpy(full_key, key_str, sizeof(full_key) - 1);
                            full_key[sizeof(full_key) - 1] = '\0';
                        }

                        yaml_node_to_flat_items(doc, value_node, full_key, items);
                        free(key_str);
                    }
                }
                pair++;
            }
            break;
        }
        default:
            break;
    }
}

/* Parse YAML using libyaml - returns flat metadata items for backward compatibility.
 * On a libyaml parse error, a description is written to errbuf (if non-NULL).
 * added_lines is the number of leading lines in text that are not in the
 * source file, so reported line numbers match the file. */
static apex_metadata_item *parse_yaml_with_libyaml(const char *text, size_t text_len, size_t *consumed,
                                                   char *errbuf, size_t errbuf_size, size_t added_lines) {
    yaml_parser_t parser;
    yaml_document_t document;
    apex_metadata_item *items = NULL;

    if (errbuf && errbuf_size) errbuf[0] = '\0';

    /* Skip opening --- if present */
    const char *yaml_start = text;
    size_t line_offset = 0;
    if (text_len >= 3 && strncmp(text, "---", 3) == 0) {
        const char *newline = strchr(text + 3, '\n');
        if (newline) {
            yaml_start = newline + 1;
            line_offset = 1;
        } else {
            yaml_start = text + 3;
        }
    }

    /* Find the end of YAML front matter (---) */
    const char *end_marker = strstr(yaml_start, "\n---");
    if (!end_marker) {
        end_marker = strstr(yaml_start, "\n...");
    }
    if (!end_marker) {
        /* Look for --- at start of line */
        const char *p = yaml_start;
        while ((p = strstr(p, "\n---")) != NULL) {
            if (p[4] == '\n' || p[4] == '\0' || p[4] == '\r') {
                end_marker = p;
                break;
            }
            p += 4;
        }
    }

    size_t yaml_content_len = text_len - (yaml_start - text);
    if (end_marker && end_marker > yaml_start) {
        yaml_content_len = (size_t)(end_marker - yaml_start);
    }

    if (!yaml_parser_initialize(&parser)) {
        return NULL;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_start, yaml_content_len);

    if (!yaml_parser_load(&parser, &document)) {
        if (errbuf && errbuf_size) {
            snprintf(errbuf, errbuf_size, "%s (line %lu, column %lu)",
                     parser.problem ? parser.problem : "parse error",
                     (unsigned long)(parser.problem_mark.line + line_offset + 1 - added_lines),
                     (unsigned long)(parser.problem_mark.column + 1));
        }
        yaml_parser_delete(&parser);
        return NULL;
    }

    yaml_node_t *yaml_root = yaml_document_get_root_node(&document);
    if (!yaml_root || yaml_root->type != YAML_MAPPING_NODE) {
        yaml_document_delete(&document);
        yaml_parser_delete(&parser);
        return NULL;
    }

    /* Calculate consumed bytes */
    if (end_marker) {
        *consumed = (end_marker - text) + 4; /* Include \n--- */
    } else {
        *consumed = text_len;
    }

    /* Convert YAML mapping to flat metadata items */
    yaml_node_to_flat_items(&document, yaml_root, NULL, &items);

    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    return items;
}
#endif

/**
 * Parse YAML front matter
 * Format: --- at start, key: value pairs, --- to close
 * If libyaml is available, attempts to use it for full YAML support (arrays, nested structures)
 * Falls back to simple line-by-line parser for backward compatibility.
 * If libyaml rejects the input, its error is written to errbuf (if non-NULL).
 */
static apex_metadata_item *parse_yaml_metadata_ex(const char *text, size_t *consumed,
                                                  char *errbuf, size_t errbuf_size, size_t added_lines) {
    apex_metadata_item *items = NULL;
    if (errbuf && errbuf_size) errbuf[0] = '\0';
#ifdef APEX_HAVE_LIBYAML
    /* Try libyaml first for full YAML support */
    size_t text_len = strlen(text);
    items = parse_yaml_with_libyaml(text, text_len, consumed, errbuf, errbuf_size, added_lines);
    if (items) {
        return items;
    }
    /* Fall through to simple parser if libyaml parsing failed */
#else
    (void)added_lines;
#endif
    const char *line_start = text;
    const char *line_end;

    /* Skip opening --- */
    if (strncmp(text, "---", 3) != 0) return NULL;
    line_start = strchr(text + 3, '\n');
    if (!line_start) return NULL;
    line_start++;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        /* Check for closing --- or ... */
        char *trimmed = trim_whitespace(line);
        if (strcmp(trimmed, "---") == 0 || strcmp(trimmed, "...") == 0) {
            *consumed = (line_end + 1) - text;
            return items;
        }

        if (*trimmed == '#') {
            line_start = line_end + 1;
            continue;
        }

        /* Parse key: value */
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim_whitespace(line);
            char *value_ptr = trim_whitespace(colon + 1);

            char final_value[1024];
            yaml_unquote_scalar(value_ptr, final_value, sizeof(final_value));

            if (*key) {
                add_metadata_item(&items, key, final_value);
            }
        }

        line_start = line_end + 1;
    }

    return items;
}

static apex_metadata_item *parse_yaml_metadata(const char *text, size_t *consumed) {
    return parse_yaml_metadata_ex(text, consumed, NULL, 0, 0);
}

/**
 * Parse MultiMarkdown metadata
 * Format: key: value pairs at start of document, blank line to end
 */
/* Undelimited MMD keys may contain letters, digits, spaces, hyphens, and
 * underscores only. Other ASCII punctuation means the line is ordinary text. */
static bool is_valid_mmd_metadata_key(const char *key) {
    if (!key || !*key) return false;

    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        if ((*p >= 'A' && *p <= 'Z') ||
            (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') ||
            *p == ' ' || *p == '-' || *p == '_') {
            continue;
        }
        /* Allow UTF-8 bytes for non-ASCII keys; reject other ASCII punctuation. */
        if (*p >= 0x80) continue;
        return false;
    }
    return true;
}

/* Reject a sole long "Key: value" line that looks like a prose sentence. */
#define APEX_MMD_SINGLE_LINE_MAX 100

static apex_metadata_item *parse_mmd_metadata(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *line_start = text;
    const char *line_end;
    bool found_metadata = false;
    size_t metadata_count = 0;
    size_t sole_line_len = 0;

    /* Any non-metadata content before/after key:value ends or rejects the block.
     * Undelimited MMD metadata must start at the very top of the file. */
    #define APEX_MMD_ABORT_OR_END() do { \
        if (found_metadata) { \
            if (metadata_count == 1 && sole_line_len > APEX_MMD_SINGLE_LINE_MAX) { \
                apex_free_metadata(items); \
                *consumed = 0; \
                return NULL; \
            } \
            *consumed = line_start - text; \
            return items; \
        } \
        *consumed = 0; \
        return NULL; \
    } while (0)

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        /* Check for blank line (end of metadata). A leading blank means the
         * document does not start with undelimited MMD metadata. */
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0') {
            if (found_metadata) {
                if (metadata_count == 1 && sole_line_len > APEX_MMD_SINGLE_LINE_MAX) {
                    apex_free_metadata(items);
                    *consumed = 0;
                    return NULL;
                }
                *consumed = (line_end + 1) - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        /* Abbreviation definitions (*[abbr]: expansion or [>abbr]: expansion) */
        if ((trimmed[0] == '*' && trimmed[1] == '[') ||
            (trimmed[0] == '[' && trimmed[1] == '>')) {
            APEX_MMD_ABORT_OR_END();
        }

        /* HTML comments (<!--...-->) */
        if (strncmp(trimmed, "<!--", 4) == 0) {
            APEX_MMD_ABORT_OR_END();
        }

        /* Kramdown markers ({::...}) */
        if (strncmp(trimmed, "{::", 3) == 0) {
            APEX_MMD_ABORT_OR_END();
        }

        /* Markdown headings (# or ##) */
        if (trimmed[0] == '#') {
            APEX_MMD_ABORT_OR_END();
        }

        /* IAL/ALD syntax ({: ...}) */
        if (strncmp(trimmed, "{:", 2) == 0) {
            APEX_MMD_ABORT_OR_END();
        }

        /* Leanpub index syntax ({i: term}) */
        if (strstr(trimmed, "{i:") != NULL) {
            APEX_MMD_ABORT_OR_END();
        }

        /* TOC markers ({{TOC...}}) */
        if (strncmp(trimmed, "{{TOC", 5) == 0) {
            APEX_MMD_ABORT_OR_END();
        }

        /* List markers (-, +, *, or numbered lists) */
        const char *list_check = trimmed;
        if (*list_check == '-' || *list_check == '+' || *list_check == '*') {
            if (list_check[1] == ' ' || list_check[1] == '\t') {
                APEX_MMD_ABORT_OR_END();
            }
        }
        if (isdigit((unsigned char)*list_check)) {
            const char *p = list_check + 1;
            while (isdigit((unsigned char)*p)) p++;
            if ((*p == '.' || *p == ')') && (p[1] == ' ' || p[1] == '\t')) {
                APEX_MMD_ABORT_OR_END();
            }
        }

        /* Parse key: value */
        char *colon = strchr(line, ':');
        if (colon) {
            /* Markdown links/images */
            if (strstr(line, "[!") != NULL ||
                (strchr(line, '[') && strchr(line, ']') && strstr(line, "]("))) {
                APEX_MMD_ABORT_OR_END();
            }
            /* Protocol / URL before the colon */
            size_t key_len = (size_t)(colon - line);
            if (key_len >= 7 && (
                (key_len >= 7 && strncmp(line, "http://", 7) == 0) ||
                (key_len >= 8 && strncmp(line, "https://", 8) == 0) ||
                (key_len >= 7 && strncmp(line, "mailto:", 7) == 0) ||
                strstr(line, "://") != NULL)) {
                APEX_MMD_ABORT_OR_END();
            }

            /* HTML/autolink marker before the colon */
            if (memchr(line, '<', key_len) != NULL) {
                APEX_MMD_ABORT_OR_END();
            }

            /* Require space/tab after colon (MMD "KEY: VALUE"; also avoids URLs) */
            char *after_colon = colon + 1;
            if (*after_colon != ' ' && *after_colon != '\t') {
                APEX_MMD_ABORT_OR_END();
            }

            *colon = '\0';
            char *key = trim_whitespace(line);
            char *value = trim_whitespace(colon + 1);

            if (*key && *value && is_valid_mmd_metadata_key(key)) {
                add_metadata_item(&items, key, value);
                found_metadata = true;
                metadata_count++;
                if (metadata_count == 1) {
                    sole_line_len = len;
                }
            } else {
                APEX_MMD_ABORT_OR_END();
            }
        } else {
            APEX_MMD_ABORT_OR_END();
        }

        #undef APEX_MMD_ABORT_OR_END

        line_start = line_end + 1;
    }

    if (found_metadata) {
        if (metadata_count == 1 && sole_line_len > APEX_MMD_SINGLE_LINE_MAX) {
            apex_free_metadata(items);
            *consumed = 0;
            return NULL;
        }
        *consumed = strlen(text);
    }
    return items;
}

/* Check if a line consists only of at least min_count of delim_char (ignoring
 * surrounding whitespace). */
static bool is_delimiter_line(const char *line, char delim_char, size_t min_count) {
    if (!line) return false;
    char temp[1024];
    size_t len = strlen(line);
    if (len >= sizeof(temp)) len = sizeof(temp) - 1;
    memcpy(temp, line, len);
    temp[len] = '\0';

    char *trimmed = trim_whitespace(temp);
    size_t tlen = strlen(trimmed);
    if (tlen < min_count) return false;
    for (size_t i = 0; i < tlen; i++) {
        if (trimmed[i] != delim_char) return false;
    }
    return true;
}

/* Parse MultiMarkdown metadata when wrapped in delimiter lines.
 * Opening delimiter: --- (or longer)
 * Closing delimiter: --- (or longer) OR ... (or longer)
 */
static apex_metadata_item *parse_mmd_metadata_delimited(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *line_start = text;
    const char *line_end = strchr(line_start, '\n');
    bool found_metadata = false;

    if (!line_end) return NULL;

    /* First line must be a dash delimiter */
    {
        size_t len = line_end - line_start;
        char line[1024];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';
        if (!is_delimiter_line(line, '-', 3)) return NULL;
    }

    line_start = line_end + 1;
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';
        char *trimmed = trim_whitespace(line);

        /* End markers supported by MultiMarkdown compatibility */
        if (is_delimiter_line(trimmed, '-', 3) || is_delimiter_line(trimmed, '.', 3)) {
            if (found_metadata) {
                *consumed = (line_end + 1) - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        /* Blank line also terminates metadata block */
        if (*trimmed == '\0') {
            if (found_metadata) {
                *consumed = (line_end + 1) - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        char *colon = strchr(line, ':');
        if (!colon) {
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        if (colon[1] != ' ' && colon[1] != '\t') {
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        *colon = '\0';
        char *key = trim_whitespace(line);
        char *value = trim_whitespace(colon + 1);
        if (*key && *value) {
            add_metadata_item(&items, key, value);
            found_metadata = true;
        } else {
            if (found_metadata) {
                *consumed = line_start - text;
                return items;
            }
            *consumed = 0;
            return NULL;
        }

        line_start = line_end + 1;
    }

    return NULL;
}

/**
 * Parse Pandoc title block metadata
 * Format: % Title, % Author, % Date as first three lines
 */
static apex_metadata_item *parse_pandoc_metadata(const char *text, size_t *consumed) {
    apex_metadata_item *items = NULL;
    const char *keys[] = {"title", "author", "date"};
    int key_index = 0;
    const char *line_start = text;
    const char *line_end;

    while (key_index < 3 && (line_end = strchr(line_start, '\n')) != NULL) {
        size_t len = line_end - line_start;
        char line[1024];

        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, line_start, len);
        line[len] = '\0';

        char *trimmed = trim_whitespace(line);

        /* Must start with % */
        if (*trimmed == '%') {
            char *value = trim_whitespace(trimmed + 1);
            if (*value) {
                add_metadata_item(&items, keys[key_index], value);
            }
            key_index++;
        } else {
            /* Non-Pandoc line */
            break;
        }

        line_start = line_end + 1;
    }

    if (key_index > 0) {
        *consumed = line_start - text;
    }
    return items;
}

/**
 * Detect and extract metadata from the start of document text
 * This modifies the input by removing the metadata section
 * Returns the extracted metadata
 */
apex_metadata_item *apex_extract_metadata_for_mode(char **text_ptr, apex_mode_t mode) {
    if (!text_ptr || !*text_ptr || !**text_ptr) return NULL;

    char *text = *text_ptr;
    size_t consumed = 0;
    apex_metadata_item *items = NULL;

    if (mode == APEX_MODE_MULTIMARKDOWN && strncmp(text, "---", 3) == 0) {
        /* In MMD mode, delimiter blocks are treated as MultiMarkdown metadata,
         * including dot-delimited closers (.../......). */
        items = parse_mmd_metadata_delimited(text, &consumed);
        if (!items) {
            /* Keep YAML compatibility in MMD mode when content is actual YAML. */
            items = parse_yaml_metadata(text, &consumed);
        }
        if (!items) {
            items = parse_mmd_metadata(text, &consumed);
        }
    }
    /* Try YAML first (most explicit) for non-MMD modes */
    else if (strncmp(text, "---", 3) == 0) {
        items = parse_yaml_metadata(text, &consumed);
    }
    /* Try Pandoc */
    else if (*text == '%') {
        items = parse_pandoc_metadata(text, &consumed);
    }
    /* Try MMD (least specific) */
    else {
        items = parse_mmd_metadata(text, &consumed);
    }

    /* Remove metadata from text if found */
    if (items && consumed > 0) {
        /* Skip past the metadata */
        *text_ptr = text + consumed;
    }

    return items;
}

apex_metadata_item *apex_extract_metadata(char **text_ptr) {
    return apex_extract_metadata_for_mode(text_ptr, APEX_MODE_UNIFIED);
}

/**
 * Placeholder extension creation - for future full integration
 * For now, metadata is handled via preprocessing
 */
cmark_syntax_extension *create_metadata_extension(void) {
    /* Return NULL for now - we handle metadata via preprocessing */
    /* In the future, this could create a proper block extension */
    return NULL;
}

/**
 * Get metadata from a document (stub for now)
 */
apex_metadata_item *apex_get_metadata(cmark_node *document) {
    (void)document;  /* Unused parameter */
    /* For now, metadata must be extracted before parsing */
    /* This would require storing metadata in the document's user_data */
    return NULL;
}

/**
 * Normalize metadata key by removing spaces and converting to lowercase
 * This matches MultiMarkdown's behavior where "HTML Header Level" becomes "htmlheaderlevel"
 */
static char *normalize_metadata_key(const char *key) {
    if (!key) return NULL;

    size_t len = strlen(key);
    char *normalized = malloc(len + 1);
    if (!normalized) return NULL;

    char *out = normalized;
    for (const char *in = key; *in; in++) {
        if (!isspace((unsigned char)*in)) {
            *out++ = (char)tolower((unsigned char)*in);
        }
    }
    *out = '\0';
    return normalized;
}

/**
 * Get a specific metadata value (case-insensitive, spaces ignored)
 * Matches MultiMarkdown behavior where "HTML Header Level" matches "htmlheaderlevel"
 */
const char *apex_metadata_get(apex_metadata_item *metadata, const char *key) {
    if (!key || !metadata) return NULL;

    /* Validate key is a valid string (not just a non-NULL pointer) */
    if (strlen(key) == 0) return NULL;

    /* Normalize the search key */
    char *normalized_key = normalize_metadata_key(key);
    if (!normalized_key) return NULL;

    /* Try exact case-insensitive match first (for backwards compatibility) */
    for (apex_metadata_item *item = metadata; item != NULL; item = item->next) {
        /* Validate item and its key before using */
        if (!item) break;
        if (!item->key) continue;

        /* Additional safety: check if key is a valid string */
        size_t key_len = strlen(item->key);
        if (key_len == 0) continue;

        if (strcasecmp(item->key, key) == 0) {
            free(normalized_key);
            return item->value;
        }
    }

    /* Try normalized match (spaces removed, lowercase) */
    for (apex_metadata_item *item = metadata; item != NULL; item = item->next) {
        if (!item || !item->key) continue;  /* Skip items with NULL keys */

        /* Additional safety: check if key is a valid string */
        size_t key_len = strlen(item->key);
        if (key_len == 0) continue;

        char *normalized_item_key = normalize_metadata_key(item->key);
        if (normalized_item_key) {
            bool match = (strcmp(normalized_item_key, normalized_key) == 0);
            free(normalized_item_key);
            if (match) {
                free(normalized_key);
                return item->value;
            }
        }
    }

    free(normalized_key);
    return NULL;
}

static void apex_fprint_yaml_escaped_double_quoted(FILE *fp, const char *value) {
    if (!value) return;
    for (const char *p = value; *p; p++) {
        if (*p == '\\' || *p == '"') {
            fputc('\\', fp);
        }
        fputc((unsigned char)*p, fp);
    }
}

void apex_metadata_fprint_yaml_mapping(FILE *fp, const apex_metadata_item *metadata) {
    if (!fp) return;
    for (const apex_metadata_item *item = metadata; item; item = item->next) {
        if (!item->key || !item->value) continue;
        bool needs_quotes = strchr(item->value, ':') || strchr(item->value, '\n') ||
                            strchr(item->value, '"') || strchr(item->value, '\\');
        if (needs_quotes) {
            fprintf(fp, "%s: \"", item->key);
            apex_fprint_yaml_escaped_double_quoted(fp, item->value);
            fprintf(fp, "\"\n");
        } else {
            fprintf(fp, "%s: %s\n", item->key, item->value);
        }
    }
}

void apex_metadata_fprint_yaml_document(FILE *fp, const apex_metadata_item *metadata) {
    if (!fp) return;
    fprintf(fp, "---\n");
    apex_metadata_fprint_yaml_mapping(fp, metadata);
    fprintf(fp, "---\n");
}

/**
 * Free transform chain
 */
static void free_transform_chain(apex_transform *transform) {
    while (transform) {
        apex_transform *next = transform->next;
        free(transform->name);
        free(transform->options);
        free(transform);
        transform = next;
    }
}

/**
 * Free string array
 */
static void free_string_array(apex_string_array *arr) {
    if (arr) {
        for (size_t i = 0; i < arr->count; i++) {
            free(arr->items[i]);
        }
        free(arr->items);
        free(arr);
    }
}

/**
 * Parse transform chain from string like "KEY:TRANSFORM1:TRANSFORM2(OPTIONS)"
 * Returns the first transform in the chain, or NULL on error
 * The key part is stored separately and returned via key_out
 */
static apex_transform *parse_transform_chain(const char *input, char **key_out) {
    if (!input || !key_out) return NULL;

    apex_transform *first = NULL;
    apex_transform *current = NULL;

    /* Find first colon to separate key from transforms */
    const char *first_colon = strchr(input, ':');
    if (!first_colon) {
        /* No transforms, just a key */
        *key_out = strdup(input);
        return NULL;
    }

    /* Extract key */
    size_t key_len = first_colon - input;
    *key_out = malloc(key_len + 1);
    if (!*key_out) return NULL;
    memcpy(*key_out, input, key_len);
    (*key_out)[key_len] = '\0';

    /* Parse transforms */
    const char *p = first_colon + 1;

    while (*p) {
        apex_transform *transform = calloc(1, sizeof(apex_transform));
        if (!transform) {
            free_transform_chain(first);
            free(*key_out);
            *key_out = NULL;
            return NULL;
        }

        if (!first) {
            first = transform;
        }
        if (current) {
            current->next = transform;
        }
        current = transform;

        /* Find end of transform name (colon or opening paren) */
        const char *name_end = p;
        while (*name_end && *name_end != ':' && *name_end != '(') {
            name_end++;
        }

        size_t name_len = name_end - p;
        transform->name = malloc(name_len + 1);
        if (!transform->name) {
            free_transform_chain(first);
            free(*key_out);
            *key_out = NULL;
            return NULL;
        }
        memcpy(transform->name, p, name_len);
        transform->name[name_len] = '\0';

        p = name_end;

        /* Check for options in parentheses */
        if (*p == '(') {
            p++;  /* Skip opening paren */
            const char *opt_start = p;
            const char *opt_end = strchr(p, ')');
            if (!opt_end) {
                /* Malformed - missing closing paren */
                free_transform_chain(first);
                free(*key_out);
                *key_out = NULL;
                return NULL;
            }

            size_t opt_len = opt_end - opt_start;
            transform->options = malloc(opt_len + 1);
            if (!transform->options) {
                free_transform_chain(first);
                free(*key_out);
                *key_out = NULL;
                return NULL;
            }
            memcpy(transform->options, opt_start, opt_len);
            transform->options[opt_len] = '\0';

            p = opt_end + 1;
        }

        /* Skip colon separator if present */
        if (*p == ':') {
            p++;
        } else if (*p != '\0') {
            /* Unexpected character */
            break;
        }
    }

    return first;
}

/**
 * Parse date string into struct tm
 * Supports: YYYY-MM-DD HH:MM:SS, YYYY-MM-DD HH:MM, YYYY-MM-DD
 */
static bool parse_date(const char *date_str, struct tm *tm_out) {
    if (!date_str || !tm_out) return false;

    memset(tm_out, 0, sizeof(struct tm));

    /* Try different formats */
    #ifdef HAVE_STRPTIME
    const char *formats[] = {
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d %H:%M",
        "%Y-%m-%d"
    };

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        struct tm temp;
        memset(&temp, 0, sizeof(temp));

        char *end = strptime(date_str, formats[i], &temp);
        if (end && *end == '\0') {
            *tm_out = temp;
            return true;
        }
    }
    #else
    /* Manual parsing fallback */
    int year = 0, mon = 0, mday = 0, hour = 0, min = 0, sec = 0;
    int items = sscanf(date_str, "%d-%d-%d %d:%d:%d", &year, &mon, &mday, &hour, &min, &sec);
    if (items >= 3) {
        /* At least got date part */
        tm_out->tm_year = year - 1900;  /* tm_year is years since 1900 */
        tm_out->tm_mon = mon - 1;       /* tm_mon is 0-based */
        tm_out->tm_mday = mday;
        if (items >= 4) tm_out->tm_hour = hour;
        if (items >= 5) tm_out->tm_min = min;
        if (items >= 6) tm_out->tm_sec = sec;
        return true;
    }
    #endif

    return false;
}

/* Simple string split fallback (for when regex fails) */
static apex_string_array *split_string_simple(const char *str, const char *delimiter) {
    if (!str) return NULL;

    apex_string_array *arr = calloc(1, sizeof(apex_string_array));
    if (!arr) return NULL;

    arr->capacity = 8;
    arr->items = malloc(arr->capacity * sizeof(char*));
    if (!arr->items) {
        free(arr);
        return NULL;
    }

    if (!delimiter || delimiter[0] == '\0') {
        delimiter = ",";  /* Default to comma */
    }

    char *str_copy = strdup(str);
    if (!str_copy) {
        free(arr->items);
        free(arr);
        return NULL;
    }

    char *token = strtok(str_copy, delimiter);
    while (token) {
        /* Trim whitespace from token */
        char *start = token;
        while (*start && isspace((unsigned char)*start)) start++;
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char)*(end-1))) end--;
        *end = '\0';

        if (arr->count >= arr->capacity) {
            arr->capacity *= 2;
            arr->items = realloc(arr->items, arr->capacity * sizeof(char*));
            if (!arr->items) {
                free(str_copy);
                free_string_array(arr);
                return NULL;
            }
        }

        arr->items[arr->count] = strdup(start);
        if (!arr->items[arr->count]) {
            free(str_copy);
            free_string_array(arr);
            return NULL;
        }
        arr->count++;

        token = strtok(NULL, delimiter);
    }

    free(str_copy);
    return arr;
}

/**
 * Create string array from string using regex delimiter
 */
static apex_string_array *split_string(const char *str, const char *delimiter_pattern) {
    if (!str) return NULL;

    apex_string_array *arr = calloc(1, sizeof(apex_string_array));
    if (!arr) return NULL;

    arr->capacity = 8;
    arr->items = malloc(arr->capacity * sizeof(char*));
    if (!arr->items) {
        free(arr);
        return NULL;
    }

    const char *pattern = delimiter_pattern && delimiter_pattern[0] ? delimiter_pattern : "\\s+";

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        /* Regex compilation failed, fall back to simple string split */
        free(arr->items);
        free(arr);
        const char *fallback = delimiter_pattern && delimiter_pattern[0] ? delimiter_pattern : ",";
        return split_string_simple(str, fallback);
    }

    const char *search_pos = str;
    regmatch_t matches[1];

    while (regexec(&regex, search_pos, 1, matches, 0) == 0) {
        /* Extract token before match */
        size_t token_len = (size_t)matches[0].rm_so;
        if (token_len > 0) {
            if (arr->count >= arr->capacity) {
                arr->capacity *= 2;
                arr->items = realloc(arr->items, arr->capacity * sizeof(char*));
                if (!arr->items) {
                    regfree(&regex);
                    free_string_array(arr);
                    return NULL;
                }
            }

            char *token = malloc(token_len + 1);
            if (!token) {
                regfree(&regex);
                free_string_array(arr);
                return NULL;
            }
            memcpy(token, search_pos, token_len);
            token[token_len] = '\0';

            /* Trim whitespace from token */
            char *start = token;
            while (*start && isspace((unsigned char)*start)) start++;
            char *end = start + strlen(start);
            while (end > start && isspace((unsigned char)*(end-1))) end--;
            *end = '\0';

            if (*start) {  /* Only add non-empty tokens */
                arr->items[arr->count] = strdup(start);
                if (!arr->items[arr->count]) {
                    free(token);
                    regfree(&regex);
                    free_string_array(arr);
                    return NULL;
                }
                arr->count++;
            }
            free(token);
        }

        search_pos += matches[0].rm_eo;
        if (*search_pos == '\0') break;
    }

    /* Handle remaining text after last match */
    if (*search_pos) {
        size_t remaining_len = strlen(search_pos);
        if (remaining_len > 0) {
            if (arr->count >= arr->capacity) {
                arr->capacity *= 2;
                arr->items = realloc(arr->items, arr->capacity * sizeof(char*));
                if (!arr->items) {
                    regfree(&regex);
                    free_string_array(arr);
                    return NULL;
                }
            }

            char *token = strdup(search_pos);
            if (token) {
                /* Trim whitespace */
                char *start = token;
                while (*start && isspace((unsigned char)*start)) start++;
                char *end = start + strlen(start);
                while (end > start && isspace((unsigned char)*(end-1))) end--;
                *end = '\0';

                if (*start) {
                    arr->items[arr->count] = strdup(start);
                    if (arr->items[arr->count]) {
                        arr->count++;
                    }
                }
                free(token);
            }
        }
    }

    regfree(&regex);

    /* If no matches found, return array with single element (original string) */
    if (arr->count == 0) {
        arr->items[0] = strdup(str);
        if (arr->items[0]) {
            arr->count = 1;
        }
    }

    return arr;
}

/**
 * Apply a single transform to a value
 * Returns newly allocated string, or NULL on error
 * If is_array is true, value is treated as an array (apex_string_array*)
 */
static char *apply_transform(const char *transform_name, const char *options,
                            const char *value, apex_string_array **array_ptr, bool *is_array) {
    if (!transform_name || !value) return NULL;

    /* Handle array transforms */
    if (strcmp(transform_name, "split") == 0) {
        const char *delim = options && options[0] ? options : " ";
        apex_string_array *arr = split_string(value, delim);
        if (!arr) return NULL;
        if (*array_ptr) free_string_array(*array_ptr);
        *array_ptr = arr;
        *is_array = true;
        /* Return first element as representation */
        return arr->count > 0 ? strdup(arr->items[0]) : strdup("");
    }

    if (strcmp(transform_name, "join") == 0) {
        if (!*is_array || !*array_ptr) {
            /* Not an array yet, try to split on commas */
            apex_string_array *arr = split_string(value, ",");
            if (!arr) return NULL;
            if (*array_ptr) free_string_array(*array_ptr);
            *array_ptr = arr;
            *is_array = true;
        }

        const char *delim = options && options[0] ? options : ", ";
        apex_string_array *arr = *array_ptr;

        if (arr->count == 0) return strdup("");
        if (arr->count == 1) return strdup(arr->items[0]);

        /* Calculate total length */
        size_t total_len = 0;
        size_t delim_len = strlen(delim);
        for (size_t i = 0; i < arr->count; i++) {
            total_len += strlen(arr->items[i]);
            if (i < arr->count - 1) total_len += delim_len;
        }

        char *result = malloc(total_len + 1);
        if (!result) return NULL;

        char *p = result;
        for (size_t i = 0; i < arr->count; i++) {
            size_t len = strlen(arr->items[i]);
            memcpy(p, arr->items[i], len);
            p += len;
            if (i < arr->count - 1) {
                memcpy(p, delim, delim_len);
                p += delim_len;
            }
        }
        *p = '\0';

        /* Join converts array back to string */
        *is_array = false;
        return result;
    }

    if (strcmp(transform_name, "first") == 0) {
        if (!*is_array || !*array_ptr) {
            /* Not an array yet, try to split on commas */
            apex_string_array *arr = split_string(value, ",");
            if (!arr) return NULL;
            if (*array_ptr) free_string_array(*array_ptr);
            *array_ptr = arr;
            *is_array = true;
        }
        apex_string_array *arr = *array_ptr;
        if (arr->count == 0) return strdup("");
        return strdup(arr->items[0]);
    }

    if (strcmp(transform_name, "last") == 0) {
        if (!*is_array || !*array_ptr) {
            /* Not an array yet, try to split on commas */
            apex_string_array *arr = split_string(value, ",");
            if (!arr) return NULL;
            if (*array_ptr) free_string_array(*array_ptr);
            *array_ptr = arr;
            *is_array = true;
        }
        apex_string_array *arr = *array_ptr;
        if (arr->count == 0) return strdup("");
        return strdup(arr->items[arr->count - 1]);
    }

    if (strcmp(transform_name, "slice") == 0) {
        if (!*is_array || !*array_ptr) {
            /* Not an array yet - split string into individual characters */
            size_t value_len = strlen(value);
            apex_string_array *arr = calloc(1, sizeof(apex_string_array));
            if (!arr) return NULL;

            if (value_len == 0) {
                /* Empty string - create empty array */
                arr->capacity = 1;
                arr->items = malloc(sizeof(char*));
                if (!arr->items) {
                    free(arr);
                    return NULL;
                }
                arr->count = 0;
            } else {
                arr->capacity = value_len;
                arr->items = malloc(arr->capacity * sizeof(char*));
                if (!arr->items) {
                    free(arr);
                    return NULL;
                }

                for (size_t i = 0; i < value_len; i++) {
                    arr->items[i] = malloc(2);
                    if (!arr->items[i]) {
                        free_string_array(arr);
                        return NULL;
                    }
                    arr->items[i][0] = value[i];
                    arr->items[i][1] = '\0';
                    arr->count++;
                }
            }

            if (*array_ptr) free_string_array(*array_ptr);
            *array_ptr = arr;
            *is_array = true;
        }

        if (!options) return strdup(value);

        long start = 0, len = -1;
        if (sscanf(options, "%ld,%ld", &start, &len) >= 1) {
            apex_string_array *arr = *array_ptr;
            if (start < 0) start = 0;
            if (start >= (long)arr->count) return strdup("");
            if (len < 0) len = (long)arr->count - start;
            if (len > 0 && (size_t)(start + len) > arr->count) len = (long)arr->count - start;

            /* Create new array with slice */
            apex_string_array *slice = calloc(1, sizeof(apex_string_array));
            if (!slice) return NULL;
            slice->capacity = len;
            slice->items = malloc(len * sizeof(char*));
            if (!slice->items) {
                free(slice);
                return NULL;
            }

            for (long i = 0; i < len; i++) {
                slice->items[i] = strdup(arr->items[start + i]);
                if (!slice->items[i]) {
                    free_string_array(slice);
                    return NULL;
                }
            }
            slice->count = len;

            /* Replace old array */
            if (*array_ptr) free_string_array(*array_ptr);
            *array_ptr = slice;
            *is_array = true;

            /* Return joined representation with no separator */
            size_t total_len = 0;
            for (size_t i = 0; i < slice->count; i++) {
                total_len += strlen(slice->items[i]);
            }
            char *result = malloc(total_len + 1);
            if (!result) return NULL;
            char *p = result;
            for (size_t i = 0; i < slice->count; i++) {
                size_t item_len = strlen(slice->items[i]);
                memcpy(p, slice->items[i], item_len);
                p += item_len;
            }
            *p = '\0';
            return result;
        }
        return strdup(value);
    }

    /* String transforms */
    if (*is_array) {
        /* Convert array to string first */
        apex_string_array *arr = *array_ptr;
        if (arr->count == 0) value = "";
        else if (arr->count == 1) value = arr->items[0];
        else {
            /* Join array */
            size_t total_len = 0;
            for (size_t i = 0; i < arr->count; i++) {
                total_len += strlen(arr->items[i]);
                if (i < arr->count - 1) total_len += 2;
            }
            char *joined = malloc(total_len + 1);
            if (!joined) return NULL;
            char *p = joined;
            for (size_t i = 0; i < arr->count; i++) {
                size_t len = strlen(arr->items[i]);
                memcpy(p, arr->items[i], len);
                p += len;
                if (i < arr->count - 1) {
                    *p++ = ',';
                    *p++ = ' ';
                }
            }
            *p = '\0';
            value = joined;
            *is_array = false;
        }
    }

    if (strcmp(transform_name, "upper") == 0) {
        char *result = strdup(value);
        if (!result) return NULL;
        for (char *p = result; *p; p++) {
            *p = (char)toupper((unsigned char)*p);
        }
        return result;
    }

    if (strcmp(transform_name, "lower") == 0) {
        char *result = strdup(value);
        if (!result) return NULL;
        for (char *p = result; *p; p++) {
            *p = (char)tolower((unsigned char)*p);
        }
        return result;
    }

    if (strcmp(transform_name, "trim") == 0) {
        const char *start = value;
        while (*start && isspace((unsigned char)*start)) start++;
        if (*start == '\0') return strdup("");
        const char *end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) end--;
        size_t len = end - start + 1;
        char *result = malloc(len + 1);
        if (!result) return NULL;
        memcpy(result, start, len);
        result[len] = '\0';
        return result;
    }

    if (strcmp(transform_name, "title") == 0) {
        char *result = strdup(value);
        if (!result) return NULL;
        bool prev_space = true;
        for (char *p = result; *p; p++) {
            if (isspace((unsigned char)*p)) {
                prev_space = true;
            } else {
                if (prev_space) {
                    *p = (char)toupper((unsigned char)*p);
                } else {
                    *p = (char)tolower((unsigned char)*p);
                }
                prev_space = false;
            }
        }
        return result;
    }

    if (strcmp(transform_name, "strftime") == 0) {
        if (!options) return strdup(value);

        struct tm tm;
        if (!parse_date(value, &tm)) {
            /* Date parsing failed, return original */
            return strdup(value);
        }

        /* Use strftime to format */
        char buffer[256];
        size_t result_len = strftime(buffer, sizeof(buffer), options, &tm);
        if (result_len == 0) {
            /* Format failed, return original */
            return strdup(value);
        }
        return strdup(buffer);
    }

    if (strcmp(transform_name, "capitalize") == 0) {
        char *result = strdup(value);
        if (!result) return NULL;
        if (*result) {
            *result = (char)toupper((unsigned char)*result);
        }
        return result;
    }

    if (strcmp(transform_name, "slug") == 0 || strcmp(transform_name, "slugify") == 0) {
        char *result = malloc(strlen(value) * 2 + 1);  /* Worst case: every char becomes hyphen */
        if (!result) return NULL;
        char *out = result;
        bool prev_was_hyphen = false;

        for (const char *p = value; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (isalnum(c)) {
                *out++ = (char)tolower(c);
                prev_was_hyphen = false;
            } else if (isspace(c) || c == '_') {
                if (!prev_was_hyphen) {
                    *out++ = '-';
                    prev_was_hyphen = true;
                }
            }
            /* Skip other special characters */
        }

        /* Remove trailing hyphens */
        while (out > result && out[-1] == '-') {
            out--;
        }

        *out = '\0';
        return result;
    }

    if (strcmp(transform_name, "replace") == 0) {
        if (!options) return strdup(value);

        /* Parse options: OLD,NEW or regex:OLD,NEW */
        char *options_copy = strdup(options);
        if (!options_copy) return strdup(value);

        char *old_str = options_copy;
        char *new_str = strchr(options_copy, ',');
        bool use_regex = false;

        /* Check if it's regex: prefix */
        if (strncmp(old_str, "regex:", 6) == 0) {
            use_regex = true;
        }

        if (!new_str) {
            /* No comma found, treat entire options as search string */
            free(options_copy);
            return strdup(value);
        }

        /* Extract old_str and new_str before modifying options_copy */
        /* Calculate length before any pointer modifications */
        size_t old_str_start_len = new_str - old_str;
        if (old_str_start_len == 0) {
            free(options_copy);
            return strdup(value);
        }
        char *old_pattern = malloc(old_str_start_len + 1);
        if (!old_pattern) {
            free(options_copy);
            return strdup(value);
        }
        memcpy(old_pattern, old_str, old_str_start_len);
        old_pattern[old_str_start_len] = '\0';

        /* If regex, remove the "regex:" prefix from the pattern */
        if (use_regex) {
            /* We already detected regex: prefix, so remove it from old_pattern */
            if (old_str_start_len > 6 && strncmp(old_pattern, "regex:", 6) == 0) {
                size_t pattern_len = old_str_start_len - 6;
                memmove(old_pattern, old_pattern + 6, pattern_len);
                old_pattern[pattern_len] = '\0';
            }
        }

        *new_str = '\0';
        new_str++;

        char *result = NULL;

        if (use_regex) {
            /* Use regex replacement */
            regex_t regex;
            int ret = regcomp(&regex, old_pattern, REG_EXTENDED);
            if (ret == 0) {
                regmatch_t matches[1];
                /* Count matches first to estimate size */
                size_t count = 0;
                const char *search_pos = value;
                while (regexec(&regex, search_pos, 1, matches, 0) == 0) {
                    count++;
                    search_pos += matches[0].rm_eo;
                    if (*search_pos == '\0') break;
                }

                if (count > 0) {
                    /* Build result with replacements */
                    size_t result_cap = strlen(value) + count * (strlen(new_str) + 10);
                    result = malloc(result_cap);
                    if (result) {
                        char *out = result;
                        const char *src = value;

                        search_pos = src;
                        while (regexec(&regex, search_pos, 1, matches, 0) == 0) {
                            /* Copy text before match */
                            size_t before_len = matches[0].rm_so;
                            memcpy(out, search_pos, before_len);
                            out += before_len;

                            /* Copy replacement */
                            size_t repl_len = strlen(new_str);
                            memcpy(out, new_str, repl_len);
                            out += repl_len;

                            search_pos += matches[0].rm_eo;
                        }

                        /* Copy remaining text */
                        strcpy(out, search_pos);
                    } else {
                        /* Malloc failed, return original */
                        result = strdup(value);
                    }
                } else {
                    /* No matches, return original */
                    result = strdup(value);
                }

                regfree(&regex);
            } else {
                /* Regex compilation failed, return original */
                result = strdup(value);
            }
                free(old_pattern);
        } else {
            /* Simple string replacement */
            size_t old_len = strlen(old_pattern);
            size_t new_len = strlen(new_str);
            size_t value_len = strlen(value);

            /* Count occurrences */
            size_t count = 0;
            const char *p = value;
            while ((p = strstr(p, old_pattern)) != NULL) {
                count++;
                p += old_len;
            }

            if (count == 0) {
                result = strdup(value);
            } else {
                size_t result_len = value_len + count * (new_len > old_len ? (new_len - old_len) : 0);
                result = malloc(result_len + 1);
                if (result) {
                    char *out = result;
                    const char *src = value;
                    const char *next;

                    while ((next = strstr(src, old_pattern)) != NULL) {
                        /* Copy text before match */
                        size_t before_len = next - src;
                        memcpy(out, src, before_len);
                        out += before_len;

                        /* Copy replacement */
                        memcpy(out, new_str, new_len);
                        out += new_len;

                        src = next + old_len;
                    }

                    /* Copy remaining text */
                    strcpy(out, src);
                } else {
                    /* Malloc failed, return original */
                    result = strdup(value);
                }
            }
            free(old_pattern);
        }

        free(options_copy);
        if (!result) return strdup(value);
        return result;
    }

    if (strcmp(transform_name, "substring") == 0 || strcmp(transform_name, "substr") == 0) {
        if (!options) return strdup(value);

        long start = 0, end = -1;
        if (sscanf(options, "%ld,%ld", &start, &end) >= 1) {
            size_t len = strlen(value);
            if (start < 0) start = len + start;  /* Negative index from end */
            if (end < 0) end = len + end;
            if (start < 0) start = 0;
            if (end < 0) end = (long)len;
            if (start > (long)len) start = len;
            if (end > (long)len) end = len;
            if (start > end) return strdup("");

            size_t substr_len = end - start;
            char *result = malloc(substr_len + 1);
            if (!result) return NULL;
            memcpy(result, value + start, substr_len);
            result[substr_len] = '\0';
            return result;
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "truncate") == 0) {
        if (!options) return strdup(value);

        long max_len = 0;
        char suffix[64] = "";
        if (sscanf(options, "%ld,%63s", &max_len, suffix) >= 1 ||
            sscanf(options, "%ld", &max_len) == 1) {
            size_t len = strlen(value);
            if ((long)len <= max_len) return strdup(value);

            size_t suffix_len = strlen(suffix);
            size_t trunc_len = max_len > (long)suffix_len ? max_len - suffix_len : max_len;

            char *result = malloc(trunc_len + suffix_len + 1);
            if (!result) return NULL;
            memcpy(result, value, trunc_len);
            memcpy(result + trunc_len, suffix, suffix_len);
            result[trunc_len + suffix_len] = '\0';
            return result;
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "default") == 0) {
        if (value[0] == '\0') {
            return options ? strdup(options) : strdup("");
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "escape") == 0 || strcmp(transform_name, "html_escape") == 0) {
        size_t len = strlen(value);
        size_t result_cap = len * 6 + 1;  /* Worst case: every char becomes &xxxx; */
        char *result = malloc(result_cap);
        if (!result) return NULL;

        char *out = result;
        for (const char *p = value; *p; p++) {
            unsigned char c = (unsigned char)*p;
            switch (c) {
                case '&': strcpy(out, "&amp;"); out += 5; break;
                case '<': strcpy(out, "&lt;"); out += 4; break;
                case '>': strcpy(out, "&gt;"); out += 4; break;
                case '"': strcpy(out, "&quot;"); out += 6; break;
                case '\'': strcpy(out, "&#39;"); out += 5; break;
                default:
                    if (c >= 32 && c < 127) {
                        *out++ = (char)c;
                    } else {
                        /* Encode other characters as entities */
                        int written = snprintf(out, 16, "&#%u;", c);
                        if (written > 0) out += written;
                    }
                    break;
            }
        }
        *out = '\0';
        return result;
    }

    if (strcmp(transform_name, "basename") == 0) {
        const char *last_slash = strrchr(value, '/');
        if (last_slash) {
            return strdup(last_slash + 1);
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "urlencode") == 0) {
        size_t len = strlen(value);
        char *result = malloc(len * 3 + 1);  /* Worst case: every char becomes %XX */
        if (!result) return NULL;

        char *out = result;
        for (const char *p = value; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                *out++ = (char)c;
            } else {
                int written = snprintf(out, 8, "%%%02X", c);
                if (written > 0) out += written;
            }
        }
        *out = '\0';
        return result;
    }

    if (strcmp(transform_name, "urldecode") == 0) {
        size_t len = strlen(value);
        char *result = malloc(len + 1);
        if (!result) return NULL;

        char *out = result;
        for (const char *p = value; *p; p++) {
            if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
                char hex[3] = {p[1], p[2], '\0'};
                *out++ = (char)strtoul(hex, NULL, 16);
                p += 2;
            } else if (*p == '+') {
                *out++ = ' ';
            } else {
                *out++ = *p;
            }
        }
        *out = '\0';
        return result;
    }

    if (strcmp(transform_name, "prefix") == 0) {
        if (!options) return strdup(value);
        size_t prefix_len = strlen(options);
        size_t value_len = strlen(value);
        char *result = malloc(prefix_len + value_len + 1);
        if (!result) return NULL;
        memcpy(result, options, prefix_len);
        memcpy(result + prefix_len, value, value_len);
        result[prefix_len + value_len] = '\0';
        return result;
    }

    if (strcmp(transform_name, "suffix") == 0) {
        if (!options) return strdup(value);
        size_t suffix_len = strlen(options);
        size_t value_len = strlen(value);
        char *result = malloc(value_len + suffix_len + 1);
        if (!result) return NULL;
        memcpy(result, value, value_len);
        memcpy(result + value_len, options, suffix_len);
        result[value_len + suffix_len] = '\0';
        return result;
    }

    if (strcmp(transform_name, "remove") == 0) {
        if (!options) return strdup(value);

        size_t remove_len = strlen(options);
        if (remove_len == 0) return strdup(value);

        size_t value_len = strlen(value);
        char *result = malloc(value_len + 1);
        if (!result) return NULL;

        char *out = result;
        for (const char *p = value; *p; p++) {
            if (strncmp(p, options, remove_len) == 0) {
                p += remove_len - 1;  /* -1 because loop increments */
            } else {
                *out++ = *p;
            }
        }
        *out = '\0';
        return result;
    }

    if (strcmp(transform_name, "repeat") == 0) {
        if (!options) return strdup(value);

        long count = 1;
        if (sscanf(options, "%ld", &count) == 1 && count > 0) {
            size_t value_len = strlen(value);
            if (value_len == 0) return strdup("");

            char *result = malloc(value_len * count + 1);
            if (!result) return NULL;

            char *out = result;
            for (long i = 0; i < count; i++) {
                memcpy(out, value, value_len);
                out += value_len;
            }
            *out = '\0';
            return result;
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "reverse") == 0) {
        size_t len = strlen(value);
        char *result = malloc(len + 1);
        if (!result) return NULL;

        for (size_t i = 0; i < len; i++) {
            result[i] = value[len - 1 - i];
        }
        result[len] = '\0';
        return result;
    }

    if (strcmp(transform_name, "format") == 0) {
        if (!options) return strdup(value);

        /* Try to parse as number for formatting */
        double num = 0.0;
        if (sscanf(value, "%lf", &num) == 1) {
            char buffer[256];
            int written = snprintf(buffer, sizeof(buffer), options, num);
            if (written > 0 && written < (int)sizeof(buffer)) {
                return strdup(buffer);
            }
        }
        /* If not a number or format failed, return original */
        return strdup(value);
    }

    if (strcmp(transform_name, "length") == 0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%zu", strlen(value));
        return strdup(buffer);
    }

    if (strcmp(transform_name, "pad") == 0) {
        if (!options) return strdup(value);

        long width = 0;
        char pad_char = ' ';
        if (sscanf(options, "%ld,%c", &width, &pad_char) >= 1 ||
            sscanf(options, "%ld", &width) == 1) {
            size_t len = strlen(value);
            if ((long)len >= width) return strdup(value);

            size_t pad_count = width - len;
            char *result = malloc(width + 1);
            if (!result) return NULL;

            memset(result, pad_char, pad_count);
            memcpy(result + pad_count, value, len);
            result[width] = '\0';
            return result;
        }
        return strdup(value);
    }

    if (strcmp(transform_name, "contains") == 0) {
        if (!options) return strdup("false");
        return strstr(value, options) ? strdup("true") : strdup("false");
    }

    /* Unknown transform, return original */
    return strdup(value);
}

/**
 * Apply transform chain to a value
 * Returns newly allocated string
 */
static char *apply_transform_chain(const char *value, apex_transform *chain) {
    if (!value || !chain) return strdup(value ? value : "");

    char *current_value = strdup(value);
    apex_string_array *array = NULL;
    bool is_array = false;

    apex_transform *transform = chain;
    while (transform) {
        char *new_value = apply_transform(transform->name, transform->options,
                                         current_value, &array, &is_array);
        if (!new_value) {
            /* Transform failed, return original value */
            free(current_value);
            if (array) free_string_array(array);
            return strdup(value ? value : "");
        }

        if (current_value != value) {  /* Don't free original value */
            free(current_value);
        }
        current_value = new_value;

        transform = transform->next;
    }

    if (array) free_string_array(array);

    return current_value;
}

/**
 * Replace [%key] patterns with metadata values
 * If options->enable_metadata_transforms is true, supports [%key:transform:transform2] syntax
 */
char *apex_metadata_replace_variables(const char *text, apex_metadata_item *metadata, const apex_options *options) {
    if (!text || !metadata) {
        return text ? strdup(text) : NULL;
    }

    bool transforms_enabled = options && options->enable_metadata_transforms;

    /* Build result incrementally */
    size_t result_capacity = strlen(text) + 512;  /* Extra space for potential expansions */
    char *result = malloc(result_capacity);
    if (!result) return NULL;

    size_t result_len = 0;
    const char *last_pos = text;  /* Track position in original text */

    const char *p = text;
    while ((p = strstr(p, "[%")) != NULL) {
        /* Find the matching closing ']' for '[%', accounting for brackets inside the pattern */
        /* Since '[%' opens a bracket, we need to find the matching ']' */
        /* We scan forward and track bracket depth to handle nested brackets in regex patterns */
        const char *end = p + 2;
        int bracket_depth = 1;  /* Start at 1 because '[%' opened a bracket */

        while (*end && bracket_depth > 0) {
            if (*end == '[') {
                bracket_depth++;
            } else if (*end == ']') {
                bracket_depth--;
            }
            if (bracket_depth > 0) {
                end++;
            }
        }

        if (!*end && bracket_depth > 0) {
            /* Malformed - no matching closing bracket found */
            break;
        }

        /* Copy text before [%...] */
        size_t prefix_len = p - last_pos;
        if (prefix_len > 0) {
            if (result_len + prefix_len + 1 > result_capacity) {
                result_capacity = (result_len + prefix_len + 1) * 2;
                char *new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            memcpy(result + result_len, last_pos, prefix_len);
            result_len += prefix_len;
        }

        /* Extract key and transforms */
        size_t pattern_len = end - (p + 2);
        char pattern[512];
        if (pattern_len >= sizeof(pattern)) {
            /* Pattern too long, keep original */
            size_t keep_len = (end - p) + 1;
            if (result_len + keep_len + 1 > result_capacity) {
                result_capacity = (result_len + keep_len + 1) * 2;
                char *new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            memcpy(result + result_len, p, keep_len);
            result_len += keep_len;
            last_pos = end + 1;
            p = end + 1;
            continue;
        }

        memcpy(pattern, p + 2, pattern_len);
        pattern[pattern_len] = '\0';

        /* Parse key and transforms */
        char *key = NULL;
        apex_transform *transform_chain = NULL;
        const char *value = NULL;
        char *transformed_value = NULL;

        if (transforms_enabled && strchr(pattern, ':')) {
            /* Has transforms */
            transform_chain = parse_transform_chain(pattern, &key);
            if (key) {
                value = apex_metadata_get(metadata, key);
                if (value && transform_chain) {
                    transformed_value = apply_transform_chain(value, transform_chain);
                    /* If transform chain failed, fall back to original value */
                    if (!transformed_value) {
                        transformed_value = strdup(value);
                    }
                } else if (value) {
                    transformed_value = strdup(value);
                }
                free(key);
                free_transform_chain(transform_chain);
            } else {
                /* Parse failed, treat as simple key */
                value = apex_metadata_get(metadata, pattern);
                if (value) {
                    transformed_value = strdup(value);
                }
            }
        } else {
            /* Simple key, no transforms */
            value = apex_metadata_get(metadata, pattern);
            if (value) {
                transformed_value = strdup(value);
            }
        }

        if (transformed_value) {
            size_t value_len = strlen(transformed_value);
            if (result_len + value_len + 1 > result_capacity) {
                result_capacity = (result_len + value_len + 1) * 2;
                char *new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    free(transformed_value);
                    return NULL;
                }
                result = new_result;
            }
            memcpy(result + result_len, transformed_value, value_len);
            result_len += value_len;
            free(transformed_value);
        } else {
            /* Keep original pattern if not found */
            size_t keep_len = (end - p) + 1;
            if (result_len + keep_len + 1 > result_capacity) {
                result_capacity = (result_len + keep_len + 1) * 2;
                char *new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
            }
            memcpy(result + result_len, p, keep_len);
            result_len += keep_len;
        }

        last_pos = end + 1;
        p = end + 1;
    }

    /* Copy remaining text after last [%...] */
    if (last_pos && *last_pos) {
        size_t remaining = strlen(last_pos);
        if (result_len + remaining + 1 > result_capacity) {
            result_capacity = result_len + remaining + 1;
            char *new_result = realloc(result, result_capacity);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
        }
        strcpy(result + result_len, last_pos);
        result_len += remaining;
    }

    result[result_len] = '\0';
    return result;
}

static bool path_has_yaml_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || strchr(dot, '/')) return false;
    return strcasecmp(dot, ".yml") == 0 || strcasecmp(dot, ".yaml") == 0;
}

/**
 * Load metadata from a file
 * Auto-detects format based on first characters; .yml/.yaml files are always YAML
 */
apex_metadata_item *apex_load_metadata_from_file(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    /* Read file into buffer */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size > 1024 * 1024) {  /* Max 1MB */
        fclose(fp);
        return NULL;
    }

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    apex_metadata_item *items = NULL;
    size_t consumed = 0;

    /* .yml/.yaml files are YAML even without --- markers; add them so both
     * libyaml and the fallback parser see a delimited block. */
    size_t added_lines = 0;
    if (strncmp(buffer, "---", 3) != 0 && path_has_yaml_extension(filepath)) {
        bool needs_newline = bytes_read > 0 && buffer[bytes_read - 1] != '\n';
        size_t wrapped_len = 4 + bytes_read + (needs_newline ? 1 : 0) + 4;
        char *wrapped = malloc(wrapped_len + 1);
        if (!wrapped) {
            free(buffer);
            return NULL;
        }
        snprintf(wrapped, wrapped_len + 1, "---\n%s%s---\n", buffer, needs_newline ? "\n" : "");
        free(buffer);
        buffer = wrapped;
        added_lines = 1;
    }

    /* Auto-detect format */
    if (strncmp(buffer, "---", 3) == 0) {
        /* YAML format */
        char yaml_error[256];
        items = parse_yaml_metadata_ex(buffer, &consumed, yaml_error, sizeof(yaml_error), added_lines);
        if (yaml_error[0]) {
            fprintf(stderr, "Warning: YAML error in '%s': %s; using simple key: value parsing\n",
                    filepath, yaml_error);
        }
    } else if (buffer[0] == '%') {
        /* Pandoc format */
        items = parse_pandoc_metadata(buffer, &consumed);
    } else {
        /* MMD format (default) */
        items = parse_mmd_metadata(buffer, &consumed);
    }

    free(buffer);
    return items;
}

/**
 * Parse a single KEY=VALUE pair, handling quoted values
 * Returns key and value in allocated strings (caller must free)
 */
static int parse_key_value_pair(const char *input, char **key_out, char **value_out) {
    if (!input) return 0;

    const char *equals = strchr(input, '=');
    if (!equals) return 0;

    /* Extract key */
    size_t key_len = equals - input;
    *key_out = malloc(key_len + 1);
    if (!*key_out) return 0;
    memcpy(*key_out, input, key_len);
    (*key_out)[key_len] = '\0';
    trim_whitespace(*key_out);

    /* Extract value */
    const char *value_start = equals + 1;

    /* Check if value is quoted */
    if (*value_start == '"' || *value_start == '\'') {
        char quote = *value_start;
        value_start++;
        const char *value_end = strchr(value_start, quote);
        if (!value_end) {
            /* Unmatched quote, treat rest as value */
            *value_out = strdup(value_start);
            return 1;
        }
        size_t value_len = value_end - value_start;
        *value_out = malloc(value_len + 1);
        if (!*value_out) {
            free(*key_out);
            return 0;
        }
        memcpy(*value_out, value_start, value_len);
        (*value_out)[value_len] = '\0';
    } else {
        /* Unquoted value - take until comma or end */
        const char *value_end = strchr(value_start, ',');
        if (!value_end) {
            value_end = value_start + strlen(value_start);
        }
        size_t value_len = value_end - value_start;
        *value_out = malloc(value_len + 1);
        if (!*value_out) {
            free(*key_out);
            return 0;
        }
        memcpy(*value_out, value_start, value_len);
        (*value_out)[value_len] = '\0';
        trim_whitespace(*value_out);
    }

    return 1;
}

/**
 * Parse command-line metadata from KEY=VALUE string
 * Handles quoted values and comma-separated pairs
 */
apex_metadata_item *apex_parse_command_metadata(const char *arg) {
    if (!arg || !*arg) return NULL;

    apex_metadata_item *items = NULL;
    const char *p = arg;

    while (*p) {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Find the equals sign for this pair */
        const char *equals = strchr(p, '=');
        if (!equals) break;  /* No equals sign, invalid */

        /* Find where this KEY=VALUE pair ends */
        const char *value_start = equals + 1;
        const char *pair_end = NULL;

        /* Check if value is quoted */
        if (*value_start == '"' || *value_start == '\'') {
            char quote = *value_start;
            const char *quote_end = strchr(value_start + 1, quote);
            if (quote_end) {
                pair_end = quote_end + 1;
            } else {
                /* Unmatched quote - take rest of string */
                pair_end = p + strlen(p);
            }
        } else {
            /* Unquoted value - find next comma that's followed by KEY= pattern
             * We look for comma followed by whitespace and then KEY= */
            const char *search = value_start;
            pair_end = p + strlen(p);  /* Default to end of string */

            const char *comma = strchr(search, ',');
            if (comma) {
                /* Found a comma - check if it starts a new KEY= pair */
                const char *after_comma = comma + 1;
                while (*after_comma && isspace((unsigned char)*after_comma)) after_comma++;

                /* Look for KEY= pattern (alphanumeric key followed by =) */
                if ((isalnum((unsigned char)*after_comma) || *after_comma == '_')) {
                    const char *next_equals = strchr(after_comma, '=');
                    if (next_equals) {
                        /* Check if there's another comma between after_comma and next_equals */
                        const char *comma_between = strchr(after_comma, ',');
                        if (!comma_between || comma_between > next_equals) {
                            /* This comma starts the next KEY= pair */
                            pair_end = comma;
                        }
                    }
                }
            }
            /* If no comma or comma doesn't start KEY= pair, pair_end stays at end of string */
        }

        /* Extract this pair */
        size_t pair_len = pair_end - p;
        char *pair = malloc(pair_len + 1);
        if (!pair) break;
        memcpy(pair, p, pair_len);
        pair[pair_len] = '\0';

        /* Parse key=value */
        char *key = NULL;
        char *value = NULL;
        if (parse_key_value_pair(pair, &key, &value)) {
            if (key && value) {
                add_metadata_item(&items, key, value);
                free(key);
                free(value);
            }
        }

        free(pair);

        /* Move to next pair */
        if (pair_end < p + strlen(p) && *pair_end == ',') {
            p = pair_end + 1;
        } else {
            break;
        }
    }

    return items;
}

/**
 * Merge multiple metadata lists with precedence
 * Later lists take precedence over earlier ones
 */
/* Add or replace each item of src in *result (case-insensitive keys). */
static void merge_metadata_into(apex_metadata_item **result, const apex_metadata_item *src) {
    for (; src; src = src->next) {
        apex_metadata_item *prev = NULL;
        apex_metadata_item *curr = *result;
        while (curr) {
            if (strcasecmp(curr->key, src->key) == 0) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    *result = curr->next;
                }
                free(curr->key);
                free(curr->value);
                free(curr);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        add_metadata_item(result, src->key, src->value);
    }
}

apex_metadata_item *apex_merge_metadata(apex_metadata_item *first, ...) {
    apex_metadata_item *result = NULL;

    /* Start with a copy of the first list (if any) */
    for (apex_metadata_item *src = first; src; src = src->next) {
        add_metadata_item(&result, src->key, src->value);
    }

    /* Merge remaining lists */
    va_list args;
    va_start(args, first);

    apex_metadata_item *next_list;
    while ((next_list = va_arg(args, apex_metadata_item*)) != NULL) {
        merge_metadata_into(&result, next_list);
    }

    va_end(args);
    return result;
}

apex_metadata_item *apex_merge_metadata_lists(apex_metadata_item *const *lists, size_t count) {
    apex_metadata_item *result = NULL;
    if (!lists) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (lists[i]) merge_metadata_into(&result, lists[i]);
    }
    return result;
}

/**
 * Helper function to check if a string represents a true value
 */
static bool is_true_value(const char *value) {
    if (!value) return false;
    /* Downcase the value for comparison */
    char *lower = strdup(value);
    if (!lower) return false;
    for (char *p = lower; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    bool result = (strcmp(lower, "true") == 0 ||
                   strcmp(lower, "yes") == 0 ||
                   strcmp(lower, "1") == 0);
    free(lower);
    return result;
}

/**
 * Helper function to check if a string represents a false value
 */
static bool is_false_value(const char *value) {
    if (!value) return false;
    /* Downcase the value for comparison */
    char *lower = strdup(value);
    if (!lower) return false;
    for (char *p = lower; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    bool result = (strcmp(lower, "false") == 0 ||
                   strcmp(lower, "no") == 0 ||
                   strcmp(lower, "0") == 0);
    free(lower);
    return result;
}

/**
 * Apply metadata values to apex_options structure
 * Maps metadata keys to command-line options, allowing per-document control
 */
void apex_apply_metadata_to_options(apex_metadata_item *metadata, apex_options *options) {
    if (!metadata || !options) return;

    /* First pass: handle mode if specified (this resets options to mode defaults) */
    apex_metadata_item *item = metadata;
    while (item) {
        const char *key = item->key;
        const char *value = item->value;

        if (key && value && strcasecmp(key, "mode") == 0) {
            /* Convert string to enum and apply mode (this resets all options) */
            char *lower = strdup(value);
            if (lower) {
                for (char *p = lower; *p; p++) {
                    *p = (char)tolower((unsigned char)*p);
                }
                if (strcmp(lower, "commonmark") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_COMMONMARK);
                } else if (strcmp(lower, "gfm") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_GFM);
                } else if (strcmp(lower, "mmd") == 0 || strcmp(lower, "multimarkdown") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
                } else if (strcmp(lower, "kramdown") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_KRAMDOWN);
                } else if (strcmp(lower, "unified") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_UNIFIED);
                } else if (strcmp(lower, "quarto") == 0) {
                    *options = apex_options_for_mode(APEX_MODE_QUARTO);
                }
                free(lower);
            }
            break;  /* Only process first mode found */
        }
        item = item->next;
    }

    /* Second pass: apply all other metadata options */
    item = metadata;
    while (item) {
        const char *key = item->key;
        const char *value = item->value;

        if (!key || !value) {
            item = item->next;
            continue;
        }

        /* Skip mode - already processed */
        if (strcasecmp(key, "mode") == 0) {
            item = item->next;
            continue;
        }

        /* Boolean flags (with --[no-] variants) */
        if (strcasecmp(key, "indices") == 0) {
            if (is_true_value(value)) {
                options->enable_indices = true;
                options->enable_mmark_index_syntax = true;
                options->enable_textindex_syntax = true;
            } else if (is_false_value(value)) {
                options->enable_indices = false;
            }
        } else if (strcasecmp(key, "wikilinks") == 0 || strcasecmp(key, "wiki-links") == 0) {
            if (is_true_value(value)) {
                options->enable_wiki_links = true;
            } else if (is_false_value(value)) {
                options->enable_wiki_links = false;
            }
        } else if (strcasecmp(key, "includes") == 0 || strcasecmp(key, "file-includes") == 0) {
            if (is_true_value(value)) {
                options->enable_file_includes = true;
            } else if (is_false_value(value)) {
                options->enable_file_includes = false;
            }
        } else if (strcasecmp(key, "relaxed-tables") == 0 || strcasecmp(key, "relaxed_tables") == 0) {
            if (is_true_value(value)) {
                options->relaxed_tables = true;
            } else if (is_false_value(value)) {
                options->relaxed_tables = false;
            }
        } else if (strcasecmp(key, "grid-tables") == 0 || strcasecmp(key, "grid_tables") == 0) {
            if (is_true_value(value)) {
                options->enable_grid_tables = true;
            } else if (is_false_value(value)) {
                options->enable_grid_tables = false;
            }
        } else if (strcasecmp(key, "alpha-lists") == 0 || strcasecmp(key, "alpha_lists") == 0) {
            if (is_true_value(value)) {
                options->allow_alpha_lists = true;
            } else if (is_false_value(value)) {
                options->allow_alpha_lists = false;
            }
        } else if (strcasecmp(key, "mixed-lists") == 0 || strcasecmp(key, "mixed_lists") == 0) {
            if (is_true_value(value)) {
                options->allow_mixed_list_markers = true;
            } else if (is_false_value(value)) {
                options->allow_mixed_list_markers = false;
            }
        } else if (strcasecmp(key, "sup-sub") == 0 || strcasecmp(key, "sup_sub") == 0) {
            if (is_true_value(value)) {
                options->enable_sup_sub = true;
            } else if (is_false_value(value)) {
                options->enable_sup_sub = false;
            }
        } else if (strcasecmp(key, "autolink") == 0) {
            if (is_true_value(value)) {
                options->enable_autolink = true;
            } else if (is_false_value(value)) {
                options->enable_autolink = false;
            }
        } else if (strcasecmp(key, "strikethrough") == 0 || strcasecmp(key, "strike-through") == 0) {
            if (is_true_value(value)) {
                options->enable_strikethrough = true;
            } else if (is_false_value(value)) {
                options->enable_strikethrough = false;
            }
        } else if (strcasecmp(key, "emoji") == 0 || strcasecmp(key, "enable-emoji") == 0 || strcasecmp(key, "enable_emoji") == 0) {
            if (is_true_value(value)) {
                options->enable_emoji = true;
            } else if (is_false_value(value)) {
                options->enable_emoji = false;
            }
        } else if (strcasecmp(key, "emoji-autocorrect") == 0 || strcasecmp(key, "emoji_autocorrect") == 0) {
            if (is_true_value(value)) {
                options->enable_emoji_autocorrect = true;
            } else if (is_false_value(value)) {
                options->enable_emoji_autocorrect = false;
            }
        } else if (strcasecmp(key, "transforms") == 0 || strcasecmp(key, "metadata-transforms") == 0) {
            if (is_true_value(value)) {
                options->enable_metadata_transforms = true;
            } else if (is_false_value(value)) {
                options->enable_metadata_transforms = false;
            }
        } else if (strcasecmp(key, "unsafe") == 0) {
            if (is_true_value(value)) {
                options->unsafe = true;
            } else if (is_false_value(value)) {
                options->unsafe = false;
            }
        } else if (strcasecmp(key, "plugins") == 0 || strcasecmp(key, "enable-plugins") == 0 || strcasecmp(key, "enable_plugins") == 0) {
            if (is_true_value(value)) {
                options->enable_plugins = true;
            } else if (is_false_value(value)) {
                options->enable_plugins = false;
            }
        } else if (strcasecmp(key, "tables") == 0) {
            if (is_true_value(value)) {
                options->enable_tables = true;
            } else if (is_false_value(value)) {
                options->enable_tables = false;
            }
        } else if (strcasecmp(key, "footnotes") == 0) {
            if (is_true_value(value)) {
                options->enable_footnotes = true;
            } else if (is_false_value(value)) {
                options->enable_footnotes = false;
            }
        } else if (strcasecmp(key, "one-line-definitions") == 0 || strcasecmp(key, "one_line_definitions") == 0) {
            if (is_true_value(value)) {
                options->enable_definition_lists = true;
            } else if (is_false_value(value)) {
                options->enable_definition_lists = false;
            }
        } else if (strcasecmp(key, "smart") == 0 || strcasecmp(key, "smart-typography") == 0) {
            if (is_true_value(value)) {
                options->enable_smart_typography = true;
            } else if (is_false_value(value)) {
                options->enable_smart_typography = false;
            }
        } else if (strcasecmp(key, "math") == 0) {
            if (is_true_value(value)) {
                options->enable_math = true;
            } else if (is_false_value(value)) {
                options->enable_math = false;
            }
        } else if (strcasecmp(key, "callouts") == 0) {
            if (is_true_value(value)) {
                options->enable_callouts = true;
            } else if (is_false_value(value)) {
                options->enable_callouts = false;
            }
        } else if (strcasecmp(key, "py-callouts") == 0 || strcasecmp(key, "py_callouts") == 0) {
            if (is_true_value(value)) {
                options->enable_py_callouts = true;
            } else if (is_false_value(value)) {
                options->enable_py_callouts = false;
            }
        } else if (strcasecmp(key, "quarto-extensions") == 0 || strcasecmp(key, "quarto_extensions") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_extensions = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_extensions = false;
            }
        } else if (strcasecmp(key, "quarto-raw") == 0 || strcasecmp(key, "quarto_raw") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_raw = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_raw = false;
            }
        } else if (strcasecmp(key, "quarto-list-continuation") == 0 || strcasecmp(key, "quarto_list_continuation") == 0 ||
                   strcasecmp(key, "quarto-example-lists") == 0 || strcasecmp(key, "quarto_example_lists") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_example_lists = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_example_lists = false;
            }
        } else if (strcasecmp(key, "quarto-line-blocks") == 0 || strcasecmp(key, "quarto_line_blocks") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_line_blocks = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_line_blocks = false;
            }
        } else if (strcasecmp(key, "quarto-roman-lists") == 0 || strcasecmp(key, "quarto_roman_lists") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_roman_lists = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_roman_lists = false;
            }
        } else if (strcasecmp(key, "quarto-code-attrs") == 0 || strcasecmp(key, "quarto_code_attrs") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_code_attrs = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_code_attrs = false;
            }
        } else if (strcasecmp(key, "quarto-strict-lists") == 0 || strcasecmp(key, "quarto_strict_lists") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_strict_lists = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_strict_lists = false;
            }
        } else if (strcasecmp(key, "quarto-xrefs") == 0 || strcasecmp(key, "quarto_xrefs") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_xrefs = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_xrefs = false;
            }
        } else if (strcasecmp(key, "quarto-callouts") == 0 || strcasecmp(key, "quarto_callouts") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_callouts = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_callouts = false;
            }
        } else if (strcasecmp(key, "quarto-diagrams") == 0 || strcasecmp(key, "quarto_diagrams") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_diagrams = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_diagrams = false;
            }
        } else if (strcasecmp(key, "quarto-shortcodes") == 0 || strcasecmp(key, "quarto_shortcodes") == 0) {
            if (is_true_value(value)) {
                options->enable_quarto_shortcodes = true;
            } else if (is_false_value(value)) {
                options->enable_quarto_shortcodes = false;
            }
        } else if (strcasecmp(key, "divs") == 0) {
            if (is_true_value(value)) {
                options->enable_divs = true;
            } else if (is_false_value(value)) {
                options->enable_divs = false;
            }
        } else if (strcasecmp(key, "spans") == 0) {
            if (is_true_value(value)) {
                options->enable_spans = true;
            } else if (is_false_value(value)) {
                options->enable_spans = false;
            }
        } else if (strcasecmp(key, "ids") == 0 || strcasecmp(key, "header-ids") == 0) {
            if (is_true_value(value)) {
                options->generate_header_ids = true;
            } else if (is_false_value(value)) {
                options->generate_header_ids = false;
            }
        } else if (strcasecmp(key, "header-anchors") == 0 || strcasecmp(key, "header_anchors") == 0) {
            if (is_true_value(value)) {
                options->header_anchors = true;
            } else if (is_false_value(value)) {
                options->header_anchors = false;
            }
        } else if (strcasecmp(key, "embed-images") == 0 || strcasecmp(key, "embed_images") == 0) {
            if (is_true_value(value)) {
                options->embed_images = true;
            } else if (is_false_value(value)) {
                options->embed_images = false;
            }
        } else if (strcasecmp(key, "image-captions") == 0 || strcasecmp(key, "image_captions") == 0) {
            if (is_true_value(value)) {
                options->enable_image_captions = true;
            } else if (is_false_value(value)) {
                options->enable_image_captions = false;
            }
        } else if (strcasecmp(key, "title-captions-only") == 0 || strcasecmp(key, "title_captions_only") == 0) {
            if (is_true_value(value)) {
                options->title_captions_only = true;
                options->enable_image_captions = true;  /* implied when title-captions-only is set */
            } else if (is_false_value(value)) {
                options->title_captions_only = false;
            }
        } else if (strcasecmp(key, "link-citations") == 0 || strcasecmp(key, "link_citations") == 0) {
            if (is_true_value(value)) {
                options->link_citations = true;
            } else if (is_false_value(value)) {
                options->link_citations = false;
            }
        } else if (strcasecmp(key, "show-tooltips") == 0 || strcasecmp(key, "show_tooltips") == 0) {
            if (is_true_value(value)) {
                options->show_tooltips = true;
            } else if (is_false_value(value)) {
                options->show_tooltips = false;
            }
        } else if (strcasecmp(key, "suppress-bibliography") == 0 || strcasecmp(key, "suppress_bibliography") == 0) {
            if (is_true_value(value)) {
                options->suppress_bibliography = true;
            } else if (is_false_value(value)) {
                options->suppress_bibliography = false;
            }
        } else if (strcasecmp(key, "suppress-index") == 0 || strcasecmp(key, "suppress_index") == 0) {
            if (is_true_value(value)) {
                options->suppress_index = true;
            } else if (is_false_value(value)) {
                options->suppress_index = false;
            }
        } else if (strcasecmp(key, "group-index-by-letter") == 0 || strcasecmp(key, "group_index_by_letter") == 0) {
            if (is_true_value(value)) {
                options->group_index_by_letter = true;
            } else if (is_false_value(value)) {
                options->group_index_by_letter = false;
            }
        } else if (strcasecmp(key, "obfuscate-emails") == 0 || strcasecmp(key, "obfuscate_emails") == 0) {
            if (is_true_value(value)) {
                options->obfuscate_emails = true;
            } else if (is_false_value(value)) {
                options->obfuscate_emails = false;
            }
        } else if (strcasecmp(key, "pretty") == 0) {
            if (is_true_value(value)) {
                options->pretty = true;
            } else if (is_false_value(value)) {
                options->pretty = false;
            }
        } else if (strcasecmp(key, "standalone") == 0) {
            if (is_true_value(value)) {
                options->standalone = true;
            } else if (is_false_value(value)) {
                options->standalone = false;
            }
        } else if (strcasecmp(key, "hardbreaks") == 0 || strcasecmp(key, "hard-breaks") == 0) {
            if (is_true_value(value)) {
                options->hardbreaks = true;
            } else if (is_false_value(value)) {
                options->hardbreaks = false;
            }
        } else if (strcasecmp(key, "widont") == 0) {
            if (is_true_value(value)) {
                options->enable_widont = true;
            } else if (is_false_value(value)) {
                options->enable_widont = false;
            }
        } else if (strcasecmp(key, "code-is-poetry") == 0 || strcasecmp(key, "code_is_poetry") == 0) {
            if (is_true_value(value)) {
                options->code_is_poetry = true;
                options->highlight_language_only = true;
            } else if (is_false_value(value)) {
                options->code_is_poetry = false;
            }
        } else if (strcasecmp(key, "markdown-in-html") == 0 || strcasecmp(key, "markdown_in_html") == 0) {
            if (is_true_value(value)) {
                options->enable_markdown_in_html = true;
            } else if (is_false_value(value)) {
                options->enable_markdown_in_html = false;
            }
        } else if (strcasecmp(key, "random-footnote-ids") == 0 || strcasecmp(key, "random_footnote_ids") == 0) {
            if (is_true_value(value)) {
                options->random_footnote_ids = true;
            } else if (is_false_value(value)) {
                options->random_footnote_ids = false;
            }
        } else if (strcasecmp(key, "hashtags") == 0) {
            if (is_true_value(value)) {
                options->enable_hashtags = true;
            } else if (is_false_value(value)) {
                options->enable_hashtags = false;
            }
        } else if (strcasecmp(key, "style-hashtags") == 0 || strcasecmp(key, "style_hashtags") == 0) {
            if (is_true_value(value)) {
                options->style_hashtags = true;
            } else if (is_false_value(value)) {
                options->style_hashtags = false;
            }
        } else if (strcasecmp(key, "proofreader") == 0) {
            if (is_true_value(value)) {
                options->proofreader_mode = true;
                options->enable_critic_markup = true;
                options->critic_mode = 2; /* markup mode */
            } else if (is_false_value(value)) {
                options->proofreader_mode = false;
            }
        } else if (strcasecmp(key, "hr-page-break") == 0 || strcasecmp(key, "hr_page_break") == 0) {
            if (is_true_value(value)) {
                options->hr_page_break = true;
            } else if (is_false_value(value)) {
                options->hr_page_break = false;
            }
        } else if (strcasecmp(key, "title-from-h1") == 0 || strcasecmp(key, "title_from_h1") == 0) {
            if (is_true_value(value)) {
                options->title_from_h1 = true;
            } else if (is_false_value(value)) {
                options->title_from_h1 = false;
            }
        } else if (strcasecmp(key, "page-break-before-footnotes") == 0 || strcasecmp(key, "page_break_before_footnotes") == 0) {
            if (is_true_value(value)) {
                options->page_break_before_footnotes = true;
            } else if (is_false_value(value)) {
                options->page_break_before_footnotes = false;
            }
        }
        /* String options */
        else if (strcasecmp(key, "bibliography") == 0) {
            /* Bibliography can be a single file or comma-separated list */
            /* Note: This will be handled by the citations extension, but we enable citations here */
            options->enable_citations = true;
            /* The actual bibliography file loading is handled in the citations extension */
        } else if (strcasecmp(key, "csl") == 0) {
            options->csl_file = value;
            options->enable_citations = true;
        } else if (strcasecmp(key, "title") == 0) {
            options->document_title = value;
        } else if (strcasecmp(key, "style") == 0 || strcasecmp(key, "css") == 0) {
            /* Metadata only supports single CSS file, so create array with one element */
            const char **css_array = malloc(2 * sizeof(const char*));
            if (css_array) {
                css_array[0] = value;
                css_array[1] = NULL;
                options->stylesheet_paths = css_array;
                options->stylesheet_count = 1;
            }
            options->standalone = true;  /* Imply standalone if CSS is specified */
        } else if (strcasecmp(key, "id-format") == 0 || strcasecmp(key, "id_format") == 0) {
            /* Convert string to enum: gfm=0, mmd=1, kramdown=2 */
            char *lower = strdup(value);
            if (lower) {
                for (char *p = lower; *p; p++) {
                    *p = (char)tolower((unsigned char)*p);
                }
                if (strcmp(lower, "gfm") == 0) {
                    options->id_format = 0;
                } else if (strcmp(lower, "mmd") == 0) {
                    options->id_format = 1;
                } else if (strcmp(lower, "kramdown") == 0) {
                    options->id_format = 2;
                }
                free(lower);
            }
        } else if (strcasecmp(key, "toc-min-max") == 0 || strcasecmp(key, "toc_min_max") == 0) {
            int min = 0, max = 0;
            if (sscanf(value, "%d,%d", &min, &max) == 2 &&
                min >= 1 && max <= 6 && min <= max) {
                options->toc_min = min;
                options->toc_max = max;
            }
        } else if (strcasecmp(key, "base-dir") == 0 || strcasecmp(key, "base_dir") == 0) {
            options->base_directory = value;
        } else if (strcasecmp(key, "wikilink-space") == 0 || strcasecmp(key, "wikilink_space") == 0) {
            /* Convert string to enum: dash=0, none=1, underscore=2, space=3 */
            char *lower = strdup(value);
            if (lower) {
                for (char *p = lower; *p; p++) {
                    *p = (char)tolower((unsigned char)*p);
                }
                if (strcmp(lower, "dash") == 0) {
                    options->wikilink_space = 0;
                } else if (strcmp(lower, "none") == 0) {
                    options->wikilink_space = 1;
                } else if (strcmp(lower, "underscore") == 0) {
                    options->wikilink_space = 2;
                } else if (strcmp(lower, "space") == 0) {
                    options->wikilink_space = 3;
                }
                free(lower);
            }
        } else if (strcasecmp(key, "wikilink-extension") == 0 || strcasecmp(key, "wikilink_extension") == 0) {
            options->wikilink_extension = value;
        } else if (strcasecmp(key, "wikilink-sanitize") == 0 || strcasecmp(key, "wikilink_sanitize") == 0) {
            options->wikilink_sanitize = (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
        /* Terminal-specific options from config/metadata.
         * Keys are typically flattened from YAML mappings, e.g.:
         * terminal.theme  (from terminal: { theme: brett })
         * terminal.width  (from terminal: { width: 80 })
         */
        else if (strcasecmp(key, "terminal.theme") == 0 ||
                 strcasecmp(key, "terminal_theme") == 0) {
            /* Only set theme_name if it wasn't already set by CLI or previous metadata. */
            if (!options->theme_name || !options->theme_name[0]) {
                options->theme_name = value;
            }
        } else if (strcasecmp(key, "terminal.width") == 0 ||
                   strcasecmp(key, "terminal_width") == 0) {
            int w = atoi(value);
            if (w > 0) {
                options->terminal_width = w;
            }
        } else if (strcasecmp(key, "terminal.inline_images") == 0 ||
                   strcasecmp(key, "terminal_inline_images") == 0) {
            if (is_true_value(value)) {
                options->terminal_inline_images = true;
            } else if (is_false_value(value)) {
                options->terminal_inline_images = false;
            }
        } else if (strcasecmp(key, "terminal.image_width") == 0 ||
                   strcasecmp(key, "terminal_image_width") == 0) {
            int iw = atoi(value);
            if (iw > 0) {
                options->terminal_image_width = iw;
            }
        } else if (strcasecmp(key, "paginate") == 0 ||
                   strcasecmp(key, "terminal.paginate") == 0 ||
                   strcasecmp(key, "terminal_paginate") == 0) {
            if (strcasecmp(value, "symbols") == 0) {
                options->paginate = true;
                options->paginate_symbols = true;
            } else if (is_true_value(value)) {
                options->paginate = true;
                options->paginate_symbols = false;
            } else if (is_false_value(value)) {
                options->paginate = false;
                options->paginate_symbols = false;
            }
        }
        /* Syntax highlighting options */
        else if (strcasecmp(key, "code-highlight") == 0 || strcasecmp(key, "code_highlight") == 0) {
            /* Accept full names and abbreviations */
            char *lower = strdup(value);
            if (lower) {
                for (char *p = lower; *p; p++) {
                    *p = (char)tolower((unsigned char)*p);
                }
                if (strcmp(lower, "pygments") == 0 || strcmp(lower, "p") == 0 || strcmp(lower, "pyg") == 0) {
                    options->code_highlighter = "pygments";
                } else if (strcmp(lower, "skylighting") == 0 || strcmp(lower, "s") == 0 || strcmp(lower, "sky") == 0) {
                    options->code_highlighter = "skylighting";
                } else if (strcmp(lower, "shiki") == 0 || strcmp(lower, "sh") == 0) {
                    options->code_highlighter = "shiki";
                } else if (is_false_value(lower) || strcmp(lower, "none") == 0) {
                    options->code_highlighter = NULL;
                }
                free(lower);
            }
        } else if (strcasecmp(key, "code-line-numbers") == 0 || strcasecmp(key, "code_line_numbers") == 0) {
            if (is_true_value(value)) {
                options->code_line_numbers = true;
            } else if (is_false_value(value)) {
                options->code_line_numbers = false;
            }
        } else if (strcasecmp(key, "highlight-language-only") == 0 || strcasecmp(key, "highlight_language_only") == 0) {
            if (is_true_value(value)) {
                options->highlight_language_only = true;
            } else if (is_false_value(value)) {
                options->highlight_language_only = false;
            }
        } else if (strcasecmp(key, "code-highlight-theme") == 0 || strcasecmp(key, "code_highlight_theme") == 0) {
            /* Theme name for external highlighters (tool-specific). */
            options->code_highlight_theme = value;
        }

        item = item->next;
    }
}

#ifdef APEX_HAVE_LIBYAML
/* Convert a YAML mapping node to a flat metadata item list */
static apex_metadata_item *yaml_mapping_to_metadata_items(yaml_document_t *doc, yaml_node_t *mapping_node) {
    if (!mapping_node || mapping_node->type != YAML_MAPPING_NODE) {
        return NULL;
    }

    apex_metadata_item *items = NULL;
    yaml_node_pair_t *pair = mapping_node->data.mapping.pairs.start;
    while (pair < mapping_node->data.mapping.pairs.top) {
        yaml_node_t *key_node = yaml_document_get_node(doc, pair->key);
        yaml_node_t *value_node = yaml_document_get_node(doc, pair->value);

        if (key_node && key_node->type == YAML_SCALAR_NODE && value_node) {
            char *key = (char *)key_node->data.scalar.value;
            size_t key_len = key_node->data.scalar.length;
            char *key_str = malloc(key_len + 1);
            if (key_str) {
                memcpy(key_str, key, key_len);
                key_str[key_len] = '\0';

                if (value_node->type == YAML_SCALAR_NODE) {
                    char *value = (char *)value_node->data.scalar.value;
                    size_t value_len = value_node->data.scalar.length;
                    char *value_str = malloc(value_len + 1);
                    if (value_str) {
                        memcpy(value_str, value, value_len);
                        value_str[value_len] = '\0';
                        add_metadata_item(&items, key_str, value_str);
                        free(value_str);
                    }
                } else if (value_node->type == YAML_MAPPING_NODE) {
                    /* For nested mappings like handler: { command: "..." }, flatten with dot notation */
                    yaml_node_pair_t *nested_pair = value_node->data.mapping.pairs.start;
                    while (nested_pair < value_node->data.mapping.pairs.top) {
                        yaml_node_t *nested_key = yaml_document_get_node(doc, nested_pair->key);
                        yaml_node_t *nested_value = yaml_document_get_node(doc, nested_pair->value);
                        if (nested_key && nested_key->type == YAML_SCALAR_NODE &&
                            nested_value && nested_value->type == YAML_SCALAR_NODE) {
                            char *nested_key_str = (char *)nested_key->data.scalar.value;
                            size_t nested_key_len = nested_key->data.scalar.length;
                            char *nested_value_str = (char *)nested_value->data.scalar.value;
                            size_t nested_value_len = nested_value->data.scalar.length;

                            char *full_key = malloc(key_len + nested_key_len + 2);
                            char *full_value = malloc(nested_value_len + 1);
                            if (full_key && full_value) {
                                memcpy(full_key, key_str, key_len);
                                full_key[key_len] = '.';
                                memcpy(full_key + key_len + 1, nested_key_str, nested_key_len);
                                full_key[key_len + nested_key_len + 1] = '\0';
                                memcpy(full_value, nested_value_str, nested_value_len);
                                full_value[nested_value_len] = '\0';
                                add_metadata_item(&items, full_key, full_value);
                            }
                            free(full_key);
                            free(full_value);
                        }
                        nested_pair++;
                    }
                }

                free(key_str);
            }
        }
        pair++;
    }
    return items;
}

yaml_document_t *apex_load_yaml_document(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size > 1024 * 1024) {
        fclose(fp);
        return NULL;
    }

    char *buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    /* Skip YAML front matter markers if present */
    const char *yaml_start = buffer;
    size_t yaml_len = bytes_read;
    if (bytes_read >= 3 && strncmp(buffer, "---", 3) == 0) {
        const char *newline = strchr(buffer + 3, '\n');
        if (newline) {
            yaml_start = newline + 1;
            yaml_len = bytes_read - (yaml_start - buffer);
        } else {
            yaml_start = buffer + 3;
            yaml_len = bytes_read - 3;
        }

        /* Find closing --- */
        const char *end_marker = strstr(yaml_start, "\n---");
        if (!end_marker) {
            end_marker = strstr(yaml_start, "\n...");
        }
        if (end_marker && end_marker > yaml_start) {
            yaml_len = (size_t)(end_marker - yaml_start);
        }
    }

    yaml_parser_t parser;
    yaml_document_t *document = malloc(sizeof(yaml_document_t));
    if (!document) {
        free(buffer);
        return NULL;
    }

    if (!yaml_parser_initialize(&parser)) {
        free(buffer);
        free(document);
        return NULL;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_start, yaml_len);

    if (!yaml_parser_load(&parser, document)) {
        yaml_parser_delete(&parser);
        free(buffer);
        free(document);
        return NULL;
    }

    yaml_parser_delete(&parser);
    free(buffer);
    return document;
}

apex_metadata_item **apex_extract_plugin_bundle(const char *filepath, size_t *count) {
    *count = 0;
    if (!filepath) return NULL;

    yaml_document_t *doc = apex_load_yaml_document(filepath);
    if (!doc) return NULL;

    yaml_node_t *root = yaml_document_get_root_node(doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        yaml_document_delete(doc);
        free(doc);
        return NULL;
    }

    /* Find "bundle" key */
    yaml_node_pair_t *pair = root->data.mapping.pairs.start;
    yaml_node_t *bundle_node = NULL;
    while (pair < root->data.mapping.pairs.top) {
        yaml_node_t *key_node = yaml_document_get_node(doc, pair->key);
        if (key_node && key_node->type == YAML_SCALAR_NODE) {
            char *key = (char *)key_node->data.scalar.value;
            if (strcmp(key, "bundle") == 0) {
                bundle_node = yaml_document_get_node(doc, pair->value);
                break;
            }
        }
        pair++;
    }

    if (!bundle_node || bundle_node->type != YAML_SEQUENCE_NODE) {
        yaml_document_delete(doc);
        free(doc);
        return NULL;
    }

    /* Extract each bundle entry */
    size_t bundle_count = bundle_node->data.sequence.items.top - bundle_node->data.sequence.items.start;
    apex_metadata_item **bundles = calloc(bundle_count, sizeof(apex_metadata_item *));
    if (!bundles) {
        yaml_document_delete(doc);
        free(doc);
        return NULL;
    }

    int idx = 0;
    yaml_node_item_t *item = bundle_node->data.sequence.items.start;
    while (item < bundle_node->data.sequence.items.top) {
        yaml_node_t *entry = yaml_document_get_node(doc, *item);
        if (entry && entry->type == YAML_MAPPING_NODE) {
            bundles[idx] = yaml_mapping_to_metadata_items(doc, entry);
            idx++;
        }
        item++;
    }

    *count = (size_t)idx;
    yaml_document_delete(doc);
    free(doc);
    return bundles;
}
#endif
