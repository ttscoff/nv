/**
 * Math Extension for Apex
 *
 * Simple implementation that detects math delimiters and outputs HTML
 * for MathJax/KaTeX to process on the client side.
 */

#include "math.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"
#include "inlines.h"
#include "html.h"

/* Node type for math */
extern cmark_node_type APEX_NODE_MATH;

/**
 * Scan for $...$ inline math
 * Returns length if found, 0 otherwise
 *
 * Rules to avoid false positives:
 * - Must have non-whitespace after opening $
 * - Must have non-whitespace before closing $
 * - No spaces immediately inside the delimiters
 */
static int scan_dollar_math(const char *input, int len, bool *is_display) {
    if (len < 3) return 0;

    /* Check for $$ (display math) */
    if (input[0] == '$' && input[1] == '$') {
        *is_display = true;
        /* Find closing $$ */
        for (int i = 2; i < len - 1; i++) {
            if (input[i] == '$' && input[i + 1] == '$') {
                return i + 2;
            }
        }
        return 0;
    }

    /* Check for single $ (inline math) */
    if (input[0] == '$') {
        *is_display = false;

        /*
         * Next character must not be whitespace, '$', or a digit.
         * Treat "$40", "$80", etc. as currency, not math delimiters.
         */
        if (len < 2 || input[1] == ' ' || input[1] == '\t' || input[1] == '\n' || input[1] == '$' ||
            (input[1] >= '0' && input[1] <= '9')) {
            return 0;
        }

        /* Find closing $ */
        for (int i = 1; i < len; i++) {
            if (input[i] == '$') {
                /* Make sure it's not escaped */
                if (i > 0 && input[i - 1] == '\\') continue;

                /* Previous character should not be whitespace (avoid "$5 " matching) */
                if (i > 1 && (input[i - 1] == ' ' || input[i - 1] == '\t' || input[i - 1] == '\n')) {
                    return 0;
                }

                /* Closing delimiter should not be followed by a digit (currency like "$5"). */
                if (i + 1 < len && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    continue;
                }

                /* Must have at least one character of content */
                if (i == 1) return 0;

                return i + 1;
            }
        }
    }

    return 0;
}

/**
 * Scan for \(...\) or \[...\] LaTeX-style math
 * Returns length if found, 0 otherwise
 */
static int scan_latex_math(const char *input, int len, bool *is_display) {
    if (len < 4) return 0;

    /* Check for \[ (display math) */
    if (input[0] == '\\' && input[1] == '[') {
        *is_display = true;
        /* Find closing \] */
        for (int i = 2; i < len - 1; i++) {
            if (input[i] == '\\' && input[i + 1] == ']') {
                return i + 2;
            }
        }
        return 0;
    }

    /* Check for \( (inline math) */
    if (input[0] == '\\' && input[1] == '(') {
        *is_display = false;
        /* Find closing \) */
        for (int i = 2; i < len - 1; i++) {
            if (input[i] == '\\' && input[i + 1] == ')') {
                /* Validate: math content should have at least one valid math character */
                /* Math should contain letters, numbers, or common math operators */
                /* Reject if content is empty or only contains non-math special characters */
                int content_len = i - 2;
                if (content_len <= 0) {
                    return 0;
                }

                bool has_math_content = false;
                for (int j = 2; j < i; j++) {
                    unsigned char c = (unsigned char)input[j];
                    /* Allow letters, numbers, common math operators, and whitespace */
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '*' ||
                        c == '/' || c == '=' || c == '^' || c == '_' || c == ' ' ||
                        c == '.' || c == ',' || c == '(' || c == ')' || c == '\\') {
                        has_math_content = true;
                        break;
                    }
                }
                /* If no valid math content found, don't treat as math */
                if (!has_math_content) {
                    return 0;
                }
                return i + 2;
            }
        }
    }

    return 0;
}

/**
 * Match function for $ character
 */
static cmark_node *match_math_dollar(cmark_syntax_extension *self,
                                      cmark_parser *parser,
                                      cmark_node *parent,
                                      unsigned char character,
                                      cmark_inline_parser *inline_parser) {
    (void)self;
    (void)parent;
    if (character != '$') return NULL;

    /* Get current position and remaining input */
    int pos = cmark_inline_parser_get_offset(inline_parser);
    cmark_chunk *chunk = cmark_inline_parser_get_chunk(inline_parser);

    if (pos >= chunk->len) return NULL;

    const char *input = (const char *)chunk->data + pos;
    int remaining = chunk->len - pos;

    /* Check for math */
    bool is_display = false;
    int consumed = scan_dollar_math(input, remaining, &is_display);
    if (consumed == 0) return NULL;

    /* Extract math content */
    int start_offset = is_display ? 2 : 1;
    int end_offset = is_display ? 2 : 1;
    int content_len = consumed - start_offset - end_offset;

    if (content_len <= 0) return NULL;

    /* Create HTML inline node with appropriate class */
    cmark_node *node = cmark_node_new_with_mem(CMARK_NODE_HTML_INLINE, parser->mem);

    /* Build HTML string */
    char *html = malloc(content_len + 100);
    if (!html) return NULL;

    if (is_display) {
        snprintf(html, content_len + 100,
                "<span class=\"math display\">\\[%.*s\\]</span>",
                content_len, input + start_offset);
    } else {
        snprintf(html, content_len + 100,
                "<span class=\"math inline\">\\(%.*s\\)</span>",
                content_len, input + start_offset);
    }

    cmark_node_set_literal(node, html);
    free(html);

    /* Set line/column info */
    node->start_line = node->end_line = cmark_inline_parser_get_line(inline_parser);
    node->start_column = cmark_inline_parser_get_column(inline_parser) - 1;
    node->end_column = cmark_inline_parser_get_column(inline_parser) + consumed - 1;

    /* Advance parser */
    cmark_inline_parser_set_offset(inline_parser, pos + consumed);

    return node;
}

/**
 * Match function for \ character (LaTeX style)
 */
static cmark_node *match_math_backslash(cmark_syntax_extension *self,
                                        cmark_parser *parser,
                                        cmark_node *parent,
                                        unsigned char character,
                                        cmark_inline_parser *inline_parser) {
    (void)self;
    (void)parent;
    if (character != '\\') return NULL;

    /* Get current position and remaining input */
    int pos = cmark_inline_parser_get_offset(inline_parser);
    cmark_chunk *chunk = cmark_inline_parser_get_chunk(inline_parser);

    if (pos >= chunk->len) return NULL;

    const char *input = (const char *)chunk->data + pos;
    int remaining = chunk->len - pos;

    /* Check for math */
    bool is_display = false;
    int consumed = scan_latex_math(input, remaining, &is_display);
    if (consumed == 0) return NULL;

    /* Extract math content */
    int start_offset = 2;  /* Skip \( or \[ */
    int end_offset = 2;    /* Skip \) or \] */
    int content_len = consumed - start_offset - end_offset;

    if (content_len <= 0) return NULL;

    /* Create HTML inline node */
    cmark_node *node = cmark_node_new_with_mem(CMARK_NODE_HTML_INLINE, parser->mem);

    /* Build HTML string */
    char *html = malloc(content_len + 100);
    if (!html) return NULL;

    if (is_display) {
        snprintf(html, content_len + 100,
                "<span class=\"math display\">\\[%.*s\\]</span>",
                content_len, input + start_offset);
    } else {
        snprintf(html, content_len + 100,
                "<span class=\"math inline\">\\(%.*s\\)</span>",
                content_len, input + start_offset);
    }

    cmark_node_set_literal(node, html);
    free(html);

    /* Set line/column info */
    node->start_line = node->end_line = cmark_inline_parser_get_line(inline_parser);
    node->start_column = cmark_inline_parser_get_column(inline_parser) - 1;
    node->end_column = cmark_inline_parser_get_column(inline_parser) + consumed - 1;

    /* Advance parser */
    cmark_inline_parser_set_offset(inline_parser, pos + consumed);

    return node;
}

/* One match callback handles both $...$ and \(...\) / \[...\]. */
static cmark_node *match_math(cmark_syntax_extension *self,
                              cmark_parser *parser,
                              cmark_node *parent,
                              unsigned char character,
                              cmark_inline_parser *inline_parser) {
    if (character == '$') {
        return match_math_dollar(self, parser, parent, character, inline_parser);
    }
    if (character == '\\') {
        return match_math_backslash(self, parser, parent, character, inline_parser);
    }
    return NULL;
}

/**
 * Create the math extension
 */
cmark_syntax_extension *create_math_extension(void) {
    cmark_syntax_extension *ext = cmark_syntax_extension_new("math");

    /* Register special characters $ and \ */
    cmark_llist *special_chars = NULL;
    cmark_mem *mem = cmark_get_default_mem_allocator();
    special_chars = cmark_llist_append(mem, special_chars, (void *)'$');
    special_chars = cmark_llist_append(mem, special_chars, (void *)'\\');
    cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

    cmark_syntax_extension_set_match_inline_func(ext, match_math);

    return ext;
}

