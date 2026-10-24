/**
 * Callouts Extension for Apex
 * Implementation
 */

#include "callouts.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <stdio.h>

/**
 * Detect callout type from string (case-insensitive)
 */
static callout_type_t detect_callout_type(const char *type_str, int len) {
    char type_upper[64];
    if (len >= (int)sizeof(type_upper)) return CALLOUT_NONE;

    /* Convert to uppercase for comparison */
    for (int i = 0; i < len; i++) {
        type_upper[i] = toupper((unsigned char)type_str[i]);
    }
    type_upper[len] = '\0';

    /* Check all callout types */
    if (strcmp(type_upper, "NOTE") == 0) return CALLOUT_NOTE;
    if (strcmp(type_upper, "ABSTRACT") == 0 || strcmp(type_upper, "SUMMARY") == 0 || strcmp(type_upper, "TLDR") == 0) return CALLOUT_ABSTRACT;
    if (strcmp(type_upper, "INFO") == 0) return CALLOUT_INFO;
    if (strcmp(type_upper, "TODO") == 0) return CALLOUT_TODO;
    if (strcmp(type_upper, "TIP") == 0 || strcmp(type_upper, "HINT") == 0 || strcmp(type_upper, "IMPORTANT") == 0) return CALLOUT_TIP;
    if (strcmp(type_upper, "SUCCESS") == 0 || strcmp(type_upper, "CHECK") == 0 || strcmp(type_upper, "DONE") == 0) return CALLOUT_SUCCESS;
    if (strcmp(type_upper, "QUESTION") == 0 || strcmp(type_upper, "HELP") == 0 || strcmp(type_upper, "FAQ") == 0) return CALLOUT_QUESTION;
    if (strcmp(type_upper, "WARNING") == 0 || strcmp(type_upper, "CAUTION") == 0 || strcmp(type_upper, "ATTENTION") == 0) return CALLOUT_WARNING;
    if (strcmp(type_upper, "FAILURE") == 0 || strcmp(type_upper, "FAIL") == 0 || strcmp(type_upper, "MISSING") == 0) return CALLOUT_FAILURE;
    if (strcmp(type_upper, "DANGER") == 0 || strcmp(type_upper, "ERROR") == 0) return CALLOUT_DANGER;
    if (strcmp(type_upper, "BUG") == 0) return CALLOUT_BUG;
    if (strcmp(type_upper, "EXAMPLE") == 0) return CALLOUT_EXAMPLE;
    if (strcmp(type_upper, "QUOTE") == 0 || strcmp(type_upper, "CITE") == 0) return CALLOUT_QUOTE;

    return CALLOUT_NONE;
}

/**
 * Get callout type name for HTML class
 */
static const char *callout_type_name(callout_type_t type) {
    switch (type) {
        case CALLOUT_NOTE: return "note";
        case CALLOUT_ABSTRACT: return "abstract";
        case CALLOUT_INFO: return "info";
        case CALLOUT_TODO: return "todo";
        case CALLOUT_TIP: return "tip";
        case CALLOUT_SUCCESS: return "success";
        case CALLOUT_QUESTION: return "question";
        case CALLOUT_WARNING: return "warning";
        case CALLOUT_FAILURE: return "failure";
        case CALLOUT_DANGER: return "danger";
        case CALLOUT_BUG: return "bug";
        case CALLOUT_EXAMPLE: return "example";
        case CALLOUT_QUOTE: return "quote";
        default: return "note";
    }
}

/**
 * Check if a blockquote is a Bear/Obsidian style callout
 * Pattern: > [!TYPE] Title or > [!TYPE]+ Title or > [!TYPE]- Title
 *
 * On success, *marker_len is the number of bytes at the start of the first
 * text node taken up by the marker and the whitespace after it. Everything
 * else on the marker line (including inline markup) is the title.
 */
static bool is_bear_callout(cmark_node *blockquote, bool enable_py_callouts, callout_type_t *type,
                            size_t *marker_len, bool *collapsible, bool *default_open) {
    if (cmark_node_get_type(blockquote) != CMARK_NODE_BLOCK_QUOTE) return false;

    /* Get first child (should be paragraph) */
    cmark_node *first_child = cmark_node_first_child(blockquote);
    if (!first_child || cmark_node_get_type(first_child) != CMARK_NODE_PARAGRAPH) return false;

    /* Get the text content */
    cmark_node *text_node = cmark_node_first_child(first_child);
    if (!text_node || cmark_node_get_type(text_node) != CMARK_NODE_TEXT) return false;

    const char *text = cmark_node_get_literal(text_node);
    if (!text) return false;

    /* markdown-callouts collapsed syntax in blockquote: ? TYPE: Title */
    if (enable_py_callouts && text[0] == '?' && text[1] == ' ') {
        const char *type_start = text + 2;
        const char *p = type_start;
        while (*p && (isalnum((unsigned char)*p) || *p == '-' || *p == '_')) p++;
        if (*p == ':') {
            int type_len = (int)(p - type_start);
            if (type_len > 0) {
                *type = detect_callout_type(type_start, type_len);
                if (*type != CALLOUT_NONE) {
                    *collapsible = true;
                    *default_open = false;
                    const char *title_start = p + 1;
                    while (*title_start == ' ' || *title_start == '\t') title_start++;
                    *marker_len = (size_t)(title_start - text);
                    return true;
                }
            }
        }
    }

    /* Check for [!TYPE] pattern */
    if (text[0] != '[' || text[1] != '!') return false;

    const char *type_start = text + 2;
    const char *type_end = strchr(type_start, ']');
    if (!type_end) return false;

    /* Extract type first */
    int type_len = (int)(type_end - type_start);
    if (type_len <= 0) return false;

    *type = detect_callout_type(type_start, type_len);
    if (*type == CALLOUT_NONE) return false;

    /* Check for collapsible markers + or - after the ] */
    *collapsible = false;
    *default_open = true;

    if (*(type_end + 1) == '+') {
        *collapsible = true;
        *default_open = true;
        type_end++;
    } else if (*(type_end + 1) == '-') {
        *collapsible = true;
        *default_open = false;
        type_end++;
    }

    const char *title_start = type_end + 1;
    while (*title_start == ' ' || *title_start == '\t') title_start++;
    *marker_len = (size_t)(title_start - text);

    return true;
}

static bool is_line_break(cmark_node *node) {
    cmark_node_type t = cmark_node_get_type(node);
    return t == CMARK_NODE_SOFTBREAK || t == CMARK_NODE_LINEBREAK;
}

/**
 * Move the inlines on the marker line out of the first paragraph and render
 * them as the title HTML. Returns NULL when the marker line has no title.
 */
static char *extract_callout_title_html(cmark_node *para, int render_options) {
    /* The HTML renderer dereferences a paragraph's parent, so render from a document */
    cmark_node *title_doc = cmark_node_new(CMARK_NODE_DOCUMENT);
    cmark_node *title_para = cmark_node_new(CMARK_NODE_PARAGRAPH);
    if (!title_doc || !title_para || !cmark_node_append_child(title_doc, title_para)) {
        if (title_para) cmark_node_free(title_para);
        if (title_doc) cmark_node_free(title_doc);
        return NULL;
    }

    cmark_node *child = cmark_node_first_child(para);
    while (child && !is_line_break(child)) {
        cmark_node *next = cmark_node_next(child);
        cmark_node_append_child(title_para, child);
        child = next;
    }
    if (child) {
        cmark_node_free(child);
    }

    char *title = NULL;
    if (cmark_node_first_child(title_para)) {
        char *html = cmark_render_html(title_doc, render_options & ~CMARK_OPT_SOURCEPOS, NULL);
        if (html) {
            const char *start = html;
            if (strncmp(start, "<p>", 3) == 0) start += 3;
            const char *end = start + strlen(start);
            while (end > start && isspace((unsigned char)end[-1])) end--;
            if (end - start >= 4 && strncmp(end - 4, "</p>", 4) == 0) end -= 4;
            while (end > start && isspace((unsigned char)end[-1])) end--;
            while (start < end && isspace((unsigned char)*start)) start++;
            if (end > start) {
                title = strndup(start, (size_t)(end - start));
            }
            free(html);
        }
    }

    cmark_node_free(title_doc);
    return title;
}

/**
 * Convert blockquote to callout HTML
 */
static void convert_blockquote_to_callout(cmark_node *blockquote, callout_type_t type,
                                         size_t marker_len, bool collapsible, bool default_open,
                                         int render_options) {
    const char *type_name = callout_type_name(type);

    /* Strip the marker from the first text node, then lift the rest of the
     * marker line into the title */
    cmark_node *first_para = cmark_node_first_child(blockquote);
    cmark_node *first_text = cmark_node_first_child(first_para);
    const char *text = cmark_node_get_literal(first_text);
    if (text && strlen(text) > marker_len) {
        char *rest = strdup(text + marker_len);
        if (rest) {
            cmark_node_set_literal(first_text, rest);
            free(rest);
        }
    } else {
        cmark_node_free(first_text);
    }

    char *title = extract_callout_title_html(first_para, render_options);
    if (!cmark_node_first_child(first_para)) {
        cmark_node_free(first_para);
    }

    const char *title_html = title ? title : type_name;
    size_t start_size = strlen(title_html) + 256;
    char *html_start = malloc(start_size);
    if (!html_start) {
        free(title);
        return;
    }
    const char *html_end;

    if (collapsible) {
        snprintf(html_start, start_size,
                "<details class=\"callout callout-%s\"%s>\n<summary>%s</summary>\n<div class=\"callout-content\">\n",
                type_name, default_open ? " open" : "", title_html);
        html_end = "\n</div>\n</details>";
    } else {
        snprintf(html_start, start_size,
                "<div class=\"callout callout-%s\">\n<div class=\"callout-title\">%s</div>\n<div class=\"callout-content\">\n",
                type_name, title_html);
        html_end = "\n</div>\n</div>";
    }
    free(title);

    /* Wrap the blockquote; its content renders normally inside the wrapper */
    cmark_node *html_before = cmark_node_new(CMARK_NODE_HTML_BLOCK);
    cmark_node_set_literal(html_before, html_start);
    free(html_start);

    cmark_node *html_after = cmark_node_new(CMARK_NODE_HTML_BLOCK);
    cmark_node_set_literal(html_after, html_end);

    cmark_node_insert_before(blockquote, html_before);
    cmark_node_insert_after(blockquote, html_after);
}

/**
 * Process callouts in AST
 */
void apex_process_callouts_in_tree(cmark_node *node, bool enable_py_callouts, int render_options) {
    if (!node) return;

    if (cmark_node_get_type(node) == CMARK_NODE_BLOCK_QUOTE) {
        callout_type_t type;
        size_t marker_len = 0;
        bool collapsible, default_open;

        if (is_bear_callout(node, enable_py_callouts, &type, &marker_len, &collapsible, &default_open)) {
            convert_blockquote_to_callout(node, type, marker_len, collapsible, default_open,
                                          render_options);
        }
    }

    /* Recurse into children, including converted callouts, so nested
     * callouts (> > [!TYPE]) are converted too */
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        cmark_node *next = cmark_node_next(child);
        apex_process_callouts_in_tree(child, enable_py_callouts, render_options);
        child = next;
    }
}

static bool append_chunk(char **out, size_t *len, size_t *cap, const char *chunk, size_t chunk_len) {
    if (!out || !len || !cap || !chunk) return false;
    if (*len + chunk_len + 1 > *cap) {
        size_t new_cap = *cap ? *cap : 256;
        while (*len + chunk_len + 1 > new_cap) {
            new_cap *= 2;
        }
        char *new_out = realloc(*out, new_cap);
        if (!new_out) return false;
        *out = new_out;
        *cap = new_cap;
    }
    memcpy(*out + *len, chunk, chunk_len);
    *len += chunk_len;
    (*out)[*len] = '\0';
    return true;
}

static bool append_str(char **out, size_t *len, size_t *cap, const char *str) {
    if (!str) return true;
    return append_chunk(out, len, cap, str, strlen(str));
}

static char *trim_ascii_whitespace(char *s) {
    if (!s) return s;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return s;
}

char *apex_preprocess_py_callouts(const char *text) {
    if (!text) return NULL;

    size_t cap = strlen(text) * 2 + 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    out[0] = '\0';

    bool in_fenced_code = false;
    const char *p = text;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        bool has_nl = line_end != NULL;
        size_t line_len = has_nl ? (size_t)(line_end - line_start) : strlen(line_start);

        const char *content = line_start;
        while ((size_t)(content - line_start) < line_len && (*content == ' ' || *content == '\t')) content++;
        size_t content_len = line_len - (size_t)(content - line_start);

        if (content_len >= 3 && strncmp(content, "```", 3) == 0) {
            in_fenced_code = !in_fenced_code;
        }

        if (!in_fenced_code) {
            if (content_len > 4 && strncmp(content, ">? ", 3) == 0) {
                const char *spec = content + 3;
                const char *spec_end = content + content_len;
                const char *type_start = spec;
                while (spec < spec_end && (isalnum((unsigned char)*spec) || *spec == '-' || *spec == '_')) spec++;
                if (spec > type_start && spec < spec_end && *spec == ':') {
                    int type_len = (int)(spec - type_start);
                    callout_type_t ct = detect_callout_type(type_start, type_len);
                    if (ct != CALLOUT_NONE) {
                        spec++; /* skip : */
                        while (spec < spec_end && *spec == ' ') spec++;

                        char header[640];
                        if (spec < spec_end) {
                            snprintf(header, sizeof(header), "> [!%.*s]- %.*s",
                                     type_len, type_start, (int)(spec_end - spec), spec);
                        } else {
                            snprintf(header, sizeof(header), "> [!%.*s]-", type_len, type_start);
                        }
                        if (!append_str(&out, &len, &cap, header)) goto fail;
                        if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                        p = has_nl ? line_end + 1 : line_start + line_len;
                        continue;
                    }
                }
            }

            const char *spec = content;
            const char *spec_end = content + content_len;
            const char *type_start = spec;
            while (spec < spec_end && (isalnum((unsigned char)*spec) || *spec == '-' || *spec == '_')) spec++;
            if (spec > type_start && spec < spec_end && *spec == ':') {
                int type_len = (int)(spec - type_start);
                callout_type_t ct = detect_callout_type(type_start, type_len);
                if (ct != CALLOUT_NONE) {
                    spec++; /* skip : */
                    while (spec < spec_end && *spec == ' ') spec++;

                    char header[128];
                    snprintf(header, sizeof(header), "> [!%.*s]", type_len, type_start);
                    if (!append_str(&out, &len, &cap, header)) goto fail;
                    if (!append_chunk(&out, &len, &cap, "\n", 1)) goto fail;

                    if (!append_str(&out, &len, &cap, "> ")) goto fail;
                    if (spec < spec_end && !append_chunk(&out, &len, &cap, spec, (size_t)(spec_end - spec))) goto fail;
                    if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;

                    p = has_nl ? line_end + 1 : line_start + line_len;
                    if (!append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                    while (*p) {
                        const char *body_start = p;
                        const char *body_end = strchr(p, '\n');
                        bool body_nl = body_end != NULL;
                        size_t body_len = body_nl ? (size_t)(body_end - body_start) : strlen(body_start);
                        bool blank = true;
                        for (size_t i = 0; i < body_len; i++) {
                            if (body_start[i] != ' ' && body_start[i] != '\t' && body_start[i] != '\r') {
                                blank = false;
                                break;
                            }
                        }
                        if (blank) break;
                        if (!append_str(&out, &len, &cap, "> ")) goto fail;
                        if (!append_chunk(&out, &len, &cap, body_start, body_len)) goto fail;
                        if (body_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                        p = body_nl ? body_end + 1 : body_start + body_len;
                    }
                    if (!append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                    continue;
                }
            }
        }

        if (!in_fenced_code && content_len >= 4 && strncmp(content, "!!! ", 4) == 0) {
            const char *spec = content + 4;
            const char *spec_end = content + content_len;
            while (spec < spec_end && *spec == ' ') spec++;
            const char *type_start = spec;
            while (spec < spec_end && (isalnum((unsigned char)*spec) || *spec == '-' || *spec == '_')) spec++;
            if (spec > type_start) {
                int type_len = (int)(spec - type_start);
                callout_type_t ct = detect_callout_type(type_start, type_len);
                if (ct != CALLOUT_NONE) {
                    char title_buf[512];
                    title_buf[0] = '\0';
                    while (spec < spec_end && *spec == ' ') spec++;
                    if (spec < spec_end) {
                        if (*spec == '"' || *spec == '\'') {
                            char quote = *spec++;
                            const char *title_start = spec;
                            while (spec < spec_end && *spec != quote) spec++;
                            size_t tlen = (size_t)(spec - title_start);
                            if (tlen >= sizeof(title_buf)) tlen = sizeof(title_buf) - 1;
                            memcpy(title_buf, title_start, tlen);
                            title_buf[tlen] = '\0';
                        } else {
                            size_t tlen = (size_t)(spec_end - spec);
                            if (tlen >= sizeof(title_buf)) tlen = sizeof(title_buf) - 1;
                            memcpy(title_buf, spec, tlen);
                            title_buf[tlen] = '\0';
                            trim_ascii_whitespace(title_buf);
                        }
                    }

                    char header[640];
                    if (title_buf[0]) {
                        snprintf(header, sizeof(header), "> [!%.*s] %s", type_len, type_start, title_buf);
                    } else {
                        snprintf(header, sizeof(header), "> [!%.*s]", type_len, type_start);
                    }
                    if (!append_str(&out, &len, &cap, header)) goto fail;
                    if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;

                    p = has_nl ? line_end + 1 : line_start + line_len;
                    while (*p) {
                        const char *body_start = p;
                        const char *body_end = strchr(p, '\n');
                        bool body_nl = body_end != NULL;
                        size_t body_len = body_nl ? (size_t)(body_end - body_start) : strlen(body_start);

                        size_t indent = 0;
                        while (indent < body_len && body_start[indent] == ' ') indent++;
                        bool blank = true;
                        for (size_t i = 0; i < body_len; i++) {
                            if (body_start[i] != ' ' && body_start[i] != '\t' && body_start[i] != '\r') {
                                blank = false;
                                break;
                            }
                        }

                        if (!blank && indent < 4) break;

                        if (!append_str(&out, &len, &cap, ">")) goto fail;
                        if (!blank) {
                            if (!append_str(&out, &len, &cap, " ")) goto fail;
                            size_t strip = indent >= 4 ? 4 : indent;
                            if (!append_chunk(&out, &len, &cap, body_start + strip, body_len - strip)) goto fail;
                        }
                        if (body_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;

                        p = body_nl ? body_end + 1 : body_start + body_len;
                    }
                    if (!append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                    continue;
                }
            }
        }

        if (!append_chunk(&out, &len, &cap, line_start, line_len)) goto fail;
        if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
        p = has_nl ? line_end + 1 : line_start + line_len;
    }

    return out;

fail:
    free(out);
    return NULL;
}

char *apex_preprocess_quarto_callouts(const char *text) {
    if (!text) return NULL;

    size_t cap = strlen(text) * 2 + 256;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    out[0] = '\0';

    bool in_fenced_code = false;
    const char *p = text;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        bool has_nl = line_end != NULL;
        size_t line_len = has_nl ? (size_t)(line_end - line_start) : strlen(line_start);

        const char *content = line_start;
        while ((size_t)(content - line_start) < line_len && (*content == ' ' || *content == '\t')) content++;
        size_t content_len = line_len - (size_t)(content - line_start);

        if (content_len >= 3 && strncmp(content, "```", 3) == 0) {
            in_fenced_code = !in_fenced_code;
        }

        if (!in_fenced_code && content_len >= 3 && strncmp(content, ":::", 3) == 0) {
            const char *after = content + 3;
            while (after < content + content_len && *after == ' ') after++;
            if (after < content + content_len && *after == '{') {
                const char *close = content + content_len - 1;
                while (close > after && *close != '}') close--;
                if (*close == '}') {
                    size_t attrs_len = (size_t)(close - (after + 1));
                    char attrs[768];
                    if (attrs_len >= sizeof(attrs)) attrs_len = sizeof(attrs) - 1;
                    memcpy(attrs, after + 1, attrs_len);
                    attrs[attrs_len] = '\0';

                    char *attrs_work = strdup(attrs);
                    if (!attrs_work) goto fail;
                    char *token = strtok(attrs_work, " \t");
                    char type_buf[64] = {0};
                    char title_buf[256] = {0};
                    bool collapsible = false;
                    bool default_open = true;
                    while (token) {
                        if (strncmp(token, ".callout-", 9) == 0) {
                            strncpy(type_buf, token + 9, sizeof(type_buf) - 1);
                        } else if (strncmp(token, "title=", 6) == 0) {
                            const char *v = token + 6;
                            size_t vlen = strlen(v);
                            if (vlen >= 2 && ((v[0] == '"' && v[vlen - 1] == '"') || (v[0] == '\'' && v[vlen - 1] == '\''))) {
                                v++;
                                vlen -= 2;
                            }
                            if (vlen >= sizeof(title_buf)) vlen = sizeof(title_buf) - 1;
                            memcpy(title_buf, v, vlen);
                            title_buf[vlen] = '\0';
                        } else if (strncmp(token, "collapse=", 9) == 0) {
                            const char *v = token + 9;
                            if (strcasecmp(v, "\"true\"") == 0 || strcasecmp(v, "true") == 0) {
                                collapsible = true;
                                default_open = false;
                            } else if (strcasecmp(v, "\"false\"") == 0 || strcasecmp(v, "false") == 0) {
                                collapsible = true;
                                default_open = true;
                            }
                        }
                        token = strtok(NULL, " \t");
                    }
                    free(attrs_work);

                    if (type_buf[0]) {
                        callout_type_t ct = detect_callout_type(type_buf, (int)strlen(type_buf));
                        if (ct != CALLOUT_NONE) {
                            char header[512];
                            char marker = collapsible ? (default_open ? '+' : '-') : ']';
                            if (marker == ']') {
                                if (title_buf[0]) snprintf(header, sizeof(header), "> [!%s] %s", type_buf, title_buf);
                                else snprintf(header, sizeof(header), "> [!%s]", type_buf);
                            } else {
                                if (title_buf[0]) snprintf(header, sizeof(header), "> [!%s]%c %s", type_buf, marker, title_buf);
                                else snprintf(header, sizeof(header), "> [!%s]%c", type_buf, marker);
                            }
                            if (!append_str(&out, &len, &cap, header)) goto fail;
                            if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;

                            p = has_nl ? line_end + 1 : line_start + line_len;
                            bool had_title_heading = false;
                            bool emitted_body_content = false;
                            while (*p) {
                                const char *body_start = p;
                                const char *body_end = strchr(p, '\n');
                                bool body_nl = body_end != NULL;
                                size_t body_len = body_nl ? (size_t)(body_end - body_start) : strlen(body_start);

                                const char *body_content = body_start;
                                while ((size_t)(body_content - body_start) < body_len && (*body_content == ' ' || *body_content == '\t')) body_content++;
                                size_t body_content_len = body_len - (size_t)(body_content - body_start);

                                if (body_content_len >= 3 && strncmp(body_content, ":::", 3) == 0) {
                                    p = body_nl ? body_end + 1 : body_start + body_len;
                                    break;
                                }

                                if (!title_buf[0] && !had_title_heading && body_content_len > 2 &&
                                    body_content[0] == '#' && body_content[1] == '#') {
                                    had_title_heading = true;
                                    if (body_nl) p = body_end + 1;
                                    else p = body_start + body_len;
                                    continue;
                                }

                                /* Avoid emitting an empty leading paragraph after heading/title removal. */
                                if (body_len == 0 && !emitted_body_content) {
                                    p = body_nl ? body_end + 1 : body_start + body_len;
                                    continue;
                                }

                                if (!append_str(&out, &len, &cap, ">")) goto fail;
                                if (body_len > 0) {
                                    if (!append_str(&out, &len, &cap, " ")) goto fail;
                                    if (!append_chunk(&out, &len, &cap, body_start, body_len)) goto fail;
                                    emitted_body_content = true;
                                }
                                if (body_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
                                p = body_nl ? body_end + 1 : body_start + body_len;
                            }
                            continue;
                        }
                    }
                }
            }
        }

        if (!append_chunk(&out, &len, &cap, line_start, line_len)) goto fail;
        if (has_nl && !append_chunk(&out, &len, &cap, "\n", 1)) goto fail;
        p = has_nl ? line_end + 1 : line_start + line_len;
    }

    return out;

fail:
    free(out);
    return NULL;
}

