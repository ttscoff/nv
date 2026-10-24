/**
 * Advanced Footnotes Extension for Apex
 * Implementation
 *
 * Extends cmark-gfm's footnote system to support block-level Markdown
 * content in footnote definitions.
 */

#include "advanced_footnotes.h"
#include "parser.h"
#include "node.h"
#include "inlines.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Check if a footnote definition has block-level content
 * (multiple paragraphs, code blocks, lists, etc.)
 */
static bool has_block_content(const char *content) {
    if (!content) return false;

    /* Check for multiple paragraphs (blank lines) */
    const char *p = content;
    bool found_text = false;
    bool found_blank = false;

    while (*p) {
        if (*p == '\n') {
            if (p[1] == '\n' || (p[1] == '\r' && p[2] == '\n')) {
                /* Blank line */
                if (found_text) {
                    found_blank = true;
                }
            }
        } else if (!found_blank && *p != ' ' && *p != '\t' && *p != '\r') {
            found_text = true;
        } else if (found_blank && *p != ' ' && *p != '\t' && *p != '\r') {
            /* Text after blank line - block content */
            return true;
        }
        p++;
    }

    /* Check for code blocks (4+ spaces indent) */
    p = content;
    while (*p) {
        if (*p == '\n' && p[1] == ' ' && p[2] == ' ' && p[3] == ' ' && p[4] == ' ') {
            return true;
        }
        p++;
    }

    /* Check for fenced code blocks */
    if (strstr(content, "```") || strstr(content, "~~~")) {
        return true;
    }

    /* Check for lists */
    p = content;
    while (*p) {
        if (*p == '\n' && (p[1] == '-' || p[1] == '*' || p[1] == '+' ||
                           (p[1] >= '0' && p[1] <= '9'))) {
            /* Potential list item */
            const char *q = p + 2;
            while (*q >= '0' && *q <= '9') q++;
            if (*q == '.' || p[1] == '-' || p[1] == '*' || p[1] == '+') {
                return true;
            }
        }
        p++;
    }

    return false;
}

/**
 * Re-parse footnote content as block-level Markdown
 */
static void reparse_footnote_blocks(cmark_node *footnote_def, cmark_parser *parser) {
    (void)parser;
    if (!footnote_def) return;

    /* Get the footnote content */
    cmark_node *first_child = cmark_node_first_child(footnote_def);
    if (!first_child) return;

    /* If it's already parsed as blocks, nothing to do */
    cmark_node_type type = cmark_node_get_type(first_child);
    if (type == CMARK_NODE_PARAGRAPH || type == CMARK_NODE_CODE_BLOCK ||
        type == CMARK_NODE_LIST || type == CMARK_NODE_BLOCK_QUOTE) {
        return; /* Already has block content */
    }

    /* Get text content */
    const char *literal = cmark_node_get_literal(first_child);
    if (!literal) return;

    /* Check if it needs block parsing */
    if (!has_block_content(literal)) return;

    /* Create a new parser for the footnote content */
    cmark_parser *sub_parser = cmark_parser_new(CMARK_OPT_FOOTNOTES);
    if (!sub_parser) return;

    /* Parse the content */
    cmark_parser_feed(sub_parser, literal, strlen(literal));
    cmark_node *parsed = cmark_parser_finish(sub_parser);

    if (parsed) {
        /* Remove old content */
        while (first_child) {
            cmark_node *next = cmark_node_next(first_child);
            cmark_node_unlink(first_child);
            cmark_node_free(first_child);
            first_child = next;
        }

        /* Add parsed blocks */
        cmark_node *child = cmark_node_first_child(parsed);
        while (child) {
            cmark_node *next = cmark_node_next(child);
            cmark_node_unlink(child);
            cmark_node_append_child(footnote_def, child);
            child = next;
        }

        cmark_node_free(parsed);
    }

    cmark_parser_free(sub_parser);
}

/**
 * Post-process footnotes to support block-level content
 */
cmark_node *apex_process_advanced_footnotes(cmark_node *root, cmark_parser *parser) {
    if (!root) return root;

    cmark_iter *iter = cmark_iter_new(root);
    cmark_event_type ev_type;
    cmark_node *cur;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cur = cmark_iter_get_node(iter);

        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node_type type = cmark_node_get_type(cur);

            /* Check if this is a footnote definition */
            if (type == CMARK_NODE_FOOTNOTE_DEFINITION) {
                reparse_footnote_blocks(cur, parser);
            }
        }
    }

    cmark_iter_free(iter);
    return root;
}

/**
 * Postprocess function for the extension
 */
static cmark_node *postprocess(cmark_syntax_extension *ext,
                               cmark_parser *parser,
                               cmark_node *root) {
    (void)ext;
    return apex_process_advanced_footnotes(root, parser);
}

/**
 * Create advanced footnotes extension
 */
cmark_syntax_extension *create_advanced_footnotes_extension(void) {
    cmark_syntax_extension *ext = cmark_syntax_extension_new("advanced_footnotes");
    if (!ext) return NULL;

    /* Set postprocess callback */
    cmark_syntax_extension_set_postprocess_func(ext, postprocess);

    return ext;
}

