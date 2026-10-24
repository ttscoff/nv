/**
 * Quarto/Pandoc diagram fences (mermaid, graphviz/dot)
 */

#include "quarto_diagrams.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char MERMAID_SCRIPT[] =
    "<script src=\"https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js\"></script>";

static const char *line_end_ptr(const char *p) {
    const char *e = strchr(p, '\n');
    return e ? e : p + strlen(p);
}

static bool append_chunk(char **out, size_t *len, size_t *cap, const char *chunk, size_t chunk_len) {
    if (chunk_len == 0) {
        return true;
    }
    if (*len + chunk_len + 1 > *cap) {
        size_t new_cap = (*cap == 0 ? 256 : *cap * 2);
        while (*len + chunk_len + 1 > new_cap) {
            new_cap *= 2;
        }
        char *grown = realloc(*out, new_cap);
        if (!grown) {
            return false;
        }
        *out = grown;
        *cap = new_cap;
    }
    memcpy(*out + *len, chunk, chunk_len);
    *len += chunk_len;
    (*out)[*len] = '\0';
    return true;
}

static bool append_str(char **out, size_t *len, size_t *cap, const char *str) {
    if (!str) {
        return true;
    }
    return append_chunk(out, len, cap, str, strlen(str));
}

static bool parse_diagram_type(const char *info, size_t info_len, char *class_out, size_t class_size) {
    if (!info || info_len == 0 || !class_out || class_size == 0) {
        return false;
    }

    const char *start = info;
    const char *end = info + info_len;
    while (start < end && isspace((unsigned char)*start)) {
        start++;
    }
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    if (end <= start) {
        return false;
    }

    if (*start == '{') {
        start++;
        if (end > start && *(end - 1) == '}') {
            end--;
        }
        if (start < end && *start == '=') {
            return false;
        }
        if (start < end && *start == '.') {
            start++;
        }
    }

    size_t name_len = (size_t)(end - start);
    if (name_len == 0 || name_len >= class_size) {
        return false;
    }

    char name[64];
    for (size_t i = 0; i < name_len; i++) {
        name[i] = (char)tolower((unsigned char)start[i]);
    }
    name[name_len] = '\0';

    if (strcmp(name, "mermaid") == 0) {
        strncpy(class_out, "mermaid", class_size);
        class_out[class_size - 1] = '\0';
        return true;
    }
    if (strcmp(name, "dot") == 0 || strcmp(name, "graphviz") == 0) {
        strncpy(class_out, "graphviz", class_size);
        class_out[class_size - 1] = '\0';
        return true;
    }
    return false;
}

static bool emit_diagram_block(char **out, size_t *len, size_t *cap,
                               const char *body, size_t body_len,
                               const char *diagram_class) {
    if (!append_str(out, len, cap, "<pre class=\"") ||
        !append_str(out, len, cap, diagram_class) ||
        !append_str(out, len, cap, "\">")) {
        return false;
    }
    if (body_len > 0 && !append_chunk(out, len, cap, body, body_len)) {
        return false;
    }
    if (body_len == 0 || body[body_len - 1] != '\n') {
        if (!append_str(out, len, cap, "\n")) {
            return false;
        }
    }
    return append_str(out, len, cap, "</pre>\n");
}

static bool try_diagram_fence(const char *line_start, const char *line_end,
                              const char **read_out, char **out, size_t *len, size_t *cap,
                              bool *changed) {
    const char *content = line_start;
    while (content < line_end && (*content == ' ' || *content == '\t')) {
        content++;
    }
    if ((size_t)(line_end - content) < 3 || strncmp(content, "```", 3) != 0) {
        return false;
    }

    int tick_count = 0;
    const char *ticks = content;
    while (ticks < line_end && *ticks == '`') {
        tick_count++;
        ticks++;
    }
    if (tick_count < 3) {
        return false;
    }
    while (ticks < line_end && (*ticks == ' ' || *ticks == '\t')) {
        ticks++;
    }
    if (ticks >= line_end) {
        return false;
    }

    char diagram_class[32];
    if (!parse_diagram_type(ticks, (size_t)(line_end - ticks), diagram_class, sizeof(diagram_class))) {
        return false;
    }

    const char *body_start = (*line_end == '\n') ? line_end + 1 : line_end;
    const char *scan = body_start;
    while (*scan) {
        const char *cl_start = scan;
        const char *cl_end = line_end_ptr(scan);
        const char *cl_content = cl_start;
        while (cl_content < cl_end && (*cl_content == ' ' || *cl_content == '\t')) {
            cl_content++;
        }
        int close_ticks = 0;
        while (cl_content + close_ticks < cl_end && cl_content[close_ticks] == '`') {
            close_ticks++;
        }
        if (close_ticks >= tick_count) {
            const char *after_ticks = cl_content + close_ticks;
            while (after_ticks < cl_end && (*after_ticks == ' ' || *after_ticks == '\t')) {
                after_ticks++;
            }
            if (after_ticks == cl_end) {
                size_t body_len = (size_t)(cl_start - body_start);
                if (!emit_diagram_block(out, len, cap, body_start, body_len, diagram_class)) {
                    return false;
                }
                *changed = true;
                *read_out = (*cl_end == '\n') ? cl_end + 1 : cl_end;
                return true;
            }
        }
        scan = (*cl_end == '\n') ? cl_end + 1 : cl_end;
    }

    return false;
}

char *apex_preprocess_quarto_diagrams(const char *text, bool unsafe) {
    if (!text || !unsafe) {
        return NULL;
    }

    size_t cap = strlen(text) * 2 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    bool in_fenced_code = false;

    const char *read = text;
    while (*read) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool at_line_start = (read == text || read[-1] == '\n');

        if (!in_fenced_code && at_line_start) {
            const char *after_block = read;
            if (try_diagram_fence(line_start, line_end, &after_block, &out, &len, &cap, &changed)) {
                read = after_block;
                continue;
            }
        }

        if (at_line_start) {
            const char *content = line_start;
            while (content < line_end && (*content == ' ' || *content == '\t')) {
                content++;
            }
            if ((size_t)(line_end - content) >= 3 && strncmp(content, "```", 3) == 0) {
                in_fenced_code = !in_fenced_code;
            }
        }

        size_t line_len = (size_t)(line_end - line_start);
        if (*line_end == '\n') {
            line_len++;
        }
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

bool apex_html_has_mermaid_diagram(const char *html) {
    return html && strstr(html, "class=\"mermaid\"") != NULL;
}

const char *apex_mermaid_script_tag(void) {
    return MERMAID_SCRIPT;
}
