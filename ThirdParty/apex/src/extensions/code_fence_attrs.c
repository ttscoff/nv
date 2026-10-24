/**
 * Pandoc/Quarto fenced code block attributes ({.python filename="..."})
 */

#include "code_fence_attrs.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char MARKER_PREFIX[] = "<!-- apex-code-fence-attrs ";
static const char MARKER_SUFFIX[] = " -->";

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

static void append_escaped_attr(char **buf, size_t *len, size_t *cap, const char *value) {
    if (!value) {
        return;
    }
    for (const char *p = value; *p; p++) {
        if (*p == '"') {
            append_str(buf, len, cap, "&quot;");
        } else if (*p == '&') {
            append_str(buf, len, cap, "&amp;");
        } else {
            append_chunk(buf, len, cap, p, 1);
        }
    }
}

static bool is_linenos_key(const char *key) {
    return key && (strcasecmp(key, "linenos") == 0 ||
                   strcasecmp(key, "line-numbers") == 0 ||
                   strcasecmp(key, "line_numbers") == 0);
}

static bool is_linenos_class(const char *cls) {
    return cls && (strcasecmp(cls, "numberLines") == 0 ||
                   strcasecmp(cls, "numberlines") == 0 ||
                   strcasecmp(cls, "line-numbers") == 0);
}

static bool truthy_value(const char *value) {
    if (!value || !*value) {
        return true;
    }
    return strcasecmp(value, "true") == 0 ||
           strcasecmp(value, "yes") == 0 ||
           strcasecmp(value, "1") == 0;
}

typedef struct {
    char *id;
    char **classes;
    size_t class_count;
    char **keys;
    char **values;
    size_t attr_count;
} parsed_attrs;

static void free_parsed_attrs(parsed_attrs *a) {
    if (!a) {
        return;
    }
    free(a->id);
    if (a->classes) {
        for (size_t i = 0; i < a->class_count; i++) {
            free(a->classes[i]);
        }
        free(a->classes);
    }
    if (a->keys) {
        for (size_t i = 0; i < a->attr_count; i++) {
            free(a->keys[i]);
            free(a->values[i]);
        }
        free(a->keys);
        free(a->values);
    }
    memset(a, 0, sizeof(*a));
}

static bool push_class(parsed_attrs *a, char *class_name) {
    if (!class_name || !*class_name) {
        free(class_name);
        return true;
    }
    char **grown = realloc(a->classes, (a->class_count + 1) * sizeof(char *));
    if (!grown) {
        free(class_name);
        return false;
    }
    a->classes = grown;
    a->classes[a->class_count++] = class_name;
    return true;
}

static bool push_attr(parsed_attrs *a, char *key, char *value) {
    if (!key) {
        free(value);
        return false;
    }
    char **new_keys = realloc(a->keys, (a->attr_count + 1) * sizeof(char *));
    char **new_values = realloc(a->values, (a->attr_count + 1) * sizeof(char *));
    if (!new_keys || !new_values) {
        free(new_keys);
        free(new_values);
        free(key);
        free(value);
        return false;
    }
    a->keys = new_keys;
    a->values = new_values;
    a->keys[a->attr_count] = key;
    a->values[a->attr_count] = value;
    a->attr_count++;
    return true;
}

static bool parse_braced_attrs(const char *info, size_t info_len, parsed_attrs *out) {
    if (!info || info_len == 0 || !out) {
        return false;
    }

    char *buffer = malloc(info_len + 1);
    if (!buffer) {
        return false;
    }
    memcpy(buffer, info, info_len);
    buffer[info_len] = '\0';

    char *p = buffer;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '{') {
        free(buffer);
        return false;
    }
    p++;
    if (*p == '=') {
        free(buffer);
        return false;
    }

    char *end = buffer + info_len;
    while (end > p && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    if (end > p && *(end - 1) == '}') {
        end--;
    }
    *end = '\0';

    while (*p) {
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }

        if (*p == '#') {
            p++;
            char *id_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '}') {
                p++;
            }
            if (p > id_start) {
                char saved = *p;
                *p = '\0';
                free(out->id);
                out->id = strdup(id_start);
                *p = saved;
            }
            continue;
        }

        if (*p == '.') {
            p++;
            char *class_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '.' && *p != '#' && *p != '}') {
                p++;
            }
            if (p > class_start) {
                char saved = *p;
                *p = '\0';
                if (!push_class(out, strdup(class_start))) {
                    free(buffer);
                    return false;
                }
                *p = saved;
            }
            continue;
        }

        char *key_start = p;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '}') {
            p++;
        }
        if (*p != '=') {
            p++;
            continue;
        }
        char saved = *p;
        *p = '\0';
        char *key = strdup(key_start);
        *p = saved;
        p++;

        char *value = NULL;
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            char *value_start = p;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p + 1)) {
                    p++;
                }
                p++;
            }
            if (*p == quote) {
                *p = '\0';
                value = strdup(value_start);
                p++;
            }
        } else {
            char *value_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '}') {
                p++;
            }
            char saved_val = *p;
            *p = '\0';
            value = strdup(value_start);
            *p = saved_val;
        }

        if (!push_attr(out, key, value)) {
            free(buffer);
            return false;
        }
    }

    free(buffer);
    return out->class_count > 0 || out->attr_count > 0 || out->id;
}

static char *build_marker_from_parsed(const parsed_attrs *parsed) {
    char *marker_body = NULL;
    size_t len = 0;
    size_t cap = 0;

    const char *language = NULL;
    size_t extra_class_start = 0;

    for (size_t i = 0; i < parsed->class_count; i++) {
        if (is_linenos_class(parsed->classes[i])) {
            append_str(&marker_body, &len, &cap, "data-linenos=\"true\" ");
            continue;
        }
        if (!language) {
            language = parsed->classes[i];
            continue;
        }
        if (extra_class_start == 0) {
            append_str(&marker_body, &len, &cap, "class=\"");
        } else {
            append_str(&marker_body, &len, &cap, " ");
        }
        append_escaped_attr(&marker_body, &len, &cap, parsed->classes[i]);
        extra_class_start++;
    }
    if (extra_class_start > 0) {
        append_str(&marker_body, &len, &cap, "\" ");
    }

    if (parsed->id) {
        append_str(&marker_body, &len, &cap, "id=\"");
        append_escaped_attr(&marker_body, &len, &cap, parsed->id);
        append_str(&marker_body, &len, &cap, "\" ");
    }

    for (size_t i = 0; i < parsed->attr_count; i++) {
        const char *key = parsed->keys[i];
        const char *value = parsed->values[i];
        if (is_linenos_key(key)) {
            if (truthy_value(value)) {
                append_str(&marker_body, &len, &cap, "data-linenos=\"true\" ");
            }
            continue;
        }
        char attr_name[128];
        if (strcasecmp(key, "filename") == 0) {
            snprintf(attr_name, sizeof(attr_name), "data-filename");
        } else if (strncmp(key, "data-", 5) == 0) {
            snprintf(attr_name, sizeof(attr_name), "%s", key);
        } else {
            snprintf(attr_name, sizeof(attr_name), "data-%s", key);
        }
        append_str(&marker_body, &len, &cap, attr_name);
        append_str(&marker_body, &len, &cap, "=\"");
        append_escaped_attr(&marker_body, &len, &cap, value ? value : "");
        append_str(&marker_body, &len, &cap, "\" ");
    }

    if (!marker_body || len == 0) {
        free(marker_body);
        if (!language) {
            return NULL;
        }
        marker_body = strdup(" ");
        return marker_body;
    }

    (void)language;
    return marker_body;
}

static bool try_transform_open_fence(const char *line_start, const char *line_end,
                                     char **out, size_t *len, size_t *cap, bool *changed) {
    const char *p = line_start;
    while (p < line_end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    if ((size_t)(line_end - p) < 3 || strncmp(p, "```", 3) != 0) {
        return false;
    }

    int tick_count = 0;
    const char *ticks = p;
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

    const char *info_start = ticks;
    const char *info_end = line_end;
    while (info_end > info_start && (info_end[-1] == ' ' || info_end[-1] == '\t')) {
        info_end--;
    }
    if (info_end <= info_start || *info_start != '{') {
        return false;
    }

    parsed_attrs parsed = {0};
    if (!parse_braced_attrs(info_start, (size_t)(info_end - info_start), &parsed)) {
        free_parsed_attrs(&parsed);
        return false;
    }

    char *language = NULL;
    for (size_t i = 0; i < parsed.class_count; i++) {
        if (!is_linenos_class(parsed.classes[i])) {
            language = strdup(parsed.classes[i]);
            break;
        }
    }

    char *marker_body = build_marker_from_parsed(&parsed);
    free_parsed_attrs(&parsed);
    if (!marker_body && !language) {
        free(language);
        return false;
    }

    if (marker_body) {
        char *trim = marker_body;
        while (*trim == ' ') {
            trim++;
        }
        if (*trim) {
            if (!append_str(out, len, cap, MARKER_PREFIX) ||
                !append_str(out, len, cap, marker_body) ||
                !append_str(out, len, cap, MARKER_SUFFIX) ||
                !append_str(out, len, cap, "\n")) {
                free(marker_body);
                free(language);
                return false;
            }
        }
    }
    free(marker_body);

    if (!append_chunk(out, len, cap, line_start, (size_t)(p - line_start))) {
        return false;
    }
    for (int i = 0; i < tick_count; i++) {
        if (!append_str(out, len, cap, "`")) {
            return false;
        }
    }
    if (language && *language) {
        if (!append_str(out, len, cap, language)) {
            free(language);
            return false;
        }
    }
    free(language);
    if (line_end < line_start + strlen(line_start) && *line_end == '\n') {
        if (!append_str(out, len, cap, "\n")) {
            return false;
        }
    }

    *changed = true;
    return true;
}

char *apex_preprocess_code_fence_attrs(const char *text) {
    if (!text) {
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
        bool has_newline = (*line_end == '\n');
        bool at_line_start = (read == text || read[-1] == '\n');

        if (!in_fenced_code && at_line_start &&
            try_transform_open_fence(line_start, line_end, &out, &len, &cap, &changed)) {
            read = has_newline ? line_end + 1 : line_end;
            in_fenced_code = true;
            continue;
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

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (!append_chunk(&out, &len, &cap, line_start, line_len)) {
            free(out);
            return NULL;
        }
        read = has_newline ? line_end + 1 : line_end;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}

char *apex_postprocess_code_fence_attrs_html(const char *html) {
    if (!html || !strstr(html, MARKER_PREFIX)) {
        return NULL;
    }

    size_t cap = strlen(html) + 1024;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;

    const char *read = html;
    const size_t prefix_len = sizeof(MARKER_PREFIX) - 1;
    const size_t suffix_len = sizeof(MARKER_SUFFIX) - 1;

    while (*read) {
        const char *marker = strstr(read, MARKER_PREFIX);
        if (!marker) {
            break;
        }

        size_t prefix_copy = (size_t)(marker - read);
        if (len + prefix_copy + 1 > cap) {
            cap = (len + prefix_copy + 1) * 2;
            char *grown = realloc(out, cap);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        if (prefix_copy > 0) {
            memcpy(out + len, read, prefix_copy);
            len += prefix_copy;
        }

        const char *attrs_start = marker + prefix_len;
        const char *attrs_end = strstr(attrs_start, MARKER_SUFFIX);
        if (!attrs_end) {
            read = marker + prefix_len;
            continue;
        }

        size_t attrs_len = (size_t)(attrs_end - attrs_start);
        const char *scan = attrs_end + suffix_len;
        while (*scan == ' ' || *scan == '\t' || *scan == '\n' || *scan == '\r') {
            scan++;
        }
        if (strncmp(scan, "<p>", 3) == 0) {
            scan += 3;
            while (*scan == ' ' || *scan == '\t' || *scan == '\n' || *scan == '\r') {
                scan++;
            }
        }

        const char *target = NULL;
        const char *candidates[] = {
            "<div class=\"highlight\"",
            "<div class=\"sourceCode\"",
            "<pre",
            NULL
        };
        for (size_t i = 0; candidates[i]; i++) {
            const char *found = strstr(scan, candidates[i]);
            if (found && (!target || found < target)) {
                target = found;
            }
        }
        if (!target || target > scan + 512) {
            read = attrs_end + suffix_len;
            changed = true;
            continue;
        }

        const char *tag_end = strchr(target, '>');
        if (!tag_end) {
            read = attrs_end + suffix_len;
            changed = true;
            continue;
        }

        size_t tag_len = (size_t)(tag_end - target);
        if (len + tag_len + attrs_len + 16 > cap) {
            cap = (len + tag_len + attrs_len + 16) * 2;
            char *grown = realloc(out, cap);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }

        memcpy(out + len, target, tag_len);
        len += tag_len;

        if (attrs_len > 0) {
            out[len++] = ' ';
            memcpy(out + len, attrs_start, attrs_len);
            len += attrs_len;
        }

        out[len++] = '>';
        read = tag_end + 1;
        changed = true;
    }

    if (!changed) {
        free(out);
        return NULL;
    }

    size_t tail_len = strlen(read);
    if (len + tail_len + 1 > cap) {
        cap = len + tail_len + 2;
        char *grown = realloc(out, cap);
        if (!grown) {
            free(out);
            return NULL;
        }
        out = grown;
    }
    memcpy(out + len, read, tail_len);
    len += tail_len;
    out[len] = '\0';
    return out;
}
