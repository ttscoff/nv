/**
 * Pandoc Fenced Divs Extension for Apex
 * Implementation
 */

#include "fenced_divs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/**
 * Count consecutive colons at the start of a line
 * Returns the number of colons found
 */
static size_t count_colons(const char *line) {
    size_t count = 0;
    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Count consecutive colons */
    while (*p == ':') {
        count++;
        p++;
    }

    return count;
}

/**
 * Check if a line is a fenced div opening (has attributes)
 * Returns true if it's an opening fence with attributes
 */
static bool is_opening_fence(const char *line, size_t colon_count,
                             const char **attr_start, size_t *attr_len) {
    if (colon_count < 3) return false;

    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Skip colons */
    p += colon_count;

    /* Skip whitespace after colons */
    while (isspace((unsigned char)*p)) p++;

    /* Check if there are attributes (non-whitespace, non-colon content) */
    const char *attr_begin = p;
    const char *line_end = p;
    while (*line_end && *line_end != '\n' && *line_end != '\r') {
        line_end++;
    }

    /* Find the end of attributes (before any trailing colons) */
    const char *attr_end = line_end;
    const char *check = line_end - 1;

    /* Work backwards to find trailing colons */
    while (check >= attr_begin && *check == ':') {
        check--;
    }

    /* Skip whitespace before trailing colons */
    while (check >= attr_begin && isspace((unsigned char)*check)) {
        check--;
    }

    /* If we found non-colon content, attributes end at check+1 */
    if (check >= attr_begin) {
        attr_end = check + 1;
    }

    /* Check if we have actual attribute content (not just whitespace/colons) */
    const char *content_check = attr_begin;
    while (content_check < attr_end && (isspace((unsigned char)*content_check) || *content_check == ':')) {
        content_check++;
    }

    if (content_check < attr_end) {
        *attr_start = attr_begin;
        *attr_len = attr_end - attr_begin;
        return true;
    }

    return false;
}

/**
 * Check if a line is a closing fence (just colons, no attributes)
 */
static bool is_closing_fence(const char *line, size_t colon_count) {
    if (colon_count < 3) return false;

    const char *p = line;

    /* Skip leading whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Skip colons */
    p += colon_count;

    /* Skip trailing whitespace */
    while (isspace((unsigned char)*p)) p++;

    /* Should be end of line */
    return (*p == '\0' || *p == '\n' || *p == '\r');
}

/**
 * Parse block type from attribute text
 * Looks for >blocktype pattern at the start
 * Returns newly allocated block type string (defaults to "div"), or NULL on error
 * If block type is found, updates attr_text and attr_len to exclude it
 */
static char *parse_block_type(const char **attr_text, size_t *attr_len) {
    if (!attr_text || !*attr_text || !attr_len || *attr_len == 0) {
        return strdup("div");
    }

    const char *p = *attr_text;
    size_t len = *attr_len;

    /* Skip leading whitespace */
    while (len > 0 && isspace((unsigned char)*p)) {
        p++;
        len--;
    }

    /* Check for > prefix */
    if (len > 0 && *p == '>') {
        p++;
        len--;
        const char *type_start = p;

        /* Extract block type (word characters and hyphens) */
        while (len > 0 && (isalnum((unsigned char)*p) || *p == '-')) {
            p++;
            len--;
        }

        if (p > type_start) {
            size_t type_len = p - type_start;
            char *block_type = malloc(type_len + 1);
            if (block_type) {
                memcpy(block_type, type_start, type_len);
                block_type[type_len] = '\0';

                /* Update attr_text and attr_len to skip the >blocktype part */
                /* Skip whitespace after block type */
                while (len > 0 && isspace((unsigned char)*p)) {
                    p++;
                    len--;
                }

                *attr_text = p;
                *attr_len = len;
                return block_type;
            }
        }
    }

    /* No block type specified, default to div */
    return strdup("div");
}

/**
 * Parse attributes from fenced div info string
 * Format: {#id .class .class2 key="value" key2='value2'}
 * Or: .class (single unbraced word treated as class)
 * Or: >blocktype {#id .class ...} (block type followed by attributes)
 * Returns newly allocated HTML attribute string, or NULL on error
 */
static char *parse_attributes(const char *attr_text, size_t attr_len) {
    if (!attr_text || attr_len == 0) return NULL;

    char *buffer = malloc(attr_len + 1);
    if (!buffer) return NULL;
    memcpy(buffer, attr_text, attr_len);
    buffer[attr_len] = '\0';

    char *id = NULL;
    char **classes = NULL;
    size_t class_count = 0;
    size_t class_capacity = 0;
    char **keys = NULL;
    char **values = NULL;
    size_t attr_count = 0;
    size_t attr_capacity = 0;

    char *p = buffer;

    /* Trim whitespace */
    while (isspace((unsigned char)*p)) p++;
    char *end = buffer + attr_len;
    while (end > p && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';

    /* Check if it's wrapped in braces */
    bool has_braces = false;
    if (*p == '{') {
        has_braces = true;
        p++;
        if (end > p && *(end - 1) == '}') {
            end--;
            *end = '\0';
        }
    }

    /* If no braces and single word, treat as class */
    if (!has_braces) {
        char *word_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (p > word_start) {
            size_t word_len = p - word_start;
            char *class = malloc(word_len + 1);
            if (class) {
                memcpy(class, word_start, word_len);
                class[word_len] = '\0';

                class_capacity = 1;
                classes = malloc(sizeof(char*));
                if (classes) {
                    classes[0] = class;
                    class_count = 1;
                } else {
                    free(class);
                }
            }
        }
    } else {
        /* Parse attributes inside braces */
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
                    if (id) free(id);
                    id = strdup(id_start);
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

                    if (class_count >= class_capacity) {
                        class_capacity = class_capacity ? class_capacity * 2 : 4;
                        classes = realloc(classes, sizeof(char*) * class_capacity);
                        if (!classes) break;
                    }
                    classes[class_count++] = strdup(class_start);
                    *p = saved;
                }
                continue;
            }

            /* Check for key="value" or key='value' */
            char *key_start = p;
            while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '}') p++;

            if (*p == '=') {
                char saved = *p;
                *p = '\0';
                char *key = strdup(key_start);
                *p = saved;
                p++; /* Skip = */

                /* Parse value */
                char *value = NULL;
                if (*p == '"' || *p == '\'') {
                    char quote = *p++;
                    char *value_start = p;
                    while (*p && *p != quote) {
                        if (*p == '\\' && *(p+1)) p++;
                        p++;
                    }
                    if (*p == quote) {
                        *p = '\0';
                        value = strdup(value_start);
                        p++; /* Skip closing quote */
                    }
                } else {
                    char *value_start = p;
                    while (*p && !isspace((unsigned char)*p) && *p != '}') p++;
                    char saved_val = *p;
                    *p = '\0';
                    value = strdup(value_start);
                    *p = saved_val;
                }

                if (key && value) {
                    if (attr_count >= attr_capacity) {
                        attr_capacity = attr_capacity ? attr_capacity * 2 : 4;
                        keys = realloc(keys, sizeof(char*) * attr_capacity);
                        values = realloc(values, sizeof(char*) * attr_capacity);
                        if (!keys || !values) {
                            free(key);
                            free(value);
                            break;
                        }
                    }
                    keys[attr_count] = key;
                    values[attr_count] = value;
                    attr_count++;
                } else {
                    if (key) free(key);
                    if (value) free(value);
                }
                continue;
            }

            /* Unknown token, skip */
            p++;
        }
    }

    /* Build HTML attribute string */
    size_t html_capacity = 256;
    char *html_attrs = malloc(html_capacity);
    if (!html_attrs) {
        if (id) free(id);
        for (size_t i = 0; i < class_count; i++) free(classes[i]);
        if (classes) free(classes);
        for (size_t i = 0; i < attr_count; i++) {
            free(keys[i]);
            free(values[i]);
        }
        if (keys) free(keys);
        if (values) free(values);
        free(buffer);
        return NULL;
    }

    size_t html_len = 0;
    html_attrs[0] = '\0';

    /* Add ID */
    if (id) {
        size_t needed = strlen(id) + 5; /* id="..." */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " id=\"%s\"", id);
    }

    /* Add classes */
    if (class_count > 0) {
        size_t class_str_len = 0;
        for (size_t i = 0; i < class_count; i++) {
            class_str_len += strlen(classes[i]) + 1; /* +1 for space */
        }
        size_t needed = class_str_len + 8; /* class="..." */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " class=\"");
        for (size_t i = 0; i < class_count; i++) {
            if (i > 0) html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " ");
            html_len += snprintf(html_attrs + html_len, html_capacity - html_len, "%s", classes[i]);
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, "\"");
    }

    /* Add other attributes */
    for (size_t i = 0; i < attr_count; i++) {
        size_t needed = strlen(keys[i]) + strlen(values[i]) + 4; /* key="value" */
        if (html_len + needed >= html_capacity) {
            html_capacity = (html_len + needed) * 2;
            char *new_attrs = realloc(html_attrs, html_capacity);
            if (!new_attrs) {
                free(html_attrs);
                if (id) free(id);
                for (size_t i = 0; i < class_count; i++) free(classes[i]);
                if (classes) free(classes);
                for (size_t i = 0; i < attr_count; i++) {
                    free(keys[i]);
                    free(values[i]);
                }
                if (keys) free(keys);
                if (values) free(values);
                free(buffer);
                return NULL;
            }
            html_attrs = new_attrs;
        }
        html_len += snprintf(html_attrs + html_len, html_capacity - html_len, " %s=\"%s\"", keys[i], values[i]);
    }

    /* Cleanup */
    if (id) free(id);
    for (size_t i = 0; i < class_count; i++) free(classes[i]);
    if (classes) free(classes);
    for (size_t i = 0; i < attr_count; i++) {
        free(keys[i]);
        free(values[i]);
    }
    if (keys) free(keys);
    if (values) free(values);
    free(buffer);

    return html_attrs;
}

/**
 * Block-level HTML tag names recognized by cmark-gfm (scanners.re blocktagname).
 * Custom elements (e.g. custom-element) are not in this list, so cmark treats
 * them as inline and produces wrong structure. We wrap those in <div> and fix up later.
 */
static bool is_cmark_block_tag(const char *tag_name) {
    static const char *const block_tags[] = {
        "address", "article", "aside", "base", "basefont", "blockquote", "body",
        "caption", "center", "col", "colgroup", "dd", "details", "dialog", "dir",
        "div", "dl", "dt", "fieldset", "figcaption", "figure", "footer", "form",
        "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header",
        "hr", "html", "iframe", "legend", "li", "link", "main", "menu", "menuitem",
        "nav", "noframes", "ol", "optgroup", "option", "p", "param", "section",
        "source", "title", "summary", "table", "tbody", "td", "tfoot", "th", "thead",
        "tr", "track", "ul", NULL
    };
    for (const char *const *t = block_tags; *t; t++) {
        const char *a = tag_name;
        const char *b = *t;
        while (*a && *b) {
            int ca = (unsigned char)tolower((unsigned char)*a);
            int cb = (unsigned char)*b;
            if (ca != cb) break;
            a++;
            b++;
        }
        if (!*a && !*b) return true;
    }
    return false;
}

/**
 * Process fenced divs in text
 */
char *apex_process_fenced_divs(const char *text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;

    /* Track nesting level for divs */
    typedef struct {
        size_t colon_count;
        char *html_attrs;
        char *block_type;  /* HTML element type (div, aside, article, details, etc.) */
        bool wrapped;      /* true if we emitted <div data-apex-fenced-element="..."> for cmark */
    } div_stack_item;

    div_stack_item *div_stack = NULL;
    size_t div_stack_size = 0;
    size_t div_stack_capacity = 0;

    while (*read) {
        /* Find end of current line */
        const char *line_end = read;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        size_t line_len = line_end - read;
        char *line = malloc(line_len + 1);
        if (!line) {
            /* Cleanup and return */
            for (size_t i = 0; i < div_stack_size; i++) {
                if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                if (div_stack[i].block_type) free(div_stack[i].block_type);
            }
            if (div_stack) free(div_stack);
            free(output);
            return NULL;
        }
        memcpy(line, read, line_len);
        line[line_len] = '\0';

        size_t colon_count = count_colons(line);
        const char *attr_start = NULL;
        size_t attr_len = 0;
        bool is_opening = is_opening_fence(line, colon_count, &attr_start, &attr_len);
        bool is_closing = is_closing_fence(line, colon_count);

        if (is_opening) {
            /* Opening fence with attributes */
            /* Parse block type first (may modify attr_start and attr_len) */
            const char *attr_text = attr_start;
            size_t attr_len_remaining = attr_len;
            char *block_type = parse_block_type(&attr_text, &attr_len_remaining);

            /* Parse attributes (excluding block type) */
            char *html_attrs = parse_attributes(attr_text, attr_len_remaining);

            /* Push to stack */
            if (div_stack_size >= div_stack_capacity) {
                div_stack_capacity = div_stack_capacity ? div_stack_capacity * 2 : 4;
                div_stack = realloc(div_stack, sizeof(div_stack_item) * div_stack_capacity);
                if (!div_stack) {
                    free(line);
                    if (block_type) free(block_type);
                    if (html_attrs) free(html_attrs);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                        if (div_stack[i].block_type) free(div_stack[i].block_type);
                    }
                    free(output);
                    return NULL;
                }
            }

            const char *tag_name = block_type ? block_type : "div";
            bool use_wrapper = !is_cmark_block_tag(tag_name);

            div_stack[div_stack_size].colon_count = colon_count;
            div_stack[div_stack_size].html_attrs = html_attrs;
            div_stack[div_stack_size].block_type = block_type ? block_type : strdup("div");
            div_stack[div_stack_size].wrapped = use_wrapper;
            div_stack_size++;

            /* For custom elements (not in cmark's block list), emit <div data-apex-fenced-element="tagname" ...>
             * so cmark sees a block tag; we fix it back to <tagname> in post-process. */
            const char *emit_tag = use_wrapper ? "div" : tag_name;
            size_t emit_tag_len = strlen(emit_tag);
            size_t markdown_attr_len = 13; /*  markdown="1" */
            size_t wrapper_attr_len = use_wrapper ? (22 + strlen(tag_name)) : 0; /*  data-apex-fenced-element="tagname" */
            size_t needed = 1 + emit_tag_len + 1 + (html_attrs ? strlen(html_attrs) : 0) + wrapper_attr_len + markdown_attr_len + 1;
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                        if (div_stack[i].block_type) free(div_stack[i].block_type);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            if (use_wrapper) {
                if (html_attrs) {
                    write += snprintf(write, remaining, "<div data-apex-fenced-element=\"%s\"%s markdown=\"1\">", tag_name, html_attrs);
                } else {
                    write += snprintf(write, remaining, "<div data-apex-fenced-element=\"%s\" markdown=\"1\">", tag_name);
                }
            } else {
                if (html_attrs) {
                    write += snprintf(write, remaining, "<%s%s markdown=\"1\">", emit_tag, html_attrs);
                } else {
                    write += snprintf(write, remaining, "<%s markdown=\"1\">", emit_tag);
                }
            }
            remaining = output_capacity - (write - output);

            /* Emit newline after opening tag so inner content is on its own line(s);
             * otherwise cmark can treat tag+content as one HTML block and IAL/paragraphs won't parse. */
            if (remaining > 0) {
                *write++ = '\n';
                remaining--;
            }

            /* Skip the fence line */
            read = line_end;
            if (*read == '\r') read++;
            if (*read == '\n') read++;
        } else if (is_closing && div_stack_size > 0) {
            /* Closing fence - pop from stack */
            div_stack_size--;
            char *block_type = div_stack[div_stack_size].block_type;
            bool was_wrapped = div_stack[div_stack_size].wrapped;
            if (div_stack[div_stack_size].html_attrs) {
                free(div_stack[div_stack_size].html_attrs);
            }

            /* Write closing tag: </div> when we wrapped for cmark, else </block_type> */
            const char *close_tag = was_wrapped ? "div" : (block_type ? block_type : "div");
            size_t tag_name_len = strlen(close_tag);
            size_t needed = 2 + tag_name_len + 1; /* </tag> */
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    if (block_type) free(block_type);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                        if (div_stack[i].block_type) free(div_stack[i].block_type);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            write += snprintf(write, remaining, "</%s>", close_tag);
            if (block_type) free(block_type);
            remaining = output_capacity - (write - output);

            /* Skip the fence line */
            read = line_end;
            if (*read == '\r') read++;
            if (*read == '\n') read++;
        } else {
            /* Regular line - copy as-is */
            size_t needed = line_len + 1; /* +1 for newline */
            if (remaining < needed) {
                size_t written = write - output;
                output_capacity = (written + needed) * 2;
                char *new_output = realloc(output, output_capacity);
                if (!new_output) {
                    free(line);
                    for (size_t i = 0; i < div_stack_size; i++) {
                        if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                        if (div_stack[i].block_type) free(div_stack[i].block_type);
                    }
                    if (div_stack) free(div_stack);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = output_capacity - written;
            }

            memcpy(write, read, line_len);
            write += line_len;
            remaining -= line_len;

            /* Copy newline */
            if (*line_end == '\r') {
                *write++ = '\r';
                remaining--;
                line_end++;
            }
            if (*line_end == '\n') {
                *write++ = '\n';
                remaining--;
                line_end++;
            }

            read = line_end;
        }

        free(line);
    }

    /* Close any remaining divs (shouldn't happen in valid input) */
    while (div_stack_size > 0) {
        div_stack_size--;
        char *block_type = div_stack[div_stack_size].block_type;
        bool was_wrapped = div_stack[div_stack_size].wrapped;
        if (div_stack[div_stack_size].html_attrs) {
            free(div_stack[div_stack_size].html_attrs);
        }
        const char *close_tag = was_wrapped ? "div" : (block_type ? block_type : "div");
        size_t tag_name_len = strlen(close_tag);
        size_t needed = 2 + tag_name_len + 1; /* </tag> */
        if (remaining < needed) {
            size_t written = write - output;
            output_capacity = (written + needed) * 2;
            char *new_output = realloc(output, output_capacity);
            if (!new_output) {
                if (block_type) free(block_type);
                for (size_t i = 0; i < div_stack_size; i++) {
                    if (div_stack[i].html_attrs) free(div_stack[i].html_attrs);
                    if (div_stack[i].block_type) free(div_stack[i].block_type);
                }
                if (div_stack) free(div_stack);
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = output_capacity - written;
        }
        write += snprintf(write, remaining, "</%s>", close_tag);
        if (block_type) free(block_type);
        remaining = output_capacity - (write - output);
    }

    if (div_stack) free(div_stack);

    *write = '\0';
    return output;
}

#define FENCED_ELEMENT_PREFIX "<div data-apex-fenced-element=\""
#define FENCED_ELEMENT_PREFIX_LEN (sizeof(FENCED_ELEMENT_PREFIX) - 1)

/**
 * Post-process HTML to replace wrapper <div data-apex-fenced-element="tagname">...</div>
 * with <tagname>...</tagname>. Called after rendering so custom elements get correct output.
 * Returns newly allocated string, or NULL on error (caller keeps ownership of html).
 */
char *apex_postprocess_fenced_divs_html(const char *html) {
    if (!html) return NULL;

    const char *search = html;
    size_t html_len = strlen(html);
    /* Output may be shorter (removing data-apex-fenced-element attr) or longer (longer tagname) */
    size_t cap = html_len + 1024;
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *read = html;
    char *write = out;
    size_t remaining = cap;

    while ((search = strstr(search, FENCED_ELEMENT_PREFIX)) != NULL) {
        /* Copy from read to search */
        size_t chunk = (size_t)(search - read);
        if (chunk >= remaining) {
            size_t written = (size_t)(write - out);
            cap = written + chunk + html_len + 1024;
            char *new_out = realloc(out, cap);
            if (!new_out) {
                free(out);
                return NULL;
            }
            out = new_out;
            write = out + written;
            remaining = cap - written;
        }
        memcpy(write, read, chunk);
        write += chunk;
        remaining -= chunk;

        const char *p = search + FENCED_ELEMENT_PREFIX_LEN;
        const char *quote_end = strchr(p, '"');
        if (!quote_end) break;
        size_t tagname_len = (size_t)(quote_end - p);
        if (tagname_len == 0 || tagname_len > 127) break;

        char tagname[128];
        memcpy(tagname, p, tagname_len);
        tagname[tagname_len] = '\0';

        /* Find end of opening tag */
        const char *tag_end = strchr(quote_end + 1, '>');
        if (!tag_end) break;

        /* Build new opening tag: <tagname + rest of tag with data-apex-fenced-element="tagname" removed */
        const char *after_attr = quote_end + 1;
        while (after_attr < tag_end && (isspace((unsigned char)*after_attr))) after_attr++;

        size_t rest_len = (size_t)(tag_end - after_attr);
        size_t open_tag_len = 1 + tagname_len + 1 + rest_len + 1; /* <tagname ...> */
        if (rest_len >= remaining) {
            size_t written = (size_t)(write - out);
            cap = written + open_tag_len + 256;
            char *new_out = realloc(out, cap);
            if (!new_out) {
                free(out);
                return NULL;
            }
            out = new_out;
            write = out + written;
            remaining = cap - written;
        }

        *write++ = '<';
        memcpy(write, tagname, tagname_len);
        write += tagname_len;
        if (rest_len > 0) {
            *write++ = ' ';
            memcpy(write, after_attr, rest_len);
            write += rest_len;
        }
        *write++ = '>';
        remaining = cap - (size_t)(write - out);

        read = tag_end + 1;

        /* Find matching </div> by counting nested divs */
        int depth = 1;
        const char *scan = read;
        while (*scan && depth > 0) {
            if (strncmp(scan, "</div>", 6) == 0) {
                depth--;
                if (depth == 0) {
                    /* Copy content from read to scan */
                    size_t content_len = (size_t)(scan - read);
                    if (content_len >= remaining) {
                        size_t written = (size_t)(write - out);
                        cap = written + content_len + tagname_len + 16;
                        char *new_out = realloc(out, cap);
                        if (!new_out) {
                            free(out);
                            return NULL;
                        }
                        out = new_out;
                        write = out + written;
                        remaining = cap - written;
                    }
                    memcpy(write, read, content_len);
                    write += content_len;
                    remaining -= content_len;

                    /* Write </tagname> */
                    size_t close_len = 2 + tagname_len + 1;
                    if (close_len >= remaining) {
                        size_t written = (size_t)(write - out);
                        cap = written + close_len + 64;
                        char *new_out = realloc(out, cap);
                        if (!new_out) {
                            free(out);
                            return NULL;
                        }
                        out = new_out;
                        write = out + written;
                        remaining = cap - written;
                    }
                    *write++ = '<';
                    *write++ = '/';
                    memcpy(write, tagname, tagname_len);
                    write += tagname_len;
                    *write++ = '>';
                    remaining = cap - (size_t)(write - out);

                    read = scan + 6;
                    break;
                }
                scan += 6;
                continue;
            }
            if (scan[0] == '<' && scan[1] == 'd' && scan[2] == 'i' && scan[3] == 'v' &&
                (scan[4] == '>' || isspace((unsigned char)scan[4]))) {
                depth++;
                scan += 4;
                while (*scan && *scan != '>') scan++;
                if (*scan == '>') scan++;
                continue;
            }
            scan++;
        }
        if (depth != 0) break; /* Unmatched, stop */
        search = read;
    }

    /* Copy remainder */
    size_t rest = strlen(read);
    if (rest >= remaining) {
        size_t written = (size_t)(write - out);
        cap = written + rest + 1;
        char *new_out = realloc(out, cap);
        if (!new_out) {
            free(out);
            return NULL;
        }
        out = new_out;
        write = out + written;
    }
    memcpy(write, read, rest + 1);

    return out;
}

