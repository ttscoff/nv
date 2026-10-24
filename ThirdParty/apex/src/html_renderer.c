/**
 * Custom HTML Renderer for Apex
 * Implementation
 */

#include "html_renderer.h"
#include "table.h"  /* For CMARK_NODE_TABLE */
#include "extensions/header_ids.h"
#include <string.h>
#include <strings.h>  /* For strncasecmp */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

/**
 * Inject attributes into HTML opening tags
 * This postprocesses the HTML output to add attributes stored in user_data
 */
__attribute__((unused))
static char *inject_attributes_in_html(const char *html, cmark_node *document) {
    if (!html || !document) return html ? strdup(html) : NULL;

    /* For now, we'll use a simpler approach: */
    /* Since we can't easily modify cmark's renderer, we'll inject attributes */
    /* by pattern matching on the HTML output */

    /* This is a simplified implementation */
    /* A full implementation would require forking cmark's HTML renderer */

    return strdup(html);
}

/**
 * Walk AST and collect nodes with attributes
 */
typedef struct attr_node {
    cmark_node *node;
    char *attrs;
    cmark_node_type node_type;
    int element_index;  /* nth element of this type (0=first p, 1=second p, etc.) */
    char *text_fingerprint;  /* First 50 chars of text content for matching */
    struct attr_node *next;
} attr_node;

/**
 * Extract IAL attributes (id, class, key="value") from attribute string,
 * excluding internal attributes like data-caption, data-remove, colspan, rowspan
 * Returns a newly allocated string with just the IAL attributes, or NULL if none found
 */
static char *extract_ial_from_table_attrs(const char *attrs) {
    if (!attrs) return NULL;

    size_t result_cap = strlen(attrs) + 1;
    char *result = malloc(result_cap);
    if (!result) return NULL;
    char *write = result;
    *write = '\0';

    const char *p = attrs;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;

    while (*p) {
        const char *attr_start = p;

        /* Find end of attribute name or = */
        const char *attr_name_end = p;
        while (*attr_name_end && *attr_name_end != '=' && *attr_name_end != ' ' && *attr_name_end != '\t') {
            attr_name_end++;
        }

        size_t attr_name_len = attr_name_end - attr_start;

        /* Check if this is an internal attribute we should skip */
        bool skip = false;
        if (attr_name_len == 11 && strncmp(attr_start, "data-caption", 11) == 0) skip = true;
        else if (attr_name_len == 11 && strncmp(attr_start, "data-remove", 11) == 0) skip = true;
        else if (attr_name_len == 7 && strncmp(attr_start, "colspan", 7) == 0) skip = true;
        else if (attr_name_len == 7 && strncmp(attr_start, "rowspan", 7) == 0) skip = true;

        if (!skip) {
            /* This is an IAL attribute - find the full attribute (name="value" or name='value') */
            const char *attr_end = attr_name_end;
            if (*attr_end == '=') {
                attr_end++;
                if (*attr_end == '"' || *attr_end == '\'') {
                    char q = *attr_end;
                    attr_end++;
                    while (*attr_end && *attr_end != q) {
                        if (*attr_end == '\\' && *(attr_end + 1)) attr_end++;
                        attr_end++;
                    }
                    if (*attr_end == q) attr_end++;
                } else {
                    /* Unquoted value */
                    while (*attr_end && *attr_end != ' ' && *attr_end != '\t') attr_end++;
                }
            }

            /* Copy this attribute to result */
            size_t attr_len = attr_end - attr_start;
            if ((size_t)(write - result) + attr_len + 2 >= result_cap) {
                /* Need to realloc */
                size_t current_len = write - result;
                result_cap = (current_len + attr_len + 2) * 2;
                char *new_result = realloc(result, result_cap);
                if (!new_result) {
                    free(result);
                    return NULL;
                }
                result = new_result;
                write = result + current_len;
            }

            /* Add space before attribute if needed (always for first attribute, or if previous doesn't end with space) */
            if (write == result || (write > result && write[-1] != ' ')) {
                *write++ = ' ';
            }
            memcpy(write, attr_start, attr_len);
            write += attr_len;
            *write = '\0';

            p = attr_end;
        } else {
            /* Skip this attribute - find its end */
            if (*attr_name_end == '=') {
                attr_name_end++;
                if (*attr_name_end == '"' || *attr_name_end == '\'') {
                    char q = *attr_name_end;
                    attr_name_end++;
                    while (*attr_name_end && *attr_name_end != q) {
                        if (*attr_name_end == '\\' && *(attr_name_end + 1)) attr_name_end++;
                        attr_name_end++;
                    }
                    if (*attr_name_end == q) attr_name_end++;
                } else {
                    while (*attr_name_end && *attr_name_end != ' ' && *attr_name_end != '\t') attr_name_end++;
                }
            }
            p = attr_name_end;
        }

        /* Skip whitespace before next attribute */
        while (*p && isspace((unsigned char)*p)) p++;
    }

    if (write == result) {
        /* No IAL attributes found */
        free(result);
        return NULL;
    }

    return result;
}

/**
 * Extract value of an attribute from an HTML tag.
 * Returns newly allocated string or NULL. Caller must free.
 */
static char *extract_attr_from_tag(const char *tag_start, const char *tag_end, const char *attr_name) {
    size_t attr_len = strlen(attr_name);
    const char *p = tag_start;
    while (p < tag_end) {
        if ((p == tag_start || isspace((unsigned char)p[-1])) &&
            strncasecmp(p, attr_name, attr_len) == 0 && p[attr_len] == '=') {
            p += attr_len + 1;
            if (p >= tag_end) return NULL;
            char q = *p;
            if (q != '"' && q != '\'') return NULL;
            p++;
            const char *val_start = p;
            while (p < tag_end && *p != q) {
                if (*p == '\\' && p + 1 < tag_end) p++;
                p++;
            }
            if (p >= tag_end) return NULL;
            size_t len = (size_t)(p - val_start);
            char *out = malloc(len + 1);
            if (out) {
                memcpy(out, val_start, len);
                out[len] = '\0';
            }
            return out;
        }
        p++;
    }
    return NULL;
}

/**
 * Replace extension in URL path. Caller must free. Returns NULL if no extension.
 */
static char *url_with_extension(const char *url, const char *new_ext) {
    if (!url || !new_ext) return NULL;
    const char *last_dot = strrchr(url, '.');
    const char *path_end = strchr(url, '?');
    if (!path_end) path_end = strchr(url, '#');
    if (!path_end) path_end = url + strlen(url);
    if (!last_dot || last_dot >= path_end) return NULL;

    size_t prefix_len = (size_t)(last_dot - url);
    size_t ext_len = strlen(new_ext);
    size_t tail_len = strlen(path_end);
    char *out = malloc(prefix_len + 1 + ext_len + tail_len + 1);
    if (!out) return NULL;
    memcpy(out, url, prefix_len);
    out[prefix_len] = '.';
    memcpy(out + prefix_len + 1, new_ext, ext_len + 1);
    if (tail_len > 0) memcpy(out + prefix_len + 1 + ext_len, path_end, tail_len + 1);
    return out;
}

/**
 * Find end of HTML tag (the >), respecting quoted attribute values.
 */
static const char *find_tag_end(const char *tag_start) {
    const char *p = tag_start;
    char in_quote = 0;
    while (*p) {
        if (in_quote) {
            if (*p == '\\' && p[1]) p++;
            else if (*p == in_quote) in_quote = 0;
        } else if (*p == '"' || *p == '\'') {
            in_quote = *p;
        } else if (*p == '>') {
            return p;
        }
        p++;
    }
    return NULL;
}

/**
 * Get video MIME type from URL extension.
 */
static const char *video_type_from_url(const char *url) {
    if (!url) return "video/mp4";
    const char *dot = strrchr(url, '.');
    if (!dot) return "video/mp4";
    const char *ext = dot + 1;
    const char *end = strchr(ext, '?');
    if (!end) end = strchr(ext, '#');
    if (!end) end = ext + strlen(ext);
    size_t len = (size_t)(end - ext);
    if (len >= 3 && strncasecmp(ext, "mp4", 3) == 0) return "video/mp4";
    if (len >= 4 && strncasecmp(ext, "webm", 4) == 0) return "video/webm";
    if (len >= 3 && strncasecmp(ext, "ogg", 3) == 0) return "video/ogg";
    if (len >= 3 && strncasecmp(ext, "ogv", 3) == 0) return "video/ogg";
    if (len >= 3 && strncasecmp(ext, "mov", 3) == 0) return "video/quicktime";
    if (len >= 3 && strncasecmp(ext, "m4v", 3) == 0) return "video/mp4";
    return "video/mp4";
}

/**
 * Extract value of data-apex-picture-webp or data-apex-picture-avif from attrs string.
 * Format: data-apex-picture-webp="value" or data-apex-picture-avif="value"
 * Caller must free.
 */
static char *extract_data_apex_picture_srcset(const char *attrs, const char *format) {
    char key[64];
    snprintf(key, sizeof(key), "data-apex-picture-%s=\"", format);
    const char *p = strstr(attrs, key);
    if (!p) return NULL;
    p += strlen(key);
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

/**
 * Return attrs with internal data-apex-* and core img attrs removed.
 * Keeps user-specified attrs (e.g. width/height/loading/class/style) for img fallback.
 * Caller must free.
 */
static char *filter_img_fallback_attrs(const char *attrs) {
    if (!attrs) return NULL;

    size_t cap = strlen(attrs) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    char *w = out;
    const char *p = attrs;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        const char *token_start = p;
        const char *name_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '=') p++;
        const char *name_end = p;

        if (*p == '=') {
            p++; /* consume '=' */
            if (*p == '"' || *p == '\'') {
                char q = *p++;
                while (*p && *p != q) p++;
                if (*p == q) p++;
            } else {
                while (*p && !isspace((unsigned char)*p)) p++;
            }
        }

        const char *token_end = p;
        size_t name_len = (size_t)(name_end - name_start);
        bool skip_attr = false;

        if (name_len == 0) continue;

        if (name_len >= 10 && strncasecmp(name_start, "data-apex-", 10) == 0) {
            skip_attr = true;
        } else if ((name_len == 3 && strncasecmp(name_start, "src", 3) == 0) ||
                   (name_len == 3 && strncasecmp(name_start, "alt", 3) == 0) ||
                   (name_len == 5 && strncasecmp(name_start, "title", 5) == 0)) {
            skip_attr = true;
        }

        if (!skip_attr) {
            if (w != out) *w++ = ' ';
            size_t token_len = (size_t)(token_end - token_start);
            memcpy(w, token_start, token_len);
            w += token_len;
        }
    }

    *w = '\0';
    return out;
}

/* Counters for element indexing */
typedef struct {
    int para_count;
    int heading_count;
    int table_count;
    int blockquote_count;
    int list_count;
    int item_count;
    int code_count;
    int link_count;
    int image_count;
    int strong_count;
    int emph_count;
    int code_inline_count;
    int hr_count;
} element_counters;

/**
 * Get text fingerprint from node (first 50 chars for matching)
 */
static char *get_node_text_fingerprint(cmark_node *node) {
    if (!node) return NULL;

    cmark_node_type type = cmark_node_get_type(node);

    /* For headings, get the literal */
    if (type == CMARK_NODE_HEADING) {
        cmark_node *text = cmark_node_first_child(node);
        if (text && cmark_node_get_type(text) == CMARK_NODE_TEXT) {
            const char *literal = cmark_node_get_literal(text);
            if (literal) {
                size_t len = strlen(literal);
                if (len > 50) len = 50;
                char *fingerprint = malloc(len + 1);
                if (fingerprint) {
                    memcpy(fingerprint, literal, len);
                    fingerprint[len] = '\0';
                    return fingerprint;
                }
            }
        }
    }

    /* For paragraphs, get text from first text node */
    if (type == CMARK_NODE_PARAGRAPH) {
        cmark_node *child = cmark_node_first_child(node);
        if (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
            const char *literal = cmark_node_get_literal(child);
            if (literal) {
                size_t len = strlen(literal);
                if (len > 50) len = 50;
                char *fingerprint = malloc(len + 1);
                if (fingerprint) {
                    memcpy(fingerprint, literal, len);
                    fingerprint[len] = '\0';
                    return fingerprint;
                }
            }
        }
    }

    /* For links, use URL + link text (first text child) to disambiguate
     * same-href links and avoid matching raw HTML <a> tags. */
    if (type == CMARK_NODE_LINK) {
        const char *url = cmark_node_get_url(node);
        if (url) {
            size_t url_len = strlen(url);
            if (url_len > 50) url_len = 50;
            const char *text = NULL;
            for (cmark_node *child = cmark_node_first_child(node); child;
                 child = cmark_node_next(child)) {
                if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                    text = cmark_node_get_literal(child);
                    if (text && *text) break;
                    text = NULL;
                }
            }
            size_t text_len = text ? strlen(text) : 0;
            if (text_len > 20) text_len = 20;
            size_t total = url_len + (text_len ? 1 + text_len : 0);
            if (total > 50) total = 50;
            char *fingerprint = malloc(total + 1);
            if (fingerprint) {
                memcpy(fingerprint, url, url_len);
                size_t pos = url_len;
                if (text_len && pos + 1 + text_len <= 50) {
                    fingerprint[pos++] = '|';
                    memcpy(fingerprint + pos, text, text_len);
                    pos += text_len;
                }
                fingerprint[pos] = '\0';
                return fingerprint;
            }
        }
    }

    /* For images, use URL + alt (from first child) to disambiguate same-src images */
    if (type == CMARK_NODE_IMAGE) {
        const char *url = cmark_node_get_url(node);
        if (url) {
            size_t url_len = strlen(url);
            if (url_len > 50) url_len = 50;
            cmark_node *child = cmark_node_first_child(node);
            const char *alt = (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) ?
                cmark_node_get_literal(child) : NULL;
            size_t alt_len = alt ? strlen(alt) : 0;
            if (alt_len > 20) alt_len = 20;
            size_t total = url_len + (alt_len ? 1 + alt_len : 0);
            if (total > 50) total = 50;
            char *fingerprint = malloc(total + 1);
            if (fingerprint) {
                memcpy(fingerprint, url, url_len);
                size_t pos = url_len;
                if (alt_len && pos + 1 + alt_len <= 50) {
                    fingerprint[pos++] = '|';
                    memcpy(fingerprint + pos, alt, alt_len);
                    pos += alt_len;
                }
                fingerprint[pos] = '\0';
                return fingerprint;
            }
        }
    }

    return NULL;
}

static void collect_nodes_with_attrs_recursive(cmark_node *node, attr_node **list, element_counters *counters) {
    if (!node) return;

    cmark_node_type type = cmark_node_get_type(node);

    /* Increment counter for this element type */
    int elem_idx = -1;
    if (type == CMARK_NODE_PARAGRAPH) elem_idx = counters->para_count++;
    /* One node type for every heading level. HEADING+1..+5 are thematic
     * breaks, footnotes, and the GFM table nodes, not h2-h6. */
    else if (type == CMARK_NODE_HEADING) elem_idx = counters->heading_count++;
    else if (type == CMARK_NODE_TABLE) {
        /* For tables, increment the counter first, then use (count - 1) as the index */
        /* This ensures the index matches the HTML renderer's count of <table> tags */
        elem_idx = counters->table_count++;
    }
    else if (type == CMARK_NODE_BLOCK_QUOTE) elem_idx = counters->blockquote_count++;
    else if (type == CMARK_NODE_LIST) elem_idx = counters->list_count++;
    else if (type == CMARK_NODE_ITEM) elem_idx = counters->item_count++;
    else if (type == CMARK_NODE_CODE_BLOCK) elem_idx = counters->code_count++;
    else if (type == CMARK_NODE_THEMATIC_BREAK) elem_idx = counters->hr_count++;
    /* Inline elements need indices too - each type has its own counter */
    else if (type == CMARK_NODE_LINK) elem_idx = counters->link_count++;
    else if (type == CMARK_NODE_IMAGE) elem_idx = counters->image_count++;
    else if (type == CMARK_NODE_STRONG) elem_idx = counters->strong_count++;
    else if (type == CMARK_NODE_EMPH) elem_idx = counters->emph_count++;
    else if (type == CMARK_NODE_CODE) elem_idx = counters->code_inline_count++;

    /* Check if this node has attributes */
    void *user_data = cmark_node_get_user_data(node);
    if (user_data) {
        attr_node *new_node = malloc(sizeof(attr_node));
        if (new_node) {
            new_node->node = node;
            new_node->attrs = (char *)user_data;
            new_node->node_type = type;
            new_node->element_index = elem_idx;
            new_node->text_fingerprint = get_node_text_fingerprint(node);
            new_node->next = *list;
            *list = new_node;
        }

        /* If node is marked for removal, don't traverse children */
        if (strstr((char *)user_data, "data-remove")) {
            return;
        }
    }

    /* Recurse */
    for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        collect_nodes_with_attrs_recursive(child, list, counters);
    }
}

static void collect_nodes_with_attrs(cmark_node *node, attr_node **list) {
    element_counters counters = {0};
    collect_nodes_with_attrs_recursive(node, list, &counters);

    /* Reverse the list: prepend builds [last_visited, ..., first_visited];
     * we need document order [first, ..., last] for matching. */
    attr_node *reversed = NULL;
    while (*list) {
        attr_node *next = (*list)->next;
        (*list)->next = reversed;
        reversed = *list;
        *list = next;
    }
    *list = reversed;
}

/**
 * Enhanced HTML rendering with attribute support
 */
char *apex_render_html_with_attributes(cmark_node *document, int options) {
    if (!document) return NULL;

    /* First, render normally */
    char *html = cmark_render_html(document, options, NULL);
    if (!html) return NULL;

    /* Collect all nodes with attributes */
    attr_node *attr_list = NULL;
    collect_nodes_with_attrs(document, &attr_list);

    if (!attr_list) {
        return html; /* No attributes to inject */
    }

    /* Build new HTML with attributes injected */
    size_t html_len = strlen(html);

    /* Calculate needed capacity: original HTML + all attribute strings */
    size_t attrs_size = 0;
    for (attr_node *a = attr_list; a; a = a->next) {
        attrs_size += strlen(a->attrs);
    }
    size_t capacity = html_len + attrs_size + 1024; /* +1KB buffer */
    char *output = malloc(capacity);
    if (!output) {
        /* Clean up attr list */
        while (attr_list) {
            attr_node *next = attr_list->next;
            free(attr_list);
            attr_list = next;
        }
        return html;
    }

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    /* Track which attributes we've used */
    int attr_count = 0;
    for (attr_node *a = attr_list; a; a = a->next) attr_count++;
    bool *used = calloc(attr_count + 1, sizeof(bool));

    /* Track element counts in HTML (same as AST walker) */
    element_counters html_counters = {0};

    /* Process HTML, injecting attributes */
    while (*read) {
        /* Check if we're at an opening tag */
        if (*read == '<' && read[1] != '/' && read[1] != '!') {
            const char *tag_start = read + 1;
            const char *tag_name_end = tag_start;

            /* Get tag name */
            while (*tag_name_end && !isspace((unsigned char)*tag_name_end) &&
                   *tag_name_end != '>' && *tag_name_end != '/') {
                tag_name_end++;
            }

            /* Find the end of the tag (> or />) */
            const char *tag_end = tag_name_end;
            while (*tag_end && *tag_end != '>') tag_end++;

            /* Check if this is a block tag or table cell we care about */
            size_t tag_len = (size_t)(tag_name_end - tag_start);

            /* Determine element type and increment counter */
            cmark_node_type elem_type = 0;
            int elem_idx = -1;

            if (tag_len == 1 && *tag_start == 'p') {
                elem_type = CMARK_NODE_PARAGRAPH;
                elem_idx = html_counters.para_count++;
            } else if (tag_len == 2 && tag_start[0] == 'h' && tag_start[1] >= '1' && tag_start[1] <= '6') {
                elem_type = CMARK_NODE_HEADING;
                elem_idx = html_counters.heading_count++;
            } else if (tag_len == 10 && memcmp(tag_start, "blockquote", 10) == 0) {
                elem_type = CMARK_NODE_BLOCK_QUOTE;
                elem_idx = html_counters.blockquote_count++;
            } else if (tag_len == 5 && memcmp(tag_start, "table", 5) == 0) {
                elem_type = CMARK_NODE_TABLE;
                elem_idx = html_counters.table_count++;
            } else if (tag_len == 2 && (memcmp(tag_start, "ul", 2) == 0 || memcmp(tag_start, "ol", 2) == 0)) {
                elem_type = CMARK_NODE_LIST;
                elem_idx = html_counters.list_count++;
            } else if (tag_len == 2 && memcmp(tag_start, "li", 2) == 0) {
                elem_type = CMARK_NODE_ITEM;
                elem_idx = html_counters.item_count++;
            } else if (tag_len == 2 && memcmp(tag_start, "hr", 2) == 0) {
                elem_type = CMARK_NODE_THEMATIC_BREAK;
                elem_idx = html_counters.hr_count++;
            } else if (tag_len == 3 && memcmp(tag_start, "pre", 3) == 0) {
                elem_type = CMARK_NODE_CODE_BLOCK;
                elem_idx = html_counters.code_count++;
            } else if (tag_len == 1 && *tag_start == 'a') {
                /* Links - inline elements */
                elem_type = CMARK_NODE_LINK;
                elem_idx = html_counters.link_count++;
            } else if (tag_len == 3 && memcmp(tag_start, "img", 3) == 0) {
                /* Images - inline elements */
                elem_type = CMARK_NODE_IMAGE;
                elem_idx = html_counters.image_count++;
            } else if (tag_len == 6 && memcmp(tag_start, "strong", 6) == 0) {
                /* Strong - inline elements */
                elem_type = CMARK_NODE_STRONG;
                elem_idx = html_counters.strong_count++;
            } else if (tag_len == 2 && memcmp(tag_start, "em", 2) == 0) {
                /* Emphasis - inline elements */
                elem_type = CMARK_NODE_EMPH;
                elem_idx = html_counters.emph_count++;
            } else if (tag_len == 4 && memcmp(tag_start, "code", 4) == 0) {
                /* Code - inline elements */
                elem_type = CMARK_NODE_CODE;
                elem_idx = html_counters.code_inline_count++;
            }

            /* Check if we should skip this element (marked for removal) */
            /* We do this BEFORE the main matching to remove elements first */
            bool should_remove = false;
            int removal_idx = -1;
            if (elem_type != 0) {
                int check_idx = 0;
                for (attr_node *a = attr_list; a; a = a->next, check_idx++) {
                    /* Check by element type and index for removal */
                    if (!used[check_idx] &&
                        a->node_type == elem_type &&
                        a->element_index == elem_idx) {
                        if (strstr(a->attrs, "data-remove")) {
                            should_remove = true;
                            removal_idx = check_idx;
                            break;
                        }
                        /* Found matching element but not for removal - stop checking */
                        break;
                    }
                }
            }

            if (should_remove) {
                /* Skip this entire element */
                const char *close_start = read;
                int depth = 1;
                while (*close_start && depth > 0) {
                    if (*close_start == '<') {
                        if (close_start[1] == '/') {
                            /* Closing tag */
                            const char *tag_check = close_start + 2;
                            if (memcmp(tag_check, tag_start, tag_len) == 0 &&
                                (tag_check[tag_len] == '>' || isspace((unsigned char)tag_check[tag_len]))) {
                                depth--;
                                if (depth == 0) {
                                    /* Found matching close tag */
                                    while (*close_start && *close_start != '>') close_start++;
                                    if (*close_start == '>') close_start++;
                                    read = close_start;
                                    if (removal_idx >= 0) used[removal_idx] = true;
                                    goto skip_element;
                                }
                            }
                        } else if (close_start[1] != '!' && close_start[1] != '?') {
                            /* Another opening tag of same type */
                            const char *tag_check = close_start + 1;
                            if (memcmp(tag_check, tag_start, tag_len) == 0 &&
                                (tag_check[tag_len] == '>' || tag_check[tag_len] == ' ')) {
                                depth++;
                            }
                        }
                    }
                    close_start++;
                }
            }

            skip_element:
            if (read != html && *read != '<') {
                continue; /* We skipped an element */
            }

            /* Handle both block and inline elements with attributes */
            if (elem_type != 0) {
                /* Extract fingerprint for matching */
                char html_fingerprint[51] = {0};
                size_t fp_idx = 0;

                if (elem_type == CMARK_NODE_LINK || elem_type == CMARK_NODE_IMAGE) {
                    /* For links/images, extract href/src and for images also alt (to disambiguate same-src) */
                    const char *url_attr = (elem_type == CMARK_NODE_LINK) ? "href=\"" : "src=\"";
                    const char *url_start = strstr(read, url_attr);
                    if (url_start) {
                        url_start += strlen(url_attr);
                        const char *url_end = strchr(url_start, '"');
                        if (url_end) {
                            size_t url_len = url_end - url_start;
                            if (url_len > 50) url_len = 50;
                            memcpy(html_fingerprint, url_start, url_len);
                            fp_idx = url_len;
                            if (elem_type == CMARK_NODE_IMAGE && fp_idx < 49) {
                                const char *alt_attr = "alt=\"";
                                const char *alt_start = strstr(read, alt_attr);
                                if (alt_start && alt_start < tag_end) {
                                    alt_start += strlen(alt_attr);
                                    const char *alt_end = strchr(alt_start, '"');
                                    if (alt_end) {
                                        size_t alt_len = alt_end - alt_start;
                                        if (alt_len > 20) alt_len = 20;
                                        if (fp_idx + 1 + alt_len <= 50) {
                                            html_fingerprint[fp_idx++] = '|';
                                            memcpy(html_fingerprint + fp_idx, alt_start, alt_len);
                                            fp_idx += alt_len;
                                        }
                                    }
                                }
                            } else if (elem_type == CMARK_NODE_LINK && fp_idx < 49) {
                                /* Append visible text before any nested tag (skip image-only links). */
                                const char *content_start = tag_end;
                                if (*content_start == '>') content_start++;
                                if (*content_start && *content_start != '<') {
                                    size_t text_len = 0;
                                    while (content_start[text_len] &&
                                           content_start[text_len] != '<' &&
                                           text_len < 20) {
                                        text_len++;
                                    }
                                    if (text_len > 0 && fp_idx + 1 + text_len <= 50) {
                                        html_fingerprint[fp_idx++] = '|';
                                        memcpy(html_fingerprint + fp_idx, content_start, text_len);
                                        fp_idx += text_len;
                                    }
                                }
                            }
                            html_fingerprint[fp_idx] = '\0';
                        }
                    }
                } else if (elem_type == CMARK_NODE_STRONG || elem_type == CMARK_NODE_EMPH || elem_type == CMARK_NODE_CODE) {
                    /* For inline elements (strong, emph, code), extract text content */
                    const char *content_start = tag_name_end;
                    while (*content_start && *content_start != '>') content_start++;
                    if (*content_start == '>') content_start++;

                    const char *text_p = content_start;
                    while (*text_p && *text_p != '<' && fp_idx < 50) {
                        html_fingerprint[fp_idx++] = *text_p++;
                    }
                    html_fingerprint[fp_idx] = '\0';
                } else {
                    /* For block elements, extract text content.
                     * Paragraph attributes are fingerprinted from the first text
                     * node, which ends at a softbreak, so stop at the first newline. */
                    const char *content_start = tag_name_end;
                    while (*content_start && *content_start != '>') content_start++;
                    if (*content_start == '>') content_start++;

                    const char *text_p = content_start;
                    while (*text_p && *text_p != '<' && fp_idx < 50) {
                        if (elem_type == CMARK_NODE_PARAGRAPH &&
                            (*text_p == '\n' || *text_p == '\r')) {
                            break;
                        }
                        html_fingerprint[fp_idx++] = *text_p++;
                    }
                    html_fingerprint[fp_idx] = '\0';
                }

                /* Find matching attribute - try fingerprint first, then index */
                attr_node *matching = NULL;
                int idx = 0;

                /* For tables, use sequential matching (first unused table) since index may not match */
                if (elem_type == CMARK_NODE_TABLE) {
                    for (attr_node *a = attr_list; a; a = a->next, idx++) {
                        if (used[idx]) continue;
                        if (a->node_type == CMARK_NODE_TABLE) {
                            matching = a;
                            used[idx] = true;
                            break;
                        }
                    }
                } else if (elem_type == CMARK_NODE_IMAGE) {
                    /* Images: match by element_index (document order). */
                    for (attr_node *a = attr_list; a; a = a->next, idx++) {
                        if (used[idx]) continue;
                        if (a->node_type != CMARK_NODE_IMAGE) continue;
                        if (a->element_index == elem_idx) {
                            matching = a;
                            used[idx] = true;
                            break;
                        }
                    }
                } else if (elem_type == CMARK_NODE_LINK) {
                    /* Links: match by href(+text) fingerprint among unused attrs.
                     * Raw HTML <a> tags inflate html link_count, so element_index is
                     * not reliable. Consume matching fingerprints in document order. */
                    if (fp_idx > 0) {
                        for (attr_node *a = attr_list; a; a = a->next, idx++) {
                            if (used[idx]) continue;
                            if (a->node_type != CMARK_NODE_LINK) continue;
                            if (!a->text_fingerprint) continue;
                            if (strncmp(a->text_fingerprint, html_fingerprint, 50) == 0) {
                                matching = a;
                                used[idx] = true;
                                break;
                            }
                        }
                    }
                    /* Fall back to index only when no fingerprint is available. */
                    if (!matching) {
                        idx = 0;
                        for (attr_node *a = attr_list; a; a = a->next, idx++) {
                            if (used[idx]) continue;
                            if (a->node_type != CMARK_NODE_LINK) continue;
                            if (!a->text_fingerprint && a->element_index == elem_idx) {
                                matching = a;
                                used[idx] = true;
                                break;
                            }
                        }
                    }
                } else {
                    /* For other elements, use the existing matching logic */
                    for (attr_node *a = attr_list; a; a = a->next, idx++) {
                        if (used[idx]) continue;

                        bool type_match = (a->node_type == elem_type);

                        if (!type_match) continue;

                        /* Try fingerprint match first (works for both block and inline) */
                        if (a->text_fingerprint && fp_idx > 0 &&
                            strncmp(a->text_fingerprint, html_fingerprint, 50) == 0) {
                            /* For inline elements, also check element_index to handle duplicates. */
                            if (elem_type == CMARK_NODE_STRONG || elem_type == CMARK_NODE_EMPH ||
                                elem_type == CMARK_NODE_CODE) {
                                if (a->element_index == elem_idx) {
                                    matching = a;
                                    used[idx] = true;
                                    break;
                                }
                            } else {
                                /* For other elements, fingerprint match is sufficient */
                                matching = a;
                                used[idx] = true;
                                break;
                            }
                        }

                        /* Fall back to index match if no fingerprint */
                        if (!a->text_fingerprint && a->element_index == elem_idx) {
                            matching = a;
                            used[idx] = true;
                            break;
                        }
                    }
                }

                if (matching) {
                    /* Skip internal attributes and table span attributes */
                    /* Table spans are handled by apex_inject_table_attributes() */
                    /* For tables, we need to extract IAL attributes even if data-caption is present */
                    bool skip_all = false;
                    bool extract_ial_from_caption = false;

                    if (strstr(matching->attrs, "data-remove") ||
                        strstr(matching->attrs, "colspan=") ||
                        strstr(matching->attrs, "rowspan=")) {
                        skip_all = true;
                    } else if (strstr(matching->attrs, "data-caption") && elem_type == CMARK_NODE_TABLE) {
                        /* For tables with captions, extract IAL attributes (id, class, etc.) but skip data-caption */
                        extract_ial_from_caption = true;
                    }

                    if (skip_all) {
                        /* These are handled elsewhere, don't inject here */
                    } else if (extract_ial_from_caption) {
                        /* Extract IAL attributes from the attribute string, excluding data-caption */
                        char *ial_attrs = extract_ial_from_table_attrs(matching->attrs);
                        if (ial_attrs && *ial_attrs) {
                            /* Find where to inject attributes (before closing > of <table> tag) */
                            const char *inject_point = tag_end;
                            if (*inject_point == '>') {
                                /* Copy up to injection point */
                                size_t prefix_len = inject_point - read;
                                if (prefix_len <= remaining) {
                                    memcpy(write, read, prefix_len);
                                    write += prefix_len;
                                    remaining -= prefix_len;
                                }

                                /* Inject IAL attributes - ensure leading space */
                                /* Check if we need to add a space before the attributes */
                                bool needs_leading_space = (ial_attrs[0] != ' ');
                                size_t ial_len = strlen(ial_attrs);
                                size_t total_len = ial_len + (needs_leading_space ? 1 : 0);

                                if (total_len <= remaining) {
                                    if (needs_leading_space) {
                                        *write++ = ' ';
                                        remaining--;
                                    }
                                    memcpy(write, ial_attrs, ial_len);
                                    write += ial_len;
                                    remaining -= ial_len;
                                } else {
                                    /* Buffer too small - need to expand */
                                    size_t current_pos = write - output;
                                    size_t new_cap = (current_pos + total_len + 1) * 2;
                                    char *new_output = realloc(output, new_cap);
                                    if (new_output) {
                                        output = new_output;
                                        write = output + current_pos;
                                        remaining = new_cap - current_pos;
                                        if (needs_leading_space) {
                                            *write++ = ' ';
                                            remaining--;
                                        }
                                        memcpy(write, ial_attrs, ial_len);
                                        write += ial_len;
                                        remaining -= ial_len;
                                    }
                                }
                                if (remaining > 0) {
                                    *write++ = '>';
                                    remaining--;
                                }
                                read = inject_point + 1;
                                free(ial_attrs);
                                continue;
                            }
                        }
                        if (ial_attrs) free(ial_attrs);
                        /* No IAL attributes to inject, but table still needs to be copied - fall through */
                    } else if (elem_type == CMARK_NODE_IMAGE &&
                               (strstr(matching->attrs, "data-apex-replace-video") ||
                                strstr(matching->attrs, "data-apex-replace-picture"))) {
                        /* Replace img with video or picture element */
                        const char *img_tag_end = find_tag_end(read);
                        if (img_tag_end && img_tag_end > read) {
                            char *src = extract_attr_from_tag(read, img_tag_end + 1, "src");
                            char *alt = extract_attr_from_tag(read, img_tag_end + 1, "alt");
                            char *title = extract_attr_from_tag(read, img_tag_end + 1, "title");
                            /* Fallback: title may be in IAL attrs (cmark may not emit it on img) */
                            if ((!title || !*title) && matching->attrs) {
                                size_t alen = strlen(matching->attrs);
                                char *fake_tag = malloc(alen + 10);
                                if (fake_tag) {
                                    snprintf(fake_tag, alen + 10, "<img %s>", matching->attrs);
                                    char *t = extract_attr_from_tag(fake_tag, fake_tag + strlen(fake_tag) + 1, "title");
                                    free(fake_tag);
                                    if (t) { free(title); title = t; }
                                }
                            }
                            if (!src) src = strdup("");
                            if (!alt) alt = strdup("");

                            char *replacement = NULL;
                            size_t repl_len = 0;

                            if (strstr(matching->attrs, "data-apex-replace-video")) {
                                /* Build <video> with <source> elements. Order: webm, ogg, mp4/mov/m4v (primary) */
                                size_t cap = 256 + (src ? strlen(src) * 4 : 0);
                                replacement = malloc(cap);
                                if (replacement) {
                                    char *w = replacement;
                                    w += snprintf(w, cap, "<video");
                                    if (alt && *alt) w += snprintf(w, cap - (size_t)(w - replacement), " title=\"%s\"", alt);
                                    w += snprintf(w, cap - (size_t)(w - replacement), ">");

                                    if (strstr(matching->attrs, "data-apex-video-webm")) {
                                        char *u = url_with_extension(src, "webm");
                                        if (u) { w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"video/webm\">", u); free(u); }
                                    }
                                    if (strstr(matching->attrs, "data-apex-video-ogg")) {
                                        char *u = url_with_extension(src, "ogg");
                                        if (u) { w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"video/ogg\">", u); free(u); }
                                    }
                                    if (strstr(matching->attrs, "data-apex-video-mp4")) {
                                        char *u = url_with_extension(src, "mp4");
                                        if (u) { w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"video/mp4\">", u); free(u); }
                                    }
                                    if (strstr(matching->attrs, "data-apex-video-mov")) {
                                        char *u = url_with_extension(src, "mov");
                                        if (u) { w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"video/quicktime\">", u); free(u); }
                                    }
                                    if (strstr(matching->attrs, "data-apex-video-m4v")) {
                                        char *u = url_with_extension(src, "m4v");
                                        if (u) { w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"video/mp4\">", u); free(u); }
                                    }
                                    /* Primary src as fallback (always include) */
                                    w += snprintf(w, cap - (size_t)(w - replacement), "<source src=\"%s\" type=\"%s\">", src, video_type_from_url(src));
                                    w += snprintf(w, cap - (size_t)(w - replacement), "</video>");
                                    repl_len = (size_t)(w - replacement);
                                }
                            } else {
                                /* Build <picture> with <source> elements and <img> fallback */
                                char *webp_srcset = extract_data_apex_picture_srcset(matching->attrs, "webp");
                                char *avif_srcset = extract_data_apex_picture_srcset(matching->attrs, "avif");
                                char *img_fallback_attrs = filter_img_fallback_attrs(matching->attrs);

                                size_t cap = 512 + (src ? strlen(src) * 2 : 0) +
                                             (webp_srcset ? strlen(webp_srcset) : 0) +
                                             (avif_srcset ? strlen(avif_srcset) : 0) +
                                             (img_fallback_attrs ? strlen(img_fallback_attrs) : 0);
                                replacement = malloc(cap);
                                if (replacement) {
                                    char *w = replacement;
                                    w += snprintf(w, cap, "<picture>");
                                    if (avif_srcset) w += snprintf(w, cap - (size_t)(w - replacement), "<source type=\"image/avif\" srcset=\"%s\">", avif_srcset);
                                    if (webp_srcset) w += snprintf(w, cap - (size_t)(w - replacement), "<source type=\"image/webp\" srcset=\"%s\">", webp_srcset);
                                    /* Preserve title on img for caption logic (apex_convert_image_captions) */
                                    const char *img_attrs = (img_fallback_attrs && *img_fallback_attrs) ? img_fallback_attrs : NULL;
                                    if (title && *title) {
                                        if (img_attrs) {
                                            w += snprintf(w, cap - (size_t)(w - replacement), "<img src=\"%s\" alt=\"%s\" title=\"%s\" %s></picture>", src, alt, title, img_attrs);
                                        } else {
                                            w += snprintf(w, cap - (size_t)(w - replacement), "<img src=\"%s\" alt=\"%s\" title=\"%s\"></picture>", src, alt, title);
                                        }
                                    } else {
                                        if (img_attrs) {
                                            w += snprintf(w, cap - (size_t)(w - replacement), "<img src=\"%s\" alt=\"%s\" %s></picture>", src, alt, img_attrs);
                                        } else {
                                            w += snprintf(w, cap - (size_t)(w - replacement), "<img src=\"%s\" alt=\"%s\"></picture>", src, alt);
                                        }
                                    }
                                    repl_len = (size_t)(w - replacement);
                                }
                                free(webp_srcset);
                                free(avif_srcset);
                                free(img_fallback_attrs);
                            }

                            if (replacement && repl_len > 0 && repl_len <= remaining) {
                                memcpy(write, replacement, repl_len);
                                write += repl_len;
                                remaining -= repl_len;
                                read = img_tag_end + 1;
                                free(replacement);
                                free(src);
                                free(alt);
                                free(title);
                                continue;
                            }
                            free(replacement);
                            free(src);
                            free(alt);
                            free(title);
                        }
                        /* Fall through to normal inject if replacement failed */
                    } else {
                        /* Find where to inject attributes */
                        const char *inject_point = NULL;

                        if (elem_type == CMARK_NODE_IMAGE || elem_type == CMARK_NODE_LINK ||
                            elem_type == CMARK_NODE_STRONG || elem_type == CMARK_NODE_EMPH ||
                            elem_type == CMARK_NODE_CODE) {
                            /* For inline elements (img, a), inject before the closing > or /> */
                            /* Find the closing > for this tag */
                            const char *close_pos = tag_end;
                            bool is_self_closing = false;
                            if (*close_pos == '>') {
                                /* Check if it's a self-closing tag /> */
                                if (close_pos > tag_name_end && close_pos[-1] == '/') {
                                    inject_point = close_pos - 1; /* Before /> */
                                    is_self_closing = true;
                                } else {
                                    inject_point = close_pos; /* Before > */
                                }
                            } else {
                                /* Fallback: after tag name if we can't find > */
                                inject_point = tag_name_end;
                                while (*inject_point && isspace((unsigned char)*inject_point) && *inject_point != '>') inject_point++;
                            }

                            /* Copy up to injection point (but for self-closing tags, don't include the space before /) */
                            size_t prefix_len;
                            if (is_self_closing && inject_point > read && inject_point[-1] == ' ') {
                                /* Don't copy the space before / - we'll add it back after attributes */
                                prefix_len = inject_point - read - 1;
                            } else {
                                prefix_len = inject_point - read;
                            }

                            if (prefix_len < remaining && prefix_len > 0) {
                                memcpy(write, read, prefix_len);
                                write += prefix_len;
                                remaining -= prefix_len;
                            }

                            /* Always add a space before attributes (they need to be separated from existing attributes) */
                            /* The only exception is if inject_point is at > and there's already a space before it */
                            /* But since we're injecting attributes, we always need a space before them */
                            if (remaining > 0) {
                                *write++ = ' ';
                                remaining--;
                            }

                            /* Inject attributes */
                            size_t attr_len = strlen(matching->attrs);
                            if (attr_len <= remaining) {
                                memcpy(write, matching->attrs, attr_len);
                                write += attr_len;
                                remaining -= attr_len;
                            }

                            /* For self-closing tags, ensure space before / */
                            if (is_self_closing && remaining > 0) {
                                *write++ = ' ';
                                remaining--;
                            }

                            read = inject_point;
                        } else {
                            /* For block elements, inject after tag name */
                            inject_point = tag_name_end;
                            while (*inject_point && isspace((unsigned char)*inject_point)) inject_point++;

                            /* Copy up to injection point */
                            size_t prefix_len = inject_point - read;
                            if (prefix_len < remaining) {
                                memcpy(write, read, prefix_len);
                                write += prefix_len;
                                remaining -= prefix_len;
                            }

                            /* Always add space before attributes for block elements */
                            /* We're injecting right after the tag name, so we need a space */
                            /* Check if there's already whitespace to avoid doubling spaces */
                            bool needs_space = true;
                            if (inject_point > read) {
                                /* Check the character immediately before inject_point */
                                const char *before_inject = inject_point - 1;
                                if (isspace((unsigned char)*before_inject)) {
                                    /* There's already whitespace, don't add another */
                                    needs_space = false;
                                }
                            }

                            if (needs_space && remaining > 0) {
                                *write++ = ' ';
                                remaining--;
                            }

                            /* Inject attributes */
                            size_t attr_len = strlen(matching->attrs);
                            if (attr_len <= remaining) {
                                memcpy(write, matching->attrs, attr_len);
                                write += attr_len;
                                remaining -= attr_len;
                            }

                            read = inject_point;
                        }
                        continue;
                    }
                }
            }
        }

        /* Copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    free(used);

    *write = '\0';

    /* Clean up */
    while (attr_list) {
        attr_node *next = attr_list->next;
        free(attr_list->text_fingerprint);
        free(attr_list);
        attr_list = next;
    }

    free(html);
    return output;
}

/**
 * Inject header IDs into HTML output
 */
char *apex_inject_header_ids(const char *html, cmark_node *document, bool generate_ids, bool use_anchors, int id_format) {
    if (!html || !document || !generate_ids) {
        return html ? strdup(html) : NULL;
    }

    /* Collect all headers from AST with their IDs (level + text for matching) */
    typedef struct header_id_map {
        int level;
        char *text;
        char *id;
        int index;
        bool used;
        struct header_id_map *next;
    } header_id_map;

    header_id_map *header_map = NULL;
    int header_count = 0;

    /* Walk AST to collect headers (only markdown HEADING nodes, not raw HTML) */
    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type event;
    while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        if (event == CMARK_EVENT_ENTER && cmark_node_get_type(node) == CMARK_NODE_HEADING) {
            int level = cmark_node_get_heading_level(node);
            char *text = apex_extract_heading_text(node);
            char *id = NULL;

            /* Check if ID already exists from IAL or manual ID (stored in user_data) */
            char *user_data = (char *)cmark_node_get_user_data(node);
            if (user_data) {
                const char *id_attr = strstr(user_data, "id=\"");
                if (id_attr) {
                    const char *id_start = id_attr + 4;
                    const char *id_end = strchr(id_start, '"');
                    if (id_end && id_end > id_start) {
                        size_t id_len = id_end - id_start;
                        id = malloc(id_len + 1);
                        if (id) {
                            memcpy(id, id_start, id_len);
                            id[id_len] = '\0';
                        }
                    }
                }
            }

            if (!id) {
                id = apex_generate_header_id(text, (apex_id_format_t)id_format);
            }

            header_id_map *entry = malloc(sizeof(header_id_map));
            if (entry) {
                entry->level = level;
                entry->text = text;
                entry->id = id;
                entry->index = header_count++;
                entry->used = false;
                entry->next = header_map;
                header_map = entry;
            } else {
                free(text);
                free(id);
            }
        }
    }
    cmark_iter_free(iter);

    if (!header_map) {
        return strdup(html);
    }

    /* Reverse the list to get document order */
    header_id_map *reversed = NULL;
    while (header_map) {
        header_id_map *next = header_map->next;
        header_map->next = reversed;
        reversed = header_map;
        header_map = next;
    }
    header_map = reversed;

    /* Process HTML to inject IDs / optional permalink anchors.
     * Anchor tags are much larger than id="" attributes; under-estimating
     * capacity used to silently drop the HTML tail (For_Brett: lost items
     * 144–145 when header_anchors was on and the Annotations pad was stripped). */
    size_t html_len = strlen(html);
    size_t per_header = use_anchors ? 220 : 100;
    size_t capacity = html_len + header_count * per_header + 64;
    char *output = malloc(capacity);
    if (!output) {
        /* Clean up */
        while (header_map) {
            header_id_map *next = header_map->next;
            free(header_map->text);
            free(header_map->id);
            free(header_map);
            header_map = next;
        }
        return strdup(html);
    }

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    #define APEX_HEADER_ID_ENSURE(needed) do { \
        size_t _need = (size_t)(needed); \
        if (remaining < _need + 1) { \
            size_t _used = (size_t)(write - output); \
            size_t _new_cap = (_used + _need + 1) * 2; \
            if (_new_cap < _used + _need + 64) { \
                _new_cap = _used + _need + 64; \
            } \
            char *_new_out = realloc(output, _new_cap); \
            if (!_new_out) { \
                *write = '\0'; \
                goto apex_inject_header_ids_done; \
            } \
            output = _new_out; \
            write = output + _used; \
            remaining = _new_cap - _used; \
            capacity = _new_cap; \
        } \
    } while (0)

    while (*read) {
        /* Look for header opening tags: <h1>, <h2>, etc. */
        if (*read == '<' && read[1] == 'h' &&
            read[2] >= '1' && read[2] <= '6' &&
            (read[3] == '>' || isspace((unsigned char)read[3]))) {

            /* Find the end of the tag */
            const char *tag_start = read;
            const char *tag_end = read + 3;
            while (*tag_end && *tag_end != '>') tag_end++;
            if (*tag_end != '>') {
                /* Malformed tag, just copy */
                APEX_HEADER_ID_ENSURE(1);
                *write++ = *read++;
                remaining--;
                continue;
            }

            /* Extract header content from HTML (between > and </hN>) for matching */
            int html_level = tag_start[2] - '0';
            const char *content_start = tag_end + 1;
            const char *closing = strstr(content_start, "</h");
            const char *content_end = content_start;
            if (closing && closing[2] >= '1' && closing[2] <= '6' && closing[3] == '>') {
                content_end = closing;
            }
            char content_buf[512];
            size_t content_len = content_end > content_start ? (size_t)(content_end - content_start) : 0;
            if (content_len >= sizeof(content_buf)) content_len = sizeof(content_buf) - 1;
            memcpy(content_buf, content_start, content_len);
            content_buf[content_len] = '\0';
            /* Decode &amp; to & and strip tags for comparison with AST text */
            {
                char *r = content_buf, *w = content_buf;
                while (*r) {
                    if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; }
                    else if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; }
                    else if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; }
                    else if (*r == '<') { while (*r && *r != '>') r++; if (*r == '>') r++; }
                    else { *w++ = *r++; }
                }
                *w = '\0';
            }
            /* Trim whitespace and newlines for comparison with AST text */
            char *trim_start = content_buf;
            while (*trim_start == ' ' || *trim_start == '\t' || *trim_start == '\n' || *trim_start == '\r') trim_start++;
            size_t trim_len = strlen(trim_start);
            while (trim_len > 0 && (trim_start[trim_len - 1] == ' ' || trim_start[trim_len - 1] == '\t' || trim_start[trim_len - 1] == '\n' || trim_start[trim_len - 1] == '\r'))
                trim_start[--trim_len] = '\0';

            /* Match by (level, text); fallback to first unused at level only when text extraction
               differs (avoids assigning to raw HTML headers which have no AST entry at that level) */
            header_id_map *header = NULL;
            for (header_id_map *p = header_map; p; p = p->next) {
                if (!p->used && p->level == html_level && p->text && strcmp(p->text, trim_start) == 0) {
                    header = p;
                    p->used = true;
                    break;
                }
            }
            if (!header) {
                for (header_id_map *p = header_map; p; p = p->next) {
                    if (!p->used && p->level == html_level) {
                        header = p;
                        p->used = true;
                        break;
                    }
                }
            }

            /* Check if ID already exists in the tag */
            bool has_id = false;
            const char *id_attr = strstr(tag_start, "id=");
            const char *id_start = NULL;
            const char *id_end = NULL;
            if (id_attr && id_attr < tag_end) {
                has_id = true;
                id_start = id_attr + 3;
                while (id_start < tag_end && (*id_start == ' ' || *id_start == '"' || *id_start == '\'')) {
                    id_start++;
                }
                id_end = id_start;
                while (id_end < tag_end && *id_end != '"' && *id_end != '\'' && *id_end != ' ' && *id_end != '>') {
                    id_end++;
                }
            }

            if (use_anchors && header && header->id) {
                /* For anchor tags: copy the entire header tag, then inject anchor after '>' */
                size_t tag_len = tag_end - tag_start + 1;  /* Include '>' */
                APEX_HEADER_ID_ENSURE(tag_len);
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end + 1;

                /* Inject anchor tag after the header tag */
                char anchor_tag[512];
                snprintf(anchor_tag, sizeof(anchor_tag),
                        "<a href=\"#%s\" aria-hidden=\"true\" class=\"anchor\" id=\"%s\"></a>",
                        header->id, header->id);
                size_t anchor_len = strlen(anchor_tag);
                APEX_HEADER_ID_ENSURE(anchor_len);
                memcpy(write, anchor_tag, anchor_len);
                write += anchor_len;
                remaining -= anchor_len;
            } else if (!use_anchors && header && header->id) {
                /* For header IDs: replace existing ID or inject new one */
                if (has_id && id_attr) {
                    /* Replace existing ID: copy up to id=, skip old ID value, inject new ID, copy rest */
                    size_t before_id_len = id_attr - tag_start;
                    APEX_HEADER_ID_ENSURE(before_id_len);
                    memcpy(write, tag_start, before_id_len);
                    write += before_id_len;
                    remaining -= before_id_len;

                    /* Find the end of the old ID attribute value */
                    const char *old_id_end = id_attr + 3; /* After 'id=' */
                    /* Skip whitespace and opening quote */
                    while (old_id_end < tag_end && (old_id_end[0] == ' ' || old_id_end[0] == '"' || old_id_end[0] == '\'')) {
                        old_id_end++;
                    }
                    /* Skip the ID value until closing quote or space or > */
                    while (old_id_end < tag_end && old_id_end[0] != '"' && old_id_end[0] != '\'' && old_id_end[0] != ' ' && old_id_end[0] != '>') {
                        old_id_end++;
                    }
                    /* Skip closing quote if present */
                    if (old_id_end < tag_end && (old_id_end[0] == '"' || old_id_end[0] == '\'')) {
                        old_id_end++;
                    }

                    /* Inject new id="..." */
                    char id_attr_str[512];
                    snprintf(id_attr_str, sizeof(id_attr_str), "id=\"%s\"", header->id);
                    size_t id_len = strlen(id_attr_str);
                    APEX_HEADER_ID_ENSURE(id_len);
                    memcpy(write, id_attr_str, id_len);
                    write += id_len;
                    remaining -= id_len;

                    /* Copy rest of tag from after old ID until '>' */
                    read = old_id_end;
                    while (read < tag_end && *read != '>') {
                        APEX_HEADER_ID_ENSURE(1);
                        *write++ = *read++;
                        remaining--;
                    }

                    /* Copy closing '>' */
                    if (read < tag_end && *read == '>') {
                        APEX_HEADER_ID_ENSURE(1);
                        *write++ = *read++;
                        remaining--;
                    }
                } else {
                    /* No existing ID: copy tag up to '>', inject id attribute, then copy '>' */
                    const char *after_tag_name = tag_start + 3;
                    while (*after_tag_name && *after_tag_name != '>' && !isspace((unsigned char)*after_tag_name)) {
                        after_tag_name++;
                    }

                    /* Copy '<hN' */
                    size_t tag_prefix_len = after_tag_name - tag_start;
                    APEX_HEADER_ID_ENSURE(tag_prefix_len);
                    memcpy(write, tag_start, tag_prefix_len);
                    write += tag_prefix_len;
                    remaining -= tag_prefix_len;
                    read = after_tag_name;

                    /* Copy any existing attributes before injecting id */
                    const char *attr_start = read;
                    while (*read && *read != '>') {
                        read++;
                    }

                    /* If there are existing attributes, copy them */
                    if (read > attr_start) {
                        size_t attr_len = read - attr_start;
                        APEX_HEADER_ID_ENSURE(attr_len);
                        memcpy(write, attr_start, attr_len);
                        write += attr_len;
                        remaining -= attr_len;
                    }

                    /* Add space before id attribute if needed */
                    if (read > attr_start || *read == '>') {
                        APEX_HEADER_ID_ENSURE(1);
                        *write++ = ' ';
                        remaining--;
                    }

                    /* Inject id="..." */
                    char id_attr_str[512];
                    snprintf(id_attr_str, sizeof(id_attr_str), "id=\"%s\"", header->id);
                    size_t id_len = strlen(id_attr_str);
                    APEX_HEADER_ID_ENSURE(id_len);
                    memcpy(write, id_attr_str, id_len);
                    write += id_len;
                    remaining -= id_len;

                    /* Copy closing '>' */
                    if (*read == '>') {
                        APEX_HEADER_ID_ENSURE(1);
                        *write++ = *read++;
                        remaining--;
                    }
                }
            } else {
                /* No ID to inject, just copy the tag */
                size_t tag_len = tag_end - tag_start + 1;
                APEX_HEADER_ID_ENSURE(tag_len);
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end + 1;
            }
        } else {
            /* Copy character */
            APEX_HEADER_ID_ENSURE(1);
            *write++ = *read++;
            remaining--;
        }
    }

    APEX_HEADER_ID_ENSURE(1);
    *write = '\0';

apex_inject_header_ids_done:
    #undef APEX_HEADER_ID_ENSURE

    /* Clean up */
    while (header_map) {
        header_id_map *next = header_map->next;
        free(header_map->text);
        free(header_map->id);
        free(header_map);
        header_map = next;
    }

    return output;
}

/**
 * Clean up HTML tag spacing
 * - Compresses multiple spaces in tags to single spaces
 * - Removes spaces before closing >
 */
char *apex_clean_html_tag_spacing(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    char *output = malloc(len + 1);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    bool in_tag = false;
    bool last_was_space = false;

    while (*read) {
        if (*read == '<' && (read[1] != '/' && read[1] != '!' && read[1] != '?')) {
            /* Entering a tag */
            in_tag = true;
            last_was_space = false;
            *write++ = *read++;
        } else if (*read == '>') {
            /* Exiting a tag - skip any trailing space */
            if (last_was_space && write > output && write[-1] == ' ') {
                write--;
            }
            in_tag = false;
            last_was_space = false;
            *write++ = *read++;
        } else if (in_tag && isspace((unsigned char)*read)) {
            /* Space inside tag */
            if (!last_was_space) {
                /* First space - keep it */
                *write++ = ' ';
                last_was_space = true;
            }
            /* Skip additional spaces */
            read++;
        } else {
            /* Regular character */
            last_was_space = false;
            *write++ = *read++;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Collapse newlines and surrounding whitespace between adjacent tags.
 *
 * Example:
 *   "</table>\n\n<figure>" -> "</table><figure>"
 *
 * Strategy:
 * - Whenever we see a '>' character, look ahead over any combination of
 *   spaces/tabs/newlines/carriage returns.
 * - If the next non-whitespace character is '<' and there was at least one
 *   newline in the skipped range, we drop all of that whitespace so the tags
 *   become adjacent.
 * - Otherwise, we leave the whitespace untouched.
 *
 * This keeps text content (including code/pre blocks) intact, while
 * compacting vertical spacing between block-level HTML elements in
 * non-pretty mode.
 */
char *apex_collapse_intertag_newlines(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    char *output = malloc(len + 1);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;

    while (*read) {
        if (*read == '>') {
            /* Copy the '>' */
            *write++ = *read++;

            /* Look ahead over whitespace between this tag and the next content */
            const char *look = read;
            int newline_count = 0;
            while (*look == ' ' || *look == '\t' || *look == '\n' || *look == '\r') {
                if (*look == '\n' || *look == '\r') {
                    newline_count++;
                }
                look++;
            }

            if (newline_count > 0 && *look == '<') {
                /* We are between two tags. Compress any run of newlines here so that
                 * \n{2,} becomes exactly \n\n (one blank line), and a single newline
                 * stays a single newline.
                 */
                int to_emit = (newline_count >= 2) ? 2 : 1;
                for (int i = 0; i < to_emit; i++) {
                    *write++ = '\n';
                }
                read = look;
                continue;
            }
            /* Otherwise, fall through and let the normal loop copy whitespace */
        }

        *write++ = *read++;
    }

    *write = '\0';
    return output;
}

/**
 * Check if a table cell contains only em dashes and whitespace
 */
static bool cell_contains_only_dashes(const char *cell_start, const char *cell_end) {
    const char *p = cell_start;
    bool has_content = false;

    while (p < cell_end) {
        /* Check for em dash (—) U+2014: 0xE2 0x80 0x94 */
        if ((unsigned char)*p == 0xE2 && p + 2 < cell_end &&
            (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x94) {
            has_content = true;
            p += 3;
        } else if (*p == ':' || *p == '-' || *p == '|') {
            /* Colons, dashes, and pipes are OK in separator rows (for alignment: |:----|:---:|----:|) */
            if (*p == '-' || *p == ':') {
                has_content = true;  /* Dashes and colons count as content */
            }
            p++;
        } else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            /* Whitespace is OK */
            p++;
        } else if (*p == '<') {
            /* HTML tags are OK (opening/closing tags) */
            if (strncmp(p, "<td", 3) == 0 || strncmp(p, "</td>", 5) == 0 ||
                strncmp(p, "<th", 3) == 0 || strncmp(p, "</th>", 5) == 0) {
                /* Skip the tag */
                if (strncmp(p, "</td>", 5) == 0 || strncmp(p, "</th>", 5) == 0) {
                    p += 5;
                } else {
                    /* Skip to > */
                    while (p < cell_end && *p != '>') p++;
                    if (p < cell_end) p++;
                }
            } else {
                /* Other content - not a separator cell */
                return false;
            }
        } else {
            /* Non-dash, non-whitespace, non-tag content */
            return false;
        }
    }

    return has_content;  /* Must have at least one em dash */
}

/**
 * Convert thead to tbody for relaxed tables ONLY
 * Converts <thead><tr><th>...</th></tr></thead> to <tbody><tr><td>...</td></tr></tbody>
 * ONLY for tables that were created from relaxed table input (no separator rows in original)
 *
 * Strategy: Check if there's a separator row (with em dashes) in the tbody.
 * - If there IS a separator row in tbody → regular table (keep thead)
 * - If there is NO separator row in tbody → relaxed table (convert thead to tbody)
 *
 * This works because:
 * - Regular tables: separator row is in tbody (between header and data)
 * - Relaxed tables: separator row was inserted by preprocessing, but we removed it
 *   (or it was converted to em dashes and removed)
 */
char *apex_convert_relaxed_table_headers(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    char *output = malloc(len * 2);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = len * 2;
    bool demote_next_thead = false;
    static const char marker[] = "<!--apex-relaxed-table-->";
    static const size_t marker_len = sizeof(marker) - 1;

    while (*read) {
        /* Expand buffer if needed */
        if (remaining < 100) {
            size_t written = write - output;
            size_t new_capacity = (write - output) * 2;
            if (new_capacity < written + 100) {
                new_capacity = written + 1000;  /* Ensure we have enough space */
            }
            char *old_output = output;  /* Save old pointer */
            char *new_output = realloc(output, new_capacity);
            if (!new_output) {
                /* realloc failed - original pointer is still valid, free it */
                free(old_output);
                return NULL;
            }
            /* realloc succeeded - update pointers */
            output = new_output;
            write = output + written;
            remaining = new_capacity - written;
        }

        /* Marker from relaxed-table preprocess: demote the following thead */
        if (strncmp(read, marker, marker_len) == 0) {
            demote_next_thead = true;
            read += marker_len;
            if (*read == '\n') read++;
            continue;
        }

        /* Check for <thead> */
        if (strncmp(read, "<thead>", 7) == 0) {
            const char *after_thead = read + 7;
            const char *thead_end = strstr(after_thead, "</thead>");

            if (thead_end) {
                /* Check if thead contains only empty cells (dummy headers from headerless tables) */
                bool all_cells_empty = true;
                bool found_any_th = false;

                /* Search for all <th> or <th ...> tags in thead */
                const char *search = after_thead;
                while (search < thead_end) {
                    /* Check for <th> without attributes */
                    if (strncmp(search, "<th>", 4) == 0) {
                        found_any_th = true;
                        const char *th_end = strstr(search, "</th>");
                        if (!th_end || th_end >= thead_end) {
                            all_cells_empty = false;
                            break;
                        }
                        /* Check if content between > and </th> is empty */
                        const char *content_start = search + 4; /* After <th>, which is > */
                        if (content_start < th_end) {
                            while (content_start < th_end) {
                                if (!isspace((unsigned char)*content_start)) {
                                    all_cells_empty = false;
                                    break;
                                }
                                content_start++;
                            }
                        }
                        /* If content_start >= th_end, tags are adjacent (empty) - OK */
                        if (!all_cells_empty) break;
                        search = th_end + 5; /* Move past </th> */
                    }
                    /* Check for <th with attributes */
                    else if (strncmp(search, "<th", 3) == 0 &&
                             (search[3] == ' ' || search[3] == '\t' || search[3] == '>')) {
                        found_any_th = true;
                        /* Find the closing > of opening tag */
                        const char *tag_end = strchr(search, '>');
                        if (!tag_end || tag_end >= thead_end) {
                            all_cells_empty = false;
                            break;
                        }
                        const char *th_end = strstr(tag_end, "</th>");
                        if (!th_end || th_end >= thead_end) {
                            all_cells_empty = false;
                            break;
                        }
                        /* Check content between > and </th> */
                        const char *content_start = tag_end + 1;
                        if (content_start < th_end) {
                            while (content_start < th_end) {
                                if (!isspace((unsigned char)*content_start)) {
                                    all_cells_empty = false;
                                    break;
                                }
                                content_start++;
                            }
                        }
                        /* If content_start >= th_end, tags are adjacent (empty) - OK */
                        if (!all_cells_empty) break;
                        search = th_end + 5; /* Move past </th> */
                    } else {
                        search++;
                    }
                }

                /* If we found th cells and they're all empty, remove the entire thead */
                if (found_any_th && all_cells_empty) {
                    demote_next_thead = false;
                    read = thead_end + 8; /* Skip <thead>...</thead> */
                    continue;
                }

                /* Relaxed tables (marked during preprocess): demote thead to tbody */
                if (demote_next_thead) {
                    demote_next_thead = false;
                    memcpy(write, "<tbody>", 7);
                    write += 7;
                    remaining -= 7;
                    read += 7;  /* Skip <thead> */

                    while (read < thead_end + 8) {
                        if (strncmp(read, "<th>", 4) == 0) {
                            memcpy(write, "<td>", 4);
                            write += 4;
                            remaining -= 4;
                            read += 4;
                        } else if (strncmp(read, "</th>", 5) == 0) {
                            memcpy(write, "</td>", 5);
                            write += 5;
                            remaining -= 5;
                            read += 5;
                        } else if (strncmp(read, "<th ", 4) == 0) {
                            memcpy(write, "<td", 3);
                            write += 3;
                            remaining -= 3;
                            read += 3;
                            while (*read && *read != '>') {
                                *write++ = *read++;
                                remaining--;
                            }
                            if (*read == '>') {
                                *write++ = *read++;
                                remaining--;
                            }
                        } else if (strncmp(read, "</thead>", 8) == 0) {
                            read += 8;
                            const char *next = read;
                            while (*next && (*next == ' ' || *next == '\n' || *next == '\t')) next++;
                            if (strncmp(next, "<tbody>", 7) == 0) {
                                read = next + 7;
                            }
                            break;
                        } else {
                            *write++ = *read++;
                            remaining--;
                        }
                    }
                    continue;
                }

                demote_next_thead = false;
            }
        }

        /* Copy character */
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
    return output;
}

/**
 * Remove blank lines within tables
 * Removes lines containing only whitespace/newlines between <table> and </table> tags
 */
char *apex_remove_table_blank_lines(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    char *output = malloc(len + 1);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    bool in_table = false;
    const char *line_start = read;
    bool line_is_blank = true;

    while (*read) {
        /* Check for table tags */
        if (strncmp(read, "<table", 6) == 0 && (read[6] == '>' || read[6] == ' ')) {
            in_table = true;
        } else if (strncmp(read, "</table>", 8) == 0) {
            in_table = false;
        }

        /* On newline, check if the line was blank */
        if (*read == '\n') {
            if (in_table && line_is_blank) {
                /* Blank line in table - skip it */
                read++;
                line_start = read;
                line_is_blank = true;
                continue;
            }
            /* Not blank or not in table - write the line including newline */
            while (line_start <= read) {
                *write++ = *line_start++;
            }
            read++;
            line_start = read;
            line_is_blank = true;
            continue;
        }

        /* Check if line has non-whitespace content */
        if (*read != ' ' && *read != '\t' && *read != '\r') {
            line_is_blank = false;
        }

        read++;
    }

    /* Write any remaining content */
    while (*line_start) {
        *write++ = *line_start++;
    }

    *write = '\0';
    return output;
}

/**
 * Remove table rows that contain only em dashes (separator rows incorrectly rendered as data rows)
 * This happens when smart typography converts --- to — in separator rows
 * @param html The HTML to process
 * @return Newly allocated HTML with separator rows removed (must be freed)
 */
char *apex_remove_table_separator_rows(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    char *output = malloc(len + 1);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    bool in_table = false;
    const char *row_start = NULL;

    while (*read) {
        /* Check for table tags */
        if (strncmp(read, "<table", 6) == 0 && (read[6] == '>' || read[6] == ' ')) {
            in_table = true;
        } else if (strncmp(read, "</table>", 8) == 0) {
            in_table = false;
        } else if (in_table && strncmp(read, "<tr>", 4) == 0) {
            row_start = read;
            read += 4;

            /* Check all cells in this row */
            bool is_separator_row = true;
            const char *row_end = NULL;

            /* Find the end of this row */
            const char *search = read;
            while (*search) {
                if (strncmp(search, "</tr>", 5) == 0) {
                    row_end = search + 5;
                    break;
                }
                search++;
            }

            if (row_end) {
                /* Check each cell in the row */
                const char *cell_start = read;
                while (cell_start < row_end) {
                    if (strncmp(cell_start, "<td", 3) == 0 || strncmp(cell_start, "<th", 3) == 0) {
                        /* Find the closing tag */
                        const char *tag_end = strstr(cell_start, ">");
                        if (!tag_end) break;
                        tag_end++;

                        /* Find the closing </td> or </th> */
                        const char *cell_end = NULL;
                        if (strncmp(cell_start, "<td", 3) == 0) {
                            cell_end = strstr(tag_end, "</td>");
                            if (cell_end) cell_end += 5;
                        } else {
                            cell_end = strstr(tag_end, "</th>");
                            if (cell_end) cell_end += 5;
                        }

                        if (cell_end && cell_end <= row_end) {
                            /* Check if this cell contains only dashes */
                            if (!cell_contains_only_dashes(tag_end, cell_end - 5)) {
                                is_separator_row = false;
                                break;
                            }
                            cell_start = cell_end;
                        } else {
                            break;
                        }
                    } else {
                        cell_start++;
                    }
                }
            }

            if (is_separator_row && row_end) {
                /* Skip this entire row */
                read = row_end;
                continue;
            } else {
                /* Write the row start */
                while (row_start < read) {
                    *write++ = *row_start++;
                }
            }
            continue;
        }

        /* Copy character */
        *write++ = *read++;
    }

    *write = '\0';
    return output;
}

/**
 * Adjust header levels in HTML based on Base Header Level metadata
 * Shifts all headers by the specified offset (e.g., Base Header Level: 2 means h1->h2, h2->h3, etc.)
 */
char *apex_adjust_header_levels(const char *html, int base_header_level) {
    if (!html || base_header_level <= 0 || base_header_level > 6) {
        return html ? strdup(html) : NULL;
    }

    /* If base_header_level is 1, no adjustment needed */
    if (base_header_level == 1) {
        return strdup(html);
    }

    size_t len = strlen(html);
    size_t capacity = len + 1024;  /* Extra space for potential changes */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for header opening tags: <h1>, <h2>, etc. or closing tags: </h1>, </h2>, etc. */
        bool is_closing_tag = false;
        int header_level = -1;

        if (*read == '<') {
            /* Check for closing tag </h1> first */
            if (read[1] == '/' && read[2] == 'h' &&
                read[3] >= '1' && read[3] <= '6' && read[4] == '>') {
                is_closing_tag = true;
                header_level = read[3] - '0';
            }
            /* Check for opening tag <h1> or <h1 ...> */
            else if (read[1] == 'h' && read[2] >= '1' && read[2] <= '6' &&
                     (read[3] == '>' || isspace((unsigned char)read[3]))) {
                is_closing_tag = false;
                header_level = read[2] - '0';
            }
        }

        if (header_level >= 1 && header_level <= 6) {
            /* Calculate new level */
            int new_level = header_level + (base_header_level - 1);

            /* Clamp to valid range (1-6) */
            if (new_level > 6) {
                new_level = 6;
            } else if (new_level < 1) {
                new_level = 1;
            }

            /* Find the end of the tag */
            const char *tag_start = read;
            const char *tag_end = strchr(tag_start, '>');
            if (!tag_end) {
                /* Malformed tag, just copy */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Check if we need to adjust the level */
            if (new_level != header_level) {
                /* Need to replace h<header_level> with h<new_level> */
                size_t tag_len = tag_end - tag_start;

                /* Ensure we have enough space */
                if (remaining < tag_len + 10) {
                    size_t written = write - output;
                    capacity = (written + tag_len + 10) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }

                if (is_closing_tag) {
                    /* Closing tag: </h1> -> </h2> */
                    *write++ = '<';
                    *write++ = '/';
                    *write++ = 'h';
                    *write++ = '0' + new_level;
                    *write++ = '>';
                    remaining -= 5;
                    read = tag_end + 1;
                } else {
                    /* Opening tag: <h1> or <h1 ...> */
                    const char *h_pos = tag_start + 1;  /* After '<' */
                    size_t before_h = h_pos - tag_start;
                    memcpy(write, tag_start, before_h);
                    write += before_h;
                    remaining -= before_h;

                    /* Write 'h' */
                    *write++ = 'h';
                    remaining--;

                    /* Write new level */
                    *write++ = '0' + new_level;
                    remaining--;

                    /* Copy rest of tag */
                    const char *after_level = tag_start + 3;  /* After 'h' and level digit */
                    size_t rest_len = tag_end - after_level;
                    if (rest_len > 0 && remaining >= rest_len) {
                        memcpy(write, after_level, rest_len);
                        write += rest_len;
                        remaining -= rest_len;
                    }

                    /* Copy closing '>' */
                    *write++ = '>';
                    remaining--;

                    read = tag_end + 1;
                }
            } else {
                /* No change needed, copy tag as-is */
                size_t tag_len = tag_end - tag_start + 1;
                if (tag_len < remaining) {
                    memcpy(write, tag_start, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                } else {
                    /* Need more space */
                    size_t written = write - output;
                    capacity = (written + tag_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                    memcpy(write, tag_start, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                }
                read = tag_end + 1;
            }
        } else {
            /* Not a header tag, copy character */
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                /* Need more space */
                size_t written = write - output;
                capacity = (written + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
                *write++ = *read++;
                remaining--;
            }
        }
    }

    *write = '\0';
    return output;
}

/**
 * Adjust quote styles in HTML based on Quotes Language metadata
 * Replaces default English quote entities with language-specific quotes
 */
char *apex_adjust_quote_language(const char *html, const char *quotes_language) {
    if (!html) return NULL;

    /* Default to English if not specified */
    if (!quotes_language || *quotes_language == '\0') {
        return strdup(html);
    }

    /* Normalize quotes language (lowercase, no spaces) */
    char normalized[64] = {0};
    const char *src = quotes_language;
    char *dst = normalized;
    while (*src && (dst - normalized) < (int)sizeof(normalized) - 1) {
        if (!isspace((unsigned char)*src)) {
            *dst++ = (char)tolower((unsigned char)*src);
        }
        src++;
    }
    *dst = '\0';

    /* Determine quote replacements based on language */
    const char *double_open = NULL;
    const char *double_close = NULL;
    const char *single_open = NULL;
    const char *single_close = NULL;

    if (strcmp(normalized, "english") == 0 || strcmp(normalized, "en") == 0) {
        /* English: &ldquo; &rdquo; &lsquo; &rsquo; (default, no change needed) */
        return strdup(html);
    } else if (strcmp(normalized, "french") == 0 || strcmp(normalized, "fr") == 0) {
        /* French: « » (guillemets) with spaces, ' ' for single */
        double_open = "&laquo;&nbsp;";
        double_close = "&nbsp;&raquo;";
        single_open = "&rsquo;";
        single_close = "&rsquo;";
    } else if (strcmp(normalized, "german") == 0 || strcmp(normalized, "de") == 0) {
        /* German: „ " (bottom/top) */
        double_open = "&bdquo;";
        double_close = "&ldquo;";
        single_open = "&sbquo;";
        single_close = "&lsquo;";
    } else if (strcmp(normalized, "germanguillemets") == 0) {
        /* German guillemets: » « (reversed) */
        double_open = "&raquo;";
        double_close = "&laquo;";
        single_open = "&rsaquo;";
        single_close = "&lsaquo;";
    } else if (strcmp(normalized, "spanish") == 0 || strcmp(normalized, "es") == 0) {
        /* Spanish: « » (guillemets) */
        double_open = "&laquo;";
        double_close = "&raquo;";
        single_open = "&lsquo;";
        single_close = "&rsquo;";
    } else if (strcmp(normalized, "dutch") == 0 || strcmp(normalized, "nl") == 0) {
        /* Dutch: „ " (like German) */
        double_open = "&bdquo;";
        double_close = "&ldquo;";
        single_open = "&sbquo;";
        single_close = "&lsquo;";
    } else if (strcmp(normalized, "swedish") == 0 || strcmp(normalized, "sv") == 0) {
        /* Swedish: " " (straight quotes become curly) */
        double_open = "&rdquo;";
        double_close = "&rdquo;";
        single_open = "&rsquo;";
        single_close = "&rsquo;";
    } else {
        /* Unknown language, use English (no change) */
        return strdup(html);
    }

    /* If no replacements needed, return copy */
    if (!double_open) {
        return strdup(html);
    }

    /* Replace quote entities in HTML */
    size_t html_len = strlen(html);
    size_t capacity = html_len * 2;  /* Extra space for longer entities */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Check for double quote HTML entities */
        if (strncmp(read, "&ldquo;", 7) == 0) {
            size_t repl_len = strlen(double_open);
            if (repl_len < remaining) {
                memcpy(write, double_open, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 7;
                continue;
            }
        } else if (strncmp(read, "&rdquo;", 7) == 0) {
            size_t repl_len = strlen(double_close);
            if (repl_len < remaining) {
                memcpy(write, double_close, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 7;
                continue;
            }
        } else if (strncmp(read, "&lsquo;", 7) == 0) {
            size_t repl_len = strlen(single_open);
            if (repl_len < remaining) {
                memcpy(write, single_open, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 7;
                continue;
            }
        } else if (strncmp(read, "&rsquo;", 7) == 0) {
            size_t repl_len = strlen(single_close);
            if (repl_len < remaining) {
                memcpy(write, single_close, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 7;
                continue;
            }
        }
        /* Check for Unicode curly quotes (UTF-8 encoded) */
        /* Left double quotation mark: U+201C = 0xE2 0x80 0x9C */
        else if ((unsigned char)read[0] == 0xE2 && (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x9C) {
            size_t repl_len = strlen(double_open);
            if (repl_len < remaining) {
                memcpy(write, double_open, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 3;
                continue;
            }
        }
        /* Right double quotation mark: U+201D = 0xE2 0x80 0x9D */
        else if ((unsigned char)read[0] == 0xE2 && (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x9D) {
            size_t repl_len = strlen(double_close);
            if (repl_len < remaining) {
                memcpy(write, double_close, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 3;
                continue;
            }
        }
        /* Left single quotation mark: U+2018 = 0xE2 0x80 0x98 */
        else if ((unsigned char)read[0] == 0xE2 && (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x98) {
            size_t repl_len = strlen(single_open);
            if (repl_len < remaining) {
                memcpy(write, single_open, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 3;
                continue;
            }
        }
        /* Right single quotation mark: U+2019 = 0xE2 0x80 0x99 */
        else if ((unsigned char)read[0] == 0xE2 && (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x99) {
            size_t repl_len = strlen(single_close);
            if (repl_len < remaining) {
                memcpy(write, single_close, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read += 3;
                continue;
            }
        }

        /* Not a quote entity, copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            /* Need more space */
            size_t written = write - output;
            capacity = (written + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
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
    return output;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} aria_buf;

static bool aria_buf_append(aria_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t new_cap = b->cap ? b->cap : 64;
        while (b->len + n + 1 > new_cap) new_cap *= 2;
        char *grown = realloc(b->data, new_cap);
        if (!grown) return false;
        b->data = grown;
        b->cap = new_cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

/* strstr limited to [start, end) */
static const char *aria_find_in(const char *start, const char *end, const char *needle) {
    size_t n = strlen(needle);
    for (const char *p = start; p && p + n <= end; p++) {
        if (*p == *needle && memcmp(p, needle, n) == 0) return p;
    }
    return NULL;
}

static bool aria_has_table_figure_class(const char *start, const char *end) {
    return aria_find_in(start, end, "class=\"table-figure\"") ||
           aria_find_in(start, end, "class='table-figure'");
}

/**
 * Apply ARIA labels and accessibility attributes to HTML output
 * @param html The HTML output
 * @param document The AST document (currently unused but kept for consistency with other functions)
 * @return Newly allocated HTML with ARIA attributes injected (must be freed)
 */
char *apex_apply_aria_labels(const char *html, cmark_node *document) {
    (void)document;  /* Currently unused, but kept for API consistency */

    if (!html) return NULL;

    size_t html_len = strlen(html);

    /* Two-pass approach: First pass collects figcaption IDs, second pass injects ARIA attributes */

    /* Pass 1: Collect figcaption IDs and their positions */
    typedef struct caption_info {
        const char *figcaption_pos;  /* Position in HTML where figcaption starts */
        char *caption_id;            /* ID value (allocated) */
        const char *figure_start;    /* Position of opening <figure> tag */
        struct caption_info *next;
    } caption_info;

    caption_info *caption_list = NULL;
    int table_caption_counter = 0;

    /* First pass: find all figcaptions in table-figures and collect their IDs */
    const char *search = html;
    while (*search) {
        if (*search == '<' && strncmp(search, "<figcaption", 11) == 0) {
            const char *cap_tag_start = search;
            const char *cap_tag_end = strchr(search, '>');
            if (cap_tag_end) {
                /* Check if we're in a table-figure context */
                const char *before_cap = search - 1;
                bool in_table_figure = false;
                const char *figure_start_pos = NULL;
                while (before_cap >= html && before_cap > search - 200) {
                    if (*before_cap == '<' && strncmp(before_cap, "<figure", 7) == 0) {
                        if (aria_has_table_figure_class(before_cap, cap_tag_start)) {
                            in_table_figure = true;
                            figure_start_pos = before_cap;
                            break;
                        }
                    }
                    before_cap--;
                }

                if (in_table_figure) {
                    /* Check if ID already exists */
                    const char *id_attr = aria_find_in(cap_tag_start, cap_tag_end, "id=\"");
                    if (!id_attr) {
                        id_attr = aria_find_in(cap_tag_start, cap_tag_end, "id='");
                    }

                    char *caption_id = NULL;
                    if (id_attr) {
                        /* Extract existing ID */
                        const char *id_start = id_attr + 4;
                        const char *id_end = strchr(id_start, '"');
                        if (!id_end) id_end = strchr(id_start, '\'');
                        if (id_end && id_end > id_start) {
                            size_t id_len = id_end - id_start;
                            caption_id = malloc(id_len + 1);
                            if (caption_id) {
                                memcpy(caption_id, id_start, id_len);
                                caption_id[id_len] = '\0';
                            }
                        }
                    } else {
                        /* Generate ID */
                        table_caption_counter++;
                        caption_id = malloc(64);
                        if (caption_id) {
                            snprintf(caption_id, 64, "table-caption-%d", table_caption_counter);
                        }
                    }

                    if (caption_id) {
                        caption_info *info = malloc(sizeof(caption_info));
                        if (info) {
                            info->figcaption_pos = cap_tag_start;
                            info->caption_id = caption_id;
                            info->figure_start = figure_start_pos;
                            info->next = caption_list;
                            caption_list = info;
                        } else {
                            free(caption_id);
                        }
                    }
                }
            }
        }
        search++;
    }

    aria_buf out = {0};
    out.cap = html_len + html_len / 8 + 256;
    out.data = malloc(out.cap);
    if (!out.data) goto oom;
    out.data[0] = '\0';

    const char *read = html;

    /* Arguments are evaluated before appending so the read position always advances */
    #define APPEND_N(s, n) do { \
        if (!aria_buf_append(&out, (s), (n))) goto oom; \
    } while (0)
    #define APPEND_SAFE(str) do { \
        const char *str_ = (str); \
        APPEND_N(str_, strlen(str_)); \
    } while (0)
    #define COPY_CHAR(c) do { \
        char c_ = (c); \
        APPEND_N(&c_, 1); \
    } while (0)

    while (*read) {
        /* Check for <nav class="toc"> */
        if (*read == '<' && strncmp(read, "<nav", 4) == 0) {
            const char *tag_start = read;
            const char *tag_end = strchr(read, '>');
            if (!tag_end) {
                COPY_CHAR(*read++);
                continue;
            }

            /* Check if this is a TOC nav element */
            bool is_toc = aria_find_in(tag_start, tag_end, "class=\"toc\"") ||
                          aria_find_in(tag_start, tag_end, "class='toc'");

            if (is_toc && !aria_find_in(tag_start, tag_end, "aria-label=")) {
                /* Copy up to just before closing >, add aria-label, then close */
                APPEND_N(tag_start, (size_t)(tag_end - tag_start));
                APPEND_SAFE(" aria-label=\"Table of contents\"");
                COPY_CHAR('>');
                read = tag_end + 1;
                continue;
            }
        }

        /* Check for <figure> */
        if (*read == '<' && strncmp(read, "<figure", 7) == 0) {
            const char *tag_start = read;
            const char *tag_end = strchr(read, '>');
            if (!tag_end) {
                COPY_CHAR(*read++);
                continue;
            }

            if (!aria_find_in(tag_start, tag_end, "role=")) {
                /* Copy up to just before closing >, add role, then close */
                APPEND_N(tag_start, (size_t)(tag_end - tag_start));
                APPEND_SAFE(" role=\"figure\"");
                COPY_CHAR('>');
                read = tag_end + 1;
                continue;
            }
        }

        /* Check for <table> */
        if (*read == '<' && strncmp(read, "<table", 6) == 0) {
            const char *tag_start = read;
            const char *tag_end = strchr(read, '>');
            if (!tag_end) {
                COPY_CHAR(*read++);
                continue;
            }

            bool needs_role = !aria_find_in(tag_start, tag_end, "role=");
            bool has_aria_desc = aria_find_in(tag_start, tag_end, "aria-describedby=") != NULL;

            /* Find the enclosing table-figure, if any */
            const char *this_figure_start = NULL;
            const char *find_fig = read - 1;
            while (find_fig >= html && find_fig > read - 500) {
                if (*find_fig == '<' && strncmp(find_fig, "<figure", 7) == 0) {
                    if (aria_has_table_figure_class(find_fig, tag_start)) {
                        this_figure_start = find_fig;
                        break;
                    }
                }
                find_fig--;
            }

            /* Look for a caption in this figure (either before or after table) */
            const char *caption_id = NULL;
            if (this_figure_start && !has_aria_desc) {
                for (caption_info *cap = caption_list; cap; cap = cap->next) {
                    if (cap->figure_start == this_figure_start) {
                        caption_id = cap->caption_id;
                        break;
                    }
                }
            }

            if (needs_role || caption_id) {
                /* Copy up to just before closing >, add attributes, then close */
                APPEND_N(tag_start, (size_t)(tag_end - tag_start));
                if (needs_role) {
                    APPEND_SAFE(" role=\"table\"");
                }
                if (caption_id) {
                    APPEND_SAFE(" aria-describedby=\"");
                    APPEND_SAFE(caption_id);
                    COPY_CHAR('"');
                }
                COPY_CHAR('>');
                read = tag_end + 1;
                continue;
            }
        }

        /* Check for <figcaption> within table-figure to add IDs if missing */
        if (*read == '<' && strncmp(read, "<figcaption", 11) == 0) {
            const char *tag_start = read;
            const char *tag_end = strchr(read, '>');
            if (!tag_end) {
                COPY_CHAR(*read++);
                continue;
            }

            /* Find this figcaption in our caption_list */
            caption_info *this_caption = NULL;
            for (caption_info *cap = caption_list; cap; cap = cap->next) {
                if (cap->figcaption_pos == tag_start) {
                    this_caption = cap;
                    break;
                }
            }

            if (this_caption &&
                !aria_find_in(tag_start, tag_end, "id=\"") &&
                !aria_find_in(tag_start, tag_end, "id='")) {
                /* No ID in original, add the one we generated/collected */
                APPEND_N(tag_start, (size_t)(tag_end - tag_start));
                APPEND_SAFE(" id=\"");
                APPEND_SAFE(this_caption->caption_id);
                COPY_CHAR('"');
                COPY_CHAR('>');
                read = tag_end + 1;
                continue;
            }
        }

        /* Default: copy character */
        COPY_CHAR(*read++);
    }

    #undef APPEND_N
    #undef APPEND_SAFE
    #undef COPY_CHAR

    while (caption_list) {
        caption_info *next = caption_list->next;
        free(caption_list->caption_id);
        free(caption_list);
        caption_list = next;
    }
    return out.data;

oom:
    free(out.data);
    while (caption_list) {
        caption_info *next = caption_list->next;
        free(caption_list->caption_id);
        free(caption_list);
        caption_list = next;
    }
    return strdup(html);
}

/* Helper: trim leading/trailing ASCII whitespace from an attribute value */
static void apex_trim_attr_value(const char *s, size_t len,
                                 const char **out_s, size_t *out_len) {
    const char *start = s;
    const char *end = s + len;
    while (start < end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *out_s = start;
    *out_len = (size_t)(end - start);
}

/**
 * Rewrite img alt text and optionally strip fig-alt attribute.
 * alt_val/alt_len and new_alt/new_alt_len are the inner text (no quotes).
 * Caller must free.
 */
static char *apex_img_rewrite_alt_and_strip(const char *tag_start, const char *tag_end,
                                            const char *alt_val, size_t alt_len,
                                            const char *new_alt, size_t new_alt_len,
                                            const char *strip_start, const char *strip_end) {
    if (!tag_start || !tag_end || tag_end < tag_start || !alt_val || alt_len == 0 ||
        !new_alt || new_alt_len == 0) {
        return NULL;
    }

    const char *alt_value_end = alt_val + alt_len;
    if (alt_value_end > tag_end + 1) {
        return NULL;
    }

    size_t head_len = (size_t)(alt_val - tag_start);
    const char *suffix_start;
    size_t suffix_len;

    if (strip_start && strip_end && strip_end > strip_start &&
        strip_start >= alt_value_end && strip_end <= tag_end + 1) {
        /* fig-alt attribute removed: emit closing quote then skip to after it */
        suffix_start = strip_end;
        suffix_len = (size_t)(tag_end + 1 - suffix_start);
        size_t out_len = head_len + new_alt_len + 1 + suffix_len;

        char *out = malloc(out_len + 1);
        if (!out) {
            return NULL;
        }

        char *w = out;
        memcpy(w, tag_start, head_len);
        w += head_len;
        memcpy(w, new_alt, new_alt_len);
        w += new_alt_len;
        *w++ = '"';
        memcpy(w, suffix_start, suffix_len);
        w[suffix_len] = '\0';
        return out;
    }

    suffix_start = alt_value_end;
    suffix_len = (size_t)(tag_end + 1 - suffix_start);
    size_t out_len = head_len + new_alt_len + suffix_len;

    char *out = malloc(out_len + 1);
    if (!out) {
        return NULL;
    }

    char *w = out;
    memcpy(w, tag_start, head_len);
    w += head_len;
    memcpy(w, new_alt, new_alt_len);
    w += new_alt_len;
    memcpy(w, suffix_start, suffix_len);
    w[suffix_len] = '\0';
    return out;
}

/**
 * Copy an <img> tag, remove fig-alt attribute, and set alt to the fig-alt value.
 * Caller must free the returned string.
 */
static char *apex_img_tag_with_fig_alt(const char *tag_start, const char *tag_end,
                                       const char *fig_alt_val, size_t fig_alt_len,
                                       const char *strip_start, const char *strip_end,
                                       const char *alt_val, size_t alt_len) {
    if (!tag_start || !tag_end || tag_end < tag_start || !fig_alt_val || fig_alt_len == 0) {
        return NULL;
    }

    if (alt_val && alt_len > 0) {
        return apex_img_rewrite_alt_and_strip(tag_start, tag_end, alt_val, alt_len,
                                              fig_alt_val, fig_alt_len,
                                              strip_start, strip_end);
    }

    /* No existing alt: strip fig-alt and insert alt="..." before closing > */
    size_t tag_len = (size_t)(tag_end - tag_start + 1);
    size_t strip_len = 0;
    if (strip_start && strip_end && strip_end > strip_start &&
        strip_start >= tag_start && strip_end <= tag_end + 1) {
        strip_len = (size_t)(strip_end - strip_start);
    }

    size_t cap = tag_len + fig_alt_len + 64;
    if (strip_len > 0 && strip_len < tag_len) {
        cap = tag_len - strip_len + fig_alt_len + 64;
    }
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }

    char *p = buf;
    if (strip_len > 0) {
        size_t head = (size_t)(strip_start - tag_start);
        memcpy(p, tag_start, head);
        p += head;
        size_t tail = (size_t)(tag_end + 1 - strip_end);
        memcpy(p, strip_end, tail);
        p += tail;
        *p = '\0';
    } else {
        memcpy(buf, tag_start, tag_len);
        buf[tag_len] = '\0';
        p = buf + tag_len;
    }

    char *gt = strrchr(buf, '>');
    if (!gt) {
        free(buf);
        return NULL;
    }

    char fig_alt_buf[512];
    size_t use_len = fig_alt_len;
    if (use_len >= sizeof(fig_alt_buf)) {
        use_len = sizeof(fig_alt_buf) - 1;
    }
    memcpy(fig_alt_buf, fig_alt_val, use_len);
    fig_alt_buf[use_len] = '\0';

    char insert[600];
    int n = snprintf(insert, sizeof(insert), " alt=\"%s\"", fig_alt_buf);
    if (n < 0 || (size_t)n >= sizeof(insert)) {
        free(buf);
        return NULL;
    }

    size_t tail_len = strlen(gt);
    size_t new_len = (size_t)(gt - buf) + (size_t)n + tail_len;
    if (new_len + 1 > cap) {
        char *grown = realloc(buf, new_len + 32);
        if (!grown) {
            free(buf);
            return NULL;
        }
        buf = grown;
        gt = strrchr(buf, '>');
        if (!gt) {
            free(buf);
            return NULL;
        }
        tail_len = strlen(gt);
    }

    memmove(gt + n, gt, tail_len + 1);
    memcpy(gt, insert, (size_t)n);
    return buf;
}

/**
 * Convert <img> tags to <figure> with <figcaption> when alt/title/caption are present.
 * If caption="TEXT" is present, always wrap. Otherwise when enable_image_captions,
 * use title or alt (unless title_captions_only, then only title).
 */
char *apex_convert_image_captions(const char *html, bool enable_image_captions, bool title_captions_only) {
    if (!html) return NULL;

    size_t len = strlen(html);
    /* Allow extra space for <figure> and <figcaption> wrappers */
    size_t capacity = len * 2 + 128;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for <picture> - wrap in figure when caption from img title/alt */
        if (*read == '<' && (read[1] == 'p' || read[1] == 'P') &&
            (read[2] == 'i' || read[2] == 'I') && (read[3] == 'c' || read[3] == 'C') &&
            (read[4] == 't' || read[4] == 'T') && (read[5] == 'u' || read[5] == 'U') &&
            (read[6] == 'r' || read[6] == 'R') && (read[7] == 'e' || read[7] == 'E') &&
            (read[8] == ' ' || read[8] == '>' || read[8] == '\t')) {
            const char *picture_start = read;
            const char *picture_end = strstr(read, "</picture>");
            if (picture_end) {
                picture_end += 10; /* include </picture> */
                /* Find <img inside the picture and extract title/alt for caption */
                const char *img_in = picture_start;
                char *title_str = NULL, *alt_str = NULL;
                while ((img_in = strstr(img_in, "<img")) != NULL && img_in < picture_end) {
                    const char *img_tag_end = strchr(img_in, '>');
                    if (img_tag_end && img_tag_end < picture_end) {
                        title_str = extract_attr_from_tag(img_in, img_tag_end + 1, "title");
                        alt_str = extract_attr_from_tag(img_in, img_tag_end + 1, "alt");
                        break;
                    }
                    img_in += 4;
                }
                /* Determine caption from title or alt per options */
                const char *caption = NULL;
                size_t caption_len = 0;
                if (enable_image_captions) {
                    if (title_captions_only && title_str && *title_str) {
                        caption = title_str; caption_len = strlen(title_str);
                    } else if (title_str && *title_str) {
                        caption = title_str; caption_len = strlen(title_str);
                    } else if (alt_str && *alt_str) {
                        caption = alt_str; caption_len = strlen(alt_str);
                    }
                }
                size_t block_len = (size_t)(picture_end - picture_start);
                if (caption && caption_len > 0) {
                    size_t extra = 8 + 12 + caption_len + 13 + 9; /* figure + figcaption + </figcaption> + </figure> */
                    if (extra + block_len >= remaining) {
                        size_t used = write - output;
                        size_t new_cap = (used + extra + block_len + 1) * 2;
                        char *new_out = realloc(output, new_cap);
                        if (!new_out) { free(title_str); free(alt_str); free(output); return NULL; }
                        output = new_out; write = output + used; remaining = new_cap - used;
                    }
                    memcpy(write, "<figure>", 8); write += 8; remaining -= 8;
                    memcpy(write, picture_start, block_len); write += block_len; remaining -= block_len;
                    memcpy(write, "<figcaption>", 12); write += 12; remaining -= 12;
                    memcpy(write, caption, caption_len); write += caption_len; remaining -= caption_len;
                    memcpy(write, "</figcaption></figure>", sizeof("</figcaption></figure>") - 1);
                    write += sizeof("</figcaption></figure>") - 1;
                    remaining -= sizeof("</figcaption></figure>") - 1;
                } else {
                    if (block_len >= remaining) {
                        size_t used = write - output;
                        size_t new_cap = (used + block_len + 1) * 2;
                        char *new_out = realloc(output, new_cap);
                        if (!new_out) { free(title_str); free(alt_str); free(output); return NULL; }
                        output = new_out; write = output + used; remaining = new_cap - used;
                    }
                    memcpy(write, picture_start, block_len); write += block_len; remaining -= block_len;
                }
                free(title_str);
                free(alt_str);
                read = picture_end;
                continue;
            }
        }

        /* Look for <img tag */
        if (*read == '<' && (read[1] == 'i' || read[1] == 'I') &&
            (read[2] == 'm' || read[2] == 'M') &&
            (read[3] == 'g' || read[3] == 'G') &&
            (read[4] == ' ' || read[4] == '\t' || read[4] == '\r' ||
             read[4] == '\n' || read[4] == '>' || read[4] == '/')) {

            const char *tag_start = read;
            const char *p = read + 4;

            /* Find end of tag '>' while respecting quotes */
            bool in_quote = false;
            char quote_char = '\0';
            while (*p) {
                if (!in_quote && (*p == '"' || *p == '\'')) {
                    in_quote = true;
                    quote_char = *p;
                } else if (in_quote && *p == quote_char) {
                    in_quote = false;
                    quote_char = '\0';
                } else if (!in_quote && *p == '>') {
                    break;
                }
                p++;
            }

            if (!*p) {
                /* Malformed tag - copy rest and stop */
                size_t to_copy = strlen(read);
                if (to_copy >= remaining) {
                    size_t used = write - output;
                    size_t new_cap = (used + to_copy + 1) * 2;
                    char *new_out = realloc(output, new_cap);
                    if (!new_out) {
                        free(output);
                        return NULL;
                    }
                    output = new_out;
                    write = output + used;
                    remaining = new_cap - used;
                }
                memcpy(write, read, to_copy);
                write += to_copy;
                remaining -= to_copy;
                break;
            }

            const char *tag_end = p; /* Points at '>' */

            /* Skip img inside <picture> - picture's img is the fallback, don't wrap in figure */
            {
                bool inside_picture = false;
                const char *scan = tag_start - 1;
                while (scan >= html) {
                    if (*scan == '<') {
                        if (scan + 8 <= tag_start && strncasecmp(scan, "<picture", 8) == 0 &&
                            (scan[8] == ' ' || scan[8] == '>' || scan[8] == '\t')) {
                            inside_picture = true;
                            break;
                        }
                        if (scan + 10 <= tag_start && strncmp(scan, "</picture>", 10) == 0) {
                            break;  /* Outside - we passed closing tag first */
                        }
                        /* Other tags (source, etc.) - keep scanning backwards */
                    }
                    scan--;
                }
                if (inside_picture) {
                    size_t tag_len = (size_t)(tag_end - tag_start + 1);
                    if (tag_len >= remaining) {
                        size_t used = write - output;
                        size_t new_cap = (used + tag_len + 1) * 2;
                        char *new_out = realloc(output, new_cap);
                        if (!new_out) { free(output); return NULL; }
                        output = new_out;
                        write = output + used;
                        remaining = new_cap - used;
                    }
                    memcpy(write, tag_start, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                    read = tag_end + 1;
                    continue;
                }
            }

            /* Parse attributes between <img and > */
            const char *attr_start = tag_start + 4;
            const char *attr_end = tag_end;
            const char *title_val = NULL;
            size_t title_len = 0;
            const char *alt_val = NULL;
            size_t alt_len = 0;
            const char *caption_val = NULL;
            size_t caption_len = 0;
            const char *caption_attr_start = NULL; /* start of caption attr (for stripping) */
            const char *caption_attr_end = NULL;
            const char *fig_alt_val = NULL;
            size_t fig_alt_len = 0;
            const char *fig_alt_attr_start = NULL;
            const char *fig_alt_attr_end = NULL;

            const char *q = attr_start;
            while (q < attr_end) {
                /* Skip whitespace */
                while (q < attr_end && isspace((unsigned char)*q)) q++;
                if (q >= attr_end || *q == '/' || *q == '>') break;

                const char *name_start = q;
                while (q < attr_end && !isspace((unsigned char)*q) &&
                       *q != '=' && *q != '>' && *q != '/') {
                    q++;
                }
                const char *name_end = q;

                /* Skip whitespace before '=' */
                while (q < attr_end && isspace((unsigned char)*q)) q++;
                if (q >= attr_end || *q != '=') {
                    /* Not a name=value pair, skip token */
                    while (q < attr_end && *q != ' ' && *q != '\t' &&
                           *q != '\r' && *q != '\n' && *q != '>') {
                        q++;
                    }
                    continue;
                }
                q++; /* skip '=' */
                while (q < attr_end && isspace((unsigned char)*q)) q++;
                if (q >= attr_end) break;

                /* Parse value */
                const char *value_start = q;
                const char *value_end = NULL;
                if (*q == '"' || *q == '\'') {
                    char qc = *q;
                    value_start = q + 1;
                    q++;
                    while (q < attr_end && *q != qc) q++;
                    value_end = q;
                    if (q < attr_end) q++; /* skip closing quote */
                } else {
                    while (q < attr_end && !isspace((unsigned char)*q) &&
                           *q != '>') {
                        q++;
                    }
                    value_end = q;
                }

                size_t name_len = (size_t)(name_end - name_start);
                if (name_len > 0) {
                    /* Compare attribute name case-insensitively */
                    if (name_len == 5 &&
                        (strncasecmp(name_start, "title", 5) == 0)) {
                        title_val = value_start;
                        title_len = (size_t)(value_end - value_start);
                    } else if (name_len == 7 &&
                               (strncasecmp(name_start, "fig-alt", 7) == 0)) {
                        fig_alt_val = value_start;
                        fig_alt_len = (size_t)(value_end - value_start);
                        fig_alt_attr_start = (name_start > attr_start && isspace((unsigned char)name_start[-1])) ? name_start - 1 : name_start;
                        fig_alt_attr_end = q;
                    } else if (name_len == 3 &&
                               (strncasecmp(name_start, "alt", 3) == 0)) {
                        alt_val = value_start;
                        alt_len = (size_t)(value_end - value_start);
                    } else if (name_len == 7 &&
                               (strncasecmp(name_start, "caption", 7) == 0)) {
                        caption_val = value_start;
                        caption_len = (size_t)(value_end - value_start);
                        /* Include leading space so we strip " caption=\"...\"" */
                        caption_attr_start = (name_start > attr_start && isspace((unsigned char)name_start[-1])) ? name_start - 1 : name_start;
                        caption_attr_end = q;
                    }
                }
            }

            /* Determine caption text: caption= always wins; else title or alt per options */
            const char *caption = NULL;
            size_t caption_text_len = 0;
            bool use_caption_attr = (caption_val != NULL && caption_len > 0);

            if (use_caption_attr) {
                apex_trim_attr_value(caption_val, caption_len, &caption, &caption_text_len);
            }
            if (!use_caption_attr && (caption == NULL || caption_text_len == 0)) {
                if (!enable_image_captions) {
                    caption = NULL;
                    caption_text_len = 0;
                } else if (title_captions_only) {
                    /* Only use title, never alt */
                    if (title_val && title_len > 0) {
                        apex_trim_attr_value(title_val, title_len, &caption, &caption_text_len);
                    }
                } else {
                    /* Default: prefer title, then alt */
                    if (title_val && title_len > 0) {
                        apex_trim_attr_value(title_val, title_len, &caption, &caption_text_len);
                    } else if (alt_val && alt_len > 0) {
                        apex_trim_attr_value(alt_val, alt_len, &caption, &caption_text_len);
                    }
                }
            }

            /* Quarto/Pandoc: fig-alt is accessibility alt; markdown alt text is the caption */
            if (fig_alt_val && fig_alt_len > 0 && alt_val && alt_len > 0 && !use_caption_attr) {
                apex_trim_attr_value(alt_val, alt_len, &caption, &caption_text_len);
            }

            char *fig_alt_tag = NULL;
            if (fig_alt_val && fig_alt_len > 0) {
                fig_alt_tag = apex_img_tag_with_fig_alt(tag_start, tag_end, fig_alt_val, fig_alt_len,
                                                        fig_alt_attr_start, fig_alt_attr_end,
                                                        alt_val, alt_len);
            }

            if (!caption || caption_text_len == 0) {
                /* No caption - copy tag as-is (apply fig-alt alt rewrite when present) */
                if (fig_alt_tag) {
                    size_t tag_len = strlen(fig_alt_tag);
                    if (tag_len >= remaining) {
                        size_t used = write - output;
                        size_t new_cap = (used + tag_len + 1) * 2;
                        char *new_out = realloc(output, new_cap);
                        if (!new_out) {
                            free(fig_alt_tag);
                            free(output);
                            return NULL;
                        }
                        output = new_out;
                        write = output + used;
                        remaining = new_cap - used;
                    }
                    memcpy(write, fig_alt_tag, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                    free(fig_alt_tag);
                    read = tag_end + 1;
                    continue;
                }
                size_t tag_len = (size_t)(tag_end - tag_start + 1);
                if (tag_len >= remaining) {
                    size_t used = write - output;
                    size_t new_cap = (used + tag_len + 1) * 2;
                    char *new_out = realloc(output, new_cap);
                    if (!new_out) {
                        free(output);
                        return NULL;
                    }
                    output = new_out;
                    write = output + used;
                    remaining = new_cap - used;
                }
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end + 1;
                continue;
            }

            /* Don't wrap in another <figure> if this image is already inside a <figure>
             * (e.g. from a fenced div ::: >figure), to avoid nested figure/figcaption. */
            {
                int figure_depth = 0;
                const char *scan = html;
                while (scan < tag_start) {
                    if (*scan == '<') {
                        if (scan + 8 <= tag_start &&
                            (strncasecmp(scan + 1, "figure", 6) == 0) &&
                            (scan[7] == '>' || isspace((unsigned char)scan[7]))) {
                            figure_depth++;
                        } else if (scan + 9 <= tag_start &&
                                   (strncasecmp(scan + 1, "/figure", 7) == 0) &&
                                   (scan[8] == '>' || isspace((unsigned char)scan[8]))) {
                            if (figure_depth > 0) figure_depth--;
                        }
                    }
                    scan++;
                }
                if (figure_depth > 0) {
                    /* Already inside a figure - copy img tag as-is, no extra wrap */
                    if (fig_alt_tag) {
                        size_t tag_len = strlen(fig_alt_tag);
                        if (tag_len >= remaining) {
                            size_t used = write - output;
                            size_t new_cap = (used + tag_len + 1) * 2;
                            char *new_out = realloc(output, new_cap);
                            if (!new_out) {
                                free(fig_alt_tag);
                                free(output);
                                return NULL;
                            }
                            output = new_out;
                            write = output + used;
                            remaining = new_cap - used;
                        }
                        memcpy(write, fig_alt_tag, tag_len);
                        write += tag_len;
                        remaining -= tag_len;
                        free(fig_alt_tag);
                        read = tag_end + 1;
                        continue;
                    }
                    size_t tag_len = (size_t)(tag_end - tag_start + 1);
                    if (tag_len >= remaining) {
                        size_t used = write - output;
                        size_t new_cap = (used + tag_len + 1) * 2;
                        char *new_out = realloc(output, new_cap);
                        if (!new_out) {
                            free(output);
                            return NULL;
                        }
                        output = new_out;
                        write = output + used;
                        remaining = new_cap - used;
                    }
                    memcpy(write, tag_start, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                    read = tag_end + 1;
                    continue;
                }
            }

            /* We have caption text - wrap in <figure><img ...><figcaption>...</figcaption></figure> */
            const char *figure_open = "<figure>";
            const char *figcaption_open = "<figcaption>";
            const char *figcaption_close = "</figcaption>";
            const char *figure_close = "</figure>";

            /* When we have caption= attribute, output img tag without it (strip caption attr) */
            size_t img_tag_output_len;
            if (fig_alt_tag) {
                img_tag_output_len = strlen(fig_alt_tag);
            } else if (caption_attr_start != NULL && caption_attr_end != NULL) {
                img_tag_output_len = (size_t)(caption_attr_start - tag_start) +
                    (size_t)(tag_end + 1 - caption_attr_end);
            } else {
                img_tag_output_len = (size_t)(tag_end - tag_start + 1);
            }

            size_t extra = strlen(figure_open) + strlen(figcaption_open) +
                           caption_text_len + strlen(figcaption_close) +
                           strlen(figure_close);
            size_t needed = img_tag_output_len + extra;
            if (needed >= remaining) {
                size_t used = write - output;
                size_t new_cap = (used + needed + 1) * 2;
                char *new_out = realloc(output, new_cap);
                if (!new_out) {
                    free(output);
                    return NULL;
                }
                output = new_out;
                write = output + used;
                remaining = new_cap - used;
            }

            /* Write <figure> */
            memcpy(write, figure_open, strlen(figure_open));
            write += strlen(figure_open);
            remaining -= strlen(figure_open);

            /* Write <img ...> tag (omitting caption attribute if present) */
            if (fig_alt_tag) {
                memcpy(write, fig_alt_tag, img_tag_output_len);
                write += img_tag_output_len;
                remaining -= img_tag_output_len;
                free(fig_alt_tag);
                fig_alt_tag = NULL;
            } else if (caption_attr_start != NULL && caption_attr_end != NULL) {
                size_t part1 = (size_t)(caption_attr_start - tag_start);
                memcpy(write, tag_start, part1);
                write += part1;
                remaining -= part1;
                size_t part2 = (size_t)(tag_end + 1 - caption_attr_end);
                memcpy(write, caption_attr_end, part2);
                write += part2;
                remaining -= part2;
            } else {
                size_t tag_len = (size_t)(tag_end - tag_start + 1);
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
            }

            /* Write <figcaption>Caption</figcaption></figure> */
            memcpy(write, figcaption_open, strlen(figcaption_open));
            write += strlen(figcaption_open);
            remaining -= strlen(figcaption_open);

            memcpy(write, caption, caption_text_len);
            write += caption_text_len;
            remaining -= caption_text_len;

            memcpy(write, figcaption_close, strlen(figcaption_close));
            write += strlen(figcaption_close);
            remaining -= strlen(figcaption_close);

            memcpy(write, figure_close, strlen(figure_close));
            write += strlen(figure_close);
            remaining -= strlen(figure_close);

            read = tag_end + 1;
            continue;
        }

        /* Default: copy character */
        if (remaining < 1) {
            size_t used = write - output;
            size_t new_cap = (used + 64) * 2;
            char *new_out = realloc(output, new_cap);
            if (!new_out) {
                free(output);
                return NULL;
            }
            output = new_out;
            write = output + used;
            remaining = new_cap - used;
        }
        *write++ = *read++;
        remaining--;
    }

    if (remaining < 1) {
        size_t used = write - output;
        char *new_out = realloc(output, used + 1);
        if (!new_out) {
            free(output);
            return NULL;
        }
        output = new_out;
        write = output + used;
    }
    *write = '\0';
    return output;
}

/**
 * Strip redundant <p> that wraps only a single <img> inside <figure>, and any
 * leading "&lt; " (angle-prefix) so the result is <figure><img...></figure>.
 * Used when fenced div ::: >figure contains "< ![Image](...)" which becomes
 * <figure><p>&lt; <img...></p></figure>.
 */
char *apex_strip_figure_paragraph_wrapper(const char *html) {
    if (!html) return NULL;
    size_t len = strlen(html);
    const char *end = html + len;
    size_t capacity = len + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;
    const char *read = html;
    char *write = output;
    size_t remaining = capacity;
    int figure_depth = 0;

    while (*read) {
        /* Track when we're inside <figure>...</figure> */
        if (*read == '<') {
            const char *tag = read + 1;
            if (tag[0] != '/') {
                if ((strncasecmp(tag, "figure", 6) == 0) &&
                    (tag[6] == '>' || isspace((unsigned char)tag[6])))
                    figure_depth++;
            } else {
                if ((strncasecmp(tag + 1, "figure", 6) == 0) &&
                    (tag[7] == '>' || isspace((unsigned char)tag[7])))
                    figure_depth--;
            }
        }
        /* Look for <figure (with optional attributes) - copy it */
        if (*read == '<' && read[1] != '/') {
            const char *tag = read + 1;
            if ((strncasecmp(tag, "figure", 6) == 0) &&
                (tag[6] == '>' || isspace((unsigned char)tag[6]))) {
                while (*read && *read != '>') {
                    if (remaining < 2) {
                        size_t used = write - output;
                        capacity = (used + len) * 2;
                        char *n = realloc(output, capacity);
                        if (!n) { free(output); return NULL; }
                        output = n; write = output + used; remaining = capacity - used;
                    }
                    *write++ = *read++;
                    remaining--;
                }
                if (*read == '>') {
                    *write++ = *read++;
                    remaining--;
                }
                continue;
            }
            /* Inside figure: look for <p that wraps only &lt; + single <img> */
            if (figure_depth > 0 &&
                (strncasecmp(tag, "p", 1) == 0) &&
                (tag[1] == '>' || isspace((unsigned char)tag[1]))) {
                const char *p_open_end = read + 1;
                while (*p_open_end && *p_open_end != '>') p_open_end++;
                if (!*p_open_end) {
                    *write++ = *read++;
                    remaining--;
                    continue;
                }
                p_open_end++; /* past '>' */
                const char *inner = p_open_end;
                /* Skip optional "&lt;" or "&lt; " and whitespace */
                while (*inner == ' ' || *inner == '\t' || *inner == '\n' || *inner == '\r') inner++;
                if (inner + 4 <= end && strncmp(inner, "&lt;", 4) == 0) {
                    inner += 4;
                    while (*inner == ' ' || *inner == '\t' || *inner == '\n' || *inner == '\r') inner++;
                }
                /* Must be <img ...> */
                if (*inner != '<' || (inner[1] != 'i' && inner[1] != 'I') ||
                    (inner[2] != 'm' && inner[2] != 'M') ||
                    (inner[3] != 'g' && inner[3] != 'G') ||
                    (inner[4] != ' ' && inner[4] != '\t' && inner[4] != '>' && inner[4] != '/')) {
                    *write++ = *read++;
                    remaining--;
                    continue;
                }
                const char *img_start = inner;
                const char *img_end = inner + 4;
                while (*img_end && *img_end != '>') {
                    if (*img_end == '"' || *img_end == '\'') {
                        char q = *img_end++;
                        while (*img_end && *img_end != q) img_end++;
                        if (*img_end) img_end++;
                    } else {
                        img_end++;
                    }
                }
                if (*img_end != '>') {
                    *write++ = *read++;
                    remaining--;
                    continue;
                }
                img_end++; /* past '>' */
                /* Skip whitespace then must be </p> */
                const char *after_img = img_end;
                while (*after_img == ' ' || *after_img == '\t' || *after_img == '\n' || *after_img == '\r') after_img++;
                if (after_img + 5 <= end &&
                    (after_img[0] == '<' && after_img[1] == '/' &&
                     (after_img[2] == 'p' || after_img[2] == 'P') &&
                     (after_img[3] == '>' || isspace((unsigned char)after_img[3])))) {
                    const char *p_close = after_img + 3;
                    while (*p_close && *p_close != '>') p_close++;
                    if (*p_close == '>') p_close++;
                    /* Replace entire <p>...</p> with just the <img> */
                    size_t img_len = (size_t)(img_end - img_start);
                    if (img_len >= remaining) {
                        size_t used = write - output;
                        capacity = (used + img_len + 1) * 2;
                        char *n = realloc(output, capacity);
                        if (!n) { free(output); return NULL; }
                        output = n; write = output + used; remaining = capacity - used;
                    }
                    memcpy(write, img_start, img_len);
                    write += img_len;
                    remaining -= img_len;
                    read = p_close;
                    continue;
                }
            }
        }
        if (remaining < 2) {
            size_t used = write - output;
            capacity = (used + len) * 2;
            char *n = realloc(output, capacity);
            if (!n) { free(output); return NULL; }
            output = n; write = output + used; remaining = capacity - used;
        }
        *write++ = *read++;
        remaining--;
    }
    *write = '\0';
    return output;
}

/**
 * Find the position of the matching closing tag for a block element.
 * Given pos pointing at "<figure" (or <video, <picture), returns pointer past "</figure>".
 * Uses depth counting for nested same-named tags. Returns NULL if not found.
 */
static const char *find_block_close(const char *pos, const char *end, const char *tag_name, size_t tag_len) {
    /* Skip past the opening tag to its '>' */
    const char *p = pos;
    while (p < end && *p != '>') {
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (p < end && *p != q) p++;
            if (p < end) p++;
        } else {
            p++;
        }
    }
    if (p >= end || *p != '>') return NULL;
    p++; /* past '>' */
    int depth = 1;
    while (p < end && depth > 0) {
        const char *next = memchr(p, '<', (size_t)(end - p));
        if (!next) return NULL;
        p = next;
        if (p + 1 >= end) return NULL;
        if (p[1] == '/') {
            if (p + 2 + tag_len <= end &&
                strncasecmp(p + 2, tag_name, tag_len) == 0 &&
                (p[2 + tag_len] == '>' || isspace((unsigned char)p[2 + tag_len]))) {
                depth--;
                if (depth == 0) {
                    const char *close = p + 2 + tag_len;
                    while (close < end && *close != '>') close++;
                    return (close < end && *close == '>') ? close + 1 : NULL;
                }
            }
            p++;
        } else if (p + 1 + tag_len <= end &&
                   strncasecmp(p + 1, tag_name, tag_len) == 0 &&
                   (p[1 + tag_len] == '>' || isspace((unsigned char)p[1 + tag_len]))) {
            depth++;
            p++;
        } else {
            p++;
        }
    }
    return NULL;
}

/**
 * Strip <p> that wraps only a single block element (figure, video, picture).
 * HTML5 invalid: <p> may only contain phrasing content; figure/video/picture are flow content.
 * Transforms <p><figure>...</figure></p> -> <figure>...</figure>, etc.
 */
char *apex_strip_block_paragraph_wrapper(const char *html) {
    if (!html) return NULL;
    size_t len = strlen(html);
    const char *end = html + len;
    size_t capacity = len + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;
    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        if (*read == '<' && read[1] != '/' &&
            (strncasecmp(read + 1, "p", 1) == 0) &&
            (read[2] == '>' || isspace((unsigned char)read[2]))) {
            const char *p_open_end = read + 1;
            while (*p_open_end && *p_open_end != '>') p_open_end++;
            if (!*p_open_end || p_open_end >= end) {
                *write++ = *read++;
                remaining--;
                continue;
            }
            p_open_end++; /* past '>' */
            const char *inner = p_open_end;
            while (inner < end && (*inner == ' ' || *inner == '\t' || *inner == '\n' || *inner == '\r')) inner++;
            if (inner >= end || *inner != '<') {
                *write++ = *read++;
                remaining--;
                continue;
            }
            const char *tag_start = inner + 1;
            const char *block_close = NULL;
            if (inner + 7 <= end && strncasecmp(tag_start, "figure", 6) == 0 &&
                (tag_start[6] == '>' || isspace((unsigned char)tag_start[6]))) {
                block_close = find_block_close(inner, end, "figure", 6);
            } else if (inner + 6 <= end && strncasecmp(tag_start, "video", 5) == 0 &&
                       (tag_start[5] == '>' || isspace((unsigned char)tag_start[5]))) {
                block_close = find_block_close(inner, end, "video", 5);
            } else if (inner + 8 <= end && strncasecmp(tag_start, "picture", 7) == 0 &&
                       (tag_start[7] == '>' || isspace((unsigned char)tag_start[7]))) {
                block_close = find_block_close(inner, end, "picture", 7);
            }
            if (block_close) {
                const char *after_block = block_close;
                while (after_block < end && (*after_block == ' ' || *after_block == '\t' || *after_block == '\n' || *after_block == '\r')) after_block++;
                if (after_block + 4 <= end &&
                    after_block[0] == '<' && after_block[1] == '/' &&
                    (after_block[2] == 'p' || after_block[2] == 'P') &&
                    (after_block[3] == '>' || isspace((unsigned char)after_block[3]))) {
                    const char *p_close = after_block + 3;
                    while (*p_close && *p_close != '>') p_close++;
                    if (*p_close == '>') {
                        p_close++;
                        size_t block_size = (size_t)(block_close - inner);
                        if (block_size >= remaining) {
                            size_t used = (size_t)(write - output);
                            capacity = used + block_size + 1024;
                            char *n = realloc(output, capacity);
                            if (!n) { free(output); return NULL; }
                            output = n;
                            write = output + used;
                            remaining = capacity - used;
                        }
                        memcpy(write, inner, block_size);
                        write += block_size;
                        remaining -= block_size;
                        read = p_close;
                        continue;
                    }
                }
            }
        }
        if (remaining < 2) {
            size_t used = (size_t)(write - output);
            capacity = (used + len) * 2;
            char *n = realloc(output, capacity);
            if (!n) { free(output); return NULL; }
            output = n;
            write = output + used;
            remaining = capacity - used;
        }
        *write++ = *read++;
        remaining--;
    }
    *write = '\0';
    return output;
}

/**
 * Check if a local file exists (regular file).
 */
static bool file_exists(const char *path) {
    if (!path || !*path) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

/**
 * Resolve relative URL against base directory for filesystem checks.
 * Returns allocated path or NULL. Skips absolute and remote URLs.
 */
static char *resolve_path_for_check(const char *base_dir, const char *url) {
    if (!base_dir || !*base_dir || !url || !*url) return NULL;
    if (url[0] == '/') return NULL;  /* Absolute path */
    if (strstr(url, "://")) return NULL;  /* Remote URL */
    size_t len = strlen(base_dir) + strlen(url) + 2;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s/%s", base_dir, url);
    return out;
}

/**
 * Insert @2x before extension in URL. Caller must free.
 */
static char *url_with_2x_suffix_auto(const char *url) {
    if (!url || !*url) return NULL;
    const char *path_end = strchr(url, '?');
    if (!path_end) path_end = strchr(url, '#');
    if (!path_end) path_end = url + strlen(url);
    const char *last_dot = NULL;
    for (const char *c = url; c < path_end; c++) {
        if (*c == '.') last_dot = c;
    }
    if (!last_dot) return NULL;
    size_t prefix_len = (size_t)(last_dot - url);
    size_t suffix_len = strlen(last_dot);
    char *out = malloc(prefix_len + 4 + suffix_len + 1);
    if (!out) return NULL;
    memcpy(out, url, prefix_len);
    memcpy(out + prefix_len, "@2x", 3);
    memcpy(out + prefix_len + 3, last_dot, suffix_len + 1);
    return out;
}

/**
 * Insert @3x before extension in URL. Caller must free.
 */
static char *url_with_3x_suffix_auto(const char *url) {
    if (!url || !*url) return NULL;
    const char *path_end = strchr(url, '?');
    if (!path_end) path_end = strchr(url, '#');
    if (!path_end) path_end = url + strlen(url);
    const char *last_dot = NULL;
    for (const char *c = url; c < path_end; c++) {
        if (*c == '.') last_dot = c;
    }
    if (!last_dot) return NULL;
    size_t prefix_len = (size_t)(last_dot - url);
    size_t suffix_len = strlen(last_dot);
    char *out = malloc(prefix_len + 4 + suffix_len + 1);
    if (!out) return NULL;
    memcpy(out, url, prefix_len);
    memcpy(out + prefix_len, "@3x", 3);
    memcpy(out + prefix_len + 3, last_dot, suffix_len + 1);
    return out;
}

/**
 * Check if URL ends with .* (wildcard extension for auto-discover).
 */
static bool url_ends_with_wildcard(const char *url) {
    if (!url || !*url) return false;
    size_t len = strlen(url);
    return (len >= 2 && url[len - 2] == '.' && url[len - 1] == '*');
}

/**
 * For URL ending in .*, get base path (everything before .*). Caller must free.
 */
static char *base_from_wildcard_url(const char *url) {
    if (!url || !*url) return NULL;
    size_t len = strlen(url);
    if (len < 2 || url[len - 2] != '.' || url[len - 1] != '*') return NULL;
    char *base = malloc(len - 1);
    if (!base) return NULL;
    memcpy(base, url, len - 2);
    base[len - 2] = '\0';
    return base;
}

/**
 * Check if URL has video extension (mp4, mov, webm, ogg, ogv, m4v).
 */
static bool is_video_url_auto(const char *url) {
    if (!url || !*url) return false;
    const char *path_end = strchr(url, '?');
    if (!path_end) path_end = strchr(url, '#');
    if (!path_end) path_end = url + strlen(url);
    const char *last_dot = NULL;
    for (const char *c = url; c < path_end; c++) {
        if (*c == '.') last_dot = c;
    }
    if (!last_dot || last_dot >= path_end - 1) return false;
    const char *ext = last_dot + 1;
    size_t ext_len = (size_t)(path_end - ext);
    if (ext_len == 3 && strncasecmp(ext, "mp4", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "mov", 3) == 0) return true;
    if (ext_len == 4 && strncasecmp(ext, "webm", 4) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "ogg", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "ogv", 3) == 0) return true;
    if (ext_len == 3 && strncasecmp(ext, "m4v", 3) == 0) return true;
    return false;
}

/**
 * Expand img tags with data-apex-replace-auto=1 by discovering existing
 * format variants on disk and generating appropriate <picture> or <video>.
 * Only processes local (relative) URLs when base_directory is provided.
 * Caller must free the returned string.
 */
char *apex_expand_auto_media(const char *html, const char *base_directory) {
    if (!html) return NULL;
    if (!base_directory || !*base_directory) return strdup(html);

    size_t len = strlen(html);
    size_t capacity = len * 2 + 2048;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        if (*read == '<' && (read[1] == 'i' || read[1] == 'I') &&
            (read[2] == 'm' || read[2] == 'M') && (read[3] == 'g' || read[3] == 'G') &&
            (read[4] == ' ' || read[4] == '\t' || read[4] == '>' || read[4] == '/')) {

            const char *tag_start = read;
            const char *tag_end = find_tag_end(tag_start);
            if (!tag_end) {
                *write++ = *read++;
                remaining--;
                continue;
            }

            /* Check for data-apex-replace-auto=1 */
            if (!strstr(tag_start, "data-apex-replace-auto=1")) {
                size_t tag_len = (size_t)(tag_end - tag_start + 1);
                if (tag_len >= remaining) {
                    size_t used = (size_t)(write - output);
                    capacity = used + tag_len + 2048;
                    char *new_out = realloc(output, capacity);
                    if (!new_out) { free(output); return NULL; }
                    output = new_out;
                    write = output + used;
                    remaining = capacity - used;
                }
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end + 1;
                continue;
            }

            char *src = extract_attr_from_tag(tag_start, tag_end + 1, "src");
            char *alt = extract_attr_from_tag(tag_start, tag_end + 1, "alt");
            char *title = extract_attr_from_tag(tag_start, tag_end + 1, "title");
            if (!src) src = strdup("");
            if (!alt) alt = strdup("");

            char *replacement = NULL;
            size_t repl_len = 0;

            /* When src ends with .*, discover first existing file to use as fallback */
            char *effective_src = strdup(src ? src : "");
            char *resolved = resolve_path_for_check(base_directory, effective_src);
            if (url_ends_with_wildcard(src)) {
                char *base = base_from_wildcard_url(src);
                if (base) {
                    /* Check video extensions first, then image extensions.
                     * url_with_extension(src, ext) works: "image.*" -> "image.jpg" */
                    static const char *video_exts[] = {"mp4", "webm", "ogg", "ogv", "mov", "m4v", NULL};
                    static const char *image_exts[] = {"jpg", "jpeg", "png", "gif", "webp", "avif", NULL};
                    bool found = false;
                    for (int i = 0; video_exts[i] && !found; i++) {
                        char *candidate = url_with_extension(src, video_exts[i]);
                        if (candidate) {
                            char *cpath = resolve_path_for_check(base_directory, candidate);
                            if (cpath && file_exists(cpath)) {
                                free(effective_src);
                                effective_src = candidate;
                                free(resolved);
                                resolved = cpath;
                                found = true;
                            } else {
                                free(cpath);
                                free(candidate);
                            }
                        }
                    }
                    for (int i = 0; image_exts[i] && !found; i++) {
                        char *candidate = url_with_extension(src, image_exts[i]);
                        if (candidate) {
                            char *cpath = resolve_path_for_check(base_directory, candidate);
                            if (cpath && file_exists(cpath)) {
                                free(effective_src);
                                effective_src = candidate;
                                free(resolved);
                                resolved = cpath;
                                found = true;
                            } else {
                                free(cpath);
                                free(candidate);
                            }
                        }
                    }
                    free(base);
                    if (!found) {
                        free(resolved);
                        resolved = NULL;
                    }
                }
            } else if (resolved && !file_exists(resolved)) {
                free(resolved);
                resolved = NULL;
            }

            if (resolved && file_exists(resolved)) {
                /* Use effective_src (may differ from src when wildcard was resolved) */
                free(src);
                src = effective_src;
                if (is_video_url_auto(src)) {
                    /* Video: discover alternative formats that exist */
                    static const char *video_exts[] = {"webm", "ogg", "mp4", "mov", "m4v", NULL};
                    size_t cap = 512 + strlen(src) * 6;
                    replacement = malloc(cap);
                    if (replacement) {
                        char *w = replacement;
                        w += snprintf(w, cap, "<video");
                        if (alt && *alt) w += snprintf(w, cap - (size_t)(w - replacement), " title=\"%s\"", alt);
                        w += snprintf(w, cap - (size_t)(w - replacement), ">");

                        for (int i = 0; video_exts[i]; i++) {
                            char *variant_url = url_with_extension(src, video_exts[i]);
                            if (variant_url) {
                                char *variant_path = resolve_path_for_check(base_directory, variant_url);
                                if (variant_path && file_exists(variant_path)) {
                                    const char *mime = (strcmp(video_exts[i], "webm") == 0) ? "video/webm" :
                                        (strcmp(video_exts[i], "ogg") == 0) ? "video/ogg" :
                                        (strcmp(video_exts[i], "mov") == 0) ? "video/quicktime" : "video/mp4";
                                    w += snprintf(w, cap - (size_t)(w - replacement),
                                        "<source src=\"%s\" type=\"%s\">", variant_url, mime);
                                }
                                free(variant_path);
                                free(variant_url);
                            }
                        }
                        w += snprintf(w, cap - (size_t)(w - replacement),
                            "<source src=\"%s\" type=\"%s\">", src, video_type_from_url(src));
                        w += snprintf(w, cap - (size_t)(w - replacement), "</video>");
                        repl_len = (size_t)(w - replacement);
                    }
                } else {
                    /* Image: discover 2x, 3x, webp, avif variants */
                    bool has_2x = false, has_3x = false;
                    bool has_webp_1x = false, has_webp_2x = false, has_webp_3x = false;
                    bool has_avif_1x = false, has_avif_2x = false, has_avif_3x = false;

                    char *url_2x = url_with_2x_suffix_auto(src);
                    char *url_3x = url_with_3x_suffix_auto(src);
                    if (url_2x) {
                        char *p2 = resolve_path_for_check(base_directory, url_2x);
                        has_2x = (p2 && file_exists(p2));
                        free(p2);
                    }
                    if (url_3x) {
                        char *p3 = resolve_path_for_check(base_directory, url_3x);
                        has_3x = (p3 && file_exists(p3));
                        free(p3);
                    }

                    char *webp_1x = url_with_extension(src, "webp");
                    if (webp_1x) {
                        char *p = resolve_path_for_check(base_directory, webp_1x);
                        has_webp_1x = (p && file_exists(p));
                        free(p);
                    }
                    if (url_2x && webp_1x) {
                        char *webp_2x = url_with_extension(url_2x, "webp");
                        if (webp_2x) {
                            char *p = resolve_path_for_check(base_directory, webp_2x);
                            has_webp_2x = (p && file_exists(p));
                            free(p);
                            free(webp_2x);
                        }
                    }
                    if (url_3x && webp_1x) {
                        char *webp_3x = url_with_extension(url_3x, "webp");
                        if (webp_3x) {
                            char *p = resolve_path_for_check(base_directory, webp_3x);
                            has_webp_3x = (p && file_exists(p));
                            free(p);
                            free(webp_3x);
                        }
                    }
                    free(webp_1x);

                    char *avif_1x = url_with_extension(src, "avif");
                    if (avif_1x) {
                        char *p = resolve_path_for_check(base_directory, avif_1x);
                        has_avif_1x = (p && file_exists(p));
                        free(p);
                    }
                    if (url_2x && avif_1x) {
                        char *avif_2x = url_with_extension(url_2x, "avif");
                        if (avif_2x) {
                            char *p = resolve_path_for_check(base_directory, avif_2x);
                            has_avif_2x = (p && file_exists(p));
                            free(p);
                            free(avif_2x);
                        }
                    }
                    if (url_3x && avif_1x) {
                        char *avif_3x = url_with_extension(url_3x, "avif");
                        if (avif_3x) {
                            char *p = resolve_path_for_check(base_directory, avif_3x);
                            has_avif_3x = (p && file_exists(p));
                            free(p);
                            free(avif_3x);
                        }
                    }
                    free(avif_1x);
                    free(url_2x);
                    free(url_3x);

                    bool need_picture = has_webp_1x || has_webp_2x || has_webp_3x ||
                        has_avif_1x || has_avif_2x || has_avif_3x;
                    bool need_srcset = has_2x || has_3x;

                    if (need_picture || need_srcset) {
                        size_t cap = 1024 + strlen(src) * 8;
                        replacement = malloc(cap);
                        if (replacement) {
                            char *w = replacement;
                            if (need_picture) w += snprintf(w, cap, "<picture>");

                            /* AVIF first (preferred), then WebP */
                            if (has_avif_1x || has_avif_2x || has_avif_3x) {
                                char *av1 = url_with_extension(src, "avif");
                                char *s2 = url_with_2x_suffix_auto(src);
                                char *av2 = s2 ? url_with_extension(s2, "avif") : NULL;
                                free(s2);
                                char *s3 = url_with_3x_suffix_auto(src);
                                char *av3 = s3 ? url_with_extension(s3, "avif") : NULL;
                                free(s3);
                                char srcset[512] = "";
                                if (av1) snprintf(srcset, sizeof(srcset), "%s 1x", av1);
                                if (av2 && has_avif_2x) {
                                    size_t l = strlen(srcset);
                                    snprintf(srcset + l, sizeof(srcset) - l, "%s%s 2x", l ? ", " : "", av2);
                                }
                                if (av3 && has_avif_3x) {
                                    size_t l = strlen(srcset);
                                    snprintf(srcset + l, sizeof(srcset) - l, "%s%s 3x", l ? ", " : "", av3);
                                }
                                if (*srcset) w += snprintf(w, cap - (size_t)(w - replacement),
                                    "<source type=\"image/avif\" srcset=\"%s\">", srcset);
                                free(av1); free(av2); free(av3);
                            }
                            if (has_webp_1x || has_webp_2x || has_webp_3x) {
                                char *wb1 = url_with_extension(src, "webp");
                                char *s2 = url_with_2x_suffix_auto(src);
                                char *wb2 = s2 ? url_with_extension(s2, "webp") : NULL;
                                free(s2);
                                char *s3 = url_with_3x_suffix_auto(src);
                                char *wb3 = s3 ? url_with_extension(s3, "webp") : NULL;
                                free(s3);
                                char srcset[512] = "";
                                if (wb1) snprintf(srcset, sizeof(srcset), "%s 1x", wb1);
                                if (wb2 && has_webp_2x) {
                                    size_t l = strlen(srcset);
                                    snprintf(srcset + l, sizeof(srcset) - l, "%s%s 2x", l ? ", " : "", wb2);
                                }
                                if (wb3 && has_webp_3x) {
                                    size_t l = strlen(srcset);
                                    snprintf(srcset + l, sizeof(srcset) - l, "%s%s 3x", l ? ", " : "", wb3);
                                }
                                if (*srcset) w += snprintf(w, cap - (size_t)(w - replacement),
                                    "<source type=\"image/webp\" srcset=\"%s\">", srcset);
                                free(wb1); free(wb2); free(wb3);
                            }

                            /* Build img with optional srcset for 2x/3x */
                            char srcset_attr[512] = "";
                            if (need_srcset) {
                                char *u2 = url_with_2x_suffix_auto(src);
                                char *u3 = url_with_3x_suffix_auto(src);
                                snprintf(srcset_attr, sizeof(srcset_attr), " srcset=\"%s 1x", src);
                                if (has_2x && u2) {
                                    size_t l = strlen(srcset_attr);
                                    snprintf(srcset_attr + l, sizeof(srcset_attr) - l, ", %s 2x", u2);
                                }
                                if (has_3x && u3) {
                                    size_t l = strlen(srcset_attr);
                                    snprintf(srcset_attr + l, sizeof(srcset_attr) - l, ", %s 3x", u3);
                                }
                                strcat(srcset_attr, "\"");
                                free(u2); free(u3);
                            }
                            /* Preserve title on img for caption logic */
                            if (title && *title) {
                                w += snprintf(w, cap - (size_t)(w - replacement),
                                    "<img src=\"%s\" alt=\"%s\" title=\"%s\"%s>%s",
                                    src, alt && *alt ? alt : "", title, srcset_attr,
                                    need_picture ? "</picture>" : "");
                            } else {
                                w += snprintf(w, cap - (size_t)(w - replacement),
                                    "<img src=\"%s\" alt=\"%s\"%s>%s",
                                    src, alt && *alt ? alt : "", srcset_attr,
                                    need_picture ? "</picture>" : "");
                            }
                            repl_len = (size_t)(w - replacement);
                        }
                    }
                }
                free(resolved);
            }

            if (replacement && repl_len > 0) {
                if (repl_len > remaining) {
                    size_t used = (size_t)(write - output);
                    capacity = used + repl_len + 1024;
                    char *new_out = realloc(output, capacity);
                    if (!new_out) { free(output); free(replacement); free(src); free(alt); free(title); return NULL; }
                    output = new_out;
                    write = output + used;
                    remaining = capacity - used;
                }
                memcpy(write, replacement, repl_len);
                write += repl_len;
                remaining -= repl_len;
                read = tag_end + 1;
            } else {
                /* Copy original tag */
                size_t tag_len = (size_t)(tag_end - tag_start + 1);
                if (tag_len >= remaining) {
                    size_t used = (size_t)(write - output);
                    capacity = used + tag_len + 1024;
                    char *new_out = realloc(output, capacity);
                    if (!new_out) { free(output); free(replacement); free(src); free(alt); free(title); return NULL; }
                    output = new_out;
                    write = output + used;
                    remaining = capacity - used;
                }
                memcpy(write, tag_start, tag_len);
                write += tag_len;
                remaining -= tag_len;
                read = tag_end + 1;
            }

            free(replacement);
            if (effective_src != src) free(effective_src);
            free(src);
            free(alt);
            free(title);
            continue;
        }

        if (remaining < 2) {
            size_t used = (size_t)(write - output);
            capacity = used + len + 1024;
            char *new_out = realloc(output, capacity);
            if (!new_out) { free(output); return NULL; }
            output = new_out;
            write = output + used;
            remaining = capacity - used;
        }
        *write++ = *read++;
        remaining--;
    }

    if (remaining < 1) {
        size_t used = (size_t)(write - output);
        char *new_out = realloc(output, used + 1);
        if (!new_out) { free(output); return NULL; }
        output = new_out;
        write = output + used;
    }
    *write = '\0';
    return output;
}
