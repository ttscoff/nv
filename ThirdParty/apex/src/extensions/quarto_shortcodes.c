/**
 * Quarto/Pandoc shortcode shim
 */

#include "quarto_shortcodes.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char PAGEBREAK_HTML[] =
    "\n\n<div class=\"mkpagebreak manualbreak\" "
    "title=\"Page break created by marker\" "
    "data-description=\"PAGE (Marker)\" "
    "style=\"page-break-after:always\">"
    "<span style=\"display:none\">&nbsp;</span></div>\n\n";

static const char PAGEBREAK_MARKER[] = "{::pagebreak /}";

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

static void trim_inplace(char *s) {
    if (!s) {
        return;
    }
    size_t n = strlen(s);
    size_t start = 0;
    while (start < n && isspace((unsigned char)s[start])) {
        start++;
    }
    size_t end = n;
    while (end > start && isspace((unsigned char)s[end - 1])) {
        end--;
    }
    if (start > 0 || end < n) {
        memmove(s, s + start, end - start);
        s[end - start] = '\0';
    }
}

static void strip_outer_quotes(char *s) {
    if (!s) {
        return;
    }
    trim_inplace(s);
    size_t n = strlen(s);
    if (n >= 2 &&
        ((s[0] == '"' && s[n - 1] == '"') || (s[0] == '\'' && s[n - 1] == '\''))) {
        s[n - 1] = '\0';
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

static const char *first_arg_token(const char *args, char *token_out, size_t token_size) {
    if (!args || !token_out || token_size == 0) {
        return NULL;
    }
    token_out[0] = '\0';
    const char *p = args;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (!*p) {
        return p;
    }

    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        size_t i = 0;
        while (*p && *p != quote && i + 1 < token_size) {
            token_out[i++] = *p++;
        }
        token_out[i] = '\0';
        if (*p == quote) {
            p++;
        }
        return p;
    }

    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i + 1 < token_size) {
        token_out[i++] = *p++;
    }
    token_out[i] = '\0';
    return p;
}

static bool expand_shortcode(const char *name, const char *args,
                             char **out, size_t *len, size_t *cap,
                             bool warn_unknown, bool unsafe) {
    if (!name || !*name) {
        return false;
    }

    if (strcasecmp(name, "pagebreak") == 0) {
        if (unsafe) {
            return append_str(out, len, cap, PAGEBREAK_HTML);
        }
        return append_str(out, len, cap, PAGEBREAK_MARKER);
    }

    if (strcasecmp(name, "kbd") == 0) {
        if (!append_str(out, len, cap, "{% kbd ")) {
            return false;
        }
        if (args && *args) {
            if (!append_str(out, len, cap, args)) {
                return false;
            }
        }
        return append_str(out, len, cap, " %}");
    }

    if (strcasecmp(name, "include") == 0) {
        char path[1024];
        first_arg_token(args, path, sizeof(path));
        strip_outer_quotes(path);
        if (path[0] == '\0') {
            return false;
        }
        if (!append_str(out, len, cap, "<<[")) {
            return false;
        }
        if (!append_str(out, len, cap, path)) {
            return false;
        }
        return append_str(out, len, cap, "]");
    }

    if (warn_unknown) {
        fprintf(stderr, "Warning: unknown Quarto shortcode '{{< %s >}}' left unchanged\n", name);
    }
    return false;
}

static bool try_shortcode(const char *read, const char **read_out,
                          char **out, size_t *len, size_t *cap,
                          bool *changed, bool warn_unknown, bool unsafe) {
    if (read[0] != '{' || read[1] != '{') {
        return false;
    }

    char style = read[2];
    if (style != '<' && style != '%') {
        return false;
    }

    const char *p = read + 3;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    char name[128];
    size_t name_len = 0;
    while (*p && !isspace((unsigned char)*p) && *p != style && name_len + 1 < sizeof(name)) {
        name[name_len++] = *p++;
    }
    name[name_len] = '\0';
    if (name_len == 0) {
        return false;
    }

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    const char *args_start = p;
    const char *close = strstr(p, style == '<' ? ">}}" : "%}}");
    if (!close) {
        return false;
    }

    char args[1024];
    size_t args_len = (size_t)(close - args_start);
    if (args_len >= sizeof(args)) {
        args_len = sizeof(args) - 1;
    }
    memcpy(args, args_start, args_len);
    args[args_len] = '\0';
    trim_inplace(args);

    const char *after = close + 3;

    size_t before_len = *len;
    if (!expand_shortcode(name, args, out, len, cap, warn_unknown, unsafe)) {
        *len = before_len;
        (*out)[*len] = '\0';
        return false;
    }

    *changed = true;
    *read_out = after;
    return true;
}

static const char *line_end_ptr(const char *p) {
    const char *e = strchr(p, '\n');
    return e ? e : p + strlen(p);
}

static bool shortcode_in_fenced_code(const char *text, const char *shortcode) {
    bool in_fenced_code = false;
    const char *read = text;
    while (read < shortcode) {
        const char *line_start = read;
        const char *line_end = line_end_ptr(read);
        bool at_line_start = (read == text || read[-1] == '\n');
        if (at_line_start) {
            const char *content = line_start;
            while (content < line_end && (*content == ' ' || *content == '\t')) {
                content++;
            }
            if ((size_t)(line_end - content) >= 3 && strncmp(content, "```", 3) == 0) {
                in_fenced_code = !in_fenced_code;
            }
        }
        read = (*line_end == '\n') ? line_end + 1 : line_end;
    }
    return in_fenced_code;
}

char *apex_preprocess_quarto_shortcodes(const char *text, bool warn_unknown, bool unsafe) {
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

    const char *read = text;
    while (*read) {
        if (!shortcode_in_fenced_code(text, read)) {
            const char *sc = strstr(read, "{{");
            if (sc) {
                if (sc > read) {
                    if (!append_chunk(&out, &len, &cap, read, (size_t)(sc - read))) {
                        free(out);
                        return NULL;
                    }
                }
                const char *after = sc;
                if (try_shortcode(sc, &after, &out, &len, &cap, &changed, warn_unknown, unsafe)) {
                    read = after;
                    continue;
                }
                if (!append_str(&out, &len, &cap, "{{")) {
                    free(out);
                    return NULL;
                }
                read = sc + 2;
                continue;
            }
        }

        const char *line_end = line_end_ptr(read);
        size_t chunk_len = (size_t)(line_end - read);
        if (*line_end == '\n') {
            chunk_len++;
        }
        if (!append_chunk(&out, &len, &cap, read, chunk_len)) {
            free(out);
            return NULL;
        }
        read += chunk_len;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
