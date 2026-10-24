/*
 * RTF (Rich Text Format) output.
 * Serializes a cmark AST to RTF suitable for files, clipboard tooling, and
 * NSAttributedString(documentType: .rtf).
 */

#include "apex/ast_rtf.h"
#include "apex/apex.h"
#include "table.h"
#include "strikethrough.h"
#include "cmark-gfm-core-extensions.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* Buffer                                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} rtf_buffer;

typedef struct {
    rtf_buffer *buf;
    const apex_options *options;
    int list_depth;
    int quote_depth;
    int footnote_index;
    /* Collected footnote definition nodes for endnotes fallback */
    cmark_node **footnote_defs;
    size_t footnote_def_count;
    size_t footnote_def_cap;
} rtf_ctx;

static void rtf_buf_init(rtf_buffer *b) {
    b->buf = NULL;
    b->len = 0;
    b->capacity = 0;
}

static void rtf_buf_append(rtf_buffer *b, const char *str, size_t n) {
    if (!str || n == 0) return;
    if (b->len + n + 1 > b->capacity) {
        size_t new_cap = b->capacity ? b->capacity * 2 : 1024;
        while (new_cap < b->len + n + 1) new_cap *= 2;
        char *nb = (char *)realloc(b->buf, new_cap);
        if (!nb) return;
        b->buf = nb;
        b->capacity = new_cap;
    }
    memcpy(b->buf + b->len, str, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void rtf_buf_append_str(rtf_buffer *b, const char *str) {
    if (str) rtf_buf_append(b, str, strlen(str));
}

static void rtf_buf_append_char(rtf_buffer *b, char c) {
    rtf_buf_append(b, &c, 1);
}

static void rtf_buf_printf(rtf_buffer *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) {
        rtf_buf_append(b, tmp, (size_t)n);
        return;
    }
    char *big = (char *)malloc((size_t)n + 1);
    if (!big) return;
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    rtf_buf_append(b, big, (size_t)n);
    free(big);
}

/* ------------------------------------------------------------------------- */
/* Escape / Unicode                                                          */
/* ------------------------------------------------------------------------- */

static void rtf_append_unicode(rtf_buffer *b, unsigned int cp) {
    if (cp == 0) return;
    if (cp <= 0x7F) {
        char c = (char)cp;
        if (c == '\\' || c == '{' || c == '}') {
            rtf_buf_append_char(b, '\\');
        }
        if (c == '\n') {
            rtf_buf_append_str(b, "\\line ");
            return;
        }
        if (c == '\r') return;
        rtf_buf_append_char(b, c);
        return;
    }
    /* RTF \uN uses signed 16-bit; emit surrogates for > U+FFFF */
    if (cp <= 0xFFFF) {
        int16_t sn = (int16_t)(uint16_t)cp;
        rtf_buf_printf(b, "\\u%d?", (int)sn);
        return;
    }
    cp -= 0x10000;
    unsigned int hi = 0xD800 + (cp >> 10);
    unsigned int lo = 0xDC00 + (cp & 0x3FF);
    rtf_buf_printf(b, "\\u%d?", (int)(int16_t)(uint16_t)hi);
    rtf_buf_printf(b, "\\u%d?", (int)(int16_t)(uint16_t)lo);
}

static void rtf_append_utf8(rtf_buffer *b, const char *str, size_t len) {
    if (!str || len == 0) return;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            rtf_append_unicode(b, c);
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            unsigned int cp = ((c & 0x1F) << 6) | ((unsigned char)str[i + 1] & 0x3F);
            rtf_append_unicode(b, cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            unsigned int cp = ((c & 0x0F) << 12) |
                              (((unsigned char)str[i + 1] & 0x3F) << 6) |
                              ((unsigned char)str[i + 2] & 0x3F);
            rtf_append_unicode(b, cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            unsigned int cp = ((c & 0x07) << 18) |
                              (((unsigned char)str[i + 1] & 0x3F) << 12) |
                              (((unsigned char)str[i + 2] & 0x3F) << 6) |
                              ((unsigned char)str[i + 3] & 0x3F);
            rtf_append_unicode(b, cp);
            i += 4;
        } else {
            i++;
        }
    }
}

static void rtf_append_utf8_str(rtf_buffer *b, const char *str) {
    if (str) rtf_append_utf8(b, str, strlen(str));
}

/* Strip HTML tags; decode a few entities; emit text. */
static void rtf_append_html_stripped(rtf_buffer *b, const char *html, size_t len) {
    if (!html || len == 0) return;
    size_t i = 0;
    bool in_tag = false;
    rtf_buffer text;
    rtf_buf_init(&text);
    while (i < len) {
        char c = html[i];
        if (in_tag) {
            if (c == '>') in_tag = false;
            i++;
            continue;
        }
        if (c == '<') {
            /* Insert space/line for block-ish tags */
            if (i + 2 < len) {
                if ((html[i + 1] == 'b' && html[i + 2] == 'r') ||
                    (html[i + 1] == 'p' && (html[i + 2] == '>' || html[i + 2] == ' ')) ||
                    (html[i + 1] == '/' && html[i + 2] == 'p')) {
                    rtf_buf_append_str(&text, "\n");
                }
            }
            in_tag = true;
            i++;
            continue;
        }
        if (c == '&') {
            if (i + 3 < len && strncmp(html + i, "&lt;", 4) == 0) {
                rtf_buf_append_char(&text, '<'); i += 4; continue;
            }
            if (i + 3 < len && strncmp(html + i, "&gt;", 4) == 0) {
                rtf_buf_append_char(&text, '>'); i += 4; continue;
            }
            if (i + 4 < len && strncmp(html + i, "&amp;", 5) == 0) {
                rtf_buf_append_char(&text, '&'); i += 5; continue;
            }
            if (i + 5 < len && strncmp(html + i, "&quot;", 6) == 0) {
                rtf_buf_append_char(&text, '"'); i += 6; continue;
            }
            if (i + 5 < len && strncmp(html + i, "&nbsp;", 6) == 0) {
                rtf_buf_append_char(&text, ' '); i += 6; continue;
            }
        }
        rtf_buf_append_char(&text, c);
        i++;
    }
    if (text.buf) {
        rtf_append_utf8(b, text.buf, text.len);
        free(text.buf);
    }
}

/* ------------------------------------------------------------------------- */
/* Document header                                                           */
/* ------------------------------------------------------------------------- */

static void rtf_write_header(rtf_buffer *b) {
    rtf_buf_append_str(b,
        "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\n"
        "{\\fonttbl"
        "{\\f0\\fswiss\\fcharset0 Helvetica;}"
        "{\\f1\\fmodern\\fcharset0 Courier New;}"
        "{\\f2\\froman\\fcharset0 Times New Roman;}"
        "}\n"
        "{\\colortbl;"
        "\\red0\\green0\\blue0;"
        "\\red0\\green0\\blue255;"
        "\\red128\\green128\\blue128;"
        "\\red245\\green245\\blue245;"
        "\\red0\\green100\\blue0;"
        "\\red180\\green0\\blue0;"
        "}\n"
        "\\viewkind4\\uc1\\pard\\f0\\fs24\n");
}

/* Half-points for heading levels 1-6 */
static int heading_fs(int level) {
    static const int sizes[] = {0, 36, 32, 28, 26, 24, 22};
    if (level < 1) level = 1;
    if (level > 6) level = 6;
    return sizes[level];
}

static int indent_twips(int depth) {
    if (depth < 0) depth = 0;
    if (depth > 8) depth = 8;
    return depth * 360;
}

/* ------------------------------------------------------------------------- */
/* Forward decls                                                             */
/* ------------------------------------------------------------------------- */

static void render_inline(rtf_ctx *ctx, cmark_node *node);
static void render_block(rtf_ctx *ctx, cmark_node *node);
static void render_children_inline(rtf_ctx *ctx, cmark_node *node);
static void render_children_block(rtf_ctx *ctx, cmark_node *node);

static void render_children_inline(rtf_ctx *ctx, cmark_node *node) {
    for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
        render_inline(ctx, c);
}

static void render_children_block(rtf_ctx *ctx, cmark_node *node) {
    for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c))
        render_block(ctx, c);
}

/* ------------------------------------------------------------------------- */
/* Images                                                                    */
/* ------------------------------------------------------------------------- */

static int hex_digit(unsigned char v) {
    return v < 10 ? ('0' + v) : ('a' + (v - 10));
}

/* Local PNG/JPEG only. Remote URLs are never fetched (same policy as HTML embed).
 * Unlike HTML, RTF always attempts embed for local files — no embed_images flag. */
static bool rtf_try_embed_image(rtf_ctx *ctx, const char *url) {
    if (!url || !*url) return false;
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0 ||
        strncmp(url, "data:", 5) == 0 || strncmp(url, "//", 2) == 0) {
        return false;
    }

    const char *path = url;
    char resolved[4096];
    if (ctx->options && ctx->options->base_directory && url[0] != '/') {
        snprintf(resolved, sizeof(resolved), "%s/%s", ctx->options->base_directory, url);
        path = resolved;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return false; }
    long sz = ftell(fp);
    if (sz <= 0 || sz > 8 * 1024 * 1024) { fclose(fp); return false; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return false; }

    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    if (!data) { fclose(fp); return false; }
    if (fread(data, 1, (size_t)sz, fp) != (size_t)sz) {
        free(data);
        fclose(fp);
        return false;
    }
    fclose(fp);

    const char *blip = NULL;
    if (sz >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        blip = "pngblip";
    else if (sz >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        blip = "jpegblip";
    else {
        free(data);
        return false;
    }

    rtf_buf_printf(ctx->buf, "{\\pict\\%s\\picwgoal1440\\pichgoal1440 ", blip);
    for (long i = 0; i < sz; i++) {
        char hex[3];
        hex[0] = (char)hex_digit(data[i] >> 4);
        hex[1] = (char)hex_digit(data[i] & 0x0F);
        hex[2] = '\0';
        rtf_buf_append(ctx->buf, hex, 2);
        if ((i & 31) == 31) rtf_buf_append_char(ctx->buf, '\n');
    }
    rtf_buf_append_str(ctx->buf, "}");
    free(data);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Inlines                                                                   */
/* ------------------------------------------------------------------------- */

static void rtf_append_hyperlink_url(rtf_buffer *b, const char *url) {
    for (const char *p = url; *p; p++) {
        if (*p == '"' || *p == '\\') rtf_buf_append_char(b, '\\');
        if ((unsigned char)*p < 0x80)
            rtf_buf_append_char(b, *p);
        else
            rtf_buf_append_char(b, *p);
    }
}

static bool rtf_image_has_alt(cmark_node *node) {
    for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c)) {
        cmark_node_type ct = cmark_node_get_type(c);
        if (ct == CMARK_NODE_TEXT) {
            const char *lit = cmark_node_get_literal(c);
            if (lit) {
                while (*lit == ' ' || *lit == '\t') lit++;
                if (*lit) return true;
            }
        } else if (ct != CMARK_NODE_SOFTBREAK && ct != CMARK_NODE_LINEBREAK) {
            return true;
        }
    }
    return false;
}

static void rtf_write_hyperlink_open(rtf_buffer *b, const char *url) {
    rtf_buf_append_str(b, "{\\field{\\*\\fldinst{HYPERLINK \"");
    rtf_append_hyperlink_url(b, url);
    rtf_buf_append_str(b, "\"}}{\\fldrslt{\\ul\\cf2 ");
}

static void rtf_write_hyperlink_close(rtf_buffer *b) {
    rtf_buf_append_str(b, "\\ulnone\\cf0 }}}");
}

static void render_link(rtf_ctx *ctx, cmark_node *node, bool is_image) {
    const char *url = cmark_node_get_url(node);
    if (is_image) {
        if (url && rtf_try_embed_image(ctx, url)) return;

        /* Fallback: linked alt text (or "image"); never dump the raw URL inline. */
        const char *title = cmark_node_get_title(node);
        bool has_alt = rtf_image_has_alt(node);
        bool has_title = title && *title;

        if (url && *url) {
            rtf_write_hyperlink_open(ctx->buf, url);
            if (has_alt)
                render_children_inline(ctx, node);
            else if (has_title)
                rtf_append_utf8_str(ctx->buf, title);
            else
                rtf_buf_append_str(ctx->buf, "image");
            rtf_write_hyperlink_close(ctx->buf);
        } else if (has_alt) {
            render_children_inline(ctx, node);
        } else if (has_title) {
            rtf_append_utf8_str(ctx->buf, title);
        }
        return;
    }

    if (!url || !*url) {
        render_children_inline(ctx, node);
        return;
    }

    rtf_write_hyperlink_open(ctx->buf, url);
    render_children_inline(ctx, node);
    rtf_write_hyperlink_close(ctx->buf);
}

static void render_inline(rtf_ctx *ctx, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);

    if (t == CMARK_NODE_STRIKETHROUGH) {
        rtf_buf_append_str(ctx->buf, "{\\strike ");
        render_children_inline(ctx, node);
        rtf_buf_append_str(ctx->buf, "}");
        return;
    }

    switch (t) {
        case CMARK_NODE_TEXT: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) rtf_append_utf8_str(ctx->buf, lit);
            break;
        }
        case CMARK_NODE_CODE: {
            const char *lit = cmark_node_get_literal(node);
            rtf_buf_append_str(ctx->buf, "{\\f1\\fs20 ");
            if (lit) rtf_append_utf8_str(ctx->buf, lit);
            rtf_buf_append_str(ctx->buf, "\\f0\\fs24}");
            break;
        }
        case CMARK_NODE_SOFTBREAK:
            rtf_buf_append_str(ctx->buf, " ");
            break;
        case CMARK_NODE_LINEBREAK:
            rtf_buf_append_str(ctx->buf, "\\line ");
            break;
        case CMARK_NODE_EMPH:
            rtf_buf_append_str(ctx->buf, "{\\i ");
            render_children_inline(ctx, node);
            rtf_buf_append_str(ctx->buf, "}");
            break;
        case CMARK_NODE_STRONG:
            rtf_buf_append_str(ctx->buf, "{\\b ");
            render_children_inline(ctx, node);
            rtf_buf_append_str(ctx->buf, "}");
            break;
        case CMARK_NODE_LINK:
            render_link(ctx, node, false);
            break;
        case CMARK_NODE_IMAGE:
            render_link(ctx, node, true);
            break;
        case CMARK_NODE_HTML_INLINE: {
            const char *lit = cmark_node_get_literal(node);
            if (!lit) break;
            /* Map common tags */
            if (strncmp(lit, "<sup>", 5) == 0 || strncmp(lit, "<sup ", 5) == 0) {
                rtf_buf_append_str(ctx->buf, "{\\super ");
            } else if (strncmp(lit, "</sup>", 6) == 0) {
                rtf_buf_append_str(ctx->buf, "}");
            } else if (strncmp(lit, "<sub>", 5) == 0 || strncmp(lit, "<sub ", 5) == 0) {
                rtf_buf_append_str(ctx->buf, "{\\sub ");
            } else if (strncmp(lit, "</sub>", 6) == 0) {
                rtf_buf_append_str(ctx->buf, "}");
            } else if (strncmp(lit, "<mark>", 6) == 0 || strstr(lit, "class=\"highlight\"") ||
                       strstr(lit, "class='highlight'")) {
                rtf_buf_append_str(ctx->buf, "{\\highlight4 ");
            } else if (strncmp(lit, "</mark>", 7) == 0) {
                rtf_buf_append_str(ctx->buf, "}");
            } else if (lit[0] == '<' && lit[1] != '/') {
                /* opening unknown tag — ignore */
            } else if (lit[0] == '<' && lit[1] == '/') {
                /* closing unknown — ignore */
            } else {
                rtf_append_html_stripped(ctx->buf, lit, strlen(lit));
            }
            break;
        }
        case CMARK_NODE_FOOTNOTE_REFERENCE: {
            const char *lit = cmark_node_get_literal(node);
            ctx->footnote_index++;
            rtf_buf_append_str(ctx->buf, "{\\super\\cf2 ");
            if (lit && *lit)
                rtf_append_utf8_str(ctx->buf, lit);
            else
                rtf_buf_printf(ctx->buf, "%d", ctx->footnote_index);
            rtf_buf_append_str(ctx->buf, "}");
            /* Inline footnote content if definition follows as sibling elsewhere —
             * content is emitted in endnotes pass. */
            break;
        }
        default:
            render_children_inline(ctx, node);
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Tables                                                                    */
/* ------------------------------------------------------------------------- */

static void render_table(rtf_ctx *ctx, cmark_node *table) {
    uint16_t cols = cmark_gfm_extensions_get_table_columns(table);
    if (cols == 0) {
        /* Count from first row */
        for (cmark_node *row = cmark_node_first_child(table); row; row = cmark_node_next(row)) {
            if (cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) continue;
            for (cmark_node *cell = cmark_node_first_child(row); cell; cell = cmark_node_next(cell)) {
                if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) cols++;
            }
            break;
        }
    }
    if (cols == 0) cols = 1;

    int cell_width = 9000 / (int)cols;
    if (cell_width < 720) cell_width = 720;

    bool first_row = true;
    for (cmark_node *row = cmark_node_first_child(table); row; row = cmark_node_next(row)) {
        if (cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) continue;

        rtf_buf_append_str(ctx->buf, "\\trowd\\trgaph108\\trleft0");
        for (int i = 1; i <= (int)cols; i++) {
            rtf_buf_printf(ctx->buf, "\\clbrdrt\\brdrs\\brdrw10\\clbrdrl\\brdrs\\brdrw10"
                                     "\\clbrdrb\\brdrs\\brdrw10\\clbrdrr\\brdrs\\brdrw10"
                                     "\\cellx%d", i * cell_width);
        }
        rtf_buf_append_str(ctx->buf, "\n");

        int col = 0;
        for (cmark_node *cell = cmark_node_first_child(row); cell; cell = cmark_node_next(cell)) {
            if (cmark_node_get_type(cell) != CMARK_NODE_TABLE_CELL) continue;
            col++;
            if (first_row)
                rtf_buf_append_str(ctx->buf, "\\pard\\intbl\\b ");
            else
                rtf_buf_append_str(ctx->buf, "\\pard\\intbl ");
            /* Cell may contain block children (paragraphs) or inlines */
            for (cmark_node *c = cmark_node_first_child(cell); c; c = cmark_node_next(c)) {
                cmark_node_type ct = cmark_node_get_type(c);
                if (ct == CMARK_NODE_PARAGRAPH)
                    render_children_inline(ctx, c);
                else if ((ct & CMARK_NODE_TYPE_INLINE) == CMARK_NODE_TYPE_INLINE)
                    render_inline(ctx, c);
                else
                    render_block(ctx, c);
            }
            rtf_buf_append_str(ctx->buf, "\\cell\n");
        }
        while (col < (int)cols) {
            rtf_buf_append_str(ctx->buf, "\\pard\\intbl\\cell\n");
            col++;
        }
        rtf_buf_append_str(ctx->buf, "\\row\n");
        first_row = false;
    }
    rtf_buf_append_str(ctx->buf, "\\pard\\f0\\fs24\n");
}

/* ------------------------------------------------------------------------- */
/* HTML blocks: callouts, dl, generic strip                                  */
/* ------------------------------------------------------------------------- */

static bool looks_like_callout(const char *html) {
    return html && (strstr(html, "class=\"callout") || strstr(html, "class='callout") ||
                    strstr(html, "callout-") || strstr(html, "div class=\"note") ||
                    strstr(html, "markdown-alert"));
}

static void render_html_block(rtf_ctx *ctx, const char *lit, size_t len) {
    if (!lit || len == 0) return;

    /* Definition list as bold term + indented body */
    if (len >= 4 && lit[0] == '<' && lit[1] == 'd' && lit[2] == 'l') {
        const char *p = lit;
        const char *end = lit + len;
        while (p < end) {
            const char *dt = strstr(p, "<dt");
            if (!dt || dt >= end) break;
            const char *dt_gt = strchr(dt, '>');
            if (!dt_gt || dt_gt >= end) break;
            dt_gt++;
            const char *dt_end = strstr(dt_gt, "</dt>");
            if (!dt_end || dt_end >= end) break;
            rtf_buf_append_str(ctx->buf, "\\pard\\b ");
            rtf_append_html_stripped(ctx->buf, dt_gt, (size_t)(dt_end - dt_gt));
            rtf_buf_append_str(ctx->buf, "\\b0\\par\n");

            const char *dd = strstr(dt_end, "<dd");
            if (dd && dd < end) {
                const char *dd_gt = strchr(dd, '>');
                const char *dd_end = dd_gt ? strstr(dd_gt, "</dd>") : NULL;
                if (dd_gt && dd_end && dd_end < end) {
                    dd_gt++;
                    rtf_buf_append_str(ctx->buf, "\\pard\\li360 ");
                    rtf_append_html_stripped(ctx->buf, dd_gt, (size_t)(dd_end - dd_gt));
                    rtf_buf_append_str(ctx->buf, "\\par\n");
                    p = dd_end + 5;
                    continue;
                }
            }
            p = dt_end + 5;
        }
        rtf_buf_append_str(ctx->buf, "\\pard\\f0\\fs24\n");
        return;
    }

    if (looks_like_callout(lit)) {
        rtf_buf_append_str(ctx->buf, "\\pard\\li360\\ri360\\box\\brdrs\\brdrw15\\brdrcf3\\cbpat4\\f0\\fs22\\i ");
        rtf_append_html_stripped(ctx->buf, lit, len);
        rtf_buf_append_str(ctx->buf, "\\i0\\par\\pard\\f0\\fs24\n");
        return;
    }

    /* Generic HTML: strip tags to text paragraphs */
    rtf_buf_append_str(ctx->buf, "\\pard ");
    rtf_append_html_stripped(ctx->buf, lit, len);
    rtf_buf_append_str(ctx->buf, "\\par\n");
}

/* ------------------------------------------------------------------------- */
/* Blocks                                                                    */
/* ------------------------------------------------------------------------- */

static void render_list_item(rtf_ctx *ctx, cmark_node *item, int index, bool ordered) {
    int li = indent_twips(ctx->list_depth);
    int fi = -360;
    rtf_buf_printf(ctx->buf, "\\pard\\li%d\\fi%d ", li, fi);

    bool checked = false;
    bool is_task = false;
    {
        const char *ts = cmark_node_get_type_string(item);
        if (ts && strcmp(ts, "tasklist") == 0) {
            is_task = true;
            checked = cmark_gfm_extensions_get_tasklist_item_checked(item);
        }
    }

    if (is_task) {
        rtf_append_utf8_str(ctx->buf, checked ? "\\u9745? " : "\\u9744? ");
    } else if (ordered) {
        rtf_buf_printf(ctx->buf, "%d. ", index);
    } else {
        rtf_buf_append_str(ctx->buf, "\\bullet  ");
    }

    /* First paragraph's inlines on same line; subsequent blocks as paras */
    bool first = true;
    for (cmark_node *c = cmark_node_first_child(item); c; c = cmark_node_next(c)) {
        if (cmark_node_get_type(c) == CMARK_NODE_PARAGRAPH) {
            if (!first) {
                rtf_buf_printf(ctx->buf, "\\par\\pard\\li%d ", li);
            }
            render_children_inline(ctx, c);
            first = false;
        } else if (cmark_node_get_type(c) == CMARK_NODE_LIST) {
            rtf_buf_append_str(ctx->buf, "\\par\n");
            render_block(ctx, c);
            first = false;
        } else {
            if (!first) rtf_buf_append_str(ctx->buf, "\\par\n");
            render_block(ctx, c);
            first = false;
        }
    }
    rtf_buf_append_str(ctx->buf, "\\par\n");
}

static void render_block(rtf_ctx *ctx, cmark_node *node) {
    if (!node) return;
    cmark_node_type t = cmark_node_get_type(node);

    if (t == CMARK_NODE_TABLE) {
        render_table(ctx, node);
        return;
    }

    switch (t) {
        case CMARK_NODE_DOCUMENT:
            render_children_block(ctx, node);
            break;

        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            int fs = heading_fs(level);
            rtf_buf_printf(ctx->buf, "\\pard\\sb240\\sa120\\b\\f0\\fs%d ", fs);
            render_children_inline(ctx, node);
            rtf_buf_append_str(ctx->buf, "\\b0\\fs24\\par\n");
            break;
        }

        case CMARK_NODE_PARAGRAPH: {
            cmark_node *parent = cmark_node_parent(node);
            if (parent && cmark_node_get_type(parent) == CMARK_NODE_ITEM)
                break; /* handled by list item */
            int li = indent_twips(ctx->quote_depth);
            if (li > 0)
                rtf_buf_printf(ctx->buf, "\\pard\\li%d\\sb60\\sa60 ", li);
            else
                rtf_buf_append_str(ctx->buf, "\\pard\\sb60\\sa60 ");
            render_children_inline(ctx, node);
            rtf_buf_append_str(ctx->buf, "\\par\n");
            break;
        }

        case CMARK_NODE_BLOCK_QUOTE: {
            ctx->quote_depth++;
            rtf_buf_append_str(ctx->buf, "\\pard\\li360\\ri360\\i\\cf3 ");
            for (cmark_node *c = cmark_node_first_child(node); c; c = cmark_node_next(c)) {
                if (cmark_node_get_type(c) == CMARK_NODE_PARAGRAPH) {
                    render_children_inline(ctx, c);
                    rtf_buf_append_str(ctx->buf, "\\par\n");
                } else {
                    render_block(ctx, c);
                }
            }
            rtf_buf_append_str(ctx->buf, "\\i0\\cf0\\pard\n");
            ctx->quote_depth--;
            break;
        }

        case CMARK_NODE_LIST: {
            bool ordered = (cmark_node_get_list_type(node) == CMARK_ORDERED_LIST);
            int start = (int)cmark_node_get_list_start(node);
            if (start < 1) start = 1;
            ctx->list_depth++;
            int idx = start;
            for (cmark_node *item = cmark_node_first_child(node); item; item = cmark_node_next(item)) {
                if (cmark_node_get_type(item) != CMARK_NODE_ITEM) continue;
                int item_idx = cmark_node_get_item_index(item);
                if (item_idx > 0) idx = item_idx;
                render_list_item(ctx, item, idx, ordered);
                idx++;
            }
            ctx->list_depth--;
            rtf_buf_append_str(ctx->buf, "\\pard\\f0\\fs24\n");
            break;
        }

        case CMARK_NODE_ITEM:
            /* Handled by list */
            break;

        case CMARK_NODE_CODE_BLOCK: {
            const char *lit = cmark_node_get_literal(node);
            rtf_buf_append_str(ctx->buf, "\\pard\\li360\\cbpat4\\f1\\fs18 ");
            if (lit) {
                /* Emit line by line with \\par */
                const char *p = lit;
                while (*p) {
                    const char *nl = strchr(p, '\n');
                    if (!nl) {
                        rtf_append_utf8_str(ctx->buf, p);
                        break;
                    }
                    rtf_append_utf8(ctx->buf, p, (size_t)(nl - p));
                    rtf_buf_append_str(ctx->buf, "\\par\n");
                    p = nl + 1;
                }
            }
            rtf_buf_append_str(ctx->buf, "\\par\\pard\\f0\\fs24\\cbpat0\n");
            break;
        }

        case CMARK_NODE_THEMATIC_BREAK:
            rtf_buf_append_str(ctx->buf, "\\pard\\qc\\sb200\\sa200 ");
            rtf_append_utf8_str(ctx->buf, "────────────");
            rtf_buf_append_str(ctx->buf, "\\par\\pard\n");
            break;

        case CMARK_NODE_HTML_BLOCK: {
            const char *lit = cmark_node_get_literal(node);
            if (lit) render_html_block(ctx, lit, strlen(lit));
            break;
        }

        case CMARK_NODE_FOOTNOTE_DEFINITION: {
            /* Defer to endnotes — collect pointer */
            if (ctx->footnote_def_count + 1 > ctx->footnote_def_cap) {
                size_t nc = ctx->footnote_def_cap ? ctx->footnote_def_cap * 2 : 8;
                cmark_node **n = (cmark_node **)realloc(ctx->footnote_defs, nc * sizeof(cmark_node *));
                if (!n) break;
                ctx->footnote_defs = n;
                ctx->footnote_def_cap = nc;
            }
            ctx->footnote_defs[ctx->footnote_def_count++] = node;
            break;
        }

        default:
            render_children_block(ctx, node);
            break;
    }
}

static void render_endnotes(rtf_ctx *ctx) {
    if (ctx->footnote_def_count == 0) return;
    rtf_buf_append_str(ctx->buf, "\\pard\\sb400\\sa200\\b\\fs28 Footnotes\\b0\\fs24\\par\n");
    for (size_t i = 0; i < ctx->footnote_def_count; i++) {
        cmark_node *def = ctx->footnote_defs[i];
        const char *lab = cmark_node_get_literal(def);
        rtf_buf_append_str(ctx->buf, "\\pard\\li360\\fi-360\\super ");
        if (lab && *lab)
            rtf_append_utf8_str(ctx->buf, lab);
        else
            rtf_buf_printf(ctx->buf, "%zu", i + 1);
        rtf_buf_append_str(ctx->buf, "\\nosupersub  ");
        for (cmark_node *c = cmark_node_first_child(def); c; c = cmark_node_next(c)) {
            if (cmark_node_get_type(c) == CMARK_NODE_PARAGRAPH)
                render_children_inline(ctx, c);
            else
                render_block(ctx, c);
        }
        rtf_buf_append_str(ctx->buf, "\\par\n");
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

char *apex_cmark_to_rtf(cmark_node *document, const struct apex_options *options) {
    if (!document) return NULL;

    rtf_buffer buf;
    rtf_buf_init(&buf);
    rtf_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.buf = &buf;
    ctx.options = options;

    rtf_write_header(&buf);
    render_block(&ctx, document);
    render_endnotes(&ctx);
    rtf_buf_append_str(&buf, "}\n");

    free(ctx.footnote_defs);

    if (!buf.buf) {
        char *empty = strdup("{\\rtf1\\ansi\\deff0\\pard\\par}\n");
        return empty;
    }
    return buf.buf;
}
