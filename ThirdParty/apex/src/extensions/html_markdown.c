/**
 * HTML Markdown Attributes Extension for Apex
 * Implementation
 */

#include "html_markdown.h"
#include "ial.h"
#include "../html_renderer.h"
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    char **ids;
    size_t count;
    size_t capacity;
} ref_id_list;

static void ref_id_list_free(ref_id_list *list) {
    if (!list) return;
    if (list->ids) {
        for (size_t i = 0; i < list->count; i++) {
            free(list->ids[i]);
        }
        free(list->ids);
    }
    list->ids = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool reference_id_matches(const char *ref_id, const char *text, size_t text_len) {
    if (!ref_id || !text) return false;

    const char *p = ref_id;
    const char *t = text;
    size_t remaining = text_len;

    while (*p && isspace((unsigned char)*p)) p++;
    while (remaining > 0 && isspace((unsigned char)*t)) {
        t++;
        remaining--;
    }

    while (*p && remaining > 0) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*t)) {
            if (isspace((unsigned char)*p) && isspace((unsigned char)*t)) {
                while (*p && isspace((unsigned char)*p)) p++;
                while (remaining > 0 && isspace((unsigned char)*t)) {
                    t++;
                    remaining--;
                }
                continue;
            }
            return false;
        }
        p++;
        t++;
        remaining--;
    }

    while (*p && isspace((unsigned char)*p)) p++;
    while (remaining > 0 && isspace((unsigned char)*t)) {
        t++;
        remaining--;
    }

    return (*p == '\0' && remaining == 0);
}

static bool ref_id_list_contains(ref_id_list *list, const char *id, size_t id_len) {
    for (size_t i = 0; i < list->count; i++) {
        if (reference_id_matches(list->ids[i], id, id_len)) {
            return true;
        }
    }
    return false;
}

static void ref_id_list_add(ref_id_list *list, const char *id, size_t id_len) {
    if (!list || !id || id_len == 0) return;
    if (ref_id_list_contains(list, id, id_len)) return;

    char *copy = malloc(id_len + 1);
    if (!copy) return;
    memcpy(copy, id, id_len);
    copy[id_len] = '\0';

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 16;
        char **new_ids = realloc(list->ids, new_capacity * sizeof(char *));
        if (!new_ids) {
            free(copy);
            return;
        }
        list->ids = new_ids;
        list->capacity = new_capacity;
    }

    list->ids[list->count++] = copy;
}

static void collect_used_reference_ids(const char *text, ref_id_list *list) {
    if (!text || !list) return;

    bool in_code_block = false;
    bool in_inline_code = false;
    const char *p = text;

    while (*p) {
        if (!in_code_block && !in_inline_code && *p == '`') {
            int backtick_count = 1;
            const char *q = p + 1;
            while (*q == '`') {
                backtick_count++;
                q++;
            }
            if (backtick_count >= 3) {
                in_code_block = !in_code_block;
            } else {
                in_inline_code = !in_inline_code;
            }
            p = q;
            continue;
        }

        if (!in_code_block && !in_inline_code && *p == '[') {
            const char *label_start = p + 1;
            const char *label_end = strchr(label_start, ']');
            if (label_end && label_end[1] == '[') {
                const char *ref_start = label_end + 2;
                const char *ref_end = strchr(ref_start, ']');
                if (ref_end) {
                    if (ref_start == ref_end) {
                        ref_id_list_add(list, label_start, (size_t)(label_end - label_start));
                    } else {
                        ref_id_list_add(list, ref_start, (size_t)(ref_end - ref_start));
                    }
                    p = ref_end + 1;
                    continue;
                }
            }
        }

        p++;
    }
}

static void collect_used_footnote_ids(const char *text, ref_id_list *list) {
    if (!text || !list) return;

    bool in_code_block = false;
    bool in_inline_code = false;
    const char *p = text;

    while (*p) {
        if (!in_code_block && !in_inline_code && *p == '`') {
            int backtick_count = 1;
            const char *q = p + 1;
            while (*q == '`') {
                backtick_count++;
                q++;
            }
            if (backtick_count >= 3) {
                in_code_block = !in_code_block;
            } else {
                in_inline_code = !in_inline_code;
            }
            p = q;
            continue;
        }

        if (!in_code_block && !in_inline_code && *p == '[' && p[1] == '^') {
            const char *id_start = p + 1;
            const char *id_end = strchr(p + 2, ']');
            if (id_end) {
                const char *line_start = p;
                while (line_start > text && line_start[-1] != '\n') {
                    line_start--;
                }
                const char *ws = line_start;
                while (ws < p && (*ws == ' ' || *ws == '\t')) {
                    ws++;
                }
                if (ws == p && id_end[1] == ':') {
                    p = id_end + 1;
                    continue;
                }

                ref_id_list_add(list, id_start, (size_t)(id_end - id_start));
                p = id_end + 1;
                continue;
            }
        }

        p++;
    }
}

static void collect_defined_reference_ids(const char *text, ref_id_list *list) {
    if (!text || !list) return;

    const char *p = text;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        const char *content_start = line_start;
        while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
            content_start++;
        }

        if (content_start < line_end && *content_start == '[') {
            const char *id_end = strchr(content_start + 1, ']');
            if (id_end && id_end < line_end && id_end[1] == ':') {
                ref_id_list_add(list, content_start + 1, (size_t)(id_end - (content_start + 1)));
            }
        }

        p = (*line_end == '\n') ? line_end + 1 : line_end;
    }
}

static char *extract_matching_reference_definitions(const char *full_doc, ref_id_list *needed) {
    if (!full_doc || !needed || needed->count == 0) return NULL;

    size_t capacity = 256;
    size_t len = 0;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *p = full_doc;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        size_t line_len = (size_t)(line_end - line_start);

        const char *content_start = line_start;
        while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
            content_start++;
        }

        if (content_start < line_end && *content_start == '[') {
            const char *id_end = strchr(content_start + 1, ']');
            if (id_end && id_end < line_end && id_end[1] == ':') {
                const char *id_start = content_start + 1;
                size_t id_len = (size_t)(id_end - id_start);
                bool needed_line = false;
                for (size_t i = 0; i < needed->count; i++) {
                    if (reference_id_matches(needed->ids[i], id_start, id_len)) {
                        needed_line = true;
                        break;
                    }
                }

                if (needed_line) {
                    if (len + line_len + 2 > capacity) {
                        capacity = (len + line_len + 2) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            return NULL;
                        }
                        output = new_output;
                    }
                    if (len > 0) {
                        output[len++] = '\n';
                    }
                    memcpy(output + len, line_start, line_len);
                    len += line_len;
                }
            }
        }

        p = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (len == 0) {
        free(output);
        return NULL;
    }

    output[len] = '\0';
    return output;
}

static bool is_footnote_definition_line(const char *line_start, const char *line_end, const char *footnote_id) {
    const char *content_start = line_start;
    while (content_start < line_end && (*content_start == ' ' || *content_start == '\t')) {
        content_start++;
    }

    if (content_start >= line_end || *content_start != '[') {
        return false;
    }

    const char *id_end = strchr(content_start + 1, ']');
    if (!id_end || id_end >= line_end || id_end[1] != ':') {
        return false;
    }

    return reference_id_matches(footnote_id, content_start + 1, (size_t)(id_end - (content_start + 1)));
}

static bool is_footnote_definition_continuation(const char *line_start, size_t line_len) {
    if (line_len == 0) {
        return true;
    }
    if (*line_start == '\t') {
        return true;
    }
    return line_len >= 4 && line_start[0] == ' ' && line_start[1] == ' ' &&
           line_start[2] == ' ' && line_start[3] == ' ';
}

static char *extract_footnote_definition_block(const char *full_doc, const char *footnote_id) {
    if (!full_doc || !footnote_id) return NULL;

    const char *p = full_doc;
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        size_t line_len = (size_t)(line_end - line_start);

        if (is_footnote_definition_line(line_start, line_end, footnote_id)) {
            size_t capacity = line_len + 64;
            size_t len = 0;
            char *output = malloc(capacity);
            if (!output) return NULL;

            if (len + line_len + 2 > capacity) {
                capacity = line_len + 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
            }
            memcpy(output + len, line_start, line_len);
            len += line_len;

            p = (*line_end == '\n') ? line_end + 1 : line_end;
            while (*p) {
                line_start = p;
                line_end = strchr(p, '\n');
                if (!line_end) line_end = p + strlen(p);
                line_len = (size_t)(line_end - line_start);

                if (!is_footnote_definition_continuation(line_start, line_len)) {
                    break;
                }

                if (len + line_len + 2 > capacity) {
                    capacity = (len + line_len + 2) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                }
                output[len++] = '\n';
                memcpy(output + len, line_start, line_len);
                len += line_len;

                p = (*line_end == '\n') ? line_end + 1 : line_end;
            }

            output[len] = '\0';
            return output;
        }

        p = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    return NULL;
}

static char *extract_matching_footnote_definitions(const char *full_doc, ref_id_list *needed) {
    if (!full_doc || !needed || needed->count == 0) return NULL;

    size_t capacity = 256;
    size_t len = 0;
    char *output = malloc(capacity);
    if (!output) return NULL;

    for (size_t i = 0; i < needed->count; i++) {
        if (needed->ids[i][0] != '^') {
            continue;
        }

        char *block = extract_footnote_definition_block(full_doc, needed->ids[i]);
        if (!block) continue;

        size_t block_len = strlen(block);
        if (len > 0) {
            if (len + 1 >= capacity) {
                capacity = (len + block_len + 2) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(block);
                    free(output);
                    return NULL;
                }
                output = new_output;
            }
            output[len++] = '\n';
        }

        if (len + block_len + 2 > capacity) {
            capacity = (len + block_len + 2) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(block);
                free(output);
                return NULL;
            }
            output = new_output;
        }

        memcpy(output + len, block, block_len);
        len += block_len;
        free(block);
    }

    if (len == 0) {
        free(output);
        return NULL;
    }

    output[len] = '\0';
    return output;
}

static ref_id_list filter_needed_ids(ref_id_list *used, ref_id_list *defined_in_content, bool footnotes_only) {
    ref_id_list needed = {0};
    for (size_t i = 0; i < used->count; i++) {
        bool is_footnote = (used->ids[i][0] == '^');
        if (footnotes_only != is_footnote) {
            continue;
        }

        bool already_defined = false;
        for (size_t j = 0; j < defined_in_content->count; j++) {
            if (reference_id_matches(used->ids[i], defined_in_content->ids[j],
                                     strlen(defined_in_content->ids[j]))) {
                already_defined = true;
                break;
            }
        }
        if (!already_defined) {
            ref_id_list_add(&needed, used->ids[i], strlen(used->ids[i]));
        }
    }
    return needed;
}

/**
 * Prepend reference and footnote definitions from the full document that are
 * used in content but not already defined within content.
 * Caller must free the returned string.
 */
static char *prepend_needed_definitions(const char *full_doc, const char *content) {
    if (!full_doc || !content || strcmp(full_doc, content) == 0) {
        return NULL;
    }

    ref_id_list used = {0};
    ref_id_list defined_in_content = {0};
    collect_used_reference_ids(content, &used);
    collect_used_footnote_ids(content, &used);
    if (used.count == 0) {
        return NULL;
    }

    collect_defined_reference_ids(content, &defined_in_content);

    ref_id_list needed_links = filter_needed_ids(&used, &defined_in_content, false);
    ref_id_list needed_footnotes = filter_needed_ids(&used, &defined_in_content, true);

    ref_id_list_free(&used);
    ref_id_list_free(&defined_in_content);

    char *link_defs = needed_links.count > 0 ?
        extract_matching_reference_definitions(full_doc, &needed_links) : NULL;
    char *footnote_defs = needed_footnotes.count > 0 ?
        extract_matching_footnote_definitions(full_doc, &needed_footnotes) : NULL;

    ref_id_list_free(&needed_links);
    ref_id_list_free(&needed_footnotes);

    if (!link_defs && !footnote_defs) {
        free(link_defs);
        free(footnote_defs);
        return NULL;
    }

    size_t defs_len = 0;
    if (link_defs) defs_len += strlen(link_defs);
    if (footnote_defs) {
        if (defs_len > 0) defs_len++;
        defs_len += strlen(footnote_defs);
    }

    size_t content_len = strlen(content);
    char *result = malloc(defs_len + 2 + content_len + 1);
    if (!result) {
        free(link_defs);
        free(footnote_defs);
        return NULL;
    }

    size_t offset = 0;
    if (link_defs) {
        size_t link_len = strlen(link_defs);
        memcpy(result + offset, link_defs, link_len);
        offset += link_len;
        free(link_defs);
    }
    if (footnote_defs) {
        if (offset > 0) {
            result[offset++] = '\n';
        }
        size_t fn_len = strlen(footnote_defs);
        memcpy(result + offset, footnote_defs, fn_len);
        offset += fn_len;
        free(footnote_defs);
    }

    result[offset++] = '\n';
    result[offset++] = '\n';
    memcpy(result + offset, content, content_len);
    result[offset + content_len] = '\0';
    return result;
}

/**
 * Find the next HTML tag with markdown attribute
 * Returns position of '<' or NULL if not found
 */
static const char *find_markdown_tag(const char *text, char *tag_name, size_t tag_name_size,
                                     char *markdown_attr, size_t attr_size, size_t *tag_length) {
    const char *pos = text;

    while (*pos) {
        /* Find opening < */
        if (*pos == '<' && pos[1] != '/' && pos[1] != '!') {
            const char *tag_start = pos;
            pos++;

            /* Extract tag name */
            const char *name_start = pos;
            while (*pos && (isalnum((unsigned char)*pos) || *pos == '-' || *pos == '_')) {
                pos++;
            }

            size_t name_len = pos - name_start;
            if (name_len == 0 || name_len >= tag_name_size) {
                pos = tag_start + 1;
                continue;
            }

            memcpy(tag_name, name_start, name_len);
            tag_name[name_len] = '\0';

            /* Look for markdown attribute in tag */
            const char *tag_end = strchr(pos, '>');
            if (!tag_end) {
                pos = tag_start + 1;
                continue;
            }

            /* Search for markdown= in attributes */
            const char *attr_search = pos;
            while (attr_search < tag_end) {
                /* Skip whitespace */
                while (attr_search < tag_end && isspace((unsigned char)*attr_search)) {
                    attr_search++;
                }

                /* Check for markdown attribute */
                if (strncmp(attr_search, "markdown=", 9) == 0) {
                    attr_search += 9;

                    /* Get attribute value */
                    char quote = 0;
                    if (*attr_search == '"' || *attr_search == '\'') {
                        quote = *attr_search;
                        attr_search++;
                    }

                    const char *value_start = attr_search;
                    const char *value_end = value_start;

                    if (quote) {
                        value_end = strchr(value_start, quote);
                        if (!value_end) value_end = tag_end;
                    } else {
                        while (*value_end && !isspace((unsigned char)*value_end) && *value_end != '>') {
                            value_end++;
                        }
                    }

                    size_t value_len = value_end - value_start;
                    if (value_len < attr_size) {
                        memcpy(markdown_attr, value_start, value_len);
                        markdown_attr[value_len] = '\0';

                        *tag_length = (tag_end - tag_start) + 1;
                        return tag_start;
                    }
                }

                /* Move to next attribute */
                while (attr_search < tag_end && !isspace((unsigned char)*attr_search)) {
                    attr_search++;
                }
            }

            pos = tag_start + 1;
        } else {
            pos++;
        }
    }

    return NULL;
}

/**
 * Find matching closing tag
 * Handles nested tags correctly
 */
static const char *find_closing_tag(const char *text, const char *tag_name) {
    int depth = 1;
    const char *pos = text;
    size_t tag_len = strlen(tag_name);

    while (*pos && depth > 0) {
        if (*pos == '<') {
            /* Check for closing tag */
            if (pos[1] == '/' && strncasecmp(pos + 2, tag_name, tag_len) == 0 &&
                (pos[2 + tag_len] == '>' || isspace((unsigned char)pos[2 + tag_len]))) {
                depth--;
                if (depth == 0) {
                    /* Find the > */
                    const char *end = strchr(pos, '>');
                    return end ? end + 1 : NULL;
                }
            }
            /* Check for opening tag (nested) */
            else if (pos[1] != '/' && pos[1] != '!' &&
                     strncasecmp(pos + 1, tag_name, tag_len) == 0 &&
                     (pos[1 + tag_len] == '>' || isspace((unsigned char)pos[1 + tag_len]))) {
                depth++;
            }
        }
        pos++;
    }

    return NULL;
}

/**
 * Process HTML tags with markdown attributes
 * If img_attrs is non-NULL, image attributes (e.g. width/height from ref defs) are applied to images in markdown="1" regions.
 * full_doc is the original document used to resolve reference-style link definitions.
 */
static void attach_gfm_extensions(cmark_parser *parser, const apex_options *options) {
    if (!parser || !options) return;
    apex_ensure_cmark_extensions();
    const char *names[4];
    int count = 0;
    if (options->enable_tables) names[count++] = "table";
    if (options->enable_task_lists) names[count++] = "tasklist";
    if (options->enable_strikethrough) names[count++] = "strikethrough";
    if (options->enable_autolink &&
        (options->mode == APEX_MODE_GFM || apex_mode_is_unified_family(options->mode))) {
        names[count++] = "autolink";
    }
    for (int i = 0; i < count; i++) {
        cmark_syntax_extension *ext = cmark_find_syntax_extension(names[i]);
        if (ext) cmark_parser_attach_syntax_extension(parser, ext);
    }
}

static char *apex_process_html_markdown_impl(const char *text, void *img_attrs, const char *full_doc,
                                             const apex_options *options) {
    if (!text) return NULL;
    if (!full_doc) full_doc = text;

    image_attr_entry *attrs = (image_attr_entry *)img_attrs;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read_pos = text;
    char *write_pos = output;
    size_t remaining = output_capacity;

#define ENSURE_OUTPUT_SPACE(needed) do { \
    if ((needed) > remaining) { \
        size_t used = (size_t)(write_pos - output); \
        size_t min_capacity = used + (needed) + 1; \
        output_capacity = (min_capacity < 1024) ? 2048 : min_capacity * 2; \
        char *new_output = realloc(output, output_capacity + 1); \
        if (!new_output) { \
            free(output); \
            return NULL; \
        } \
        output = new_output; \
        write_pos = output + used; \
        remaining = output_capacity - used; \
    } \
} while (0)

    while (*read_pos) {
        char tag_name[64];
        char markdown_attr[64];
        size_t tag_length;

        /* Find next HTML tag with markdown attribute */
        const char *tag_start = find_markdown_tag(read_pos, tag_name, sizeof(tag_name),
                                                   markdown_attr, sizeof(markdown_attr), &tag_length);

        if (!tag_start) {
            /* No more markdown tags, copy rest */
            size_t rest_len = strlen(read_pos);
            if (rest_len < remaining) {
                memcpy(write_pos, read_pos, rest_len);
                write_pos += rest_len;
                remaining -= rest_len;
            }
            break;
        }

        /* Copy text before tag */
        size_t prefix_len = tag_start - read_pos;
        if (prefix_len < remaining) {
            memcpy(write_pos, read_pos, prefix_len);
            write_pos += prefix_len;
            remaining -= prefix_len;
        }

        /* Find content between tags */
        const char *content_start = tag_start + tag_length;
        const char *closing_tag = find_closing_tag(content_start, tag_name);

        if (!closing_tag) {
            /* No closing tag, just copy the opening tag */
            if (tag_length < remaining) {
                memcpy(write_pos, tag_start, tag_length);
                write_pos += tag_length;
                remaining -= tag_length;
            }
            read_pos = content_start;
            continue;
        }

        /* Extract content */
        size_t content_len = closing_tag - content_start;

        /* Find the actual closing tag start for later */
        const char *closing_tag_start = closing_tag;
        while (closing_tag_start > content_start && *(closing_tag_start - 1) != '<') {
            closing_tag_start--;
        }
        if (closing_tag_start > content_start && *(closing_tag_start - 1) == '<') {
            closing_tag_start--;
        }
        content_len = closing_tag_start - content_start;

        /* Process based on markdown attribute value */
        bool parse_markdown = false;
        bool parse_inline = false;

        if (strcmp(markdown_attr, "1") == 0 || strcmp(markdown_attr, "block") == 0) {
            parse_markdown = true;
            parse_inline = false;
        } else if (strcmp(markdown_attr, "span") == 0) {
            parse_markdown = true;
            parse_inline = true;
        } else if (strcmp(markdown_attr, "0") == 0) {
            parse_markdown = false;
        }

        if (parse_markdown && content_len > 0) {
            /* Extract content */
            char *content = malloc(content_len + 1);
            if (content) {
                memcpy(content, content_start, content_len);
                content[content_len] = '\0';

                /* Recursively process nested divs with markdown="1" BEFORE parsing */
                /* This ensures nested divs are processed before cmark-gfm sees them */
                char *processed_content = apex_process_html_markdown_impl(content, img_attrs, full_doc, options);
                if (processed_content) {
                    free(content);
                    content = processed_content;
                    content_len = strlen(content);
                }

                char *content_with_refs = prepend_needed_definitions(full_doc, content);
                if (content_with_refs) {
                    free(content);
                    content = content_with_refs;
                    content_len = strlen(content);
                }

                /* Create parser and parse */
                /* Use CMARK_OPT_UNSAFE to allow raw HTML (including nested divs) */
                int cmark_opts = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE | CMARK_OPT_FOOTNOTES;
                cmark_parser *parser = cmark_parser_new(cmark_opts);
                if (parser) {
                    attach_gfm_extensions(parser, options);
                    cmark_parser_feed(parser, content, content_len);
                    cmark_node *doc = cmark_parser_finish(parser);

                    if (doc) {
                        apex_process_ial_in_tree(doc, NULL);
                        if (attrs) {
                            apex_apply_image_attributes(doc, attrs);
                        }
                        char *html = apex_render_html_with_attributes(doc, cmark_opts);
                        if (html) {
                            /* Write opening tag (without markdown attribute) */
                            char opening_tag[2048];
                            size_t tag_written = 0;

                            /* Reconstruct tag without markdown attribute */
                            snprintf(opening_tag, sizeof(opening_tag), "<%s", tag_name);
                            tag_written = strlen(opening_tag);

                            /* Copy attributes except markdown */
                            const char *attrs_start = strchr(tag_start + 1, ' ');
                            if (attrs_start && attrs_start < content_start) {
                                const char *attrs_end = strchr(attrs_start, '>');
                                if (attrs_end) {
                                    /* Parse and copy attributes, filtering out markdown attribute */
                                    const char *attr_pos = attrs_start;
                                    while (attr_pos < attrs_end) {
                                        /* Skip whitespace */
                                        while (attr_pos < attrs_end && isspace((unsigned char)*attr_pos)) {
                                            attr_pos++;
                                        }
                                        if (attr_pos >= attrs_end) break;

                                        /* Check if this is the markdown attribute */
                                        if (strncmp(attr_pos, "markdown=", 9) == 0) {
                                            /* Skip markdown attribute */
                                            attr_pos += 9;
                                            /* Skip attribute value */
                                            if (*attr_pos == '"' || *attr_pos == '\'') {
                                                char quote = *attr_pos++;
                                                while (attr_pos < attrs_end && *attr_pos != quote) {
                                                    if (*attr_pos == '\\' && attr_pos + 1 < attrs_end) attr_pos++;
                                                    attr_pos++;
                                                }
                                                if (*attr_pos == quote) attr_pos++;
                                            } else {
                                                while (attr_pos < attrs_end && !isspace((unsigned char)*attr_pos) && *attr_pos != '>') {
                                                    attr_pos++;
                                                }
                                            }
                                            continue;
                                        }

                                        /* Copy this attribute */
                                        const char *attr_start = attr_pos;
                                        while (attr_pos < attrs_end && *attr_pos != '>') {
                                            /* Check if we've reached the start of the next attribute */
                                            if (attr_pos > attr_start && (isspace((unsigned char)*attr_pos) || *attr_pos == '>')) {
                                                /* Check if next token is markdown= */
                                                const char *next = attr_pos;
                                                while (next < attrs_end && isspace((unsigned char)*next)) next++;
                                                if (strncmp(next, "markdown=", 9) == 0) {
                                                    break; /* Stop before markdown attribute */
                                                }
                                            }
                                            attr_pos++;
                                        }

                                        /* Copy attribute to opening_tag */
                                        size_t attr_len = attr_pos - attr_start;
                                        if (tag_written + attr_len + 1 < sizeof(opening_tag)) {
                                            opening_tag[tag_written++] = ' ';
                                            memcpy(opening_tag + tag_written, attr_start, attr_len);
                                            tag_written += attr_len;
                                        }
                                    }
                                    opening_tag[tag_written++] = '>';
                                    opening_tag[tag_written] = '\0';
                                }
                            } else {
                                opening_tag[tag_written++] = '>';
                                opening_tag[tag_written] = '\0';
                            }

                            ENSURE_OUTPUT_SPACE(tag_written);
                            memcpy(write_pos, opening_tag, tag_written);
                            write_pos += tag_written;
                            remaining -= tag_written;

                            /* Write parsed HTML (trim outer <p> tags if inline) */
                            char *html_content = html;
                            size_t html_len = strlen(html);

                            if (parse_inline && html_len > 7 &&
                                strncmp(html, "<p>", 3) == 0 &&
                                strcmp(html + html_len - 5, "</p>\n") == 0) {
                                /* Strip <p> tags for inline */
                                html_content = html + 3;
                                html_len -= 8;
                                html_content[html_len] = '\0';
                            }

                            ENSURE_OUTPUT_SPACE(html_len);
                            memcpy(write_pos, html_content, html_len);
                            write_pos += html_len;
                            remaining -= html_len;

                            free(html);
                        }
                        cmark_node_free(doc);
                    }
                    cmark_parser_free(parser);
                }
                free(content);
            }

            /* Write closing tag */
            size_t closing_len = closing_tag - closing_tag_start;
            if (closing_len < remaining) {
                memcpy(write_pos, closing_tag_start, closing_len);
                write_pos += closing_len;
                remaining -= closing_len;
            }

            /* Ensure newline after closing tag so following markdown is parsed correctly */
            /* Check if closing tag ends with newline */
            bool needs_newline = true;
            if (closing_len > 0) {
                const char *last_char = closing_tag_start + closing_len - 1;
                if (*last_char == '\n' || (*last_char == '\r' && closing_len > 1 && *(last_char - 1) == '\n')) {
                    needs_newline = false;
                }
            }
            if (needs_newline && remaining > 0) {
                *write_pos++ = '\n';
                remaining--;
            }
        } else {
            /* markdown="0" or no parsing - copy everything as-is */
            size_t total_len = closing_tag - tag_start;
            if (total_len < remaining) {
                memcpy(write_pos, tag_start, total_len);
                write_pos += total_len;
                remaining -= total_len;
            }
        }

        read_pos = closing_tag;
    }

    *write_pos = '\0';
#undef ENSURE_OUTPUT_SPACE
    return output;
}

char *apex_process_html_markdown(const char *text, void *img_attrs, const apex_options *options) {
    return apex_process_html_markdown_impl(text, img_attrs, text, options);
}

