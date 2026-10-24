/**
 * Table of Contents Extension for Apex
 * Implementation
 */

#include "toc.h"
#include "header_ids.h"
#include "apex/apex.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* Placeholders for backslash-escaped {{TOC...}} markers preserved through parsing. */
static char **escaped_toc_originals = NULL;
static size_t escaped_toc_count = 0;

static void clear_escaped_toc_store(void) {
    if (escaped_toc_originals) {
        for (size_t i = 0; i < escaped_toc_count; i++) {
            free(escaped_toc_originals[i]);
        }
        free(escaped_toc_originals);
    }
    escaped_toc_originals = NULL;
    escaped_toc_count = 0;
}

static bool is_escaped_mmd_toc_start(const char *p) {
    return p[0] == '\\' && p[1] == '{' && p[2] == '\\' && p[3] == '{' &&
           strncmp(p + 4, "TOC", 3) == 0;
}

static const char *find_escaped_mmd_toc_end(const char *start) {
    const char *p = start + 7;
    while (*p) {
        if (p[0] == '\\' && p[1] == '}' && p[2] == '\\' && p[3] == '}') {
            return p + 3;
        }
        p++;
    }
    return NULL;
}

static char *build_unescaped_toc_marker(const char *escaped_start, const char *escaped_end) {
    /* \{\{ ... \}\} -> {{ ... }} */
    const char *inner = escaped_start + 4;
    size_t inner_len = (size_t)(escaped_end - inner - 3);
    size_t out_len = inner_len + 4;
    char *out = malloc(out_len + 1);
    if (!out) return NULL;
    out[0] = '{';
    out[1] = '{';
    memcpy(out + 2, inner, inner_len);
    out[2 + inner_len] = '}';
    out[3 + inner_len] = '}';
    out[out_len] = '\0';
    return out;
}

char *apex_protect_escaped_toc_markers(const char *text) {
    if (!text) return NULL;

    clear_escaped_toc_store();

    size_t len = strlen(text);
    size_t capacity = len + 64;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        if (is_escaped_mmd_toc_start(read)) {
            const char *end = find_escaped_mmd_toc_end(read);
            if (end) {
                char *original = build_unescaped_toc_marker(read, end);
                if (!original) {
                    free(output);
                    clear_escaped_toc_store();
                    return NULL;
                }

                char **new_store = realloc(escaped_toc_originals,
                                           (escaped_toc_count + 1) * sizeof(char *));
                if (!new_store) {
                    free(original);
                    free(output);
                    clear_escaped_toc_store();
                    return NULL;
                }
                escaped_toc_originals = new_store;
                escaped_toc_originals[escaped_toc_count++] = original;

                char placeholder[64];
                int ph_len = snprintf(placeholder, sizeof(placeholder),
                                      "@APEX_ESCAPED_TOC_%zu@", escaped_toc_count - 1);
                if ((size_t)ph_len >= remaining) {
                    size_t written = (size_t)(write - output);
                    capacity = written + (size_t)ph_len + len + 64;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        clear_escaped_toc_store();
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }
                memcpy(write, placeholder, (size_t)ph_len);
                write += ph_len;
                remaining -= (size_t)ph_len;
                read = end + 1;
                continue;
            }
        }

        if (remaining == 0) {
            size_t written = (size_t)(write - output);
            capacity = (written + len + 64) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                clear_escaped_toc_store();
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
    return output;
}

char *apex_restore_escaped_toc_markers(const char *html) {
    if (!html) return NULL;
    if (escaped_toc_count == 0) return strdup(html);

    size_t len = strlen(html);
    size_t capacity = len + escaped_toc_count * 32;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        if (strncmp(read, "@APEX_ESCAPED_TOC_", 18) == 0) {
            const char *idx_start = read + 18;
            const char *idx_end = strchr(idx_start, '@');
            if (idx_end && idx_end > idx_start) {
                char idx_buf[32];
                size_t idx_len = (size_t)(idx_end - idx_start);
                if (idx_len < sizeof(idx_buf)) {
                    memcpy(idx_buf, idx_start, idx_len);
                    idx_buf[idx_len] = '\0';
                    size_t index = (size_t)atoi(idx_buf);
                    if (index < escaped_toc_count && escaped_toc_originals[index]) {
                        const char *original = escaped_toc_originals[index];
                        size_t orig_len = strlen(original);
                        if (orig_len >= remaining) {
                            size_t written = (size_t)(write - output);
                            capacity = written + orig_len + 64;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(output);
                                return NULL;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        memcpy(write, original, orig_len);
                        write += orig_len;
                        remaining -= orig_len;
                        read = idx_end + 1;
                        continue;
                    }
                }
            }
        }

        if (remaining == 0) {
            size_t written = (size_t)(write - output);
            capacity = (written + len + 64) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
    clear_escaped_toc_store();
    return output;
}

/**
 * Collect headers from AST
 */
typedef struct header_item {
    int level;
    char *text;
    char *id;
    struct header_item *next;
} header_item;

/* Normalize heading text for TOC labels:
 * - trim leading/trailing whitespace
 * - collapse internal whitespace runs to a single space
 */
static char *normalize_toc_text(const char *text) {
    if (!text) return strdup("");

    size_t len = strlen(text);
    char *out = malloc(len + 1);
    if (!out) return strdup("");

    const unsigned char *read = (const unsigned char *)text;
    while (*read && isspace(*read)) read++;

    char *write = out;
    int pending_space = 0;

    while (*read) {
        if (isspace(*read)) {
            pending_space = 1;
        } else {
            if (pending_space && write > out) {
                *write++ = ' ';
            }
            *write++ = (char)*read;
            pending_space = 0;
        }
        read++;
    }

    *write = '\0';
    return out;
}

static char *heading_id_from_attrs_or_text(const char *attrs, const char *text,
                                           apex_id_format_t id_format) {
    if (attrs) {
        const char *id_attr = strstr(attrs, "id=\"");
        if (id_attr) {
            const char *id_start = id_attr + 4;
            const char *id_end = strchr(id_start, '"');
            if (id_end && id_end > id_start) {
                size_t id_len = (size_t)(id_end - id_start);
                char *id = malloc(id_len + 1);
                if (id) {
                    memcpy(id, id_start, id_len);
                    id[id_len] = '\0';
                    return id;
                }
            }
        }
    }
    return apex_generate_header_id(text, id_format);
}

static void free_headers(header_item *headers) {
    while (headers) {
        header_item *next = headers->next;
        free(headers->text);
        free(headers->id);
        free(headers);
        headers = next;
    }
}

static void clamp_toc_levels(int *min_level, int *max_level) {
    if (*min_level < 1) *min_level = 1;
    if (*max_level < 1) *max_level = 1;
    if (*min_level > 6) *min_level = 6;
    if (*max_level > 6) *max_level = 6;
    if (*min_level > *max_level) *min_level = *max_level;
}

static bool append_toc_markdown(char **buffer, size_t *capacity, size_t *length,
                                const char *text, size_t text_len) {
    if (!buffer || !*buffer || !capacity || !length) return false;

    if (*length + text_len + 1 > *capacity) {
        size_t new_capacity = *capacity;
        while (*length + text_len + 1 > new_capacity) {
            new_capacity *= 2;
        }
        char *new_buffer = realloc(*buffer, new_capacity);
        if (!new_buffer) return false;
        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, text, text_len);
    *length += text_len;
    (*buffer)[*length] = '\0';
    return true;
}



/**
 * Collect all headers from document
 */
static header_item *collect_headers(cmark_node *node, header_item **tail,
                                    apex_id_format_t id_format) {
    if (!node) return NULL;

    header_item *headers = NULL;
    if (!tail) tail = &headers;

    /* Check if current node is a header */
    if (cmark_node_get_type(node) == CMARK_NODE_HEADING) {
        /* Skip headings marked with the Kramdown-style ".no_toc" class.
         * The IAL processor stores attributes as a raw HTML attribute
         * string in the node's user_data, e.g. id="..." class="a b no_toc".
         * If we see "no_toc" in the attribute string, we exclude this
         * heading from the generated table of contents.
         */
        const char *attrs = (const char *)cmark_node_get_user_data(node);
        if (!(attrs && strstr(attrs, "no_toc") != NULL)) {
            header_item *item = malloc(sizeof(header_item));
            if (item) {
                item->level = cmark_node_get_heading_level(node);
                char *raw_text = apex_extract_heading_text(node);
                item->text = normalize_toc_text(raw_text);
                free(raw_text);
                item->id = heading_id_from_attrs_or_text(attrs, item->text, id_format);
                item->next = NULL;

                if (*tail) {
                    (*tail)->next = item;
                } else {
                    headers = item;
                }
                *tail = item;
            }
        }
    }

    /* Recursively process children */
    for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        header_item *child_headers = collect_headers(child, tail, id_format);
        if (!headers) headers = child_headers;
    }

    return headers;
}

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
} toc_buffer;

static void toc_buffer_append(toc_buffer *buf, const char *str) {
    if (buf->failed || !str) return;
    size_t len = strlen(str);
    if (buf->length + len + 1 > buf->capacity) {
        size_t capacity = buf->capacity ? buf->capacity : 4096;
        while (buf->length + len + 1 > capacity) capacity *= 2;
        char *grown = realloc(buf->data, capacity);
        if (!grown) {
            buf->failed = true;
            return;
        }
        buf->data = grown;
        buf->capacity = capacity;
    }
    memcpy(buf->data + buf->length, str, len);
    buf->length += len;
    buf->data[buf->length] = '\0';
}

/**
 * Generate TOC HTML from headers
 * Produces valid ul > li > ul nesting (nested lists inside list items)
 */
static char *generate_toc_html(header_item *headers, int min_level, int max_level) {
    if (!headers) return strdup("");

    toc_buffer buf = {0};
    /* depth: open <ul> elements. level_at[d] is the shallowest heading level
     * listed at depth d. A deeper heading nests one level under the open <li>,
     * however many heading levels it skips; headings at that same level stay
     * siblings. */
    int depth = 0;
    int level_at[8] = {0};
    bool li_open = false;

    toc_buffer_append(&buf, "<nav class=\"toc\">\n");

    for (header_item *h = headers; h; h = h->next) {
        if (h->level < min_level || h->level > max_level) continue;
        int target_level = h->level - min_level + 1;
        if (target_level < 1) target_level = 1;

        if (depth == 0) {
            toc_buffer_append(&buf, "<ul>\n");
            depth = 1;
            level_at[depth] = target_level;
        } else if (target_level > level_at[depth] && li_open && depth < 7) {
            toc_buffer_append(&buf, "<ul>\n");
            depth++;
            level_at[depth] = target_level;
            li_open = false;
        } else {
            while (depth > 1 && target_level <= level_at[depth - 1]) {
                if (li_open) toc_buffer_append(&buf, "</li>\n");
                toc_buffer_append(&buf, "</ul>\n");
                depth--;
                li_open = true;
            }
            if (li_open) toc_buffer_append(&buf, "</li>\n");
            if (target_level < level_at[depth]) level_at[depth] = target_level;
        }

        toc_buffer_append(&buf, "<li><a href=\"#");
        toc_buffer_append(&buf, h->id);
        toc_buffer_append(&buf, "\">");
        toc_buffer_append(&buf, h->text);
        toc_buffer_append(&buf, "</a>");
        li_open = true;
    }

    while (depth > 0) {
        if (li_open) toc_buffer_append(&buf, "</li>\n");
        toc_buffer_append(&buf, "</ul>\n");
        depth--;
        li_open = depth > 0;
    }

    toc_buffer_append(&buf, "</nav>\n");

    if (buf.failed || !buf.data) {
        free(buf.data);
        return strdup("");
    }
    return buf.data;
}

void apex_toc_entries_free(apex_toc_entry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; i++) {
        free(entries[i].text);
        free(entries[i].id);
    }
    free(entries);
}

apex_toc_entry *apex_generate_toc_entries(cmark_node *document, int id_format,
                                          int min_level, int max_level,
                                          size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!document || !out_count) return NULL;

    clamp_toc_levels(&min_level, &max_level);

    header_item *tail = NULL;
    header_item *headers = collect_headers(document, &tail, (apex_id_format_t)id_format);
    if (!headers) return NULL;

    size_t count = 0;
    for (header_item *h = headers; h; h = h->next) {
        if (h->level >= min_level && h->level <= max_level) count++;
    }

    if (count == 0) {
        free_headers(headers);
        return NULL;
    }

    apex_toc_entry *entries = calloc(count, sizeof(apex_toc_entry));
    if (!entries) {
        free_headers(headers);
        return NULL;
    }

    size_t idx = 0;
    for (header_item *h = headers; h; h = h->next) {
        if (h->level < min_level || h->level > max_level) continue;
        entries[idx].level = h->level;
        entries[idx].text = h->text ? strdup(h->text) : strdup("");
        entries[idx].id = h->id ? strdup(h->id) : strdup("");
        if (!entries[idx].text || !entries[idx].id) {
            apex_toc_entries_free(entries, idx + 1);
            free_headers(headers);
            return NULL;
        }
        idx++;
    }

    free_headers(headers);
    *out_count = count;
    return entries;
}

char *apex_generate_toc_markdown(cmark_node *document, int id_format,
                                 int min_level, int max_level) {
    size_t count = 0;
    apex_toc_entry *entries = apex_generate_toc_entries(document, id_format,
                                                        min_level, max_level,
                                                        &count);
    if (!entries || count == 0) {
        apex_toc_entries_free(entries, count);
        return strdup("");
    }

    size_t capacity = 1024;
    size_t length = 0;
    char *markdown = malloc(capacity);
    if (!markdown) {
        apex_toc_entries_free(entries, count);
        return strdup("");
    }
    markdown[0] = '\0';

    clamp_toc_levels(&min_level, &max_level);

    for (size_t i = 0; i < count; i++) {
        int indent = (entries[i].level - min_level) * 2;
        for (int j = 0; j < indent; j++) {
            if (!append_toc_markdown(&markdown, &capacity, &length, " ", 1)) goto fail;
        }

        const char *text = entries[i].text ? entries[i].text : "";
        const char *id = entries[i].id ? entries[i].id : "";
        if (!append_toc_markdown(&markdown, &capacity, &length, "- [", 3)) goto fail;
        if (!append_toc_markdown(&markdown, &capacity, &length, text, strlen(text))) goto fail;
        if (!append_toc_markdown(&markdown, &capacity, &length, "](#", 3)) goto fail;
        if (!append_toc_markdown(&markdown, &capacity, &length, id, strlen(id))) goto fail;
        if (!append_toc_markdown(&markdown, &capacity, &length, ")\n", 2)) goto fail;
    }

    apex_toc_entries_free(entries, count);
    return markdown;

fail:
    free(markdown);
    apex_toc_entries_free(entries, count);
    return strdup("");
}

/**
 * Return true if position 'pos' in 'html' is inside a <code> or <pre> element.
 * Used to skip TOC markers that appear in code blocks or inline code.
 */
static int is_inside_code_or_pre(const char *html, size_t pos) {
    int in_code = 0;
    int in_pre = 0;
    size_t i = 0;
    size_t len = pos;

    while (i < len) {
        if (html[i] == '<') {
            if (i + 5 <= len && (html[i+1] == 'c' || html[i+1] == 'C') &&
                (html[i+2] == 'o' || html[i+2] == 'O') &&
                (html[i+3] == 'd' || html[i+3] == 'D') &&
                (html[i+4] == 'e' || html[i+4] == 'E')) {
                char next = (i + 5 < len) ? html[i + 5] : '\0';
                if (next == '>' || next == ' ' || next == '\t' || next == '\n') {
                    in_code++;
                    i += 5;
                    continue;
                }
            }
            if (i + 4 <= len && (html[i+1] == 'p' || html[i+1] == 'P') &&
                (html[i+2] == 'r' || html[i+2] == 'R') &&
                (html[i+3] == 'e' || html[i+3] == 'E')) {
                char next = (i + 4 < len) ? html[i + 4] : '\0';
                if (next == '>' || next == ' ' || next == '\t' || next == '\n') {
                    in_pre++;
                    i += 4;
                    continue;
                }
            }
            if (i + 7 <= len && html[i+1] == '/' &&
                (html[i+2] == 'c' || html[i+2] == 'C') &&
                (html[i+3] == 'o' || html[i+3] == 'O') &&
                (html[i+4] == 'd' || html[i+4] == 'D') &&
                (html[i+5] == 'e' || html[i+5] == 'E') && html[i+6] == '>') {
                if (in_code > 0) in_code--;
                i += 7;
                continue;
            }
            if (i + 6 <= len && html[i+1] == '/' &&
                (html[i+2] == 'p' || html[i+2] == 'P') &&
                (html[i+3] == 'r' || html[i+3] == 'R') &&
                (html[i+4] == 'e' || html[i+4] == 'E') && html[i+5] == '>') {
                if (in_pre > 0) in_pre--;
                i += 6;
                continue;
            }
        }
        i++;
    }
    return (in_code > 0 || in_pre > 0);
}

/**
 * Parse TOC marker for min/max levels
 */
static void parse_toc_marker(const char *marker, int *min_level, int *max_level,
                             int default_min, int default_max) {
    int specified_min = 0;
    int specified_max = 0;

    *min_level = default_min;
    *max_level = default_max;

    if (!marker) return;

    /* Only parse inside the marker, never beyond marker end into following HTML. */
    const char *marker_end = strstr(marker, "}}");
    if (!marker_end) marker_end = strstr(marker, "-->");
    if (!marker_end) {
        marker_end = marker + strlen(marker);
    }
    size_t marker_len = (size_t)(marker_end - marker);
    char *marker_copy = (char *)malloc(marker_len + 1);
    if (!marker_copy) return;
    memcpy(marker_copy, marker, marker_len);
    marker_copy[marker_len] = '\0';

    /* Look for max and min parameters */
    const char *max_str = strstr(marker_copy, "max");
    const char *min_str = strstr(marker_copy, "min");

    if (max_str) {
        max_str += 3;
        while (*max_str && !isdigit((unsigned char)*max_str)) max_str++;
        if (*max_str) {
            *max_level = atoi(max_str);
            specified_max = 1;
        }
    }

    if (min_str) {
        min_str += 3;
        while (*min_str && !isdigit((unsigned char)*min_str)) min_str++;
        if (*min_str) {
            *min_level = atoi(min_str);
            specified_min = 1;
        }
    }

    /* Check for Pandoc style {{TOC:2-5}} */
    const char *colon = strchr(marker_copy, ':');
    if (colon) {
        colon++;
        while (*colon && isspace((unsigned char)*colon)) colon++;
        if (isdigit((unsigned char)*colon)) {
            *min_level = atoi(colon);
            specified_min = 1;
            const char *dash = strchr(colon, '-');
            if (dash) {
                dash++;
                if (isdigit((unsigned char)*dash)) {
                    *max_level = atoi(dash);
                    specified_max = 1;
                }
            }
        }
    }
    free(marker_copy);

    if (specified_min || specified_max) {
        if (!specified_min) *min_level = 1;
        if (!specified_max) *max_level = 6;
    }

    clamp_toc_levels(min_level, max_level);
}

/**
 * Find the first TOC marker (<!--TOC or {{TOC) that is not inside <code> or <pre>.
 * Returns pointer to the marker, or NULL if none valid. *is_html_comment is set
 * to 1 for <!--TOC, 0 for {{TOC.
 */
static const char *find_toc_marker_not_in_code(const char *html, int *is_html_comment) {
    const char *p = html;
    *is_html_comment = 0;

    while (1) {
        const char *next_comment = strstr(p, "<!--TOC");
        const char *next_mmd = strstr(p, "{{TOC");

        /* No more markers */
        if (!next_comment && !next_mmd) return NULL;

        /* Pick the earlier of the two */
        const char *cand = NULL;
        if (next_comment && next_mmd) {
            cand = (next_comment < next_mmd) ? next_comment : next_mmd;
        } else {
            cand = next_comment ? next_comment : next_mmd;
        }

        if (!is_inside_code_or_pre(html, (size_t)(cand - html))) {
            *is_html_comment = (cand == next_comment);
            return cand;
        }

        /* This occurrence is inside code; skip past it and search again */
        if (cand == next_comment) {
            const char *end = strstr(cand, "-->");
            p = end ? end + 3 : cand + 1;
        } else {
            const char *end = strstr(cand, "}}");
            p = end ? end + 2 : cand + 1;
        }
    }
}

/**
 * Process TOC markers in HTML
 */
char *apex_process_toc(const char *html, cmark_node *document, int id_format,
                       int default_min, int default_max) {
    if (!html || !document) return html ? strdup(html) : NULL;

    int is_html_comment = 0;
    const char *marker = find_toc_marker_not_in_code(html, &is_html_comment);

    if (!marker) {
        return strdup(html);  /* No valid TOC marker (or all are in code), return as-is */
    }

    /* Collect headers from document */
    header_item *tail = NULL;
    header_item *headers = collect_headers(document, &tail, (apex_id_format_t)id_format);
    if (!headers) return strdup(html);

    /* Parse the marker for min/max levels */
    int min_level, max_level;
    parse_toc_marker(marker, &min_level, &max_level, default_min, default_max);

    /* Generate TOC HTML */
    char *toc_html = generate_toc_html(headers, min_level, max_level);
    free_headers(headers);

    if (!toc_html) return strdup(html);

    /* Replace marker with TOC */
    size_t html_len = strlen(html);
    size_t toc_len = strlen(toc_html);
    size_t output_capacity = html_len + toc_len + 100;
    char *output = malloc(output_capacity);
    if (!output) {
        free(toc_html);
        return strdup(html);
    }

    /* Find end of marker */
    const char *marker_end = NULL;
    if (is_html_comment) {
        marker_end = strstr(marker, "-->");
        if (marker_end) marker_end += 3;
    } else {
        marker_end = strstr(marker, "}}");
        if (marker_end) marker_end += 2;
    }

    if (!marker_end) {
        free(toc_html);
        free(output);
        return strdup(html);
    }

    /* Build output: before + TOC + after */
    size_t before_len = (size_t)(marker - html);
    size_t after_len = strlen(marker_end);

    memcpy(output, html, before_len);
    memcpy(output + before_len, toc_html, toc_len);
    memcpy(output + before_len + toc_len, marker_end, after_len + 1);

    free(toc_html);
    return output;
}

