/**
 * @file renderer.c
 * @brief HTML renderer implementation
 */

#include "apex/renderer.h"
#include <stdlib.h>
#include <string.h>

/* HTML escaping */
static void escape_html(apex_buffer *buf, const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '<':
                apex_buffer_append_str(buf, "&lt;");
                break;
            case '>':
                apex_buffer_append_str(buf, "&gt;");
                break;
            case '&':
                apex_buffer_append_str(buf, "&amp;");
                break;
            case '"':
                apex_buffer_append_str(buf, "&quot;");
                break;
            default:
                apex_buffer_append_char(buf, str[i]);
                break;
        }
    }
}

static void render_node_html(apex_node *node, apex_buffer *buf, const apex_options *options) {
    if (!node) return;

    switch (node->type) {
        case APEX_NODE_DOCUMENT:
            /* Render all children */
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            break;

        case APEX_NODE_HEADING: {
            int level = node->data.heading.level;
            apex_buffer_append_str(buf, "<h");
            apex_buffer_append_char(buf, '0' + level);
            apex_buffer_append_char(buf, '>');

            if (node->literal) {
                /* Trim whitespace */
                const char *text = node->literal;
                size_t len = strlen(text);
                while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\n' || text[len-1] == '\r')) {
                    len--;
                }
                escape_html(buf, text, len);
            }

            apex_buffer_append_str(buf, "</h");
            apex_buffer_append_char(buf, '0' + level);
            apex_buffer_append_str(buf, ">\n");
            break;
        }

        case APEX_NODE_PARAGRAPH:
            apex_buffer_append_str(buf, "<p>");
            if (node->literal) {
                /* Trim trailing newlines */
                const char *text = node->literal;
                size_t len = strlen(text);
                while (len > 0 && (text[len-1] == '\n' || text[len-1] == '\r')) {
                    len--;
                }
                escape_html(buf, text, len);
            }
            apex_buffer_append_str(buf, "</p>\n");
            break;

        case APEX_NODE_CODE_BLOCK: {
            apex_buffer_append_str(buf, "<pre><code");

            if (node->data.code_block.info) {
                apex_buffer_append_str(buf, " class=\"language-");
                /* Extract first word from info string */
                const char *info = node->data.code_block.info;
                const char *end = info;
                while (*end && *end != ' ') end++;
                escape_html(buf, info, end - info);
                apex_buffer_append_char(buf, '"');
            }

            apex_buffer_append_char(buf, '>');

            if (node->literal) {
                escape_html(buf, node->literal, strlen(node->literal));
            }

            apex_buffer_append_str(buf, "</code></pre>\n");
            break;
        }

        case APEX_NODE_THEMATIC_BREAK:
            apex_buffer_append_str(buf, "<hr />\n");
            break;

        case APEX_NODE_BLOCK_QUOTE:
            apex_buffer_append_str(buf, "<blockquote>\n");
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</blockquote>\n");
            break;

        case APEX_NODE_LIST:
            /* TODO: detect ordered vs unordered */
            apex_buffer_append_str(buf, "<ul>\n");
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</ul>\n");
            break;

        case APEX_NODE_LIST_ITEM:
            apex_buffer_append_str(buf, "<li>");
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</li>\n");
            break;

        case APEX_NODE_TEXT:
            if (node->literal) {
                escape_html(buf, node->literal, strlen(node->literal));
            }
            break;

        case APEX_NODE_EMPH:
            apex_buffer_append_str(buf, "<em>");
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</em>");
            break;

        case APEX_NODE_STRONG:
            apex_buffer_append_str(buf, "<strong>");
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</strong>");
            break;

        case APEX_NODE_CODE:
            apex_buffer_append_str(buf, "<code>");
            if (node->literal) {
                escape_html(buf, node->literal, strlen(node->literal));
            }
            apex_buffer_append_str(buf, "</code>");
            break;

        case APEX_NODE_LINK:
            apex_buffer_append_str(buf, "<a href=\"");
            if (node->data.link.url) {
                escape_html(buf, node->data.link.url, strlen(node->data.link.url));
            }
            apex_buffer_append_char(buf, '"');

            if (node->data.link.title) {
                apex_buffer_append_str(buf, " title=\"");
                escape_html(buf, node->data.link.title, strlen(node->data.link.title));
                apex_buffer_append_char(buf, '"');
            }

            apex_buffer_append_char(buf, '>');
            for (apex_node *child = node->first_child; child; child = child->next) {
                render_node_html(child, buf, options);
            }
            apex_buffer_append_str(buf, "</a>");
            break;

        case APEX_NODE_IMAGE:
            apex_buffer_append_str(buf, "<img src=\"");
            if (node->data.link.url) {
                escape_html(buf, node->data.link.url, strlen(node->data.link.url));
            }
            apex_buffer_append_str(buf, "\" alt=\"");
            if (node->literal) {
                escape_html(buf, node->literal, strlen(node->literal));
            }
            apex_buffer_append_char(buf, '"');

            if (node->data.link.title) {
                apex_buffer_append_str(buf, " title=\"");
                escape_html(buf, node->data.link.title, strlen(node->data.link.title));
                apex_buffer_append_char(buf, '"');
            }

            apex_buffer_append_str(buf, " />");
            break;

        case APEX_NODE_LINEBREAK:
            apex_buffer_append_str(buf, "<br />\n");
            break;

        case APEX_NODE_SOFTBREAK:
            if (options->hardbreaks) {
                apex_buffer_append_str(buf, "<br />\n");
            } else if (options->nobreaks) {
                apex_buffer_append_char(buf, ' ');
            } else {
                apex_buffer_append_char(buf, '\n');
            }
            break;

        default:
            /* Unknown node type, skip */
            break;
    }
}

char *apex_render_html(apex_node *root, const apex_options *options) {
    if (!root || !options) {
        return NULL;
    }

    apex_buffer buf;
    apex_buffer_init(&buf, 4096);

    render_node_html(root, &buf, options);

    return apex_buffer_detach(&buf);
}

char *apex_render_xml(apex_node *root, const apex_options *options) {
    /* TODO: Implement XML rendering */
    (void)root;
    (void)options;
    return strdup("<xml>Not implemented yet</xml>");
}

