/*
 * Man page output: roff (man page source) and styled HTML.
 * Renders cmark AST to .TH/.SH-style roff or a self-contained man-style HTML document.
 */

#include "apex/ast_man.h"
#include "apex/parser.h"
#include "extensions/definition_list.h"
#include "extensions/syntax_highlight.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ------------------------------------------------------------------------- */
/* Buffer and roff escape                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} man_buffer;

static void man_buf_init(man_buffer *b) {
    b->buf = NULL;
    b->len = 0;
    b->capacity = 0;
}

static void man_buf_append(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    if (b->len + len + 1 > b->capacity) {
        size_t new_cap = b->capacity ? b->capacity * 2 : 512;
        if (new_cap < b->len + len + 1) new_cap = b->len + len + 1;
        char *new_buf = (char *)realloc(b->buf, new_cap);
        if (!new_buf) return;
        b->buf = new_buf;
        b->capacity = new_cap;
    }
    memcpy(b->buf + b->len, str, len);
    b->len += len;
    b->buf[b->len] = '\0';
}

static void man_buf_append_str(man_buffer *b, const char *str) {
    if (str) man_buf_append(b, str, strlen(str));
}

/* Append text escaped for roff: \ -> \e, - -> \-, en-dash -> \-\-, leading . or ' -> \&. or \&' */
static void man_buf_append_roff_safe(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    bool at_line_start = (b->len == 0 || (b->len > 0 && b->buf[b->len - 1] == '\n'));
    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)str[i];
        if (c == '\\') {
            man_buf_append_str(b, "\\e");
            i++;
            at_line_start = false;
        } else if (c == '\n') {
            man_buf_append(b, "\n", 1);
            i++;
            at_line_start = true;
        } else if (at_line_start && (c == '.' || c == '\'')) {
            man_buf_append_str(b, "\\&");
            man_buf_append(b, (const char *)&c, 1);
            i++;
            at_line_start = false;
        } else if (c == 0x2D) {
            /* hyphen-minus: use \- so man doesn't break line on it; keeps -- visible */
            man_buf_append_str(b, "\\-");
            i++;
            at_line_start = false;
        } else if (c == 0xE2 && i + 2 <= len && (unsigned char)str[i+1] == 0x80 && (unsigned char)str[i+2] == 0x93) {
            /* UTF-8 en-dash (U+2013): often from smart typography -- ; show as two hyphens */
            man_buf_append_str(b, "\\-\\-");
            i += 3;
            at_line_start = false;
        } else if (c == 0xE2 && i + 2 <= len && (unsigned char)str[i+1] == 0x80 && (unsigned char)str[i+2] == 0x94) {
            /* UTF-8 em-dash (U+2014) */
            man_buf_append_str(b, "\\[em]");
            i += 3;
            at_line_start = false;
        } else {
            man_buf_append(b, (const char *)&c, 1);
            i++;
            at_line_start = false;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* HTML <dl>/<dt>/<dd> from definition-list preprocessor -> roff             */
/* ------------------------------------------------------------------------- */

/* Find next '>' from str+pos; return offset of '>' or len if not found. */
static size_t find_gt(const char *str, size_t len, size_t pos) {
    for (; pos < len && str[pos] != '>'; pos++) {}
    return pos;
}

/* Decode one entity at str (e.g. &lt; &gt; &amp;) and append to buf; return number of chars consumed. */
static size_t decode_entity(man_buffer *buf, const char *str, size_t len) {
    if (len < 3 || str[0] != '&') return 0;
    if (str[1] == 'l' && str[2] == 't' && len >= 4 && str[3] == ';') {
        man_buf_append_str(buf, "<");
        return 4;
    }
    if (str[1] == 'g' && str[2] == 't' && len >= 4 && str[3] == ';') {
        man_buf_append_str(buf, ">");
        return 4;
    }
    if (str[1] == 'a' && str[2] == 'm' && str[3] == 'p' && len >= 5 && str[4] == ';') {
        man_buf_append_str(buf, "&");
        return 5;
    }
    if (str[1] == 'q' && str[2] == 'u' && str[3] == 'o' && str[4] == 't' && len >= 6 && str[5] == ';') {
        man_buf_append_str(buf, "\"");
        return 6;
    }
    if (str[1] == '#' && len >= 4 && str[2] == '3' && str[3] == '9' && len >= 5 && str[4] == ';') {
        man_buf_append_str(buf, "'");
        return 5;
    }
    return 0;
}

/* Append HTML fragment (dt/dd content) as roff: handle <strong>, <em>, <code>, entities; strip other tags. */
static void append_html_fragment_roff(man_buffer *buf, const char *str, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (str[i] == '<') {
            size_t end = find_gt(str, len, i);
            if (end < len) {
                /* tag from i to end (inclusive) */
                size_t tag_len = end - i + 1;
                if (tag_len == 8 && strncmp(str + i, "<strong>", 8) == 0)
                    man_buf_append_str(buf, "\\f[B]");
                else if (tag_len == 9 && strncmp(str + i, "</strong>", 9) == 0)
                    man_buf_append_str(buf, "\\f[]");
                else if (tag_len == 5 && strncmp(str + i, "<em>", 5) == 0)
                    man_buf_append_str(buf, "\\f[I]");
                else if (tag_len == 6 && strncmp(str + i, "</em>", 6) == 0)
                    man_buf_append_str(buf, "\\f[]");
                else if (tag_len == 6 && strncmp(str + i, "<code>", 6) == 0)
                    man_buf_append_str(buf, "\\fR");
                else if (tag_len == 7 && strncmp(str + i, "</code>", 7) == 0)
                    man_buf_append_str(buf, "\\f[]");
                i = end + 1;
                continue;
            }
        }
        if (str[i] == '&') {
            size_t consumed = decode_entity(buf, str + i, len - i);
            if (consumed > 0) {
                i += consumed;
                continue;
            }
        }
        /* plain text run */
        size_t start = i;
        while (i < len && str[i] != '<' && str[i] != '&') i++;
        if (i > start)
            man_buf_append_roff_safe(buf, str + start, i - start);
    }
}

/* Return true if str starts with <dl> (optional whitespace). */
static bool is_dl_block(const char *str, size_t len) {
    while (len > 0 && (*str == ' ' || *str == '\n' || *str == '\t')) { str++; len--; }
    return len >= 4 && str[0] == '<' && str[1] == 'd' && str[2] == 'l' && (str[3] == '>' || (len > 4 && str[3] == ' '));
}

/* Find content of first <dt>...</dt>: set *start and *content_len (inner text only). Return true if found. */
static bool find_dt(const char *str, size_t len, size_t *start, size_t *content_len) {
    const char *p = str;
    size_t rem = len;
    while (rem >= 4 && (p[0] != '<' || p[1] != 'd' || p[2] != 't')) {
        p++; rem--;
    }
    if (rem < 4) return false;
    p += 3; rem -= 3; /* skip <dt */
    while (rem > 0 && *p != '>') { p++; rem--; }
    if (rem == 0) return false;
    p++; rem--; /* skip '>' */
    while (rem > 0 && (*p == ' ' || *p == '\n')) { p++; rem--; }
    const char *inner_start = p;
    while (rem >= 6) {
        if (p[0] == '<' && p[1] == '/' && p[2] == 'd' && p[3] == 't' && p[4] == '>') {
            *start = (size_t)(inner_start - str);
            *content_len = (size_t)(p - inner_start);
            return true;
        }
        p++; rem--;
    }
    return false;
}

static bool find_dd(const char *str, size_t len, size_t *start, size_t *content_len) {
    const char *p = str;
    size_t rem = len;
    while (rem >= 4 && (p[0] != '<' || p[1] != 'd' || p[2] != 'd')) {
        p++; rem--;
    }
    if (rem < 4) return false;
    p += 3; rem -= 3; /* skip <dd */
    while (rem > 0 && *p != '>') { p++; rem--; }
    if (rem == 0) return false;
    p++; rem--; /* skip '>' */
    while (rem > 0 && (*p == ' ' || *p == '\n')) { p++; rem--; }
    const char *inner_start = p;
    while (rem >= 6) {
        if (p[0] == '<' && p[1] == '/' && p[2] == 'd' && p[3] == 'd' && p[4] == '>') {
            *start = (size_t)(inner_start - str);
            *content_len = (size_t)(p - inner_start);
            return true;
        }
        p++; rem--;
    }
    return false;
}

/* If literal is a <dl><dt>...</dt><dd>...</dd></dl> block, emit roff and return true. */
static bool render_dl_html_block_as_roff(man_buffer *buf, const char *lit, size_t lit_len) {
    if (!lit || !is_dl_block(lit, lit_len)) return false;
    size_t dt_start, dt_len, dd_start, dd_len;
    if (!find_dt(lit, lit_len, &dt_start, &dt_len)) return false;
    if (!find_dd(lit, lit_len, &dd_start, &dd_len)) return false;
    man_buf_append_str(buf, "\n.TP\n");
    append_html_fragment_roff(buf, lit + dt_start, dt_len);
    man_buf_append_str(buf, "\n");
    append_html_fragment_roff(buf, lit + dd_start, dd_len);
    man_buf_append_str(buf, "\n");
    return true;
}

/* ------------------------------------------------------------------------- */
/* Helpers: get plain text from a node (for .TH title)                       */
/* ------------------------------------------------------------------------- */

static void collect_plain_text(cmark_node *node, man_buffer *out) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    if (t == CMARK_NODE_TEXT || t == CMARK_NODE_CODE) {
        const char *lit = cmark_node_get_literal(node);
        if (lit) man_buf_append(out, lit, strlen(lit));
        return;
    }
    for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur)) {
        collect_plain_text(cur, out);
    }
}

/* Caller frees. Returns first H1 heading text or NULL. */
static char *get_first_h1_text(cmark_node *document) {
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) return NULL;
    for (cmark_node *cur = cmark_node_first_child(document); cur; cur = cmark_node_next(cur)) {
        if (cmark_node_get_type(cur) == CMARK_NODE_HEADING && cmark_node_get_heading_level(cur) == 1) {
            man_buffer b;
            man_buf_init(&b);
            collect_plain_text(cur, &b);
            if (b.len > 0 && b.buf) {
                char *s = strdup(b.buf);
                free(b.buf);
                return s;
            }
            if (b.buf) free(b.buf);
            return NULL;
        }
    }
    return NULL;
}

/* Caller frees. Returns plain text of first paragraph after NAME heading, or NULL. */
static char *get_name_section_paragraph_text(cmark_node *document) {
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) return NULL;
    cmark_node *name_heading = NULL;
    for (cmark_node *cur = cmark_node_first_child(document); cur; cur = cmark_node_next(cur)) {
        if (cmark_node_get_type(cur) == CMARK_NODE_HEADING && cmark_node_get_heading_level(cur) == 1) {
            man_buffer b;
            man_buf_init(&b);
            collect_plain_text(cur, &b);
            if (b.len > 0 && b.buf) {
                if (strcmp(b.buf, "NAME") == 0) {
                    name_heading = cur;
                    free(b.buf);
                    break;
                }
                free(b.buf);
            }
        }
    }
    if (!name_heading) return NULL;
    for (cmark_node *cur = cmark_node_next(name_heading); cur; cur = cmark_node_next(cur)) {
        if (cmark_node_get_type(cur) == CMARK_NODE_PARAGRAPH) {
            man_buffer b;
            man_buf_init(&b);
            collect_plain_text(cur, &b);
            if (b.len > 0 && b.buf) {
                char *s = strdup(b.buf);
                free(b.buf);
                return s;
            }
            if (b.buf) free(b.buf);
            return NULL;
        }
        if (cmark_node_get_type(cur) == CMARK_NODE_HEADING) break;
    }
    return NULL;
}

/* Normalize s: trim, collapse runs of whitespace (including newlines) to single space. Modifies s. */
static void normalize_whitespace(char *s) {
    if (!s || !*s) return;
    char *r = s, *w = s;
    while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') r++;
    while (*r) {
        if (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r') {
            *w++ = ' ';
            do r++; while (*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r');
        } else {
            *w++ = *r++;
        }
    }
    while (w > s && (w[-1] == ' ' || w[-1] == '\t')) w--;
    *w = '\0';
}

/* ------------------------------------------------------------------------- */
/* Inline roff rendering                                                     */
/* ------------------------------------------------------------------------- */

static void render_inline_roff(man_buffer *buf, cmark_node *node);

static void render_inline_roff(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_TEXT: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
            break;
        }
        case CMARK_NODE_CODE: {
            /* Use roman (\fR) for code; \f[C] causes "cannot select font 'C'" on some groff devices */
            man_buf_append_str(buf, "\\fR");
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
            man_buf_append_str(buf, "\\f[]");
            break;
        }
        case CMARK_NODE_LINEBREAK:
            man_buf_append_str(buf, "\n.br\n");
            break;
        case CMARK_NODE_SOFTBREAK:
            man_buf_append_str(buf, " ");
            break;
        case CMARK_NODE_STRONG:
            man_buf_append_str(buf, "\\f[B]");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\\f[]");
            break;
        case CMARK_NODE_EMPH:
            man_buf_append_str(buf, "\\f[I]");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\\f[]");
            break;
        case CMARK_NODE_LINK: {
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            const char *url = cmark_node_get_url(node);
            if (url && url[0]) {
                man_buf_append_str(buf, " (");
                man_buf_append_roff_safe(buf, url, strlen(url));
                man_buf_append_str(buf, ")");
            }
            break;
        }
        case CMARK_NODE_HTML_INLINE:
            /* skip */
            break;
        default:
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Block roff rendering                                                     */
/* ------------------------------------------------------------------------- */

static void render_block_roff(man_buffer *buf, cmark_node *node);

/* True after we emitted a <dl> block so the next paragraph is definition continuation (no .PP). */
static bool roff_last_was_dl_dd = false;

static void render_block_roff(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_DOCUMENT:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        case CMARK_NODE_HEADING: {
            roff_last_was_dl_dd = false;
            int level = cmark_node_get_heading_level(node);
            man_buf_append_str(buf, level == 1 ? "\n.SH " : "\n.SS ");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_PARAGRAPH: {
            cmark_node *parent = cmark_node_parent(node);
            cmark_node_type pt = parent ? cmark_node_get_type(parent) : (cmark_node_type)0;
            bool in_item = (pt == CMARK_NODE_ITEM);
            bool in_def_data_first =
                (pt == (cmark_node_type)APEX_NODE_DEFINITION_DATA &&
                 !cmark_node_previous(node));
            bool in_def_term = (pt == (cmark_node_type)APEX_NODE_DEFINITION_TERM);
            bool continue_after_dd = roff_last_was_dl_dd;
            bool para_has_content = (cmark_node_first_child(node) != NULL);
            if (continue_after_dd) {
                if (para_has_content)
                    roff_last_was_dl_dd = false;
                /* else leave flag set so next block (e.g. code block) is treated as continuation */
            }
            if (!in_item && !in_def_data_first && !in_def_term && !continue_after_dd) {
                man_buf_append_str(buf, "\n.PP\n");
            }
            /* After a dd continuation, join with a space so we don't get a stray line break */
            if (continue_after_dd && para_has_content && buf->len > 0 && buf->buf[buf->len - 1] != '\n')
                man_buf_append_str(buf, " ");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_roff(buf, c);
            if (para_has_content)
                man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_LIST:
            roff_last_was_dl_dd = false;
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        case CMARK_NODE_ITEM: {
            cmark_node *list = cmark_node_parent(node);
            if (list && cmark_node_get_list_type(list) == CMARK_BULLET_LIST) {
                man_buf_append_str(buf, "\n.IP \\(bu 2\n");
            } else {
                int idx = cmark_node_get_item_index(node);
                char num[32];
                snprintf(num, sizeof(num), "\n.IP \"%d.\" 4\n", idx);
                man_buf_append_str(buf, num);
            }
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
        }
        case CMARK_NODE_CODE_BLOCK: {
            const char *lit = cmark_node_get_literal(node);
            cmark_node *parent = cmark_node_parent(node);
            cmark_node_type pt = parent ? cmark_node_get_type(parent) : (cmark_node_type)0;
            bool in_item = (pt == CMARK_NODE_ITEM);
            /* Continuation: after <dl> dd, or inside list item (indented line in source) - no .PP/.nf/.fi */
            if (roff_last_was_dl_dd || in_item) {
                if (roff_last_was_dl_dd)
                    roff_last_was_dl_dd = false;
                if (buf->len > 0 && buf->buf[buf->len - 1] != '\n')
                    man_buf_append_str(buf, " ");
                if (lit) man_buf_append_roff_safe(buf, lit, strlen(lit));
                man_buf_append_str(buf, "\n");
                break;
            }
            roff_last_was_dl_dd = false;
            /* \fR not \f[C] to avoid "cannot select font 'C'" on some groff devices */
            man_buf_append_str(buf, "\n.PP\n.nf\n\\fR\n");
            if (lit) {
                /* Collapse runs of newlines to one so indented "lists" don't get extra blank lines */
                size_t lit_len = strlen(lit);
                for (size_t i = 0; i < lit_len; ) {
                    size_t run = 0;
                    while (i + run < lit_len && lit[i + run] == '\n') run++;
                    if (run > 0) {
                        man_buf_append_str(buf, "\n");
                        i += run;
                    } else {
                        size_t text = 0;
                        while (i + text < lit_len && lit[i + text] != '\n') text++;
                        if (text > 0) {
                            man_buf_append_roff_safe(buf, lit + i, text);
                            i += text;
                        } else {
                            i++;
                        }
                    }
                }
            }
            man_buf_append_str(buf, "\n\\f[]\n.fi\n");
            break;
        }
        case CMARK_NODE_BLOCK_QUOTE:
            roff_last_was_dl_dd = false;
            man_buf_append_str(buf, "\n.RS\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            man_buf_append_str(buf, "\n.RE\n");
            break;
        case CMARK_NODE_THEMATIC_BREAK:
            roff_last_was_dl_dd = false;
            man_buf_append_str(buf, "\n.PP\n  *  *  *  *  *\n");
            break;
        case CMARK_NODE_HTML_BLOCK: {
            const char *lit = cmark_node_get_literal(node);
            size_t lit_len = lit ? strlen(lit) : 0;
            if (lit_len > 0 && render_dl_html_block_as_roff(buf, lit, lit_len))
                roff_last_was_dl_dd = true;
            break;
        }
        default:
            roff_last_was_dl_dd = false;
            /* Definition list (Apex extension): term = .TP + bold term, data = body */
            if (t == (cmark_node_type)APEX_NODE_DEFINITION_LIST) {
                for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                    render_block_roff(buf, cur);
                break;
            }
            if (t == (cmark_node_type)APEX_NODE_DEFINITION_TERM) {
                man_buf_append_str(buf, "\n.TP\n");
                /* Term can contain a paragraph or direct inlines; recurse so paragraph content is emitted without .PP */
                for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                    render_block_roff(buf, cur);
                break;
            }
            if (t == (cmark_node_type)APEX_NODE_DEFINITION_DATA) {
                for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                    render_block_roff(buf, cur);
                break;
            }
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_roff(buf, cur);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Public API: roff                                                          */
/* ------------------------------------------------------------------------- */

char *apex_cmark_to_man_roff(cmark_node *document, const apex_options *options)
{
    (void)options;
    if (!document) return strdup(".TH stub 1 \"\" \"\"\n");

    const char *title = "Document";
    char *first_h1 = get_first_h1_text(document);
    if (first_h1 && first_h1[0]) {
        title = first_h1;
    }

    const char *section = "1";
    const char *date = "1 January 1970";
    const char *source = "";

    man_buffer buf;
    man_buf_init(&buf);
    /* .TH title section date source - all args quoted if they contain spaces */
    man_buf_append_str(&buf, ".TH \"");
    man_buf_append_roff_safe(&buf, title, strlen(title));
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, section);
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, date);
    man_buf_append_str(&buf, "\" \"");
    man_buf_append_str(&buf, source);
    man_buf_append_str(&buf, "\"\n");

    render_block_roff(&buf, document);

    if (first_h1) free(first_h1);

    if (!buf.buf) return strdup(".TH stub 1 \"\" \"\"\n");
    return buf.buf;
}

/* ------------------------------------------------------------------------- */
/* Man-HTML: styled HTML man page                                            */
/* ------------------------------------------------------------------------- */

/* Append string with HTML entities escaped: & < > " '. Replace UTF-8 en-dash (U+2013)
 * with "--" so option names like –standalone render as --standalone. */
static void man_buf_append_html_escaped(man_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (i + 2 < len && c == 0xE2 && (unsigned char)str[i + 1] == 0x80 && (unsigned char)str[i + 2] == 0x93) {
            man_buf_append_str(b, "--");
            i += 2; /* skip the other two bytes of en-dash */
        } else if (c == '&') man_buf_append_str(b, "&amp;");
        else if (c == '<') man_buf_append_str(b, "&lt;");
        else if (c == '>') man_buf_append_str(b, "&gt;");
        else if (c == '"') man_buf_append_str(b, "&quot;");
        else if (c == '\'') man_buf_append_str(b, "&#39;");
        else man_buf_append(b, (const char *)&c, 1);
    }
}

static const char *man_html_css =
    "body { font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; max-width: 65em; margin: 1em auto; padding: 0 1em; line-height: 1.4; color: #333; }\n"
    "body.man-standalone { margin: 0; }\n"
    ".man-headline { font-size: 1.75rem; font-weight: bold; margin: 0.5em 0 0.75em; border-bottom: none; color: #a02172; }\n"
    ".man-nav { position: fixed; left: 0; top: 0; width: 14em; height: 100vh; overflow-y: auto; padding: 1.25em 1.5em; border-right: 1px solid #e0ddd6; background: #f5f4f0; font-size: 0.9rem; }\n"
    ".man-nav ul { list-style: none; padding: 0; margin: 0; }\n"
    ".man-nav li { margin: 0.35em 0; }\n"
    ".man-nav a { color: #444; text-decoration: none; display: block; padding: 0.2em 0; }\n"
    ".man-nav a:hover { color: #2376b1; background: rgba(0,0,0,0.03); }\n"
    ".man-main { margin-left: 16em; padding: 1.5em 2em; max-width: 65em; }\n"
    ".man-section { font-weight: bold; margin-top: 1em; margin-bottom: 0.25em; }\n"
    ".man-section h2, .man-section h3, .man-section h4 { font-size: 1em; margin: 0; color: #3f789b; }\n"
    "p { margin: 0.5em 0; }\n"
    "strong, .man-option { color: #a02172; font-weight: bold; }\n"
    "code, .man-option { font-family: monospace; background: #f5f5f5; padding: 0 0.2em; }\n"
    "pre { background: #f5f5f5; padding: 0.75em; overflow-x: auto; }\n"
    "ul, ol { margin: 0.5em 0; padding-left: 1.5em; }\n"
    "a { color: #2376b1; }\n";

static void render_inline_man_html(man_buffer *buf, cmark_node *node);
static void render_block_man_html(man_buffer *buf, cmark_node *node);

static void render_inline_man_html(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_TEXT: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) man_buf_append_html_escaped(buf, lit, strlen(lit));
            break;
        }
        case CMARK_NODE_CODE:
            man_buf_append_str(buf, "<code>");
            if (cmark_node_get_literal(node))
                man_buf_append_html_escaped(buf, cmark_node_get_literal(node), strlen(cmark_node_get_literal(node)));
            man_buf_append_str(buf, "</code>");
            break;
        case CMARK_NODE_LINEBREAK:
            man_buf_append_str(buf, "<br>\n");
            break;
        case CMARK_NODE_SOFTBREAK:
            man_buf_append_str(buf, " ");
            break;
        case CMARK_NODE_STRONG:
            man_buf_append_str(buf, "<strong>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</strong>");
            break;
        case CMARK_NODE_EMPH:
            man_buf_append_str(buf, "<em>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</em>");
            break;
        case CMARK_NODE_LINK: {
            const char *url = cmark_node_get_url(node);
            if (url && url[0]) {
                man_buf_append_str(buf, "<a href=\"");
                man_buf_append_html_escaped(buf, url, strlen(url));
                man_buf_append_str(buf, "\">");
            }
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            if (url && url[0]) man_buf_append_str(buf, "</a>");
            break;
        }
        case CMARK_NODE_HTML_INLINE:
            break;
        default:
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            break;
    }
}

static void section_id_from_heading(cmark_node *node, man_buffer *buf) {
    for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c)) {
        if (cmark_node_get_type(c) == CMARK_NODE_TEXT) {
            const char *lit = cmark_node_get_literal(c);
            if (lit) {
                for (const char *p = lit; *p; p++) {
                    unsigned char ch = (unsigned char)*p;
                    if (ch == ' ' || ch == '\t') man_buf_append(buf, "-", 1);
                    else if (isalnum(ch) || ch == '-') man_buf_append(buf, (const char *)&ch, 1);
                }
            }
            break;
        }
        section_id_from_heading(c, buf);
    }
}

#define MAN_HTML_MAX_SECTIONS 48
typedef struct { char id[72]; char label[72]; } man_section_entry;

static size_t collect_man_sections(cmark_node *document, man_section_entry *out) {
    size_t n = 0;
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) return 0;
    for (cmark_node *cur = cmark_node_first_child(document); cur && n < MAN_HTML_MAX_SECTIONS; cur = cmark_node_next(cur)) {
        if (cmark_node_get_type(cur) != CMARK_NODE_HEADING || cmark_node_get_heading_level(cur) != 1) continue;
        man_buffer id_buf, label_buf;
        man_buf_init(&id_buf);
        man_buf_init(&label_buf);
        section_id_from_heading(cur, &id_buf);
        collect_plain_text(cur, &label_buf);
        if (id_buf.len > 0 && id_buf.buf) {
            size_t id_len = id_buf.len < 71 ? id_buf.len : 71;
            memcpy(out[n].id, id_buf.buf, id_len);
            out[n].id[id_len] = '\0';
        } else {
            out[n].id[0] = '\0';
        }
        if (label_buf.len > 0 && label_buf.buf) {
            size_t lab_len = label_buf.len < 71 ? label_buf.len : 71;
            memcpy(out[n].label, label_buf.buf, lab_len);
            out[n].label[lab_len] = '\0';
        } else {
            out[n].label[0] = '\0';
        }
        if (id_buf.buf) free(id_buf.buf);
        if (label_buf.buf) free(label_buf.buf);
        n++;
    }
    return n;
}

static void render_block_man_html(man_buffer *buf, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);
    switch (t) {
        case CMARK_NODE_DOCUMENT:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            break;
        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            int h = level + 1; /* h2 for level 1, h3 for level 2 */
            if (h > 4) h = 4;
            char tag[8];
            snprintf(tag, sizeof(tag), "h%d", h);
            man_buf_append_str(buf, "\n<div class=\"man-section\"><");
            man_buf_append(buf, tag, strlen(tag));
            man_buf_append_str(buf, " id=\"");
            section_id_from_heading(node, buf);
            man_buf_append_str(buf, "\">");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            man_buf_append_str(buf, "</");
            man_buf_append(buf, tag, strlen(tag));
            man_buf_append_str(buf, "></div>\n");
            break;
        }
        case CMARK_NODE_PARAGRAPH: {
            cmark_node *parent = cmark_node_parent(node);
            bool in_item = (parent && cmark_node_get_type(parent) == CMARK_NODE_ITEM && !cmark_node_previous(node));
            if (!in_item) man_buf_append_str(buf, "<p>");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
                render_inline_man_html(buf, c);
            if (!in_item) man_buf_append_str(buf, "</p>\n");
            else man_buf_append_str(buf, "\n");
            break;
        }
        case CMARK_NODE_LIST: {
            cmark_list_type list_type = cmark_node_get_list_type(node);
            man_buf_append_str(buf, list_type == CMARK_ORDERED_LIST ? "\n<ol>\n" : "\n<ul>\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, list_type == CMARK_ORDERED_LIST ? "</ol>\n" : "</ul>\n");
            break;
        }
        case CMARK_NODE_ITEM:
            man_buf_append_str(buf, "<li>");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, "</li>\n");
            break;
        case CMARK_NODE_CODE_BLOCK:
            man_buf_append_str(buf, "\n<pre><code>");
            if (cmark_node_get_literal(node))
                man_buf_append_html_escaped(buf, cmark_node_get_literal(node), strlen(cmark_node_get_literal(node)));
            man_buf_append_str(buf, "</code></pre>\n");
            break;
        case CMARK_NODE_BLOCK_QUOTE:
            man_buf_append_str(buf, "\n<blockquote>\n");
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            man_buf_append_str(buf, "</blockquote>\n");
            break;
        case CMARK_NODE_THEMATIC_BREAK:
            man_buf_append_str(buf, "\n<hr>\n");
            break;
        case CMARK_NODE_HTML_BLOCK: {
            const char *lit = cmark_node_get_literal(node);
            size_t lit_len = lit ? strlen(lit) : 0;
            if (lit_len > 0 && is_dl_block(lit, lit_len)) {
                /* Append with en-dash (U+2013) replaced by -- so option names render correctly */
                for (size_t i = 0; i < lit_len; i++) {
                    if (i + 2 < lit_len && (unsigned char)lit[i] == 0xE2
                        && (unsigned char)lit[i + 1] == 0x80 && (unsigned char)lit[i + 2] == 0x93) {
                        man_buf_append_str(buf, "--");
                        i += 2;
                    } else {
                        man_buf_append(buf, lit + i, 1);
                    }
                }
            }
            break;
        }
        default:
            for (cmark_node *cur = cmark_node_first_child(node); cur; cur = cmark_node_next(cur))
                render_block_man_html(buf, cur);
            break;
    }
}

char *apex_cmark_to_man_html(cmark_node *document, const apex_options *options)
{
    if (!document) return strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");

    bool standalone = options && options->standalone;

    if (!standalone) {
        man_buffer buf;
        man_buf_init(&buf);
        render_block_man_html(&buf, document);
        if (!buf.buf) return strdup("");
        char *out = buf.buf;
        if (options && options->code_highlighter && options->code_highlighter[0]) {
            char *hl = apex_apply_syntax_highlighting(out,
                                                      options->code_highlighter,
                                                      false,
                                                      false,
                                                      false,
                                                      options->code_highlight_theme);
            if (hl) {
                free(out);
                out = hl;
            }
        }
        return out;
    }

    char *headline_cmd = NULL;
    char *headline_desc = NULL;
    char *name_line = get_name_section_paragraph_text(document);
    if (name_line && name_line[0]) {
        const char *sep = strstr(name_line, " - ");
        if (sep) {
            size_t cmd_len = (size_t)(sep - name_line);
            headline_cmd = (char *)malloc(cmd_len + 1);
            if (headline_cmd) {
                memcpy(headline_cmd, name_line, cmd_len);
                headline_cmd[cmd_len] = '\0';
                normalize_whitespace(headline_cmd);
            }
            headline_desc = strdup(sep + 3);
            if (headline_desc) normalize_whitespace(headline_desc);
        } else {
            headline_cmd = strdup(name_line);
            if (headline_cmd) normalize_whitespace(headline_cmd);
            headline_desc = strdup("manual page");
        }
        free(name_line);
    }
    if (!headline_cmd) headline_cmd = strdup("Document");
    if (!headline_desc) headline_desc = strdup("manual page");
    if (options->document_title && options->document_title[0]) {
        const char *dt = options->document_title;
        if (strchr(dt, '(') && strchr(dt, ')')) {
            free(headline_cmd);
            headline_cmd = strdup(dt);
        }
    }

    man_section_entry sections[MAN_HTML_MAX_SECTIONS];
    size_t n_sections = collect_man_sections(document, sections);

    man_buffer buf;
    man_buf_init(&buf);
    man_buf_append_str(&buf, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>");
    man_buf_append_html_escaped(&buf, headline_cmd, strlen(headline_cmd));
    man_buf_append_str(&buf, " — ");
    man_buf_append_html_escaped(&buf, headline_desc, strlen(headline_desc));
    man_buf_append_str(&buf, "</title>\n<style>\n");
    man_buf_append_str(&buf, man_html_css);
    man_buf_append_str(&buf, "</style>\n");
    if (options->stylesheet_paths && options->stylesheet_count > 0) {
        for (size_t i = 0; i < options->stylesheet_count && options->stylesheet_paths[i]; i++) {
            man_buf_append_str(&buf, "<link rel=\"stylesheet\" href=\"");
            man_buf_append_html_escaped(&buf, options->stylesheet_paths[i], strlen(options->stylesheet_paths[i]));
            man_buf_append_str(&buf, "\">\n");
        }
    }
    man_buf_append_str(&buf, "</head>\n<body class=\"man-standalone\">\n");

    if (n_sections > 0) {
        man_buf_append_str(&buf, "<nav class=\"man-nav\"><ul>\n");
        for (size_t i = 0; i < n_sections; i++) {
            if (sections[i].id[0]) {
                man_buf_append_str(&buf, "<li><a href=\"#");
                man_buf_append_html_escaped(&buf, sections[i].id, strlen(sections[i].id));
                man_buf_append_str(&buf, "\">");
                man_buf_append_html_escaped(&buf, sections[i].label, strlen(sections[i].label));
                man_buf_append_str(&buf, "</a></li>\n");
            }
        }
        man_buf_append_str(&buf, "</ul></nav>\n");
    }

    man_buf_append_str(&buf, "<main class=\"man-main\">\n<h1 class=\"man-headline\">");
    man_buf_append_html_escaped(&buf, headline_cmd, strlen(headline_cmd));
    man_buf_append_str(&buf, " — ");
    man_buf_append_html_escaped(&buf, headline_desc, strlen(headline_desc));
    man_buf_append_str(&buf, "</h1>\n");
    if (headline_cmd) free(headline_cmd);
    if (headline_desc) free(headline_desc);
    render_block_man_html(&buf, document);
    man_buf_append_str(&buf, "\n</main>\n</body>\n</html>");

    if (!buf.buf) return strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");

    char *out = buf.buf;
    if (options->code_highlighter && options->code_highlighter[0]) {
        char *hl = apex_apply_syntax_highlighting(out,
                                                  options->code_highlighter,
                                                  false,
                                                  false,
                                                  false,
                                                  options->code_highlight_theme);
        if (hl) {
            free(out);
            out = hl;
        }
    }
    return out;
}
