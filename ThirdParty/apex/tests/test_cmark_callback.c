//
// Created by Sbarex on 10/02/26.
//

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>

/* cmark-gfm headers */
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "registry.h"
#include <string.h>

#include "render.h"
#include "parser.h"

#define DELIMITER ';'
#define DELIMITER_STR ";"

cmark_node_type CMARK_NODE_TEST;


// Match function: search the pattern ;;...;;
static cmark_node *match(__attribute__((unused)) cmark_syntax_extension *ext,
                         cmark_parser *parser,
                         __attribute__((unused)) cmark_node *parent,
                         unsigned char character,
                         cmark_inline_parser *inline_parser) {
    cmark_node *res = NULL;
    int left_flanking, right_flanking, punct_before, punct_after;
    char buffer[101] = {0};

    if (character != DELIMITER) {
        return NULL;
    }

    int delims = cmark_inline_parser_scan_delimiters(
        inline_parser, sizeof(buffer) - 1, DELIMITER,
        &left_flanking,
        &right_flanking, &punct_before, &punct_after);

    memset(buffer, DELIMITER, delims);
    buffer[delims] = 0;

    res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    cmark_node_set_literal(res, buffer);
    res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
    res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

    if ((left_flanking || right_flanking) && delims == 2) {
        cmark_inline_parser_push_delimiter(inline_parser, character, left_flanking,
                                           right_flanking, res);
    }

    return res;
}

static delimiter *insert(cmark_syntax_extension *self, __attribute__((unused)) cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
    cmark_node *tmp, *next;
    delimiter *delim, *tmp_delim;
    delimiter *res = closer->next;

    cmark_node *node = opener->inl_text;

    if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
        goto done;

    if (!cmark_node_set_type(node, CMARK_NODE_TEST))
        goto done;

    cmark_node_set_syntax_extension(node, self);

    tmp = cmark_node_next(opener->inl_text);

    while (tmp) {
        if (tmp == closer->inl_text)
            break;
        next = cmark_node_next(tmp);
        cmark_node_append_child(node, tmp);
        tmp = next;
    }

    node->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
    cmark_node_free(closer->inl_text);

    done:
      delim = closer;
    while (delim != NULL && delim != opener) {
        tmp_delim = delim->previous;
        cmark_inline_parser_remove_delimiter(inline_parser, delim);
        delim = tmp_delim;
    }

    cmark_inline_parser_remove_delimiter(inline_parser, opener);

    return res;
}

// Renderer HTML
static void html_render(__attribute__((unused)) cmark_syntax_extension *extension,
                        __attribute__((unused)) cmark_html_renderer *renderer,
                        __attribute__((unused)) cmark_node *node,
                        cmark_event_type ev_type,
                        __attribute__((unused)) int options) {
    const bool entering = ev_type == CMARK_EVENT_ENTER;
    if (entering) {
        cmark_strbuf_puts(renderer->html, "<div class=\"custom\">");
    } else {
        cmark_strbuf_puts(renderer->html, "</div>");
    }
}

// Function to get the namo the node
static const char *get_type_string(__attribute__((unused)) cmark_syntax_extension *ext, cmark_node *node) {
    return node->type == CMARK_NODE_TEST ? "my_test" : "<unknown>";
}

static int can_contain(__attribute__((unused)) cmark_syntax_extension *ext, cmark_node *node,
                       cmark_node_type child_type) {
    if (node->type != CMARK_NODE_TEST)
        return false;

    return CMARK_NODE_TYPE_INLINE_P(child_type);
}

cmark_syntax_extension *create_test_extension(void) {
    cmark_syntax_extension *ext = cmark_syntax_extension_new("my_test");
    cmark_llist *special_chars = NULL;

    cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
    cmark_syntax_extension_set_can_contain_func(ext, can_contain);
    cmark_syntax_extension_set_html_render_func(ext, html_render);
    CMARK_NODE_TEST = cmark_syntax_extension_add_node(1);

    cmark_syntax_extension_set_match_inline_func(ext, match);
    cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

    cmark_mem *mem = cmark_get_default_mem_allocator();
    special_chars = cmark_llist_append(mem, special_chars, (void *)DELIMITER);
    cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

    cmark_syntax_extension_set_emphasis(ext, 1);

    return ext;
}

static int register_extra_extensions(cmark_plugin *plugin) {
    cmark_plugin_register_syntax_extension(plugin, create_test_extension());
    return 1;
}

static void my_cmark_init_callback(struct cmark_parser *parser, __attribute__((unused)) const apex_options *options, __attribute__((unused)) int cmark_opts, __attribute__((unused)) void *user_data) {
    test_result(true, "Custom cmark init callback called");
    cmark_register_plugin(register_extra_extensions);

    cmark_syntax_extension *ext = cmark_find_syntax_extension("my_test");
    if (ext) {
        cmark_parser_attach_syntax_extension(parser, ext);
        test_result(true, "Custom cmark extension named 'my_test' registered");
    } else {
        test_result(false, "Unable to find custom cmark extension named 'my_test'!");
    }
}

static void my_cmark_done_callback(__attribute__((unused)) struct cmark_parser *parser, __attribute__((unused)) const apex_options *options, __attribute__((unused)) int cmark_opts, __attribute__((unused)) void *user_data) {
    test_result(true, "Custom cmark done callback called");
}

void test_cmark_callback(void) {
    int suite_failures = suite_start();
    print_suite_title("Cmark Callbacks Tests", false, true);

    apex_options opts = apex_options_default();
    opts.cmark_init = my_cmark_init_callback;
    opts.cmark_done = my_cmark_done_callback;

    char *html;

    const char *s = "#Custom cmark extension test\n\nHi " DELIMITER_STR DELIMITER_STR "this text must be highlighted" DELIMITER_STR DELIMITER_STR "!";
    html = apex_markdown_to_html(s, strlen(s), &opts);
    assert_contains(html, "<div class=\"custom\">", "Custom cmark extension");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Cmark Callbacks Tests", had_failures, false);
}
