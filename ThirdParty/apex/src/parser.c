/**
 * @file parser.c
 * @brief Minimal Markdown parser implementation
 *
 * This is a placeholder implementation that will be replaced with
 * cmark-gfm integration or custom parser.
 */

#include "apex/parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const apex_options *options;
    const char *input;
    size_t length;
    size_t pos;
    int line;
    int column;
} parser_state;

void *apex_parser_new(const apex_options *options) {
    parser_state *state = (parser_state *)calloc(1, sizeof(parser_state));
    if (state) {
        state->options = options;
    }
    return state;
}

void apex_parser_free(void *parser) {
    if (parser) {
        free(parser);
    }
}

static apex_node *apex_node_new(apex_node_type type) {
    apex_node *node = (apex_node *)calloc(1, sizeof(apex_node));
    if (node) {
        node->type = type;
    }
    return node;
}

static void apex_node_append_child(apex_node *parent, apex_node *child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next = NULL;

    if (parent->last_child) {
        parent->last_child->next = child;
        child->prev = parent->last_child;
        parent->last_child = child;
    } else {
        parent->first_child = child;
        parent->last_child = child;
        child->prev = NULL;
    }
}

void apex_node_free(apex_node *node) {
    if (!node) return;

    /* Free all children recursively */
    apex_node *child = node->first_child;
    while (child) {
        apex_node *next = child->next;
        apex_node_free(child);
        child = next;
    }

    /* Free node data */
    if (node->literal) {
        free(node->literal);
    }

    /* Free type-specific data */
    switch (node->type) {
        case APEX_NODE_CODE_BLOCK:
            if (node->data.code_block.info) {
                free(node->data.code_block.info);
            }
            break;
        case APEX_NODE_LINK:
        case APEX_NODE_IMAGE:
            if (node->data.link.url) {
                free(node->data.link.url);
            }
            if (node->data.link.title) {
                free(node->data.link.title);
            }
            break;
        case APEX_NODE_CALLOUT:
            if (node->data.callout.type) {
                free(node->data.callout.type);
            }
            if (node->data.callout.title) {
                free(node->data.callout.title);
            }
            break;
        default:
            break;
    }

    free(node);
}

/* Simple line-based parser for basic Markdown */
static apex_node *parse_simple(parser_state *state) {
    apex_node *doc = apex_node_new(APEX_NODE_DOCUMENT);
    const char *input = state->input;
    size_t len = state->length;
    size_t pos = 0;

    while (pos < len) {
        /* Skip empty lines */
        while (pos < len && (input[pos] == '\n' || input[pos] == '\r')) {
            pos++;
        }

        if (pos >= len) break;

        /* Check for heading */
        if (input[pos] == '#') {
            int level = 0;
            size_t start = pos;

            while (pos < len && input[pos] == '#' && level < 6) {
                level++;
                pos++;
            }

            /* Need space after # */
            if (pos < len && input[pos] == ' ') {
                pos++;
                size_t text_start = pos;

                /* Find end of line */
                while (pos < len && input[pos] != '\n') {
                    pos++;
                }

                apex_node *heading = apex_node_new(APEX_NODE_HEADING);
                heading->data.heading.level = level;
                heading->literal = strndup(input + text_start, pos - text_start);
                apex_node_append_child(doc, heading);
                continue;
            }

            /* Not a heading, reset */
            pos = start;
        }

        /* Check for code fence */
        if (pos + 3 <= len && input[pos] == '`' && input[pos+1] == '`' && input[pos+2] == '`') {
            pos += 3;
            size_t info_start = pos;

            /* Read info string */
            while (pos < len && input[pos] != '\n') {
                pos++;
            }

            char *info = (info_start < pos) ? strndup(input + info_start, pos - info_start) : NULL;
            if (pos < len) pos++; /* Skip newline */

            size_t code_start = pos;

            /* Find closing fence */
            while (pos + 3 <= len) {
                if (input[pos] == '`' && input[pos+1] == '`' && input[pos+2] == '`') {
                    apex_node *code_block = apex_node_new(APEX_NODE_CODE_BLOCK);
                    code_block->data.code_block.fenced = true;
                    code_block->data.code_block.info = info;
                    code_block->literal = strndup(input + code_start, pos - code_start);
                    apex_node_append_child(doc, code_block);

                    pos += 3;
                    /* Skip to end of line */
                    while (pos < len && input[pos] != '\n') pos++;
                    break;
                }
                pos++;
            }
            continue;
        }

        /* Regular paragraph */
        size_t para_start = pos;

        /* Read until blank line or end */
        while (pos < len) {
            if (input[pos] == '\n') {
                if (pos + 1 < len && input[pos + 1] == '\n') {
                    /* Blank line ends paragraph */
                    break;
                }
            }
            pos++;
        }

        if (pos > para_start) {
            apex_node *para = apex_node_new(APEX_NODE_PARAGRAPH);
            para->literal = strndup(input + para_start, pos - para_start);
            apex_node_append_child(doc, para);
        }
    }

    return doc;
}

apex_node *apex_parse(void *parser, const char *markdown, size_t length) {
    if (!parser || !markdown) {
        return NULL;
    }

    parser_state *state = (parser_state *)parser;
    state->input = markdown;
    state->length = length;
    state->pos = 0;
    state->line = 1;
    state->column = 1;

    return parse_simple(state);
}

