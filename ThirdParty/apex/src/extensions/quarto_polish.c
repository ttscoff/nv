/**
 * Quarto mode polish: strict lists, cross-ref markers
 */

#include "quarto_polish.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum {
    LINE_BLANK = 0,
    LINE_LIST,
    LINE_OTHER
} line_kind;

static bool line_is_blank(const char *start, const char *end) {
    const char *p = start;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) {
        p++;
    }
    return p >= end;
}

static bool line_is_list_marker(const char *start, const char *end) {
    const char *p = start;
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    if (p >= end) {
        return false;
    }

    if (*p == '-' || *p == '*' || *p == '+') {
        return (p + 1 < end && (*p == '+' || isspace((unsigned char)p[1])));
    }

    if (isdigit((unsigned char)*p)) {
        while (p < end && isdigit((unsigned char)*p)) {
            p++;
        }
        return p < end && *p == '.' && (p + 1 >= end || isspace((unsigned char)p[1]));
    }

    if (p + 3 <= end && p[0] == '(' && p[1] == '@') {
        return true;
    }

    if ((*p == 'i' || *p == 'I') && p + 1 < end && p[1] == 'i' && p + 2 < end && p[2] == ')') {
        return isspace((unsigned char)p[3]) || p + 3 >= end;
    }
    if (*p == 'i' && p + 1 < end && p[1] == ')') {
        return isspace((unsigned char)p[2]) || p + 2 >= end;
    }

    return false;
}

static line_kind classify_line(const char *start, const char *end) {
    if (line_is_blank(start, end)) {
        return LINE_BLANK;
    }
    if (line_is_list_marker(start, end)) {
        return LINE_LIST;
    }
    return LINE_OTHER;
}

char *apex_preprocess_quarto_strict_lists(const char *text) {
    if (!text) {
        return NULL;
    }

    size_t cap = strlen(text) * 2 + 64;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    line_kind prev = LINE_BLANK;
    bool in_fenced_code = false;

    const char *read = text;
    while (*read) {
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

        if (!in_fenced_code) {
            line_kind kind = classify_line(line_start, line_end);
            if (kind == LINE_LIST && prev == LINE_OTHER) {
                if (!append_chunk(&out, &len, &cap, "\n", 1)) {
                    free(out);
                    return NULL;
                }
                changed = true;
            }
            if (kind != LINE_BLANK) {
                prev = kind;
            } else {
                prev = LINE_BLANK;
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

static bool xref_prefix(const char *p) {
    static const char *prefixes[] = { "fig-", "sec-", "tbl-", "eq-", NULL };
    for (int i = 0; prefixes[i]; i++) {
        size_t n = strlen(prefixes[i]);
        if (strncmp(p, prefixes[i], n) == 0) {
            return true;
        }
    }
    return false;
}

static size_t xref_token_len(const char *start) {
    if (*start != '@') {
        return 0;
    }
    const char *p = start + 1;
    if (!xref_prefix(p)) {
        return 0;
    }
    p += 4;
    size_t len = (size_t)(p - start);
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-' || *p == ':')) {
        p++;
        len++;
    }
    while (len > 6 && strchr(".,;:!?", p[-1])) {
        p--;
        len--;
    }
    return len >= 6 ? len : 0;
}

char *apex_postprocess_quarto_xrefs_html(const char *html) {
    if (!html) {
        return NULL;
    }

    size_t cap = strlen(html) * 2 + 256;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t len = 0;
    out[0] = '\0';
    bool changed = false;
    bool in_tag = false;

    for (const char *read = html; *read;) {
        if (*read == '<') {
            in_tag = true;
            const char *gt = strchr(read, '>');
            size_t chunk = gt ? (size_t)(gt - read + 1) : strlen(read);
            if (!append_chunk(&out, &len, &cap, read, chunk)) {
                free(out);
                return NULL;
            }
            read += chunk;
            if (gt) {
                in_tag = false;
            }
            continue;
        }

        if (in_tag) {
            if (!append_chunk(&out, &len, &cap, read, 1)) {
                free(out);
                return NULL;
            }
            read++;
            continue;
        }

        size_t tok_len = xref_token_len(read);
        if (tok_len > 0) {
            if (!append_chunk(&out, &len, &cap, "<span class=\"quarto-xref\">", 26)) {
                free(out);
                return NULL;
            }
            if (!append_chunk(&out, &len, &cap, read, tok_len)) {
                free(out);
                return NULL;
            }
            if (!append_chunk(&out, &len, &cap, "</span>", 7)) {
                free(out);
                return NULL;
            }
            changed = true;
            read += tok_len;
            continue;
        }

        if (!append_chunk(&out, &len, &cap, read, 1)) {
            free(out);
            return NULL;
        }
        read++;
    }

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
