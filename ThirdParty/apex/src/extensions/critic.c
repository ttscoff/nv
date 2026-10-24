/**
 * Critic Markup Extension for Apex
 * Implementation
 */

#include "critic.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stddef.h>
#include <limits.h>

static int apex_critic_ptrdiff_to_int(ptrdiff_t v) {
    if (v <= 0) return 0;
    if (v > INT_MAX) return INT_MAX;
    return (int)v;
}

/**
 * Scan for Critic Markup patterns
 * Returns type and length if found
 */
typedef enum {
    CRITIC_NONE = 0,
    CRITIC_ADD,         /* {++text++} */
    CRITIC_DEL,         /* {--text--} */
    CRITIC_SUB,         /* {~~old~>new~~} */
    CRITIC_HIGHLIGHT,   /* {==text==} */
    CRITIC_COMMENT      /* {>>text<<} */
} critic_type_t;

static critic_type_t scan_critic_markup(const char *input, int len, int *consumed,
                                        const char **content, int *content_len,
                                        const char **old_text, int *old_len) {
    if (len < 6) return CRITIC_NONE;  /* Minimum: {++x++} */

    if (input[0] != '{') return CRITIC_NONE;

    /* Check type */
    critic_type_t type = CRITIC_NONE;
    const char *close_marker = NULL;

    if (input[1] == '+' && input[2] == '+') {
        type = CRITIC_ADD;
        close_marker = "++}";
    } else if (input[1] == '-' && input[2] == '-') {
        type = CRITIC_DEL;
        close_marker = "--}";
    } else if (input[1] == '~' && input[2] == '~') {
        type = CRITIC_SUB;
        close_marker = "~~}";
    } else if (input[1] == '=' && input[2] == '=') {
        type = CRITIC_HIGHLIGHT;
        close_marker = "==}";
    } else if (input[1] == '>' && input[2] == '>') {
        type = CRITIC_COMMENT;
        close_marker = "<<}";
    } else {
        return CRITIC_NONE;
    }

    *content = input + 3;  /* Skip opening marker */

    /* Find closing marker */
    const char *closer = strstr(*content, close_marker);
    if (!closer) return CRITIC_NONE;

    *content_len = apex_critic_ptrdiff_to_int(closer - *content);
    *consumed = apex_critic_ptrdiff_to_int(closer - input) + 3;  /* Include closing marker */

    /* For substitutions, split on ~> */
    if (type == CRITIC_SUB) {
        const char *sep = strstr(*content, "~>");
        if (sep && sep < closer) {
            *old_len = apex_critic_ptrdiff_to_int(sep - *content);
            *old_text = *content;
            *content = sep + 2;  /* Skip ~> */
            *content_len = apex_critic_ptrdiff_to_int(closer - *content);
        }
    }

    return type;
}

/**
 * Create HTML for critic markup based on mode
 */
static char *critic_to_html(critic_type_t type, const char *content, int content_len,
                            const char *old_text, int old_len, critic_mode_t mode) {
    char *html = NULL;
    size_t html_len = content_len + old_len + 200;
    html = malloc(html_len);
    if (!html) return NULL;

    switch (mode) {
        case CRITIC_ACCEPT:
            /* Show only additions and new text in substitutions */
            switch (type) {
                case CRITIC_ADD:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_SUB:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_HIGHLIGHT:
                    /* Show highlight text without markup */
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_DEL:
                case CRITIC_COMMENT:
                    html[0] = '\0';  /* Remove deletions and comments */
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;

        case CRITIC_REJECT:
            /* Show only original text */
            switch (type) {
                case CRITIC_SUB:
                    if (old_text && old_len > 0) {
                        snprintf(html, html_len, "%.*s", old_len, old_text);
                    } else {
                        html[0] = '\0';
                    }
                    break;
                case CRITIC_DEL:
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_HIGHLIGHT:
                    /* Show highlight text without markup */
                    snprintf(html, html_len, "%.*s", content_len, content);
                    break;
                case CRITIC_ADD:
                case CRITIC_COMMENT:
                    html[0] = '\0';  /* Remove additions and comments */
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;

        case CRITIC_MARKUP:
            /* Show markup with HTML classes */
            switch (type) {
                case CRITIC_ADD:
                    snprintf(html, html_len, "<ins class=\"critic\">%.*s</ins>", content_len, content);
                    break;
                case CRITIC_DEL:
                    snprintf(html, html_len, "<del class=\"critic\">%.*s</del>", content_len, content);
                    break;
                case CRITIC_SUB:
                    if (old_text && old_len > 0) {
                        snprintf(html, html_len,
                                "<del class=\"critic break\">%.*s</del><ins class=\"critic break\">%.*s</ins>",
                                old_len, old_text, content_len, content);
                    } else {
                        snprintf(html, html_len, "<ins class=\"critic\">%.*s</ins>", content_len, content);
                    }
                    break;
                case CRITIC_HIGHLIGHT:
                    snprintf(html, html_len, "<mark class=\"critic\">%.*s</mark>", content_len, content);
                    break;
                case CRITIC_COMMENT:
                    snprintf(html, html_len, "<span class=\"critic comment\">%.*s</span>", content_len, content);
                    break;
                default:
                    html[0] = '\0';
                    break;
            }
            break;
    }

    return html;
}

/**
 * Process Critic Markup in text nodes
 */
static void process_critic_in_text_node(cmark_node *node, critic_mode_t mode) {
    const char *literal = cmark_node_get_literal(node);
    if (!literal) return;

    /* Look for critic markup - scan through entire string looking for { */
    const char *search = literal;
    const char *start = NULL;

    while ((start = strchr(search, '{')) != NULL) {
        /* Check if it's valid critic markup */
        int consumed;
        const char *content;
        int content_len;
        const char *old_text = NULL;
        int old_len = 0;

        critic_type_t type = scan_critic_markup(start, (int)strlen(start), &consumed,
                                               &content, &content_len, &old_text, &old_len);

        if (type != CRITIC_NONE) {
            /* Found valid critic markup */
            size_t prefix_len = start - literal;
            const char *suffix = start + consumed;

            /* Generate HTML for the critic markup */
            char *html = critic_to_html(type, content, content_len, old_text, old_len, mode);
            if (!html) {
                search = start + 1;
                continue;
            }

            /* Create prefix text node if there's text before the markup */
            if (prefix_len > 0) {
                char *prefix = malloc(prefix_len + 1);
                if (prefix) {
                    memcpy(prefix, literal, prefix_len);
                    prefix[prefix_len] = '\0';
                    cmark_node_set_literal(node, prefix);
                    free(prefix);
                }
            }

            /* Create HTML inline node for the critic markup */
            cmark_node *html_node = cmark_node_new(CMARK_NODE_HTML_INLINE);
            cmark_node_set_literal(html_node, html);
            free(html);

            if (prefix_len > 0) {
                /* Insert HTML after the prefix text node */
                cmark_node_insert_after(node, html_node);
            } else {
                /* Replace the node entirely */
                cmark_node_insert_before(node, html_node);
                cmark_node_unlink(node);
                cmark_node_free(node);
                node = html_node;  /* Continue from the new node */
            }

            /* If there's suffix, create new text node and process it */
            if (*suffix) {
                cmark_node *suffix_node = cmark_node_new(CMARK_NODE_TEXT);
                cmark_node_set_literal(suffix_node, suffix);
                cmark_node_insert_after(html_node, suffix_node);

                /* Recursively process the suffix */
                process_critic_in_text_node(suffix_node, mode);
            }

            return;  /* Done processing this node */
        }

        /* Not critic markup, continue searching */
        search = start + 1;
    }
}

/**
 * Recursively process Critic Markup in AST
 */
void apex_process_critic_markup_in_tree(cmark_node *node, critic_mode_t mode) {
    if (!node) return;

    /* Process current node if it's text */
    if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
        process_critic_in_text_node(node, mode);
        return;  /* Don't recurse after modifying */
    }

    /* Recursively process children */
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        cmark_node *next = cmark_node_next(child);  /* Save next before potential modification */
        apex_process_critic_markup_in_tree(child, mode);
        child = next;
    }
}

static const char *critic_next_line(const char *p) {
    while (*p && *p != '\n' && *p != '\r') p++;
    if (*p == '\r' && p[1] == '\n') return p + 2;
    return *p ? p + 1 : p;
}

/* Length of a ``` or ~~~ fence run opening the line, or 0. */
static size_t critic_fence_run(const char *line, char *fence_char, const char **after_run) {
    const char *p = line;
    while (*p == ' ' || *p == '\t' || *p == '>') p++;
    if (*p != '`' && *p != '~') return 0;
    char c = *p;
    const char *run = p;
    while (*p == c) p++;
    if (p - run < 3) return 0;
    *fence_char = c;
    *after_run = p;
    return (size_t)(p - run);
}

/* Bytes at pos that are a fenced block or code span and must be copied
 * through untouched. Returns 0 when pos does not start code. */
static size_t critic_code_region_len(const char *base, const char *pos) {
    bool line_start = (pos == base || pos[-1] == '\n' || pos[-1] == '\r');
    if (line_start) {
        char fence_char;
        const char *after;
        size_t open_len = critic_fence_run(pos, &fence_char, &after);
        if (open_len) {
            bool valid = true;
            if (fence_char == '`') {
                for (const char *q = after; *q && *q != '\n' && *q != '\r'; q++) {
                    if (*q == '`') { valid = false; break; }
                }
            }
            if (valid) {
                const char *line = critic_next_line(pos);
                while (*line) {
                    char close_char;
                    const char *close_after;
                    size_t close_len = critic_fence_run(line, &close_char, &close_after);
                    if (close_len >= open_len && close_char == fence_char) {
                        while (*close_after == ' ' || *close_after == '\t') close_after++;
                        if (!*close_after || *close_after == '\n' || *close_after == '\r') {
                            return (size_t)(critic_next_line(line) - pos);
                        }
                    }
                    line = critic_next_line(line);
                }
                /* Unclosed fence: protect only the opening line */
                return (size_t)(critic_next_line(pos) - pos);
            }
        }
    }

    if (*pos != '`') return 0;

    size_t backslashes = 0;
    for (const char *b = pos; b > base && b[-1] == '\\'; b--) backslashes++;
    if (backslashes % 2) return 1;

    const char *p = pos;
    while (*p == '`') p++;
    size_t open_len = (size_t)(p - pos);

    /* Code spans may wrap lines but end at a blank line */
    while (*p) {
        if (*p == '\n' || *p == '\r') {
            const char *next = critic_next_line(p);
            const char *q = next;
            while (*q == ' ' || *q == '\t' || *q == '>') q++;
            if (!*q || *q == '\n' || *q == '\r') break;
            p = next;
            continue;
        }
        if (*p == '`') {
            const char *run = p;
            while (*p == '`') p++;
            if ((size_t)(p - run) == open_len) return (size_t)(p - pos);
            continue;
        }
        p++;
    }
    return open_len;
}

/* Indented code blocks follow the CommonMark rules: four spaces or a tab
 * at the current container, and they cannot interrupt a paragraph. List
 * continuations and paragraph lines stay eligible for critic markup. */

typedef enum {
    CRITIC_BOX_QUOTE,
    CRITIC_BOX_ITEM
} critic_box_kind;

typedef struct {
    critic_box_kind kind;
    int need;         /* ITEM: columns required to continue */
    bool has_content; /* ITEM has a child block */
} critic_box;

typedef struct {
    critic_box stack[32];
    int depth;
    bool in_paragraph;
    bool in_code;
} critic_blocks;

static int critic_col_add(int col, char c) {
    if (c == '\t') return col + (4 - (col % 4));
    return col + 1;
}

static bool critic_line_blank(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return *p == '\0' || *p == '\n' || *p == '\r';
}

static const char *critic_skip_indent(const char *p, int *col, int *indent) {
    int start = *col;
    while (*p == ' ' || *p == '\t') {
        *col = critic_col_add(*col, *p);
        p++;
    }
    *indent = *col - start;
    return p;
}

/* Consume exactly `columns` of indent. A tab may be only partly consumed. */
static const char *critic_advance_columns(const char *p, int *col, int columns) {
    int target = *col + columns;
    while (*p && *p != '\n' && *p != '\r' && *col < target) {
        if (*p == '\t') {
            int next = critic_col_add(*col, '\t');
            if (next > target) {
                *col = target;
                return p;
            }
            *col = next;
            p++;
        } else {
            *col += 1;
            p++;
        }
    }
    return p;
}

static bool critic_match_quote(const char *p, int col, const char **out, int *out_col) {
    int c = col;
    int indent = 0;
    const char *q = critic_skip_indent(p, &c, &indent);
    if (indent > 3 || *q != '>') return false;
    q++;
    c += 1;
    if (*q == ' ' || *q == '\t') {
        c = critic_col_add(c, *q);
        q++;
    }
    *out = q;
    *out_col = c;
    return true;
}

/* 1 = bullet, 2 = ordered. len is the marker without trailing space. */
static int critic_list_marker(const char *p, int *len, int *ordered_start) {
    if (*p == '-' || *p == '+' || *p == '*') {
        char n = p[1];
        if (n == ' ' || n == '\t' || n == '\n' || n == '\r' || n == '\0') {
            *len = 1;
            *ordered_start = 0;
            return 1;
        }
        return 0;
    }
    if (*p >= '1' && *p <= '9') {
        int i = 0;
        int num = 0;
        while (i < 9 && p[i] >= '0' && p[i] <= '9') {
            num = num * 10 + (p[i] - '0');
            i++;
        }
        if (i > 0 && (p[i] == '.' || p[i] == ')')) {
            char n = p[i + 1];
            if (n == ' ' || n == '\t' || n == '\n' || n == '\r' || n == '\0') {
                *len = i + 1;
                *ordered_start = num;
                return 2;
            }
        }
    }
    return 0;
}

static bool critic_is_atx(const char *p) {
    int n = 0;
    while (p[n] == '#' && n < 7) n++;
    if (n < 1 || n > 6) return false;
    return p[n] == ' ' || p[n] == '\t' || p[n] == '\n' || p[n] == '\r' || p[n] == '\0';
}

static bool critic_is_fence_opener(const char *p) {
    char c = *p;
    if (c != '`' && c != '~') return false;
    int n = 0;
    while (p[n] == c) n++;
    if (n < 3) return false;
    if (c == '`') {
        for (int i = n; p[i] && p[i] != '\n' && p[i] != '\r'; i++) {
            if (p[i] == '`') return false;
        }
    }
    return true;
}

/* Thematic break, or a setext underline when a paragraph is open. */
static bool critic_is_rule(const char *p, bool in_paragraph) {
    char kind = 0;
    int count = 0;
    bool spaced = false;
    for (const char *q = p; *q && *q != '\n' && *q != '\r'; q++) {
        if (*q == ' ' || *q == '\t') {
            if (count > 0) spaced = true;
            continue;
        }
        if (*q != '-' && *q != '_' && *q != '*' && *q != '=') return false;
        if (!kind) kind = *q;
        else if (*q != kind) return false;
        count++;
    }
    if (count < 1 || !kind) return false;
    if (!spaced && in_paragraph && (kind == '-' || kind == '=') && count >= 1) return true;
    if (!spaced && count >= 3 && (kind == '-' || kind == '_' || kind == '*')) return true;
    if (spaced && count >= 3 && (kind == '-' || kind == '_' || kind == '*')) return true;
    return false;
}

static int critic_match_boxes(const char *line, const critic_blocks *st,
                              const char **p, int *col) {
    *p = line;
    *col = 0;
    int matched = 0;
    bool blank = critic_line_blank(line);
    for (int i = 0; i < st->depth; i++) {
        if (st->stack[i].kind == CRITIC_BOX_QUOTE) {
            const char *q;
            int c;
            if (!critic_match_quote(*p, *col, &q, &c)) break;
            *p = q;
            *col = c;
            matched++;
        } else if (blank) {
            if (!st->stack[i].has_content) break;
            matched++;
        } else {
            int indent = 0;
            int c = *col;
            critic_skip_indent(*p, &c, &indent);
            if (indent < st->stack[i].need) break;
            c = *col;
            *p = critic_advance_columns(*p, &c, st->stack[i].need);
            *col = c;
            matched++;
        }
    }
    return matched;
}

static void critic_mark_content(critic_blocks *st) {
    for (int i = 0; i < st->depth; i++) {
        if (st->stack[i].kind == CRITIC_BOX_ITEM) st->stack[i].has_content = true;
    }
}

/* Would this line open a block that closes an open paragraph? */
static bool critic_interrupts_paragraph(const char *p, int col) {
    int indent = 0;
    int c = col;
    const char *q = critic_skip_indent(p, &c, &indent);
    if (indent >= 4) return false; /* indented code cannot interrupt */
    if (*q == '>') return true;
    int marker_len = 0;
    int ordered_start = 0;
    int kind = critic_list_marker(q, &marker_len, &ordered_start);
    if (kind == 1) return true;
    if (kind == 2 && ordered_start == 1) return true;
    if (critic_is_atx(q) || critic_is_fence_opener(q) || critic_is_rule(q, true)) return true;
    return false;
}

/* Interpret the remainder of a line after open containers have been matched.
 * Returns true when the line is indented code and must be copied verbatim. */
static bool critic_interpret_line(critic_blocks *st, const char *p, int col) {
    for (;;) {
        int indent = 0;
        int c = col;
        const char *q = critic_skip_indent(p, &c, &indent);
        if (*q == '\0' || *q == '\n' || *q == '\r') {
            st->in_paragraph = false;
            return false;
        }
        if (indent >= 4) {
            if (!st->in_paragraph) {
                st->in_code = true;
                critic_mark_content(st);
                return true;
            }
            return false;
        }
        if (*q == '>') {
            if (st->depth < 32) {
                st->stack[st->depth].kind = CRITIC_BOX_QUOTE;
                st->stack[st->depth].need = 0;
                st->stack[st->depth].has_content = false;
                st->depth++;
            }
            q++;
            c += 1;
            if (*q == ' ' || *q == '\t') {
                c = critic_col_add(c, *q);
                q++;
            }
            p = q;
            col = c;
            st->in_paragraph = false;
            continue;
        }
        int marker_len = 0;
        int ordered_start = 0;
        int kind = critic_list_marker(q, &marker_len, &ordered_start);
        if (kind && !(st->in_paragraph && kind == 2 && ordered_start != 1)) {
            int marker_offset = indent;
            const char *after = q + marker_len;
            int after_col = c + marker_len;
            int save_col = after_col;
            const char *s = after;
            int sc = after_col;
            while (*s == ' ' || *s == '\t') {
                if (sc - save_col > 5) break;
                int next = critic_col_add(sc, *s);
                if (sc - save_col <= 5) {
                    sc = next;
                    s++;
                    if (sc - save_col > 5) break;
                } else {
                    break;
                }
            }
            int i = sc - save_col;
            int padding;
            const char *content;
            int content_col;
            if (i >= 5 || i < 1 || *s == '\0' || *s == '\n' || *s == '\r') {
                padding = marker_len + 1;
                content_col = after_col;
                content = after;
                if (i > 0) content = critic_advance_columns(after, &content_col, 1);
            } else {
                padding = marker_len + i;
                content = s;
                content_col = sc;
            }
            if (st->depth < 32) {
                st->stack[st->depth].kind = CRITIC_BOX_ITEM;
                st->stack[st->depth].need = marker_offset + padding;
                st->stack[st->depth].has_content = false;
                st->depth++;
            }
            p = content;
            col = content_col;
            st->in_paragraph = false;
            st->in_code = false;
            continue;
        }
        if (critic_is_fence_opener(q) || critic_is_atx(q) || critic_is_rule(q, st->in_paragraph)) {
            st->in_paragraph = false;
            st->in_code = false;
            critic_mark_content(st);
            return false;
        }
        st->in_paragraph = true;
        st->in_code = false;
        critic_mark_content(st);
        return false;
    }
}

/* True when this line is indented code and should be copied unchanged.
 * Updates block state either way. Call only at the start of a line. */
static bool critic_indented_code_line(const char *line, critic_blocks *st) {
    const char *p;
    int col;
    int matched = critic_match_boxes(line, st, &p, &col);
    bool blank = critic_line_blank(line);

    if (matched < st->depth) {
        if (st->in_paragraph && !blank && !critic_interrupts_paragraph(p, col)) {
            return false; /* lazy continuation */
        }
        st->depth = matched;
        st->in_code = false;
        st->in_paragraph = false;
    }

    if (blank) {
        st->in_paragraph = false;
        if (matched < st->depth) st->in_code = false;
        return false;
    }

    if (st->in_code) {
        int indent = 0;
        int c = col;
        critic_skip_indent(p, &c, &indent);
        if (indent >= 4) return true;
        st->in_code = false;
    }

    return critic_interpret_line(st, p, col);
}

/* remaining counts bytes available before the terminating NUL. */
static bool critic_append(char **buf, char **write, size_t *remaining,
                          const char *src, size_t len) {
    if (len >= *remaining) {
        size_t written = (size_t)(*write - *buf);
        size_t new_cap = (written + len + 1) * 2;
        char *grown = realloc(*buf, new_cap);
        if (!grown) return false;
        *buf = grown;
        *write = grown + written;
        *remaining = new_cap - written - 1;
    }
    if (len) {
        memcpy(*write, src, len);
        *write += len;
        *remaining -= len;
    }
    return true;
}

/**
 * Process Critic Markup in raw text (preprocessing approach)
 * This is better than postprocessing because it avoids smart typography interference.
 * Code spans, fenced blocks, and indented code blocks are copied through
 * so the syntax can be shown literally.
 */
char *apex_process_critic_markup_text(const char *text, critic_mode_t mode) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t output_capacity = len * 2 + 1;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read_pos = text;
    char *write_pos = output;
    size_t remaining_capacity = output_capacity - 1;
    critic_blocks blocks;
    memset(&blocks, 0, sizeof(blocks));

    while (*read_pos) {
        bool line_start = (read_pos == text || read_pos[-1] == '\n' ||
                           (read_pos[-1] == '\r' && *read_pos != '\n'));
        if (line_start && critic_indented_code_line(read_pos, &blocks)) {
            const char *next = critic_next_line(read_pos);
            if (!critic_append(&output, &write_pos, &remaining_capacity, read_pos, (size_t)(next - read_pos))) {
                free(output);
                return NULL;
            }
            read_pos = next;
            continue;
        }

        size_t code_len = critic_code_region_len(text, read_pos);
        if (code_len > 0) {
            if (!critic_append(&output, &write_pos, &remaining_capacity, read_pos, code_len)) {
                free(output);
                return NULL;
            }
            read_pos += code_len;
            continue;
        }

        /* Look for { */
        if (*read_pos == '{') {
            int consumed;
            const char *content;
            int content_len;
            const char *old_text = NULL;
            int old_len = 0;

            critic_type_t type = scan_critic_markup(read_pos, (int)strlen(read_pos), &consumed,
                                                   &content, &content_len, &old_text, &old_len);

            if (type != CRITIC_NONE) {
                /* Found valid critic markup - convert to HTML */
                char *html = critic_to_html(type, content, content_len, old_text, old_len, mode);
                if (html) {
                    if (!critic_append(&output, &write_pos, &remaining_capacity, html, strlen(html))) {
                        free(html);
                        free(output);
                        return NULL;
                    }
                    free(html);
                }
                read_pos += consumed;
                continue;
            }
        }

        /* Not critic markup, copy character */
        if (!critic_append(&output, &write_pos, &remaining_capacity, read_pos, 1)) {
            free(output);
            return NULL;
        }
        read_pos++;
    }

    *write_pos = '\0';
    return output;
}

