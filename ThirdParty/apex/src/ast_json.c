#include "ast_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ------------------------------------------------------------------------- */
/* Simple JSON string builder                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} apex_json_buf;

static int apex_json_buf_init(apex_json_buf *b) {
    b->cap = 4096;
    b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (!b->data) return 0;
    b->data[0] = '\0';
    return 1;
}

static int apex_json_buf_ensure(apex_json_buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return 1;
    size_t new_cap = b->cap * 2;
    while (new_cap < b->len + extra + 1) {
        new_cap *= 2;
    }
    char *nb = (char *)realloc(b->data, new_cap);
    if (!nb) return 0;
    b->data = nb;
    b->cap  = new_cap;
    return 1;
}

static int apex_json_buf_append(apex_json_buf *b, const char *s) {
    if (!s) return 1;
    size_t slen = strlen(s);
    if (!apex_json_buf_ensure(b, slen)) return 0;
    memcpy(b->data + b->len, s, slen);
    b->len += slen;
    b->data[b->len] = '\0';
    return 1;
}

/* ------------------------------------------------------------------------- */
/* JSON string escaping                                                      */
/* ------------------------------------------------------------------------- */

static char *apex_json_escape_local(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    size_t cap = len * 6 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    char *w = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
            case '\\': *w++ = '\\'; *w++ = '\\'; break;
            case '"':  *w++ = '\\'; *w++ = '"';  break;
            case '\n': *w++ = '\\'; *w++ = 'n';  break;
            case '\r': *w++ = '\\'; *w++ = 'r';  break;
            case '\t': *w++ = '\\'; *w++ = 't';  break;
            default:
                if (c < 0x20) {
                    int written = snprintf(w, cap - (size_t)(w - out), "\\u%04X", c);
                    if (written <= 0 || (size_t)written >= cap - (size_t)(w - out)) {
                        free(out);
                        return NULL;
                    }
                    w += written;
                } else {
                    *w++ = (char)c;
                }
        }
    }
    *w = '\0';
    return out;
}

static int apex_json_buf_append_escaped_string(apex_json_buf *b,
                                               const char *text) {
    char *escaped = apex_json_escape_local(text ? text : "");
    if (!escaped) return 0;
    int ok = apex_json_buf_append(b, "\"") &&
             apex_json_buf_append(b, escaped) &&
             apex_json_buf_append(b, "\"");
    free(escaped);
    return ok;
}

/* ------------------------------------------------------------------------- */
/* Pandoc AST serialization helpers                                          */
/* ------------------------------------------------------------------------- */

static int write_inlines(apex_json_buf *b, cmark_node *first_inline);
static int write_blocks(apex_json_buf *b, cmark_node *block);

/* Write a Pandoc Attr triple: [ id, [classes], [[k,v], ...] ] */
static int write_pandoc_attr_empty(apex_json_buf *b) {
    return apex_json_buf_append(b, "[\"\",[],[]]");
}

static int write_inline(apex_json_buf *b, cmark_node *node) {
    if (!node) return 1;
    cmark_node_type t = cmark_node_get_type(node);

    switch (t) {
    case CMARK_NODE_TEXT: {
        if (!apex_json_buf_append(b, "{\"t\":\"Str\",\"c\":")) return 0;
        if (!apex_json_buf_append_escaped_string(b, cmark_node_get_literal(node))) return 0;
        if (!apex_json_buf_append(b, "}")) return 0;
        break;
    }
    case CMARK_NODE_SOFTBREAK: {
        if (!apex_json_buf_append(b, "{\"t\":\"SoftBreak\",\"c\":[]}")) return 0;
        break;
    }
    case CMARK_NODE_LINEBREAK: {
        if (!apex_json_buf_append(b, "{\"t\":\"LineBreak\",\"c\":[]}")) return 0;
        break;
    }
    case CMARK_NODE_CODE: {
        const char *lit = cmark_node_get_literal(node);
        if (!apex_json_buf_append(b, "{\"t\":\"Code\",\"c\":[")) return 0;
        if (!write_pandoc_attr_empty(b)) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!apex_json_buf_append_escaped_string(b, lit ? lit : "")) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_EMPH: {
        if (!apex_json_buf_append(b, "{\"t\":\"Emph\",\"c\":[")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_STRONG: {
        if (!apex_json_buf_append(b, "{\"t\":\"Strong\",\"c\":[")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_LINK: {
        const char *url   = cmark_node_get_url(node);
        const char *title = cmark_node_get_title(node);
        if (!apex_json_buf_append(b, "{\"t\":\"Link\",\"c\":[")) return 0;
        if (!write_pandoc_attr_empty(b)) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, ",[")) return 0;
        if (!apex_json_buf_append_escaped_string(b, url ? url : "")) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!apex_json_buf_append_escaped_string(b, title ? title : "")) return 0;
        if (!apex_json_buf_append(b, "]]}")) return 0;
        break;
    }
    case CMARK_NODE_IMAGE: {
        const char *url   = cmark_node_get_url(node);
        const char *title = cmark_node_get_title(node);
        if (!apex_json_buf_append(b, "{\"t\":\"Image\",\"c\":[")) return 0;
        if (!write_pandoc_attr_empty(b)) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, ",[")) return 0;
        if (!apex_json_buf_append_escaped_string(b, url ? url : "")) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!apex_json_buf_append_escaped_string(b, title ? title : "")) return 0;
        if (!apex_json_buf_append(b, "]]}")) return 0;
        break;
    }
    case CMARK_NODE_HTML_INLINE: {
        const char *lit = cmark_node_get_literal(node);
        if (!apex_json_buf_append(b, "{\"t\":\"RawInline\",\"c\":[\"html\",")) return 0;
        if (!apex_json_buf_append_escaped_string(b, lit ? lit : "")) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    default: {
        /* Fallback: treat as plain string of its literal (if any) */
        const char *lit = cmark_node_get_literal(node);
        if (!lit) lit = "";
        if (!apex_json_buf_append(b, "{\"t\":\"Str\",\"c\":")) return 0;
        if (!apex_json_buf_append_escaped_string(b, lit)) return 0;
        if (!apex_json_buf_append(b, "}")) return 0;
        break;
    }
    }

    return 1;
}

static int write_inlines(apex_json_buf *b, cmark_node *first_inline) {
    if (!apex_json_buf_append(b, "[")) return 0;
    cmark_node *cur = first_inline;
    int first = 1;
    while (cur) {
        if (!first) {
            if (!apex_json_buf_append(b, ",")) return 0;
        }
        if (!write_inline(b, cur)) return 0;
        first = 0;
        cur = cmark_node_next(cur);
    }
    if (!apex_json_buf_append(b, "]")) return 0;
    return 1;
}

static int write_block(apex_json_buf *b, cmark_node *node) {
    if (!node) return 1;
    cmark_node_type t = cmark_node_get_type(node);

    switch (t) {
    case CMARK_NODE_PARAGRAPH: {
        if (!apex_json_buf_append(b, "{\"t\":\"Para\",\"c\":")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, "}")) return 0;
        break;
    }
    case CMARK_NODE_HEADING: {
        int level = cmark_node_get_heading_level(node);
        if (level <= 0) level = 1;
        if (!apex_json_buf_append(b, "{\"t\":\"Header\",\"c\":[")) return 0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", level);
        if (!apex_json_buf_append(b, buf)) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!write_pandoc_attr_empty(b)) return 0;
        if (!apex_json_buf_append(b, ",")) return 0;
        if (!write_inlines(b, cmark_node_first_child(node))) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_THEMATIC_BREAK: {
        if (!apex_json_buf_append(b, "{\"t\":\"HorizontalRule\",\"c\":[]}")) return 0;
        break;
    }
    case CMARK_NODE_HTML_BLOCK: {
        const char *lit = cmark_node_get_literal(node);
        if (!apex_json_buf_append(b, "{\"t\":\"RawBlock\",\"c\":[\"html\",")) return 0;
        if (!apex_json_buf_append_escaped_string(b, lit ? lit : "")) return 0;
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_CODE_BLOCK: {
        const char *lit = cmark_node_get_literal(node);
        const char *lang = cmark_node_get_fence_info(node);
        size_t lit_len = lit ? strlen(lit) : 0;
        /* Strip single trailing newline so JSON does not contain \\n (avoids decoder issues) */
        if (lit_len > 0 && lit[lit_len - 1] == '\n') {
            lit_len--;
        }
        if (!apex_json_buf_append(b, "{\"t\":\"CodeBlock\",\"c\":[[")) return 0; /* c = [Attr, content]; Attr = [id, classes, keyvals]; parser expects [[ */
        if (!apex_json_buf_append_escaped_string(b, "")) return 0; /* id (no extra "[" so attr is [id,classes,keyvals] not [[...) */
        if (!apex_json_buf_append(b, ",[")) return 0; /* classes */
        if (lang && *lang) {
            if (!apex_json_buf_append_escaped_string(b, lang)) return 0;
        }
        /* Close Attr (keyvals then attr) with ]], then comma before content */
        if (lang && strcmp(lang, "inc") == 0) {
            if (!apex_json_buf_append(b, "]\x2c[[\"inc\",\"yes\"]]],")) return 0;
        } else {
            if (!apex_json_buf_append(b, "],[]]],")) return 0;
        }
        if (lit_len > 0) {
            char *lit_copy = (char *)malloc(lit_len + 1);
            if (lit_copy) {
                memcpy(lit_copy, lit, lit_len);
                lit_copy[lit_len] = '\0';
                if (!apex_json_buf_append_escaped_string(b, lit_copy)) { free(lit_copy); return 0; }
                free(lit_copy);
            } else if (!apex_json_buf_append_escaped_string(b, "")) {
                return 0;
            }
        } else if (!apex_json_buf_append_escaped_string(b, "")) {
            return 0;
        }
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_LIST: {
        cmark_list_type lt = cmark_node_get_list_type(node);
        int is_ordered = (lt == CMARK_ORDERED_LIST);
        if (is_ordered) {
            if (!apex_json_buf_append(b, "{\"t\":\"OrderedList\",\"c\":[")) return 0;
            /* Simple default list attributes: [start, style, delim] */
            int start = cmark_node_get_list_start(node);
            if (start <= 0) start = 1;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", start);
            if (!apex_json_buf_append(b, "[")) return 0;
            if (!apex_json_buf_append(b, buf)) return 0;
            if (!apex_json_buf_append(b, ",\"Decimal\",\"Period\"],[")) return 0;
        } else {
            if (!apex_json_buf_append(b, "{\"t\":\"BulletList\",\"c\":[")) return 0;
        }

        /* List items: each item is a list of blocks */
        cmark_node *item = cmark_node_first_child(node);
        int first_item = 1;
        while (item) {
            if (!first_item) {
                if (!apex_json_buf_append(b, ",")) return 0;
            }
            if (!apex_json_buf_append(b, "[")) return 0;
            cmark_node *child = cmark_node_first_child(item);
            int first_block = 1;
            while (child) {
                if (!first_block) {
                    if (!apex_json_buf_append(b, ",")) return 0;
                }
                if (!write_block(b, child)) return 0;
                first_block = 0;
                child = cmark_node_next(child);
            }
            if (!apex_json_buf_append(b, "]")) return 0;
            first_item = 0;
            item = cmark_node_next(item);
        }

        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    case CMARK_NODE_BLOCK_QUOTE: {
        if (!apex_json_buf_append(b, "{\"t\":\"BlockQuote\",\"c\":[")) return 0;
        cmark_node *child = cmark_node_first_child(node);
        int first_block = 1;
        while (child) {
            if (!first_block) {
                if (!apex_json_buf_append(b, ",")) return 0;
            }
            if (!write_block(b, child)) return 0;
            first_block = 0;
            child = cmark_node_next(child);
        }
        if (!apex_json_buf_append(b, "]}")) return 0;
        break;
    }
    default: {
        /* Fallback: render paragraph of plain text from this node's literal (if any) */
        const char *lit = cmark_node_get_literal(node);
        if (!apex_json_buf_append(b, "{\"t\":\"Para\",\"c\":[")) return 0;
        if (!apex_json_buf_append(b, "{\"t\":\"Str\",\"c\":")) return 0;
        if (!apex_json_buf_append_escaped_string(b, lit ? lit : "")) return 0;
        if (!apex_json_buf_append(b, "}]}")) return 0;
        break;
    }
    }

    return 1;
}

static int write_blocks(apex_json_buf *b, cmark_node *block) {
    int first = 1;
    cmark_node *cur = block;
    while (cur) {
        if (!first) {
            if (!apex_json_buf_append(b, ",")) return 0;
        }
        if (!write_block(b, cur)) return 0;
        first = 0;
        cur = cmark_node_next(cur);
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Public: cmark -> Pandoc JSON                                             */
/* ------------------------------------------------------------------------- */

char *apex_cmark_to_pandoc_json(cmark_node *document,
                                const apex_options *options) {
    (void)options; /* reserved for future metadata mapping */
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) {
        return NULL;
    }

    apex_json_buf b;
    if (!apex_json_buf_init(&b)) {
        return NULL;
    }

    /* Top-level Pandoc object. We use a fixed pandoc-api-version that is
     * compatible with modern Pandoc filters; the exact version is not
     * critical for most filters.
     */
    if (!apex_json_buf_append(&b, "{")) goto error;
    if (!apex_json_buf_append(&b, "\"pandoc-api-version\":[1,23,1],\"meta\":{},\"blocks\":[")) goto error;

    if (!write_blocks(&b, cmark_node_first_child(document))) goto error;

    if (!apex_json_buf_append(&b, "]}")) goto error;

    return b.data;

error:
    free(b.data);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Minimal JSON parser for Pandoc subset (JSON -> cmark)                     */
/* ------------------------------------------------------------------------- */

typedef struct {
    const char *s;
} json_cursor;

static void json_skip_ws(json_cursor *cur) {
    while (*cur->s && isspace((unsigned char)*cur->s)) {
        cur->s++;
    }
}

static int json_match_char(json_cursor *cur, char ch) {
    json_skip_ws(cur);
    if (*cur->s != ch) return 0;
    cur->s++;
    return 1;
}

static char *json_parse_string(json_cursor *cur) {
    json_skip_ws(cur);
    if (*cur->s != '"') return NULL;
    cur->s++; /* skip quote */
    const char *start = cur->s;
    size_t cap = 64;
    size_t len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    while (*cur->s && *cur->s != '"') {
        unsigned char c = (unsigned char)*cur->s;
        if (c == '\\') {
            cur->s++;
            c = (unsigned char)*cur->s;
            if (!c) { free(out); return NULL; }
            switch (c) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'u':
                /* For simplicity, skip \uXXXX and store as '?' */
                cur->s++;
                for (int i = 0; i < 4 && *cur->s; i++) {
                    cur->s++;
                }
                c = '?';
                break;
            default:
                /* Unknown escape; keep as-is */
                break;
            }
            /* Advance past the escaped character (for \u we already advanced in the case) */
            if (c != '?')
                cur->s++;
        } else {
            cur->s++;
        }

        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(out, cap);
            if (!nb) {
                free(out);
                return NULL;
            }
            out = nb;
        }
        out[len++] = (char)c;
    }

    if (*cur->s != '"') {
        free(out);
        return NULL;
    }
    cur->s++; /* closing quote */
    out[len] = '\0';
    (void)start;
    return out;
}

static long json_parse_int(json_cursor *cur) {
    json_skip_ws(cur);
    int neg = 0;
    if (*cur->s == '-') {
        neg = 1;
        cur->s++;
    }
    long v = 0;
    while (*cur->s && isdigit((unsigned char)*cur->s)) {
        v = v * 10 + (*cur->s - '0');
        cur->s++;
    }
    return neg ? -v : v;
}

/* Forward declarations for recursive descent on the Pandoc subset */
static cmark_node *parse_blocks_array(json_cursor *cur);
static cmark_node *parse_block_object(json_cursor *cur);
static int         json_skip_value(json_cursor *cur);

/* Skip arbitrary JSON value (used for meta and fields we don't care about) */
static int json_skip_value(json_cursor *cur) {
    json_skip_ws(cur);
    char c = *cur->s;
    if (!c) return 0;
    if (c == '"') {
        char *tmp = json_parse_string(cur);
        if (!tmp) return 0;
        free(tmp);
        return 1;
    }
    if (c == '{') {
        cur->s++;
        for (;;) {
            json_skip_ws(cur);
            if (*cur->s == '}') {
                cur->s++;
                break;
            }
            /* key */
            char *k = json_parse_string(cur);
            if (!k) return 0;
            free(k);
            if (!json_match_char(cur, ':')) return 0;
            if (!json_skip_value(cur)) return 0;
            json_skip_ws(cur);
            if (*cur->s == ',') {
                cur->s++;
                continue;
            } else if (*cur->s == '}') {
                continue;
            } else {
                break;
            }
        }
        return 1;
    }
    if (c == '[') {
        cur->s++;
        for (;;) {
            json_skip_ws(cur);
            if (*cur->s == ']') {
                cur->s++;
                break;
            }
            if (!json_skip_value(cur)) return 0;
            json_skip_ws(cur);
            if (*cur->s == ',') {
                cur->s++;
                continue;
            } else if (*cur->s == ']') {
                continue;
            } else {
                break;
            }
        }
        return 1;
    }
    /* number, true, false, null – skip simple token */
    while (*cur->s && !isspace((unsigned char)*cur->s) &&
           *cur->s != ',' && *cur->s != ']' && *cur->s != '}') {
        cur->s++;
    }
    return 1;
}

/* Parse a Pandoc Inline array into a linked list of cmark nodes attached
 * under a temporary dummy parent; returns first child.
 */
static cmark_node *parse_inlines_array(json_cursor *cur) {
    if (!json_match_char(cur, '[')) return NULL;
    cmark_node *dummy = cmark_node_new(CMARK_NODE_PARAGRAPH);
    if (!dummy) return NULL;
    /* 'last' was previously used but is no longer needed; we intentionally
     * omit it to avoid unused-variable warnings.
     */

    json_skip_ws(cur);
    if (*cur->s == ']') {
        cur->s++;
        cmark_node *first = cmark_node_first_child(dummy);
        cmark_node_free(dummy);
        return first;
    }

    int ok = 1;
    while (ok) {
        json_skip_ws(cur);
        if (*cur->s != '{') { ok = 0; break; }
        cur->s++; /* object start */

        /* Parse object with any key order (e.g. dkjson outputs "c" before "t") */
        char *tag = NULL;
        char *c_buf = NULL;
        while (1) {
            json_skip_ws(cur);
            if (*cur->s == '}') {
                cur->s++;
                break;
            }
            char *key = json_parse_string(cur);
            if (!key) { ok = 0; break; }
            if (!json_match_char(cur, ':')) { free(key); ok = 0; break; }
            if (strcmp(key, "t") == 0) {
                tag = json_parse_string(cur);
                free(key);
                if (!tag) { ok = 0; break; }
            } else if (strcmp(key, "c") == 0) {
                json_skip_ws(cur);
                const char *v_start = cur->s;
                if (!json_skip_value(cur)) { free(key); ok = 0; break; }
                size_t vlen = (size_t)(cur->s - v_start);
                c_buf = (char *)malloc(vlen + 1);
                if (!c_buf) { free(key); ok = 0; break; }
                memcpy(c_buf, v_start, vlen);
                c_buf[vlen] = '\0';
                free(key);
            } else {
                if (!json_skip_value(cur)) { free(key); ok = 0; break; }
                free(key);
            }
            json_skip_ws(cur);
            if (*cur->s == ',') cur->s++;
        }
        if (!ok || !tag || !c_buf) {
            free(tag);
            free(c_buf);
            ok = 0;
            break;
        }

        /* Parse "c" from buffer so we can dispatch by tag */
        json_cursor ccur = { c_buf };
        cmark_node *node = NULL;

        if (strcmp(tag, "Str") == 0) {
            char *txt = json_parse_string(&ccur);
            if (!txt) { free(tag); free(c_buf); ok = 0; break; }
            node = cmark_node_new(CMARK_NODE_TEXT);
            if (!node) { free(tag); free(c_buf); free(txt); ok = 0; break; }
            cmark_node_set_literal(node, txt);
            free(txt);
        } else if (strcmp(tag, "SoftBreak") == 0) {
            node = cmark_node_new(CMARK_NODE_SOFTBREAK);
        } else if (strcmp(tag, "LineBreak") == 0) {
            node = cmark_node_new(CMARK_NODE_LINEBREAK);
        } else if (strcmp(tag, "Emph") == 0 || strcmp(tag, "Strong") == 0) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); ok = 0; break; }
            cmark_node *wrapper = cmark_node_new(strcmp(tag, "Emph") == 0
                                                 ? CMARK_NODE_EMPH
                                                 : CMARK_NODE_STRONG);
            if (!wrapper) { free(tag); free(c_buf); ok = 0; break; }
            json_skip_ws(&ccur);
            if (*ccur.s != ']') {
                cmark_node *child_first = parse_inlines_array(&ccur);
                cmark_node *child = child_first;
                while (child) {
                    cmark_node *next = cmark_node_next(child);
                    cmark_node_append_child(wrapper, child);
                    child = next;
                }
            }
            node = wrapper;
        } else if (strcmp(tag, "Code") == 0) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_skip_value(&ccur)) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); ok = 0; break; }
            char *txt = json_parse_string(&ccur);
            if (!txt) { free(tag); free(c_buf); ok = 0; break; }
            node = cmark_node_new(CMARK_NODE_CODE);
            if (!node) { free(tag); free(c_buf); free(txt); ok = 0; break; }
            cmark_node_set_literal(node, txt);
            free(txt);
        } else if (strcmp(tag, "Span") == 0) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_skip_value(&ccur)) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); ok = 0; break; }

            cmark_node *child_first = parse_inlines_array(&ccur);

            cmark_node *child = child_first;
            while (child) {
                cmark_node *next = cmark_node_next(child);
                if (!cmark_node_append_child(dummy, child)) {
                    cmark_node_free(child);
                    ok = 0;
                    break;
                }
                child = next;
            }
            node = NULL;
        } else if (strcmp(tag, "Math") == 0) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, '{')) { free(tag); free(c_buf); ok = 0; break; }
            char *kind_key = json_parse_string(&ccur);
            if (!kind_key) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, ':')) { free(kind_key); free(tag); free(c_buf); ok = 0; break; }
            char *kind = json_parse_string(&ccur);
            free(kind_key);
            if (!kind) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, '}')) { free(kind); free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, ',')) { free(kind); free(tag); free(c_buf); ok = 0; break; }

            char *code = json_parse_string(&ccur);
            if (!code) { free(kind); free(tag); free(c_buf); ok = 0; break; }

            const char *delim = (strcmp(kind, "DisplayMath") == 0) ? "$$" : "$";
            size_t dlen = strlen(delim);
            size_t clen = strlen(code);
            size_t wlen = dlen * 2 + clen + 1;
            char *wrapped = (char *)malloc(wlen);
            if (!wrapped) {
                free(code);
                free(kind);
                free(tag);
                free(c_buf);
                ok = 0;
                break;
            }
            snprintf(wrapped, wlen, "%s%s%s", delim, code, delim);

            node = cmark_node_new(CMARK_NODE_HTML_INLINE);
            if (!node) {
                free(wrapped);
                free(code);
                free(kind);
                free(tag);
                free(c_buf);
                ok = 0;
                break;
            }
            cmark_node_set_literal(node, wrapped);

            free(wrapped);
            free(code);
            free(kind);
        } else if (strcmp(tag, "RawInline") == 0) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_skip_value(&ccur)) { free(tag); free(c_buf); ok = 0; break; }
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); ok = 0; break; }
            char *txt = json_parse_string(&ccur);
            if (!txt) { free(tag); free(c_buf); ok = 0; break; }
            node = cmark_node_new(CMARK_NODE_HTML_INLINE);
            if (!node) { free(tag); free(c_buf); free(txt); ok = 0; break; }
            cmark_node_set_literal(node, txt);
            free(txt);
        }
        /* Unknown inline: node stays NULL, no node created */

        free(tag);
        free(c_buf);
        /* Object closing '}' already consumed in the key-order loop */

            if (node) {
                if (!cmark_node_append_child(dummy, node)) {
                    cmark_node_free(node);
                    ok = 0;
                    break;
                }
            }

        json_skip_ws(cur);
        if (*cur->s == ',') {
            cur->s++;
            continue;
        } else if (*cur->s == ']') {
            break;
        } else {
            /* Unexpected character */
            break;
        }
    }

    /* Expect closing ']' */
    if (!json_match_char(cur, ']')) {
        cmark_node_free(dummy);
        return NULL;
    }

    cmark_node *first = cmark_node_first_child(dummy);
    if (first) {
        /* Detach children without freeing */
        cmark_node *child = first;
        while (child) {
            cmark_node *next = cmark_node_next(child);
            cmark_node_unlink(child);
            child = next;
        }
    }
    cmark_node_free(dummy);
    return first;
}

/* Parse a single Block object {"t": "...", "c": ...} (any key order, e.g. dkjson "c" before "t") */
static cmark_node *parse_block_object(json_cursor *cur) {
    if (!json_match_char(cur, '{')) return NULL;

    char *tag = NULL;
    char *c_buf = NULL;
    while (1) {
        json_skip_ws(cur);
        if (*cur->s == '}') {
            /* Only leave the loop when we have both "t" and "c" (handles dkjson "c" before "t") */
            if (tag && c_buf) {
                cur->s++;
                break;
            }
            free(tag);
            free(c_buf);
            return NULL;
        }
        char *key = json_parse_string(cur);
        if (!key) return NULL;
        if (!json_match_char(cur, ':')) { free(key); free(tag); free(c_buf); return NULL; }
        if (strcmp(key, "t") == 0) {
            tag = json_parse_string(cur);
            free(key);
            if (!tag) { free(c_buf); return NULL; }
        } else if (strcmp(key, "c") == 0) {
            json_skip_ws(cur);
            const char *v_start = cur->s;
            if (!json_skip_value(cur)) { free(key); free(tag); free(c_buf); return NULL; }
            size_t vlen = (size_t)(cur->s - v_start);
            c_buf = (char *)malloc(vlen + 1);
            if (!c_buf) { free(key); free(tag); return NULL; }
            memcpy(c_buf, v_start, vlen);
            c_buf[vlen] = '\0';
            free(key);
        } else {
            if (!json_skip_value(cur)) { free(key); free(tag); free(c_buf); return NULL; }
            free(key);
        }
        json_skip_ws(cur);
        if (*cur->s == ',') cur->s++;
    }
    if (!tag || !c_buf) {
        free(tag);
        free(c_buf);
        return NULL;
    }

    json_cursor ccur = { c_buf };
    cmark_node *node = NULL;

    if (strcmp(tag, "Para") == 0) {
        cmark_node *para = cmark_node_new(CMARK_NODE_PARAGRAPH);
        if (!para) { free(tag); free(c_buf); return NULL; }
        cmark_node *child_first = parse_inlines_array(&ccur);
        cmark_node *child = child_first;
        while (child) {
            cmark_node *next = cmark_node_next(child);
            cmark_node_append_child(para, child);
            child = next;
        }
        node = para;
    } else if (strcmp(tag, "Header") == 0) {
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        long level = json_parse_int(&ccur);
        if (level <= 0) level = 1;
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        cmark_node *heading = cmark_node_new(CMARK_NODE_HEADING);
        if (!heading) { free(tag); free(c_buf); return NULL; }
        cmark_node_set_heading_level(heading, (int)level);
        cmark_node *child_first = parse_inlines_array(&ccur);
        cmark_node *child = child_first;
        while (child) {
            cmark_node *next = cmark_node_next(child);
            cmark_node_append_child(heading, child);
            child = next;
        }
        json_skip_ws(&ccur);
        if (*ccur.s == ']')
            ccur.s++;
        node = heading;
    } else if (strcmp(tag, "HorizontalRule") == 0) {
        if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
        node = cmark_node_new(CMARK_NODE_THEMATIC_BREAK);
    } else if (strcmp(tag, "RawBlock") == 0) {
        json_skip_ws(&ccur);
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        char *txt = json_parse_string(&ccur);
        if (!txt) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, ']')) { free(tag); free(c_buf); free(txt); return NULL; }
        node = cmark_node_new(CMARK_NODE_HTML_BLOCK);
        if (!node) { free(tag); free(c_buf); free(txt); return NULL; }
        cmark_node_set_literal(node, txt);
        free(txt);
    } else if (strcmp(tag, "CodeBlock") == 0) {
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        char *id = json_parse_string(&ccur);
        (void)id;
        free(id);
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        char *lang = NULL;
        json_skip_ws(&ccur);
        if (*ccur.s != ']') {
            lang = json_parse_string(&ccur);
        }
        if (!json_match_char(&ccur, ']')) { free(tag); free(c_buf); free(lang); return NULL; }
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); free(lang); return NULL; }
        if (!json_skip_value(&ccur)) { free(tag); free(c_buf); free(lang); return NULL; }
        if (!json_match_char(&ccur, ']')) { free(tag); free(c_buf); free(lang); return NULL; }
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); free(lang); return NULL; }
        char *txt = json_parse_string(&ccur);
        if (!txt) { free(tag); free(c_buf); free(lang); return NULL; }
        node = cmark_node_new(CMARK_NODE_CODE_BLOCK);
        if (!node) { free(tag); free(c_buf); free(lang); free(txt); return NULL; }
        cmark_node_set_literal(node, txt);
        if (lang && *lang) {
            cmark_node_set_fence_info(node, lang);
        }
        free(txt);
        free(lang);
    } else if (strcmp(tag, "BulletList") == 0 || strcmp(tag, "OrderedList") == 0) {
        int is_ordered = (strcmp(tag, "OrderedList") == 0);
        if (is_ordered) {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
            long start = json_parse_int(&ccur);
            if (start <= 0) start = 1;
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
            if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
            if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
            if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        } else {
            if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        }

        cmark_node *list = cmark_node_new(CMARK_NODE_LIST);
        if (!list) { free(tag); free(c_buf); return NULL; }
        if (is_ordered) {
            cmark_node_set_list_type(list, CMARK_ORDERED_LIST);
        } else {
            cmark_node_set_list_type(list, CMARK_BULLET_LIST);
        }

        if (!json_match_char(&ccur, '[')) { cmark_node_free(list); free(tag); free(c_buf); return NULL; }
        json_skip_ws(&ccur);
        while (*ccur.s != ']') {
            if (!json_match_char(&ccur, '[')) { cmark_node_free(list); free(tag); free(c_buf); return NULL; }
            cmark_node *item = cmark_node_new(CMARK_NODE_ITEM);
            if (!item) { cmark_node_free(list); free(tag); free(c_buf); return NULL; }
            cmark_node *blocks = parse_blocks_array(&ccur);
            if (blocks) {
                cmark_node *blk = cmark_node_first_child(blocks);
                while (blk) {
                    cmark_node *next = cmark_node_next(blk);
                    cmark_node_append_child(item, blk);
                    blk = next;
                }
                cmark_node_free(blocks);
            }
            if (!json_match_char(&ccur, ']')) { cmark_node_free(item); cmark_node_free(list); free(tag); free(c_buf); return NULL; }

            cmark_node_append_child(list, item);

            json_skip_ws(&ccur);
            if (*ccur.s == ',') {
                ccur.s++;
                continue;
            } else if (*ccur.s == ']') {
                break;
            } else {
                break;
            }
        }
        if (!json_match_char(&ccur, ']')) { cmark_node_free(list); free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, ']')) { cmark_node_free(list); free(tag); free(c_buf); return NULL; }
        node = list;
    } else if (strcmp(tag, "BlockQuote") == 0) {
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        cmark_node *bq = cmark_node_new(CMARK_NODE_BLOCK_QUOTE);
        if (!bq) { free(tag); free(c_buf); return NULL; }
        cmark_node *blocks = parse_blocks_array(&ccur);
        if (blocks) {
            cmark_node *blk = cmark_node_first_child(blocks);
            while (blk) {
                cmark_node *next = cmark_node_next(blk);
                cmark_node_append_child(bq, blk);
                blk = next;
            }
            cmark_node_free(blocks);
        }
        if (!json_match_char(&ccur, ']')) { cmark_node_free(bq); free(tag); free(c_buf); return NULL; }
        node = bq;
    } else if (strcmp(tag, "Div") == 0) {
        /* c = [Attr, blocks] – skip Attr, parse inner blocks and return chain so caller appends all */
        if (!json_match_char(&ccur, '[')) { free(tag); free(c_buf); return NULL; }
        if (!json_skip_value(&ccur)) { free(tag); free(c_buf); return NULL; }
        if (!json_match_char(&ccur, ',')) { free(tag); free(c_buf); return NULL; }
        node = parse_blocks_array(&ccur);
        if (!json_match_char(&ccur, ']')) {
            if (node) cmark_node_free(node);
            free(tag);
            free(c_buf);
            return NULL;
        }
    } else {
        node = cmark_node_new(CMARK_NODE_PARAGRAPH);
    }

    free(tag);
    free(c_buf);
    return node;
}

static cmark_node *parse_blocks_array(json_cursor *cur) {
    json_skip_ws(cur);
    if (!json_match_char(cur, '[')) return NULL;
    cmark_node *dummy = cmark_node_new(CMARK_NODE_DOCUMENT);
    if (!dummy) return NULL;

    json_skip_ws(cur);
    if (*cur->s == ']') {
        cur->s++;
        cmark_node_free(dummy);
        return NULL;
    }

    int ok = 1;
    while (ok) {
        json_skip_ws(cur);
        cmark_node *blk = parse_block_object(cur);
        if (!blk) { ok = 0; break; }
        while (blk) {
            cmark_node *next = cmark_node_next(blk);
            cmark_node_append_child(dummy, blk);
            blk = next;
        }

        json_skip_ws(cur);
        if (*cur->s == ',') {
            cur->s++;
            continue;
        } else if (*cur->s == ']') {
            break;
        } else {
            break;
        }
    }

    if (!json_match_char(cur, ']')) {
        cmark_node_free(dummy);
        return NULL;
    }

    /* Return the dummy so the caller can iterate over its children and append each to doc;
     * returning the first child would require unlinking, which clears next and drops the chain. */
    return dummy;
}

/* ------------------------------------------------------------------------- */
/* Public: Pandoc JSON -> cmark                                             */
/* ------------------------------------------------------------------------- */

cmark_node *apex_pandoc_json_to_cmark(const char *json,
                                      const apex_options *options) {
    (void)options;
    if (!json) return NULL;

    json_cursor cur = { json };
    json_skip_ws(&cur);
    if (!json_match_char(&cur, '{')) return NULL;

    cmark_node *doc = cmark_node_new(CMARK_NODE_DOCUMENT);
    if (!doc) return NULL;

    /* Parse object fields; we only care about "blocks" */
    while (1) {
        json_skip_ws(&cur);
        if (*cur.s == '}') {
            cur.s++;
            break;
        }
        char *key = json_parse_string(&cur);
        if (!key) { cmark_node_free(doc); return NULL; }
        if (!json_match_char(&cur, ':')) { free(key); cmark_node_free(doc); return NULL; }

        if (strcmp(key, "blocks") == 0) {
            cmark_node *blocks = parse_blocks_array(&cur);
            if (blocks) {
                cmark_node *blk = cmark_node_first_child(blocks);
                while (blk) {
                    cmark_node *next = cmark_node_next(blk);
                    cmark_node_append_child(doc, blk);
                    blk = next;
                }
                cmark_node_free(blocks);
            }
        } else {
            /* Skip other fields (pandoc-api-version, meta, etc.) */
            if (!json_skip_value(&cur)) { free(key); cmark_node_free(doc); return NULL; }
        }

        free(key);

        json_skip_ws(&cur);
        if (*cur.s == ',') {
            cur.s++;
            continue;
        } else if (*cur.s == '}') {
            cur.s++;
            break;
        } else {
            break;
        }
    }

    return doc;
}

