/**
 * @file parser.h
 * @brief Markdown parser interface
 */

#ifndef APEX_PARSER_H
#define APEX_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apex.h"

/**
 * Node types in the AST
 */
typedef enum {
    APEX_NODE_DOCUMENT,
    APEX_NODE_PARAGRAPH,
    APEX_NODE_HEADING,
    APEX_NODE_CODE_BLOCK,
    APEX_NODE_HTML_BLOCK,
    APEX_NODE_THEMATIC_BREAK,
    APEX_NODE_BLOCK_QUOTE,
    APEX_NODE_LIST,
    APEX_NODE_LIST_ITEM,
    APEX_NODE_TEXT,
    APEX_NODE_SOFTBREAK,
    APEX_NODE_LINEBREAK,
    APEX_NODE_CODE,
    APEX_NODE_HTML_INLINE,
    APEX_NODE_EMPH,
    APEX_NODE_STRONG,
    APEX_NODE_LINK,
    APEX_NODE_IMAGE,

    /* Extended node types */
    APEX_NODE_TABLE,
    APEX_NODE_TABLE_ROW,
    APEX_NODE_TABLE_CELL,
    APEX_NODE_FOOTNOTE_REFERENCE,
    APEX_NODE_FOOTNOTE_DEFINITION,
    APEX_NODE_DEFINITION_LIST,
    APEX_NODE_DEFINITION_TERM,
    APEX_NODE_DEFINITION_DATA,
    APEX_NODE_TASK_LIST_ITEM,
    APEX_NODE_STRIKETHROUGH,
    APEX_NODE_MATH,
    APEX_NODE_CALLOUT,
    APEX_NODE_WIKI_LINK,
    APEX_NODE_CRITIC_ADDITION,
    APEX_NODE_CRITIC_DELETION,
    APEX_NODE_CRITIC_SUBSTITUTION,
    APEX_NODE_CRITIC_HIGHLIGHT,
    APEX_NODE_CRITIC_COMMENT,
    APEX_NODE_METADATA,
    APEX_NODE_TOC_MARKER,
    APEX_NODE_PAGE_BREAK,
} apex_node_type;

/**
 * AST node structure
 */
typedef struct apex_node {
    apex_node_type type;
    struct apex_node *parent;
    struct apex_node *first_child;
    struct apex_node *last_child;
    struct apex_node *prev;
    struct apex_node *next;

    /* Node data */
    char *literal;          /**< Text content for text nodes */
    int start_line;         /**< Source start line */
    int start_column;       /**< Source start column */
    int end_line;           /**< Source end line */
    int end_column;         /**< Source end column */

    /* Type-specific data */
    union {
        struct {
            int level;      /**< Heading level (1-6) */
        } heading;

        struct {
            char *info;     /**< Language/info string */
            bool fenced;    /**< Is fenced code block */
        } code_block;

        struct {
            char *url;
            char *title;
        } link;

        struct {
            bool checked;   /**< Task list checkbox state */
        } task_item;

        struct {
            char *type;     /**< Callout type (NOTE, WARNING, etc) */
            char *title;    /**< Callout title */
            bool collapsible;
            bool default_open;
        } callout;

        struct {
            bool is_inline; /**< Inline vs display math */
        } math;
    } data;
} apex_node;

/**
 * Create parser
 *
 * @param options Parser options
 * @return Parser instance
 */
void *apex_parser_new(const apex_options *options);

/**
 * Free parser
 *
 * @param parser Parser instance
 */
void apex_parser_free(void *parser);

/**
 * Parse Markdown text into AST
 *
 * @param parser Parser instance
 * @param markdown Input text
 * @param length Text length
 * @return Root node of AST
 */
apex_node *apex_parse(void *parser, const char *markdown, size_t length);

/**
 * Free AST node and all children
 *
 * @param node Node to free
 */
void apex_node_free(apex_node *node);

#ifdef __cplusplus
}
#endif

#endif /* APEX_PARSER_H */

