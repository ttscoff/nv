/**
 * ast_markdown.c - Convert cmark-gfm AST to Markdown
 *
 * Supports multiple Markdown dialects with different formatting rules.
 */

#include "apex/ast_markdown.h"
#include "apex/apex.h"
#include "cmark-gfm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Buffer for building markdown output */
typedef struct {
    char *buf;
    size_t len;
    size_t capacity;
} markdown_buffer;

static void buffer_init(markdown_buffer *buf) {
    buf->buf = NULL;
    buf->len = 0;
    buf->capacity = 0;
}

static void buffer_append(markdown_buffer *buf, const char *str, size_t len) {
    if (!str || len == 0) return;
    
    if (buf->len + len + 1 > buf->capacity) {
        size_t new_cap = buf->capacity ? buf->capacity * 2 : 256;
        if (new_cap < buf->len + len + 1) {
            new_cap = buf->len + len + 1;
        }
        char *new_buf = realloc(buf->buf, new_cap);
        if (!new_buf) return;
        buf->buf = new_buf;
        buf->capacity = new_cap;
    }
    
    memcpy(buf->buf + buf->len, str, len);
    buf->len += len;
    buf->buf[buf->len] = '\0';
}

static void buffer_append_str(markdown_buffer *buf, const char *str) {
    if (str) {
        buffer_append(buf, str, strlen(str));
    }
}


/* Escape special characters in markdown */
static void escape_markdown(markdown_buffer *buf, const char *text, size_t len, apex_markdown_dialect_t dialect) {
    if (!text) return;
    
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        bool should_escape = false;
        
        switch (c) {
            case '\\':
            case '`':
            case '*':
            case '_':
            case '[':
            case ']':
            case '(':
            case ')':
            case '#':
            case '+':
            case '.':
            case '!':
                should_escape = true;
                break;
            case '{':
            case '}':
                /* Don't escape braces for MMD (used in {{TOC}} syntax) */
                if (dialect != APEX_MD_DIALECT_MMD) {
                    should_escape = true;
                }
                break;
            case '-':
                /* Don't escape dashes for MMD (used in TOC parameters like {{TOC:2-4}}) */
                if (dialect != APEX_MD_DIALECT_MMD) {
                    should_escape = true;
                }
                break;
            default:
                break;
        }
        
        if (should_escape) {
            buffer_append(buf, "\\", 1);
        }
        buffer_append(buf, &c, 1);
    }
}

/* Serialize inline nodes */
static void serialize_inline(markdown_buffer *buf, cmark_node *node, apex_markdown_dialect_t dialect) {
    if (!node) return;
    
    cmark_node_type type = cmark_node_get_type(node);
    const char *literal = cmark_node_get_literal(node);
    
    switch (type) {
        case CMARK_NODE_TEXT:
            if (literal) {
                /* For MMD, check if this text contains a TOC marker and convert it */
                if (dialect == APEX_MD_DIALECT_MMD) {
                    const char *toc_start = strstr(literal, "{{TOC");
                    if (toc_start) {
                        /* Found TOC marker - output text before it */
                        if (toc_start > literal) {
                            escape_markdown(buf, literal, (size_t)(toc_start - literal), dialect);
                        }
                        /* Find the end of the TOC marker */
                        const char *toc_end = strstr(toc_start, "}}");
                        if (toc_end) {
                            /* Found complete TOC marker - output just {{TOC}} for MMD (no parameters) */
                            buffer_append_str(buf, "{{TOC}}");
                            /* Output any remaining text after the marker */
                            const char *remaining = toc_end + 2;
                            if (*remaining) {
                                escape_markdown(buf, remaining, strlen(remaining), dialect);
                            }
                            break;
                        }
                    }
                }
                /* Normal text - escape as needed */
                escape_markdown(buf, literal, strlen(literal), dialect);
            }
            break;
            
        case CMARK_NODE_SOFTBREAK:
            buffer_append_str(buf, "\n");
            break;
            
        case CMARK_NODE_LINEBREAK:
            buffer_append_str(buf, "  \n");
            break;
            
        case CMARK_NODE_CODE:
            if (literal) {
                buffer_append_str(buf, "`");
                buffer_append_str(buf, literal);
                buffer_append_str(buf, "`");
            }
            break;
            
        case CMARK_NODE_EMPH:
            buffer_append_str(buf, "*");
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, dialect);
            }
            buffer_append_str(buf, "*");
            break;
            
        case CMARK_NODE_STRONG:
            buffer_append_str(buf, "**");
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, dialect);
            }
            buffer_append_str(buf, "**");
            break;
            
        case CMARK_NODE_LINK:
            buffer_append_str(buf, "[");
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                if (cmark_node_get_type(child) != CMARK_NODE_LINK) {
                    serialize_inline(buf, child, dialect);
                }
            }
            buffer_append_str(buf, "](");
            const char *url = cmark_node_get_url(node);
            if (url) buffer_append_str(buf, url);
            const char *title = cmark_node_get_title(node);
            if (title) {
                buffer_append_str(buf, " \"");
                buffer_append_str(buf, title);
                buffer_append_str(buf, "\"");
            }
            buffer_append_str(buf, ")");
            break;
            
        case CMARK_NODE_IMAGE:
            buffer_append_str(buf, "![");
            const char *alt = cmark_node_get_literal(node);
            if (alt) buffer_append_str(buf, alt);
            buffer_append_str(buf, "](");
            url = cmark_node_get_url(node);
            if (url) buffer_append_str(buf, url);
            title = cmark_node_get_title(node);
            if (title) {
                buffer_append_str(buf, " \"");
                buffer_append_str(buf, title);
                buffer_append_str(buf, "\"");
            }
            buffer_append_str(buf, ")");
            break;
            
        default:
            /* For other inline types, serialize children */
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, dialect);
            }
            break;
    }
}

/* Serialize block nodes */
static void serialize_block(markdown_buffer *buf, cmark_node *node, apex_markdown_dialect_t dialect, int indent_level) {
    if (!node) return;
    
    cmark_node_type type = cmark_node_get_type(node);
    
    switch (type) {
        case CMARK_NODE_DOCUMENT:
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_block(buf, child, dialect, indent_level);
                if (cmark_node_get_type(child) != CMARK_NODE_LIST &&
                    cmark_node_get_type(child) != CMARK_NODE_ITEM) {
                    buffer_append_str(buf, "\n");
                }
            }
            break;
            
        case CMARK_NODE_PARAGRAPH: {
            /* Check if paragraph contains only a TOC marker (for MMD dialect) */
            bool is_toc_paragraph = false;
            if (dialect == APEX_MD_DIALECT_MMD) {
                /* Check if this paragraph contains only text that looks like {{TOC...}} */
                cmark_node *first_child = cmark_node_first_child(node);
                if (first_child && cmark_node_get_type(first_child) == CMARK_NODE_TEXT && 
                    !cmark_node_next(first_child)) {
                    const char *text = cmark_node_get_literal(first_child);
                    if (text && strstr(text, "{{TOC") == text) {
                        /* This is a TOC marker paragraph - convert to {{TOC}} for MMD */
                        is_toc_paragraph = true;
                        buffer_append_str(buf, "{{TOC}}\n\n");
                    }
                }
            }
            if (!is_toc_paragraph) {
                for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                    serialize_inline(buf, child, dialect);
                }
                buffer_append_str(buf, "\n\n");
            }
            break;
        }
            
        case CMARK_NODE_HEADING: {
            int level = cmark_node_get_heading_level(node);
            for (int i = 0; i < level; i++) {
                buffer_append_str(buf, "#");
            }
            buffer_append_str(buf, " ");
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_inline(buf, child, dialect);
            }
            buffer_append_str(buf, "\n\n");
            break;
        }
        
        case CMARK_NODE_CODE_BLOCK: {
            const char *info = cmark_node_get_fence_info(node);
            buffer_append_str(buf, "```");
            if (info) buffer_append_str(buf, info);
            buffer_append_str(buf, "\n");
            const char *literal = cmark_node_get_literal(node);
            if (literal) buffer_append_str(buf, literal);
            buffer_append_str(buf, "\n```\n\n");
            break;
        }
        
        case CMARK_NODE_BLOCK_QUOTE: {
            /* Serialize blockquote with > on same line as content */
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                cmark_node_type child_type = cmark_node_get_type(child);
                
                if (child_type == CMARK_NODE_PARAGRAPH) {
                    /* For paragraphs, serialize inline content directly on same line as > */
                    cmark_node *inline_child = cmark_node_first_child(child);
                    bool first_item = true;
                    
                    while (inline_child) {
                        cmark_node_type inline_type = cmark_node_get_type(inline_child);
                        
                        if (inline_type == CMARK_NODE_SOFTBREAK) {
                            /* For soft breaks in blockquotes, continue on next line with > */
                            buffer_append_str(buf, "\n> ");
                            first_item = false;
                        } else {
                            /* For first non-softbreak item, add > prefix */
                            if (first_item) {
                                buffer_append_str(buf, "> ");
                                first_item = false;
                            }
                            serialize_inline(buf, inline_child, dialect);
                        }
                        inline_child = cmark_node_next(inline_child);
                    }
                    
                    if (cmark_node_next(child)) {
                        buffer_append_str(buf, "\n");
                    } else {
                        buffer_append_str(buf, "\n\n");
                    }
                } else {
                    /* For other block types, prefix with > */
                    buffer_append_str(buf, "> ");
                    serialize_block(buf, child, dialect, indent_level);
                    /* Remove trailing newlines and add appropriate spacing */
                    if (cmark_node_next(child)) {
                        /* Ensure single newline between blockquote items */
                        if (buf->len > 0 && buf->buf[buf->len - 1] == '\n') {
                            buf->len--; /* Remove one newline */
                            buf->buf[buf->len] = '\0';
                        }
                        buffer_append_str(buf, "\n");
                    }
                }
            }
            break;
        }
        
        case CMARK_NODE_LIST: {
            cmark_list_type list_type = cmark_node_get_list_type(node);
            int item_index = 0;
            for (cmark_node *item = cmark_node_first_child(node); item; item = cmark_node_next(item), item_index++) {
                if (list_type == CMARK_ORDERED_LIST) {
                    int start = cmark_node_get_list_start(node);
                    char num[32];
                    snprintf(num, sizeof(num), "%d", start + item_index);
                    buffer_append_str(buf, num);
                    buffer_append_str(buf, ". ");
                } else {
                    buffer_append_str(buf, "- ");
                }
                serialize_block(buf, item, dialect, indent_level);
            }
            buffer_append_str(buf, "\n");
            break;
        }
        
        case CMARK_NODE_ITEM:
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_block(buf, child, dialect, indent_level);
            }
            break;
        
        case CMARK_NODE_THEMATIC_BREAK:
            buffer_append_str(buf, "---\n\n");
            break;
        
        case CMARK_NODE_HTML_BLOCK: {
            const char *literal = cmark_node_get_literal(node);
            if (literal) {
                /* Check if this is a TOC marker HTML comment */
                if (dialect == APEX_MD_DIALECT_MMD) {
                    const char *toc_start = strstr(literal, "<!--TOC");
                    if (toc_start) {
                        /* For MMD, convert HTML comment TOC marker to {{TOC}} format */
                        /* Always output just {{TOC}} (no parameters) for MMD compatibility */
                        buffer_append_str(buf, "{{TOC}}\n\n");
                        break;
                    }
                }
                /* For other dialects or non-TOC HTML, output as-is */
                buffer_append_str(buf, literal);
                buffer_append_str(buf, "\n\n");
            }
            break;
        }
        
        default:
            /* For unknown types, serialize children */
            for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
                serialize_block(buf, child, dialect, indent_level);
            }
            break;
    }
}

char *apex_cmark_to_markdown(cmark_node *document,
                             const apex_options *options,
                             apex_markdown_dialect_t dialect) {
    (void)options; /* Reserved for future use */
    if (!document || cmark_node_get_type(document) != CMARK_NODE_DOCUMENT) {
        return NULL;
    }
    
    markdown_buffer buf;
    buffer_init(&buf);
    
    serialize_block(&buf, document, dialect, 0);
    
    if (buf.buf) {
        return buf.buf;
    }
    
    /* Fallback: return empty string */
    char *empty = malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
}
