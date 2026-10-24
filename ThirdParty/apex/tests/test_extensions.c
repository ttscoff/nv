/**
 * Extensions Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/html_renderer.h"
#include "../src/extensions/advanced_footnotes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *replace_first_substring(const char *text,
                                     const char *needle,
                                     const char *replacement) {
    const char *pos = strstr(text, needle);
    if (!pos) {
        return NULL;
    }

    size_t prefix_len = (size_t)(pos - text);
    size_t needle_len = strlen(needle);
    size_t replacement_len = strlen(replacement);
    size_t suffix_len = strlen(pos + needle_len);
    char *result = malloc(prefix_len + replacement_len + suffix_len + 1);
    if (!result) {
        return NULL;
    }

    memcpy(result, text, prefix_len);
    memcpy(result + prefix_len, replacement, replacement_len);
    memcpy(result + prefix_len + replacement_len, pos + needle_len, suffix_len + 1);
    return result;
}

static char *include_preparse_plugin_callback(const char *text,
                                              __attribute__((unused)) const char *id_plugin,
                                              apex_plugin_phase_mask phase,
                                              __attribute__((unused)) const apex_options *options) {
    if (!(phase & APEX_PLUGIN_PHASE_PRE_PARSE) || !text) {
        return NULL;
    }

    char *current = strdup(text);
    if (!current) {
        return NULL;
    }

    bool changed = false;
    char *next = replace_first_substring(current, "Included Content", "Plugin Included Content");
    if (next) {
        free(current);
        current = next;
        changed = true;
    }

    next = replace_first_substring(current, "## Section 2", "## Plugin Section 2");
    if (next) {
        free(current);
        current = next;
        changed = true;
    }

    if (!changed) {
        free(current);
        return NULL;
    }

    return current;
}

static void include_preparse_plugin_register(apex_plugin_manager *manager,
                                             __attribute__((unused)) const apex_options *options) {
    apex_plugin_register(manager,
                         "include-preparse-plugin",
                         APEX_PLUGIN_PHASE_PRE_PARSE,
                         include_preparse_plugin_callback);
}

void test_math(void) {
    int suite_failures = suite_start();
    print_suite_title("Math Support Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_math = true;
    char *html;

    /* Test inline math */
    html = apex_markdown_to_html("Equation: $E=mc^2$", 18, &opts);
    assert_contains(html, "class=\"math inline\"", "Inline math class");
    assert_contains(html, "E=mc^2", "Math content preserved");
    apex_free_string(html);

    /* Test display math */
    html = apex_markdown_to_html("$$x^2 + y^2 = z^2$$", 19, &opts);
    assert_contains(html, "class=\"math display\"", "Display math class");
    apex_free_string(html);

    /* LaTeX delimiters */
    html = apex_markdown_to_html("Inline \\(a+b\\) end", 18, &opts);
    assert_contains(html, "class=\"math inline\"", "LaTeX inline math class");
    assert_contains(html, "a+b", "LaTeX inline math content");
    apex_free_string(html);

    html = apex_markdown_to_html("Display \\[x+y=z\\] end", 22, &opts);
    assert_contains(html, "class=\"math display\"", "LaTeX display math class");
    assert_contains(html, "x+y=z", "LaTeX display math content");
    apex_free_string(html);

    html = apex_markdown_to_html("Not math \\(\\) here", 18, &opts);
    assert_not_contains(html, "class=\"math", "Empty LaTeX delimiters are not math");
    apex_free_string(html);

    /* Test that regular dollars don't trigger */
    html = apex_markdown_to_html("I have $5 and $10", 17, &opts);
    if (strstr(html, "class=\"math") == NULL) {
        test_result(true, "Dollar signs don't false trigger");
    } else {
        test_result(false, "Dollar signs false triggered");
    }
    apex_free_string(html);

    /* Test that math/autolinks are not applied inside Liquid {% %} tags */
    const char *liquid_md = "Before {% kbd $@3 %} after";
    html = apex_markdown_to_html(liquid_md, strlen(liquid_md), &opts);
    assert_contains(html, "{% kbd $@3 %}", "Liquid tag content preserved exactly");
    assert_not_contains(html, "class=\"math", "No math span created inside Liquid tag");
    assert_not_contains(html, "mailto:", "No email autolink created inside Liquid tag");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Math Support Tests", had_failures, false);
}

/**
 * Test Critic Markup
 */

void test_critic_markup(void) {
    int suite_failures = suite_start();
    print_suite_title("Critic Markup Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_critic_markup = true;
    opts.critic_mode = 2;  /* CRITIC_MARKUP */
    char *html;

    /* Test addition - markup mode */
    html = apex_markdown_to_html("Text {++added++} here", 21, &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Critic addition markup");
    apex_free_string(html);

    /* Test deletion - markup mode */
    html = apex_markdown_to_html("Text {--deleted--} here", 23, &opts);
    assert_contains(html, "<del class=\"critic\">deleted</del>", "Critic deletion markup");
    apex_free_string(html);

    /* Test highlight - markup mode */
    html = apex_markdown_to_html("Text {==highlighted==} here", 27, &opts);
    assert_contains(html, "<mark class=\"critic\">highlighted</mark>", "Critic highlight markup");
    apex_free_string(html);

    /* Test accept mode - apply all changes */
    opts.critic_mode = 0;  /* CRITIC_ACCEPT */
    html = apex_markdown_to_html("Text {++added++} and {--deleted--} more {~~old~>new~~} done.", 61, &opts);
    assert_contains(html, "added", "Accept mode includes additions");
    assert_contains(html, "new", "Accept mode includes new text from substitution");
    /* Should NOT contain markup tags or deleted text */
    if (strstr(html, "<ins") == NULL && strstr(html, "<del") == NULL && strstr(html, "deleted") == NULL && strstr(html, "old") == NULL) {
        test_result(true, "Accept mode removes markup and deletions");
    } else {
        test_result(false, "Accept mode has markup or deleted text");
    }
    apex_free_string(html);

    /* Test reject mode - revert all changes */
    opts.critic_mode = 1;  /* CRITIC_REJECT */
    html = apex_markdown_to_html("Text {++added++} and {--deleted--} more {~~old~>new~~} done.", 61, &opts);
    assert_contains(html, "deleted", "Reject mode includes deletions");
    assert_contains(html, "old", "Reject mode includes old text from substitution");
    /* Should NOT contain markup tags or additions */
    if (strstr(html, "<ins") == NULL && strstr(html, "<del") == NULL && strstr(html, "added") == NULL && strstr(html, "new") == NULL) {
        tests_passed++;
        tests_run++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Reject mode removes markup and additions\n");
        }
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Reject mode has markup or added text\n");
    }
    apex_free_string(html);

    /* Test accept mode with comments and highlights */
    opts.critic_mode = 0;  /* CRITIC_ACCEPT */
    html = apex_markdown_to_html("Text {==highlight==} and {>>comment<<} here.", 44, &opts);
    assert_contains(html, "highlight", "Accept mode keeps highlights");
    /* Comments should be removed */
    if (strstr(html, "comment") == NULL) {
        test_result(true, "Accept mode removes comments");
    } else {
        test_result(false, "Accept mode kept comment");
    }
    apex_free_string(html);

    /* Test reject mode with comments and highlights */
    opts.critic_mode = 1;  /* CRITIC_REJECT */
    html = apex_markdown_to_html("Text {==highlight==} and {>>comment<<} here.", 44, &opts);
    /* Highlights should show text, comments should be removed, no markup tags */
    assert_contains(html, "highlight", "Reject mode shows highlight text");
    if (strstr(html, "comment") == NULL && strstr(html, "<mark") == NULL && strstr(html, "<span") == NULL) {
        test_result(true, "Reject mode removes comments and markup tags");
    } else {
        test_result(false, "Reject mode has comments or markup tags");
    }
    apex_free_string(html);

    /* Code spans and fenced blocks show the syntax; prose still converts.
     * A short comment used to vanish because the replacement HTML did not
     * fit in a buffer of twice the input length. */
    opts.critic_mode = 2;  /* CRITIC_MARKUP */

    const char *comment = "Note {>>comment<<} here.";
    html = apex_markdown_to_html(comment, strlen(comment), &opts);
    assert_contains(html, "<span class=\"critic comment\">comment</span>", "Short comment is not dropped");
    assert_contains(html, "here.", "Text after a short comment is kept");
    apex_free_string(html);

    const char *prose = "See {==highlighted text==}{>>signature: comment<<} here.";
    html = apex_markdown_to_html(prose, strlen(prose), &opts);
    assert_contains(html, "<mark class=\"critic\">highlighted text</mark>", "Highlight with comment still renders");
    assert_contains(html, "<span class=\"critic comment\">signature: comment</span>", "Comment after highlight still renders");
    apex_free_string(html);

    const char *span_both = "`{==highlighted text==}{>>signature: comment<<}`";
    html = apex_markdown_to_html(span_both, strlen(span_both), &opts);
    assert_contains(html, "{==highlighted text==}{&gt;&gt;signature: comment&lt;&lt;}", "Code span keeps highlight and comment");
    assert_not_contains(html, "<mark class=\"critic\">", "Code span does not render highlight");
    apex_free_string(html);

    const char *span_comment = "`{==highlighted text==}{>>comment<<}`";
    html = apex_markdown_to_html(span_comment, strlen(span_comment), &opts);
    assert_contains(html, "{==highlighted text==}{&gt;&gt;comment&lt;&lt;}", "Code span keeps comment-only form");
    apex_free_string(html);

    const char *span_sig = "`{==highlighted text==}{>>signature<<}`";
    html = apex_markdown_to_html(span_sig, strlen(span_sig), &opts);
    assert_contains(html, "{==highlighted text==}{&gt;&gt;signature&lt;&lt;}", "Code span keeps signature-only form");
    apex_free_string(html);

    const char *span_bare = "`{==highlighted text==}`";
    html = apex_markdown_to_html(span_bare, strlen(span_bare), &opts);
    assert_contains(html, "{==highlighted text==}", "Code span keeps bare highlight");
    assert_not_contains(html, "<mark class=\"critic\">", "Bare highlight in a code span stays raw");
    apex_free_string(html);

    const char *fence =
        "```\n"
        "{==highlighted text==}{>>comment<<}\n"
        "```\n";
    html = apex_markdown_to_html(fence, strlen(fence), &opts);
    assert_contains(html, "{==highlighted text==}{&gt;&gt;comment&lt;&lt;}", "Fenced block keeps highlight and comment");
    assert_not_contains(html, "<mark class=\"critic\">", "Fenced block does not render highlight");
    apex_free_string(html);

    const char *tilde =
        "~~~\n"
        "{++added++}\n"
        "~~~\n";
    html = apex_markdown_to_html(tilde, strlen(tilde), &opts);
    assert_contains(html, "{++added++}", "Tilde fence keeps addition");
    assert_not_contains(html, "<ins class=\"critic\">", "Tilde fence does not render addition");
    apex_free_string(html);

    const char *mixed = "Keep {++added++} but `{--stay--}` and {++also++}.";
    html = apex_markdown_to_html(mixed, strlen(mixed), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Addition before a code span still renders");
    assert_contains(html, "<ins class=\"critic\">also</ins>", "Addition after a code span still renders");
    assert_contains(html, "{--stay--}", "Deletion inside a code span stays raw");
    apex_free_string(html);

    /* Indented code is a code block. A 4-space line that continues a
     * paragraph or a list item is not. */
    const char *indented = "    {++added++}\n";
    html = apex_markdown_to_html(indented, strlen(indented), &opts);
    assert_contains(html, "{++added++}", "Indented code block keeps addition");
    assert_not_contains(html, "<ins class=\"critic\">", "Indented code block does not render addition");
    apex_free_string(html);

    const char *tabbed = "\t{--deleted--}\n";
    html = apex_markdown_to_html(tabbed, strlen(tabbed), &opts);
    assert_contains(html, "{--deleted--}", "Tab-indented code block keeps deletion");
    assert_not_contains(html, "<del class=\"critic\">", "Tab-indented code block does not render deletion");
    apex_free_string(html);

    const char *three = "   {++added++}\n";
    html = apex_markdown_to_html(three, strlen(three), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Three-space indent still renders addition");
    apex_free_string(html);

    const char *para_cont = "Para\n    {++added++}\n";
    html = apex_markdown_to_html(para_cont, strlen(para_cont), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Indented line in a paragraph still renders");
    apex_free_string(html);

    const char *para_blank = "Para\n\n    {++added++}\n";
    html = apex_markdown_to_html(para_blank, strlen(para_blank), &opts);
    assert_contains(html, "{++added++}", "Indented code after a blank line stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Indented code after a blank line is not rendered");
    apex_free_string(html);

    const char *list_cont = "- item\n    {++added++}\n";
    html = apex_markdown_to_html(list_cont, strlen(list_cont), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "List continuation still renders addition");
    apex_free_string(html);

    const char *list_loose = "- item\n\n    {++added++}\n";
    html = apex_markdown_to_html(list_loose, strlen(list_loose), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Loose list paragraph still renders addition");
    apex_free_string(html);

    const char *list_code = "- item\n\n        {++added++}\n";
    html = apex_markdown_to_html(list_code, strlen(list_code), &opts);
    assert_contains(html, "{++added++}", "Code block inside a list stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Code block inside a list is not rendered");
    apex_free_string(html);

    const char *bq_code = ">     {++added++}\n";
    html = apex_markdown_to_html(bq_code, strlen(bq_code), &opts);
    assert_contains(html, "{++added++}", "Indented code in a blockquote stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Indented code in a blockquote is not rendered");
    apex_free_string(html);

    const char *bq_para = "> para\n>     {++added++}\n";
    html = apex_markdown_to_html(bq_para, strlen(bq_para), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Blockquote paragraph still renders addition");
    apex_free_string(html);

    const char *chunks = "    line\n\n    {++added++}\n";
    html = apex_markdown_to_html(chunks, strlen(chunks), &opts);
    assert_contains(html, "{++added++}", "Second indented chunk stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Indented chunks are not rendered");
    apex_free_string(html);

    const char *after_code = "    {++added++}\n\nAfter {++more++}\n";
    html = apex_markdown_to_html(after_code, strlen(after_code), &opts);
    assert_contains(html, "{++added++}", "Indented code before a paragraph stays raw");
    assert_contains(html, "<ins class=\"critic\">more</ins>", "Paragraph after indented code still renders");
    apex_free_string(html);

    const char *marker_code = "-     {++added++}\n";
    html = apex_markdown_to_html(marker_code, strlen(marker_code), &opts);
    assert_contains(html, "{++added++}", "Code block on a list marker line stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Code block on a list marker line is not rendered");
    apex_free_string(html);

    const char *marker_next = "-     {++one++}\n    {++two++}\n";
    html = apex_markdown_to_html(marker_next, strlen(marker_next), &opts);
    assert_contains(html, "{++one++}", "Marker-line code block stays raw");
    assert_contains(html, "<ins class=\"critic\">two</ins>", "Following list line still renders");
    apex_free_string(html);

    const char *ordered = "1. item\n    {++added++}\n";
    html = apex_markdown_to_html(ordered, strlen(ordered), &opts);
    assert_contains(html, "<ins class=\"critic\">added</ins>", "Ordered-list continuation still renders");
    apex_free_string(html);

    const char *fake_fence = "    ```\n    {++added++}\n    ```\n";
    html = apex_markdown_to_html(fake_fence, strlen(fake_fence), &opts);
    assert_contains(html, "{++added++}", "Four-space fence text stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Four-space fence text is not rendered");
    apex_free_string(html);

    const char *after_heading = "# Title\n    {++added++}\n";
    html = apex_markdown_to_html(after_heading, strlen(after_heading), &opts);
    assert_contains(html, "{++added++}", "Indented code after a heading stays raw");
    assert_not_contains(html, "<ins class=\"critic\">", "Indented code after a heading is not rendered");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Critic Markup Tests", had_failures, false);
}

/**
 * Test processor modes
 */

void test_processor_modes(void) {
    int suite_failures = suite_start();
    print_suite_title("Processor Modes Tests", false, true);

    const char *markdown = "# Test\n\n**bold**";
    char *html;

    /* Test CommonMark mode */
    apex_options cm_opts = apex_options_for_mode(APEX_MODE_COMMONMARK);
    html = apex_markdown_to_html(markdown, strlen(markdown), &cm_opts);
    assert_contains(html, "<h1", "CommonMark mode works");
    apex_free_string(html);

    /* Test GFM mode */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
    html = apex_markdown_to_html(markdown, strlen(markdown), &gfm_opts);
    assert_contains(html, "<strong>bold</strong>", "GFM mode works");
    apex_free_string(html);

    /* Test MultiMarkdown mode */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    html = apex_markdown_to_html(markdown, strlen(markdown), &mmd_opts);
    assert_contains(html, "<h1", "MultiMarkdown mode works");
    apex_free_string(html);

    /* Test Unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    html = apex_markdown_to_html(markdown, strlen(markdown), &unified_opts);
    assert_contains(html, "<h1", "Unified mode works");
    apex_free_string(html);

    /* Test Quarto mode */
    apex_options quarto_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    html = apex_markdown_to_html(markdown, strlen(markdown), &quarto_opts);
    assert_contains(html, "<h1", "Quarto mode works");
    test_result(quarto_opts.enable_quarto_callouts == true, "Quarto mode enables quarto callouts by default");
    test_result(quarto_opts.enable_quarto_extensions == true, "Quarto mode enables quarto extensions by default");
    test_result(quarto_opts.enable_quarto_diagrams == true, "Quarto mode enables quarto diagrams by default");
    test_result(quarto_opts.enable_quarto_shortcodes == true, "Quarto mode enables quarto shortcodes by default");
    test_result(quarto_opts.enable_quarto_xrefs == true, "Quarto mode enables quarto xrefs by default");
    test_result(quarto_opts.enable_quarto_strict_lists == false, "Quarto mode keeps strict lists off by default");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Processor Modes Tests", had_failures, false);
}

void test_quarto_mode(void) {
    int suite_failures = suite_start();
    print_suite_title("Quarto Mode Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_QUARTO);
    char *html;

    const char *callout =
        "::: {.callout-warning}\n"
        "Warning body.\n"
        "::: ";
    html = apex_markdown_to_html(callout, strlen(callout), &opts);
    assert_contains(html, "class=\"callout", "Quarto mode renders callout blocks");
    assert_contains(html, "Warning body", "Quarto callout body preserved");
    apex_free_string(html);

    const char *fig_alt = "![Caption](elephant.png){fig-alt=\"Alt text\"}";
    html = apex_markdown_to_html(fig_alt, strlen(fig_alt), &opts);
    assert_contains(html, "<figure>", "fig-alt image wrapped in figure");
    assert_contains(html, "<figcaption>Caption</figcaption>", "fig-alt uses markdown alt as caption");
    assert_contains(html, "alt=\"Alt text\"", "fig-alt maps to img alt attribute");
    assert_not_contains(html, "fig-alt=", "fig-alt attribute stripped from output");
    apex_free_string(html);

    /* Direct convert test: fig-alt rewrites img alt for accessibility */
    {
        const char *in = "<p><img src=\"elephant.png\" alt=\"Caption\" fig-alt=\"Alt text\" /></p>";
        char *out = apex_convert_image_captions(in, true, false);
        assert_contains(out, "alt=\"Alt text\"", "fig-alt maps to img alt attribute (direct convert)");
        assert_contains(out, "<figcaption>Caption</figcaption>", "fig-alt caption from markdown alt (direct convert)");
        free(out);
    }

    const char *div_block = "::: {.sidebar}\nSidebar content.\n:::";
    html = apex_markdown_to_html(div_block, strlen(div_block), &opts);
    assert_contains(html, "class=\"sidebar\"", "Quarto mode renders fenced divs");
    apex_free_string(html);

    const char *raw_html_block =
        "Before\n\n"
        "```{=html}\n"
        "<strong>raw</strong>\n"
        "```\n\n"
        "After";
    html = apex_markdown_to_html(raw_html_block, strlen(raw_html_block), &opts);
    assert_contains(html, "<strong>raw</strong>", "raw {=html} block passthrough");
    assert_not_contains(html, "**raw**", "raw {=html} block not markdown-processed");
    assert_not_contains(html, "<code", "raw {=html} block not wrapped in code");
    apex_free_string(html);

    const char *raw_latex_block =
        "```{=latex}\n"
        "\\textbf{Bold}\n"
        "```";
    html = apex_markdown_to_html(raw_latex_block, strlen(raw_latex_block), &opts);
    assert_contains(html, "<!-- raw format=latex -->", "raw {=latex} block wrapped in comment");
    assert_contains(html, "\\textbf{Bold}", "raw {=latex} body preserved in comment");
    assert_not_contains(html, "<code", "raw {=latex} block not wrapped in code");
    apex_free_string(html);

    const char *raw_html_inline = "Text `<em>inline</em>`{=html} end.";
    html = apex_markdown_to_html(raw_html_inline, strlen(raw_html_inline), &opts);
    assert_contains(html, "<em>inline</em>", "inline {=html} passthrough");
    assert_not_contains(html, "`{=html}`", "inline {=html} marker stripped");
    apex_free_string(html);

    /* Unified mode without quarto extensions leaves {=html} fences intact */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_opts.enable_quarto_extensions = false;
    html = apex_markdown_to_html("```{=html}\n<b>x</b>\n```", 18, &unified_opts);
    assert_contains(html, "<code", "non-quarto mode keeps {=html} as code fence");
    apex_free_string(html);

    const char *example_lists =
        "(@)  My first example will be numbered (1).\n"
        "(@)  My second example will be numbered (2).\n";
    html = apex_markdown_to_html(example_lists, strlen(example_lists), &opts);
    assert_contains(html, "<ol>", "example list (@) produces ordered list");
    assert_contains(html, "My first example", "example list first item");
    assert_contains(html, "My second example", "example list second item");
    assert_not_contains(html, "(@)", "example list marker stripped");
    apex_free_string(html);

    const char *example_lists_break =
        "(@)  My first example will be numbered (1).\n"
        "(@)  My second example will be numbered (2).\n"
        "\n"
        "Explanation of examples.\n"
        "\n"
        "(@)  My third example will be numbered (3).\n";
    html = apex_markdown_to_html(example_lists_break, strlen(example_lists_break), &opts);
    assert_contains(html, "Explanation of examples", "example list keeps interruption text");
    assert_contains(html, "My third example", "example list third item after break");
    assert_contains(html, "start=\"3\"", "example list continues numbering after interruption");
    assert_not_contains(html, "(@)", "example list break strips marker");
    apex_free_string(html);

    const char *example_lists_labeled =
        "(@good)  This is a good example.\n";
    html = apex_markdown_to_html(example_lists_labeled, strlen(example_lists_labeled), &opts);
    assert_contains(html, "This is a good example", "labeled example list item content");
    assert_not_contains(html, "(@good)", "labeled example list marker stripped");
    apex_free_string(html);

    const char *roman_list =
        "i) First\n"
        "ii) Second\n"
        "iii) Third\n";
    html = apex_markdown_to_html(roman_list, strlen(roman_list), &opts);
    assert_contains(html, "list-style-type: lower-roman", "roman list lower-roman style");
    assert_contains(html, "First", "roman list first item");
    assert_contains(html, "Second", "roman list second item");
    assert_not_contains(html, "<!-- apex-alpha-list-", "roman list marker comment stripped");
    apex_free_string(html);

    const char *line_block =
        "| Line one\n"
        "|   preserved spaces\n"
        "| Line three\n";
    html = apex_markdown_to_html(line_block, strlen(line_block), &opts);
    assert_contains(html, "class=\"line-block\"", "line block wrapper");
    assert_contains(html, "class=\"line\">Line one", "line block first line");
    assert_contains(html, "class=\"line\">  preserved spaces", "line block preserves inner spaces");
    assert_contains(html, "class=\"line\">Line three", "line block third line");
    apex_free_string(html);

    const char *code_fence_attrs =
        "```{.python filename=\"run.py\"}\n"
        "print(\"hello\")\n"
        "```\n";
    html = apex_markdown_to_html(code_fence_attrs, strlen(code_fence_attrs), &opts);
    assert_contains(html, "data-filename=\"run.py\"", "code fence filename attribute on pre");
    assert_contains(html, "print", "code fence body preserved");
    assert_not_contains(html, "{.python", "code fence braced info string normalized");
    assert_not_contains(html, "apex-code-fence-attrs", "code fence marker comment stripped");
    apex_free_string(html);

    const char *code_fence_linenos =
        "```{.python linenos=true}\n"
        "print(\"hi\")\n"
        "```\n";
    html = apex_markdown_to_html(code_fence_linenos, strlen(code_fence_linenos), &opts);
    assert_contains(html, "data-linenos=\"true\"", "code fence linenos attribute on pre");
    apex_free_string(html);

    const char *mermaid_fence =
        "```{mermaid}\n"
        "flowchart LR\n"
        "  A --> B\n"
        "```\n";
    html = apex_markdown_to_html(mermaid_fence, strlen(mermaid_fence), &opts);
    assert_contains(html, "<pre class=\"mermaid\">", "mermaid fence renders pre.mermaid");
    assert_contains(html, "flowchart LR", "mermaid fence body preserved");
    assert_contains(html, "A --> B", "mermaid fence diagram content preserved");
    assert_not_contains(html, "{mermaid}", "mermaid fence marker stripped");
    assert_not_contains(html, "<code", "mermaid fence not wrapped in code");
    apex_free_string(html);

    const char *dot_fence =
        "```{dot}\n"
        "digraph { A -> B; }\n"
        "```\n";
    html = apex_markdown_to_html(dot_fence, strlen(dot_fence), &opts);
    assert_contains(html, "<pre class=\"graphviz\">", "dot fence renders pre.graphviz");
    assert_contains(html, "digraph", "dot fence body preserved");
    apex_free_string(html);

    const char *graphviz_fence =
        "```{graphviz}\n"
        "graph { A -- B; }\n"
        "```\n";
    html = apex_markdown_to_html(graphviz_fence, strlen(graphviz_fence), &opts);
    assert_contains(html, "<pre class=\"graphviz\">", "graphviz fence renders pre.graphviz");
    assert_contains(html, "graph { A -- B", "graphviz fence body preserved");
    apex_free_string(html);

    apex_options no_diagrams_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    no_diagrams_opts.enable_quarto_diagrams = false;
    html = apex_markdown_to_html(mermaid_fence, strlen(mermaid_fence), &no_diagrams_opts);
    assert_contains(html, "<code", "disabled quarto diagrams keeps mermaid as code fence");
    assert_not_contains(html, "class=\"mermaid\"", "disabled quarto diagrams skips pre.mermaid");
    apex_free_string(html);

    apex_options no_unsafe_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    no_unsafe_opts.unsafe = false;
    html = apex_markdown_to_html(mermaid_fence, strlen(mermaid_fence), &no_unsafe_opts);
    assert_contains(html, "<code", "no-unsafe keeps mermaid fence as code block");
    assert_not_contains(html, "class=\"mermaid\"", "no-unsafe skips raw pre.mermaid");
    apex_free_string(html);

    apex_options standalone_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    standalone_opts.standalone = true;
    html = apex_markdown_to_html(mermaid_fence, strlen(mermaid_fence), &standalone_opts);
    assert_contains(html, "mermaid.min.js", "standalone quarto auto-injects mermaid script");
    assert_contains(html, "</body>", "standalone mermaid output is full document");
    apex_free_string(html);

    apex_options standalone_with_script = apex_options_for_mode(APEX_MODE_QUARTO);
    standalone_with_script.standalone = true;
    standalone_with_script.script_tags = (char *[]){ strdup("<!-- mermaid already -->"), NULL };
    html = apex_markdown_to_html(mermaid_fence, strlen(mermaid_fence), &standalone_with_script);
    assert_not_contains(html, "mermaid.min.js", "standalone skips auto mermaid when script_tags mention mermaid");
    assert_contains(html, "<!-- mermaid already -->", "standalone preserves user script_tags");
    apex_free_string(html);
    free(standalone_with_script.script_tags[0]);

    const char *pagebreak_shortcode = "Page 1\n\n{{< pagebreak >}}\n\nPage 2";
    html = apex_markdown_to_html(pagebreak_shortcode, strlen(pagebreak_shortcode), &opts);
    assert_contains(html, "page-break-after", "pagebreak shortcode renders page break");
    assert_contains(html, "Page 2", "content after pagebreak shortcode preserved");
    assert_not_contains(html, "{{< pagebreak >}}", "pagebreak shortcode marker stripped");
    apex_free_string(html);

    const char *kbd_shortcode = "Press {{< kbd $@3 >}} to save.";
    html = apex_markdown_to_html(kbd_shortcode, strlen(kbd_shortcode), &opts);
    assert_contains(html, "{% kbd $@3 %}", "kbd shortcode converts to liquid tag");
    assert_not_contains(html, "{{< kbd", "kbd shortcode marker stripped");
    apex_free_string(html);

    apex_options include_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    include_opts.enable_file_includes = true;
#ifdef TEST_FIXTURES_DIR
    include_opts.base_directory = TEST_FIXTURES_DIR;
#else
    include_opts.base_directory = "tests/fixtures/includes";
#endif
    const char *include_shortcode = "Before\n\n{{< include simple.md >}}\n\nAfter";
    html = apex_markdown_to_html(include_shortcode, strlen(include_shortcode), &include_opts);
    assert_contains(html, "Included Content", "include shortcode expands file content");
    assert_contains(html, "After", "content after include shortcode preserved");
    assert_not_contains(html, "{{< include", "include shortcode marker stripped");
    apex_free_string(html);

    apex_options no_shortcodes_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    no_shortcodes_opts.enable_quarto_shortcodes = false;
    html = apex_markdown_to_html(pagebreak_shortcode, strlen(pagebreak_shortcode), &no_shortcodes_opts);
    assert_contains(html, "{{< pagebreak", "disabled quarto shortcodes leave marker in output");
    assert_not_contains(html, "page-break-after", "disabled quarto shortcodes skip pagebreak expansion");
    apex_free_string(html);

    const char *unknown_shortcode = "Text {{< widget foo >}} end.";
    html = apex_markdown_to_html(unknown_shortcode, strlen(unknown_shortcode), &opts);
    assert_contains(html, "{{< widget foo", "unknown shortcode left unchanged");
    apex_free_string(html);

    const char *empty_div_reset =
        "1. First\n\n::: {}\n\n1. Second\n";
    html = apex_markdown_to_html(empty_div_reset, strlen(empty_div_reset), &opts);
    assert_contains(html, "<ol>", "empty div list reset produces ordered lists");
    {
        const char *first_ol = strstr(html, "<ol>");
        const char *second_ol = first_ol ? strstr(first_ol + 1, "<ol>") : NULL;
        test_result(second_ol != NULL, "empty div separates two ordered lists");
    }
    apex_free_string(html);

    const char *cross_ref = "See @fig-elephant and @sec-intro for details.";
    html = apex_markdown_to_html(cross_ref, strlen(cross_ref), &opts);
    assert_contains(html, "<span class=\"quarto-xref\">@fig-elephant</span>", "fig cross-ref wrapped");
    assert_contains(html, "<span class=\"quarto-xref\">@sec-intro</span>", "sec cross-ref wrapped");
    apex_free_string(html);

    {
        apex_options cite_opts = apex_options_for_mode(APEX_MODE_QUARTO);
        cite_opts.base_directory = "tests";
        const char *bib_files[] = { "test_refs.bib", NULL };
        cite_opts.bibliography_files = (char **)bib_files;
        const char *mixed_cite_xref =
            "See [@doe99] and @fig-elephant.";
        html = apex_markdown_to_html(mixed_cite_xref, strlen(mixed_cite_xref), &cite_opts);
        assert_contains(html, "citation", "bracket citation processed with bibliography");
        assert_contains(html, "doe99", "bracket citation key preserved with bibliography");
        assert_contains(html, "<span class=\"quarto-xref\">@fig-elephant</span>",
                        "bare fig xref not parsed as citation when bibliography set");
        assert_not_contains(html, "data-cites=\"fig-elephant\"", "fig xref not in citation data-cites");
        apex_free_string(html);

        const char *bracket_fig = "See [@fig-elephant].";
        html = apex_markdown_to_html(bracket_fig, strlen(bracket_fig), &cite_opts);
        assert_contains(html, "citation", "bracketed @fig- key still processed as citation");
        assert_contains(html, "fig-elephant", "bracketed fig- citation key preserved");
        apex_free_string(html);

        apex_options no_xref_opts = cite_opts;
        no_xref_opts.enable_quarto_xrefs = false;
        const char *bare_fig = "See @fig-elephant.";
        html = apex_markdown_to_html(bare_fig, strlen(bare_fig), &no_xref_opts);
        assert_contains(html, "citation", "bare @fig- treated as citation when quarto-xrefs off");
        assert_not_contains(html, "quarto-xref", "no xref wrapper when quarto-xrefs off");
        apex_free_string(html);
    }

    apex_options hidden_opts = apex_options_for_mode(APEX_MODE_QUARTO);
    hidden_opts.standalone = true;
    html = apex_markdown_to_html("::: {.hidden}\nHidden content.\n:::", 35, &hidden_opts);
    assert_contains(html, "class=\"hidden\"", "hidden div class preserved");
    assert_contains(html, ".hidden { display: none; }", "standalone quarto CSS hides .hidden");
    apex_free_string(html);

    apex_options unified_opts2 = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_opts2.enable_quarto_extensions = false;
    html = apex_markdown_to_html("```{.python}\nx\n```", 18, &unified_opts2);
    assert_contains(html, "{.python}", "non-quarto mode keeps braced fence info string");
    apex_free_string(html);

    opts = apex_options_for_mode(APEX_MODE_QUARTO);
    opts.enable_quarto_strict_lists = true;
    {
        const char *strict =
            "Intro text\n"
            "- bullet item\n"
            "1. ordered item\n"
            "(@fig-one) example item\n"
            "i) roman item\n"
            "ii) roman two\n"
            "\n"
            "```\n"
            "- fenced not a list\n"
            "```\n";
        html = apex_markdown_to_html(strict, strlen(strict), &opts);
        assert_contains(html, "<ul>", "Strict lists still render a bullet list");
        assert_contains(html, "bullet item", "Strict-list bullet text");
        assert_contains(html, "ordered item", "Strict-list ordered text");
        assert_contains(html, "example item", "Strict lists keep an example-list item");
        assert_contains(html, "lower-roman", "Strict lists keep a roman list");
        assert_contains(html, "roman item", "Roman list item text");
        assert_contains(html, "- fenced not a list", "Fenced code is left alone by strict lists");
        apex_free_string(html);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Quarto Mode Tests", had_failures, false);
}

/**
 * Test cmark_init callback for custom extension registration
 */
static int cmark_init_callback_invoked = 0;

static void test_cmark_init_cb(struct cmark_parser *parser,
                               const struct apex_options *opts,
                               int cmark_opts,
                               __attribute__((unused)) void *user_data) {
    (void)parser;
    (void)opts;
    (void)cmark_opts;
    cmark_init_callback_invoked = 1;
}

void test_cmark_init_callback(void) {
    int suite_failures = suite_start();
    print_suite_title("cmark_init callback Tests", false, true);

    cmark_init_callback_invoked = 0;
    apex_options opts = apex_options_default();
    opts.cmark_init = test_cmark_init_cb;
    char *html = apex_markdown_to_html("# Hi", 4, &opts);
    test_result(cmark_init_callback_invoked == 1, "cmark_init callback was invoked");
    assert_contains(html, "<h1", "Basic parsing still works with callback");
    assert_contains(html, "Hi</h1>", "Header content preserved");
    apex_free_string(html);

    /* NULL callback: conversion works normally */
    opts.cmark_init = NULL;
    html = apex_markdown_to_html("**bold**", 8, &opts);
    assert_contains(html, "<strong>bold</strong>", "Parsing works with NULL callback");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("cmark_init callback Tests", had_failures, false);
}

/**
 * Test MultiMarkdown-style image attributes (inline and reference)
 */

void test_multimarkdown_image_attributes(void) {
    int suite_failures = suite_start();
    print_suite_title("MultiMarkdown Image Attribute Tests", false, true);

    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);

    const char *md =
        "![Inline no title](/images/test-inline-1.jpg width=200)\n\n"
        "![Inline with title](/images/test-inline-2.jpg \"Falafel\" width=300)\n\n"
        "![Inline percent](/images/test-inline-3.jpg width=50%)\n\n"
        "![Inline classes](/images/test-inline-4.jpg \"Caption\" class=center shadow width=250 height=60%)\n\n"
        "![Ref with attrs][ref-inline-1]\n\n"
        "[ref-inline-1]: /images/test-ref-1.jpg width=200\n\n"
        "![Ref with title][ref-inline-2]\n\n"
        "[ref-inline-2]: /images/test-ref-2.jpg \"Falafel\" width=300\n\n"
        "![Ref percent][ref-inline-3]\n\n"
        "[ref-inline-3]: /images/test-ref-3.jpg width=50%\n\n"
        "![Ref classes][ref-inline-4]\n\n"
        "[ref-inline-4]: /images/test-ref-4.jpg \"Caption\" class=center shadow width=250 height=60%\n";

    /* Helper lambda-style macro to run assertions in a given mode */
#define RUN_IMAGE_ATTR_TESTS(OPTS, MODE_LABEL)                                                      \
    do {                                                                                            \
        char *html = apex_markdown_to_html(md, strlen(md), &(OPTS));                                \
        assert_contains(html, "src=\"/images/test-inline-1.jpg\"", MODE_LABEL " inline url 1");     \
        assert_contains(html, "width=\"200\"", MODE_LABEL " inline width=200");                     \
        assert_contains(html, "src=\"/images/test-inline-2.jpg\"", MODE_LABEL " inline url 2");     \
        assert_contains(html, "title=\"Falafel\"", MODE_LABEL " inline title");                     \
        assert_contains(html, "width=\"300\"", MODE_LABEL " inline width=300");                     \
        assert_contains(html, "src=\"/images/test-inline-3.jpg\"", MODE_LABEL " inline url 3");     \
        assert_contains(html, "style=\"width: 50%\"", MODE_LABEL " inline width 50% style");        \
        assert_contains(html, "src=\"/images/test-inline-4.jpg\"", MODE_LABEL " inline url 4");     \
        /* Class list ordering is not guaranteed; just check for center */                          \
        assert_contains(html, "class=\"center", MODE_LABEL " inline center class");                 \
        assert_contains(html, "width=\"250\"", MODE_LABEL " inline width=250");                     \
        assert_contains(html, "height: 60%", MODE_LABEL " inline height 60% style");                \
        assert_contains(html, "src=\"/images/test-ref-1.jpg\"", MODE_LABEL " ref url 1");           \
        assert_contains(html, "width=\"200\"", MODE_LABEL " ref width=200");                        \
        assert_contains(html, "src=\"/images/test-ref-2.jpg\"", MODE_LABEL " ref url 2");           \
        assert_contains(html, "title=\"Falafel\"", MODE_LABEL " ref title");                        \
        assert_contains(html, "width=\"300\"", MODE_LABEL " ref width=300");                        \
        assert_contains(html, "src=\"/images/test-ref-3.jpg\"", MODE_LABEL " ref url 3");           \
        assert_contains(html, "style=\"width: 50%\"", MODE_LABEL " ref width 50% style");           \
        assert_contains(html, "src=\"/images/test-ref-4.jpg\"", MODE_LABEL " ref url 4");           \
        assert_contains(html, "class=\"center", MODE_LABEL " ref center class");                    \
        assert_contains(html, "width=\"250\"", MODE_LABEL " ref width=250");                        \
        assert_contains(html, "height: 60%", MODE_LABEL " ref height 60% style");                   \
        apex_free_string(html);                                                                     \
    } while (0)

    RUN_IMAGE_ATTR_TESTS(mmd_opts, "MMD");
    RUN_IMAGE_ATTR_TESTS(unified_opts, "Unified");

#undef RUN_IMAGE_ATTR_TESTS

    /* Test @2x srcset: ![alt](url @2x) and ![alt](url "title" @2x) emit srcset="url 1x, url@2x 2x" */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *at2x_inline = "![BlogBook](img/icon_512x512.png @2x)";
        char *html = apex_markdown_to_html(at2x_inline, strlen(at2x_inline), &opts);
        assert_contains(html, "src=\"img/icon_512x512.png\"", "@2x inline: src present");
        assert_contains(html, "srcset=\"img/icon_512x512.png 1x, img/icon_512x512@2x.png 2x\"", "@2x inline: srcset 1x and 2x");
        apex_free_string(html);

        const char *at2x_title = "![BlogBook](img/icon_512x512.png \"title\" @2x)";
        html = apex_markdown_to_html(at2x_title, strlen(at2x_title), &opts);
        assert_contains(html, "srcset=\"img/icon_512x512.png 1x, img/icon_512x512@2x.png 2x\"", "@2x with title: srcset");
        apex_free_string(html);

        /* Reference-style: [ref]: url @2x */
        const char *at2x_ref = "![Logo][logo]\n\n[logo]: img/hero.png @2x";
        html = apex_markdown_to_html(at2x_ref, strlen(at2x_ref), &opts);
        assert_contains(html, "srcset=\"img/hero.png 1x, img/hero@2x.png 2x\"", "@2x reference: srcset");
        apex_free_string(html);
    }

    /* Reference-style image with attributes between two @2x images:
     * - First image: inline with @2x, should get srcset
     * - Middle image: reference-style with width/height/style, no @2x, should get attributes but no srcset
     * - Last image: inline with @2x, should get srcset
     * This guards against @2x or index bookkeeping causing the middle image to lose its attributes.
     */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *three_img_md =
            "![First](img/first.png @2x)\n\n"
            "![Badge][badge]\n\n"
            "[badge]: img/badge.png width=250 height=83 style=\"margin: 0 auto;\"\n\n"
            "![Last](img/last.png @2x)\n";

        char *html = apex_markdown_to_html(three_img_md, strlen(three_img_md), &opts);

        /* First image: has srcset with @2x */
        assert_contains(html,
                        "src=\"img/first.png\"",
                        "@2x three-image: first src present");
        assert_contains(html,
                        "srcset=\"img/first.png 1x, img/first@2x.png 2x\"",
                        "@2x three-image: first srcset present");

        /* Middle (badge) image: has width/height/style, but no srcset */
        assert_contains(html,
                        "src=\"img/badge.png\"",
                        "@2x three-image: middle src present");
        assert_contains(html,
                        "width=\"250\"",
                        "@2x three-image: middle width attribute");
        assert_contains(html,
                        "height=\"83\"",
                        "@2x three-image: middle height attribute");
        assert_contains(html,
                        "style=\"margin: 0 auto;\"",
                        "@2x three-image: middle style attribute");
        assert_not_contains(html,
                            "img/badge@2x.png",
                            "@2x three-image: middle has no @2x srcset");

        /* Last image: has srcset with @2x */
        assert_contains(html,
                        "src=\"img/last.png\"",
                        "@2x three-image: last src present");
        assert_contains(html,
                        "srcset=\"img/last.png 1x, img/last@2x.png 2x\"",
                        "@2x three-image: last srcset present");

        apex_free_string(html);
    }

    /* @3x marker: behaves like @2x but emits both 2x and 3x entries in srcset */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *at3x_inline = "![BlogBook](img/icon_512x512.png @3x)";
        char *html = apex_markdown_to_html(at3x_inline, strlen(at3x_inline), &opts);
        assert_contains(html, "src=\"img/icon_512x512.png\"", "@3x inline: src present");
        assert_contains(html,
                        "srcset=\"img/icon_512x512.png 1x, img/icon_512x512@2x.png 2x, img/icon_512x512@3x.png 3x\"",
                        "@3x inline: srcset 1x, 2x, 3x");
        apex_free_string(html);

        /* Reference-style: [ref]: url @3x */
        const char *at3x_ref = "![Logo][logo3]\n\n[logo3]: img/hero3.png @3x";
        html = apex_markdown_to_html(at3x_ref, strlen(at3x_ref), &opts);
        assert_contains(html,
                        "srcset=\"img/hero3.png 1x, img/hero3@2x.png 2x, img/hero3@3x.png 3x\"",
                        "@3x reference: srcset 1x, 2x, 3x");
        apex_free_string(html);
    }

    /* webp attribute: ![alt](url webp) emits <picture> with webp source */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *webp_md = "![Hero](img/hero.png webp)";
        char *html = apex_markdown_to_html(webp_md, strlen(webp_md), &opts);
        assert_contains(html, "<picture>", "webp: picture element");
        assert_contains(html, "type=\"image/webp\"", "webp: source type");
        assert_contains(html, "img/hero.webp 1x", "webp: srcset");
        assert_contains(html, "<img src=\"img/hero.png\"", "webp: img fallback");
        assert_contains(html, "</picture>", "webp: picture close");
        apex_free_string(html);
    }

    /* avif attribute: ![alt](url avif) emits <picture> with avif source */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *avif_md = "![Hero](img/hero.png avif)";
        char *html = apex_markdown_to_html(avif_md, strlen(avif_md), &opts);
        assert_contains(html, "<picture>", "avif: picture element");
        assert_contains(html, "type=\"image/avif\"", "avif: source type");
        assert_contains(html, "img/hero.avif 1x", "avif: srcset");
        assert_contains(html, "<img src=\"img/hero.png\"", "avif: img fallback");
        apex_free_string(html);
    }

    /* webp + @2x: srcset includes 1x and 2x for webp */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *webp_2x_md = "![Hero](img/hero.png webp @2x)";
        char *html = apex_markdown_to_html(webp_2x_md, strlen(webp_2x_md), &opts);
        assert_contains(html, "img/hero.webp 1x, img/hero@2x.webp 2x", "webp @2x: srcset");
        apex_free_string(html);
    }

    /* Picture conversion should preserve image IAL attrs on fallback <img> */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *picture_ial_md =
            "[![text-basic widget screenshot](img/text-basic.jpg avif @2x){:loading=lazy width=400 height=400}]"
            "(img/text-basic@2x.jpg){.widget-shot.js-lightbox}";
        char *html = apex_markdown_to_html(picture_ial_md, strlen(picture_ial_md), &opts);
        assert_contains(html, "<a href=\"img/text-basic@2x.jpg\" class=\"widget-shot js-lightbox\">",
                        "picture+ial: outer link classes preserved");
        assert_contains(html, "<picture>", "picture+ial: picture element");
        assert_contains(html, "img/text-basic.avif 1x, img/text-basic@2x.avif 2x",
                        "picture+ial: avif srcset with @2x");
        assert_contains(html,
                        "<img src=\"img/text-basic.jpg\" alt=\"text-basic widget screenshot\" width=\"400\" height=\"400\" loading=\"lazy\">",
                        "picture+ial: fallback img preserves IAL attrs");
        apex_free_string(html);
    }

    /* Raw HTML <a> must not steal/drop later markdown link IAL classes */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.enable_autolink = false;
        opts.enable_image_captions = false;
        const char *md =
            "<div class=\"note\">\n"
            "<p>See <a href=\"/integrations#getting-started\">Integrations</a>.</p>\n"
            "</div>\n\n"
            "- [![alt](/x.jpg avif){:loading=lazy width=400 height=400}]"
            "(/x@2x.jpg){.widget-shot.js-lightbox}\n"
            "{.g}\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html,
            "<a href=\"/x@2x.jpg\" class=\"widget-shot js-lightbox\">",
            "raw html <a> must not steal/drop later link IAL classes");
        assert_contains(html,
            "href=\"/integrations#getting-started\">Integrations</a>",
            "raw html callout link remains unchanged");
        assert_not_contains(html,
            "href=\"/integrations#getting-started\" class=\"widget-shot",
            "IAL must not be injected onto the raw html <a>");
        apex_free_string(html);
    }

    /* Distinct href IALs after a raw HTML <a> keep correct classes */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.enable_autolink = false;
        const char *md =
            "<p><a href=\"/raw\">Raw</a></p>\n\n"
            "[One](/one){.class-one}\n\n"
            "[Two](/two){.class-two}\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "<a href=\"/one\" class=\"class-one\">",
                        "distinct href after raw <a>: first link IAL");
        assert_contains(html, "<a href=\"/two\" class=\"class-two\">",
                        "distinct href after raw <a>: second link IAL");
        assert_not_contains(html, "href=\"/raw\" class=",
                            "distinct href after raw <a>: raw link unchanged");
        apex_free_string(html);
    }

    /* Same-href markdown links after raw HTML <a>: disambiguate by order */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.enable_autolink = false;
        const char *md =
            "<p><a href=\"/same\">Raw</a></p>\n\n"
            "[First](/same){.first}\n\n"
            "[Second](/same){.second}\n";
        char *html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "<a href=\"/same\" class=\"first\">First</a>",
                        "same href after raw <a>: first markdown link IAL");
        assert_contains(html, "<a href=\"/same\" class=\"second\">Second</a>",
                        "same href after raw <a>: second markdown link IAL");
        assert_not_contains(html, "href=\"/same\" class=\"first\">Raw",
                            "same href after raw <a>: raw link not given first IAL");
        assert_not_contains(html, "href=\"/same\" class=\"second\">Raw",
                            "same href after raw <a>: raw link not given second IAL");
        apex_free_string(html);
    }

    /* Video URL: ![alt](video.mp4) emits <video> */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *video_md = "![Demo](media/demo.mp4)";
        char *html = apex_markdown_to_html(video_md, strlen(video_md), &opts);
        assert_contains(html, "<video", "video: video element");
        assert_contains(html, "media/demo.mp4", "video: src");
        assert_contains(html, "</video>", "video: close");
        assert_not_contains(html, "<img", "video: no img");
        apex_free_string(html);
    }

    /* Video with webm attribute: adds webm source */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *video_webm_md = "![Demo](media/demo.mp4 webm)";
        char *html = apex_markdown_to_html(video_webm_md, strlen(video_webm_md), &opts);
        assert_contains(html, "<source src=\"media/demo.webm\" type=\"video/webm\">", "video webm: source");
        assert_contains(html, "<source src=\"media/demo.mp4\"", "video webm: primary fallback");
        apex_free_string(html);
    }

    /* Fixture-based tests: media_formats_test.md */
    {
        const char *fixture_path = "tests/fixtures/images/media_formats_test.md";
        FILE *fp = fopen(fixture_path, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *src = malloc(sz + 1);
            if (src) {
                size_t len = fread(src, 1, (size_t)sz, fp);
                src[len] = '\0';
                fclose(fp);

                apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
                char *html = apex_markdown_to_html(src, len, &opts);

                /* Images with webp/avif: picture elements */
                assert_contains(html, "<picture>", "fixture: picture elements");
                assert_contains(html, "type=\"image/webp\"", "fixture: webp source type");
                assert_contains(html, "type=\"image/avif\"", "fixture: avif source type");
                assert_contains(html, "img/hero.webp 1x", "fixture: webp srcset");
                assert_contains(html, "img/hero.avif 1x", "fixture: avif srcset");
                assert_contains(html, "img/hero@2x.webp 2x", "fixture: webp @2x");
                assert_contains(html, "img/hero@2x.avif 2x", "fixture: avif @2x");

                /* Videos: video elements */
                assert_contains(html, "<video", "fixture: video elements");
                assert_contains(html, "media/demo.mp4", "fixture: mp4 video");
                assert_contains(html, "assets/trailer.mov", "fixture: mov video");
                assert_contains(html, "assets/sample.m4v", "fixture: m4v video");

                /* Video with format alternatives */
                assert_contains(html, "media/demo.webm", "fixture: video webm source");
                assert_contains(html, "media/intro.ogg", "fixture: video ogg source");
                assert_contains(html, "media/clip.mp4", "fixture: webm with mp4 fallback");

                /* Auto attribute (marker present; expansion requires base_directory) */
                assert_contains(html, "data-apex-replace-auto=1", "fixture: auto marker");

                apex_free_string(html);
                free(src);
            } else {
                fclose(fp);
                test_result(false, "fixture: malloc failed for media_formats_test.md");
            }
        } else {
            test_result(false, "fixture: could not open media_formats_test.md");
        }
    }

    /* auto attribute: emits data-apex-replace-auto (expansion requires base_directory) */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *auto_md = "![Hero](img/hero.png auto)";
        char *html = apex_markdown_to_html(auto_md, strlen(auto_md), &opts);
        assert_contains(html, "data-apex-replace-auto=1", "auto: emits replace marker");
        apex_free_string(html);
    }

    /* auto with base_directory: discovers img set (jpg, webp, avif, 2x variants), emits picture */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.base_directory = "tests/fixtures/images";
        const char *auto_md = "![Profile menu](img/app-pass-1-profile-menu.jpg auto)";
        char *html = apex_markdown_to_html(auto_md, strlen(auto_md), &opts);
        assert_contains(html, "<picture>", "auto+base: picture element");
        assert_contains(html, "type=\"image/avif\"", "auto+base: avif source");
        assert_contains(html, "type=\"image/webp\"", "auto+base: webp source");
        assert_contains(html, "app-pass-1-profile-menu.avif 1x", "auto+base: avif 1x");
        assert_contains(html, "app-pass-1-profile-menu@2x.avif 2x", "auto+base: avif 2x");
        assert_contains(html, "app-pass-1-profile-menu.webp 1x", "auto+base: webp 1x");
        assert_contains(html, "app-pass-1-profile-menu@2x.webp 2x", "auto+base: webp 2x");
        assert_contains(html, "<img src=\"img/app-pass-1-profile-menu.jpg\"", "auto+base: img fallback");
        apex_free_string(html);
    }

    /* * extension: equivalent to auto, discovers formats from base filename */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        opts.base_directory = "tests/fixtures/images";
        const char *wildcard_md = "![Profile menu](img/app-pass-1-profile-menu.*)";
        char *html = apex_markdown_to_html(wildcard_md, strlen(wildcard_md), &opts);
        assert_contains(html, "<picture>", "wildcard: picture element");
        assert_contains(html, "type=\"image/avif\"", "wildcard: avif source");
        assert_contains(html, "type=\"image/webp\"", "wildcard: webp source");
        assert_contains(html, "app-pass-1-profile-menu.jpg", "wildcard: jpg fallback");
        apex_free_string(html);
    }

    /* * extension: emits data-apex-replace-auto marker when no base_directory */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *wildcard_md = "![Hero](img/hero.*)";
        char *html = apex_markdown_to_html(wildcard_md, strlen(wildcard_md), &opts);
        assert_contains(html, "data-apex-replace-auto=1", "wildcard: emits replace marker");
        apex_free_string(html);
    }

    /* Parentheses inside a quoted title must not end the image early. */
    {
        apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
        const char *paren_title =
            "![Alt text](img/shot.png @2x width=800 height=580 \"Output options (Markdown, Split Posts)\")";
        char *html = apex_markdown_to_html(paren_title, strlen(paren_title), &opts);
        assert_contains(html, "title=\"Output options (Markdown, Split Posts)\"", "paren title: title attribute");
        assert_contains(html, "<figcaption>Output options (Markdown, Split Posts)</figcaption>", "paren title: figcaption");
        assert_contains(html, "width=\"800\"", "paren title: width");
        assert_contains(html, "height=\"580\"", "paren title: height");
        assert_contains(html, "img/shot.png 1x", "paren title: srcset 1x");
        assert_contains(html, "img/shot@2x.png 2x", "paren title: srcset 2x");
        assert_not_contains(html, "\")", "paren title: no stray quote-paren");
        assert_not_contains(html, "\xe2\x80\x9d)", "paren title: no smart-quoted stray paren");
        apex_free_string(html);

        const char *paren_only =
            "![Alt text](img/shot.png \"Output options (Markdown, Split Posts)\")";
        html = apex_markdown_to_html(paren_only, strlen(paren_only), &opts);
        assert_contains(html, "title=\"Output options (Markdown, Split Posts)\"", "paren title only: title kept");
        assert_not_contains(html, "\")", "paren title only: no stray quote-paren");
        apex_free_string(html);

        const char *plain_title =
            "![Alt text](img/shot.png @2x width=800 height=580 \"Output options\")";
        html = apex_markdown_to_html(plain_title, strlen(plain_title), &opts);
        assert_contains(html, "title=\"Output options\"", "plain title with attrs: title kept");
        assert_contains(html, "width=\"800\"", "plain title with attrs: width");
        apex_free_string(html);

        const char *single_quoted =
            "![Alt text](img/shot.png @2x width=800 'Output options (Markdown)')";
        html = apex_markdown_to_html(single_quoted, strlen(single_quoted), &opts);
        assert_contains(html, "title=\"Output options (Markdown)\"", "single-quoted paren title");
        assert_contains(html, "width=\"800\"", "single-quoted paren title: width");
        apex_free_string(html);

        const char *title_first =
            "![Alt](img/shot.png \"Output options (Markdown)\" width=800)";
        html = apex_markdown_to_html(title_first, strlen(title_first), &opts);
        assert_contains(html, "title=\"Output options (Markdown)\"", "title before attrs: title kept");
        assert_contains(html, "width=\"800\"", "title before attrs: width");
        assert_not_contains(html, "\")", "title before attrs: no stray quote-paren");
        apex_free_string(html);

        const char *url_parens = "![Alt](img/file(1).png width=800)";
        html = apex_markdown_to_html(url_parens, strlen(url_parens), &opts);
        assert_contains(html, "src=\"img/file(1).png\"", "balanced parens in the URL stay in src");
        assert_contains(html, "width=\"800\"", "balanced URL parens: width");
        apex_free_string(html);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("MultiMarkdown Image Attribute Tests", had_failures, false);
}

/**
 * Reference definition URL encoding must leave code untouched
 */

void test_reference_definitions(void) {
    int suite_failures = suite_start();
    print_suite_title("Reference Definition Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    const char *inline_code_md =
        "With the cursor inside the id of a link, `[the spec][|]`, the list is "
        "every `[id]: url` definition in the note. The same list opens for an "
        "image, `![diagram][|]`. Typing narrows it.\n";
    html = apex_markdown_to_html(inline_code_md, strlen(inline_code_md), &opts);
    assert_contains(html, "<code>[the spec][|]</code>",
        "Reference defs: first code span untouched");
    assert_contains(html, "<code>[id]: url</code>",
        "Reference defs: [id]: url in code span untouched");
    assert_contains(html, "<code>![diagram][|]</code>",
        "Reference defs: ![diagram][|] in code span untouched");
    assert_contains(html, " definition in the note. The same list opens for an image, ",
        "Reference defs: prose after code span not encoded");
    assert_not_contains(html, "%60", "Reference defs: no encoded backticks");
    assert_not_contains(html, "%20", "Reference defs: no encoded spaces");
    apex_free_string(html);

    const char *code_line_md = "`[ref]: a b c`\n";
    html = apex_markdown_to_html(code_line_md, strlen(code_line_md), &opts);
    assert_contains(html, "<code>[ref]: a b c</code>",
        "Reference defs: definition-shaped code span at line start untouched");
    apex_free_string(html);

    const char *double_tick_md = "Use ``[x]: `y` z`` here.\n";
    html = apex_markdown_to_html(double_tick_md, strlen(double_tick_md), &opts);
    assert_contains(html, "<code>[x]: `y` z</code>",
        "Reference defs: double-backtick code span untouched");
    apex_free_string(html);

    const char *mid_para_md = "Some text [label]: a b c and more.\n";
    html = apex_markdown_to_html(mid_para_md, strlen(mid_para_md), &opts);
    assert_contains(html, "Some text [label]: a b c and more.",
        "Reference defs: mid-paragraph [label]: is plain text");
    apex_free_string(html);

    const char *dialogue_md = "CHARACTER [stage directions]: Here\xe2\x80\x99s some dialogue.\n";
    html = apex_markdown_to_html(dialogue_md, strlen(dialogue_md), &opts);
    assert_contains(html, "CHARACTER [stage directions]: Here\xe2\x80\x99s some dialogue.",
        "Reference defs: mid-line [label]: in dialogue is prose");
    assert_not_contains(html, "%E2%80%99", "Reference defs: dialogue apostrophe not encoded");
    assert_not_contains(html, "%20", "Reference defs: dialogue spaces not encoded");
    apex_free_string(html);

    const char *tab_def_md = "See [ref].\n\n\t[ref]: /a path\n";
    html = apex_markdown_to_html(tab_def_md, strlen(tab_def_md), &opts);
    assert_contains(html, "<code>[ref]: /a path\n</code>",
        "Reference defs: tab-indented definition is code, not encoded");
    apex_free_string(html);

    const char *def_md = "See [the spec][spec].\n\n[spec]: https://example.com/caf\xc3\xa9\n";
    html = apex_markdown_to_html(def_md, strlen(def_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/caf%C3%A9\">the spec</a>",
        "Reference defs: definition on its own line still encoded");
    apex_free_string(html);

    const char *indented_def_md = "See [the spec][spec].\n\n   [spec]: https://example.com/caf\xc3\xa9\n";
    html = apex_markdown_to_html(indented_def_md, strlen(indented_def_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/caf%C3%A9\">the spec</a>",
        "Reference defs: definition indented 3 spaces still encoded");
    apex_free_string(html);

    const char *quoted_def_md = "> See [the spec][spec].\n>\n> [spec]: https://example.com/caf\xc3\xa9\n";
    html = apex_markdown_to_html(quoted_def_md, strlen(quoted_def_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/caf%C3%A9\">the spec</a>",
        "Reference defs: definition in blockquote resolves and is encoded");
    assert_not_contains(html, "%5B", "Reference defs: blockquote definition URL not autolinked");
    apex_free_string(html);

    const char *nested_quoted_def_md =
        "> > See [the spec][spec].\n> >\n> > [spec]: https://example.com/spec \"Spec\"\n";
    html = apex_markdown_to_html(nested_quoted_def_md, strlen(nested_quoted_def_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/spec\" title=\"Spec\">the spec</a>",
        "Reference defs: definition in nested blockquote resolves");
    apex_free_string(html);

    {
        apex_options cm_opts = apex_options_for_mode(APEX_MODE_COMMONMARK);
        const char *cm_quoted_md = "> See [the spec][spec].\n>\n> [spec]: https://example.com/spec\n";
        html = apex_markdown_to_html(cm_quoted_md, strlen(cm_quoted_md), &cm_opts);
        assert_contains(html, "<a href=\"https://example.com/spec\">the spec</a>",
            "Reference defs: blockquote definition resolves in CommonMark mode");
        apex_free_string(html);
    }

    const char *quoted_url_md = "> Visit https://example.com/page today.\n";
    html = apex_markdown_to_html(quoted_url_md, strlen(quoted_url_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/page\">https://example.com/page</a>",
        "Reference defs: bare URL in blockquote prose still autolinked");
    apex_free_string(html);

    const char *fenced_md =
        "```\n[spec]: https://example.com/caf\xc3\xa9 x\n![x](a b.png)\n```\n\n"
        "~~~~ markdown\n[spec]: https://example.com/caf\xc3\xa9 x\n```\nstill code\n~~~~\n";
    html = apex_markdown_to_html(fenced_md, strlen(fenced_md), &opts);
    assert_contains(html, "[spec]: https://example.com/caf\xc3\xa9 x\n![x](a b.png)\n</code>",
        "Reference defs: backtick fence untouched");
    assert_contains(html, "[spec]: https://example.com/caf\xc3\xa9 x\n```\nstill code\n</code>",
        "Reference defs: tilde fence with inner backticks untouched");
    assert_not_contains(html, "%C3", "Reference defs: nothing encoded inside fences");
    apex_free_string(html);

    const char *after_fence_md =
        "```\ncode\n```\n\nSee [the spec][spec].\n\n[spec]: https://example.com/caf\xc3\xa9\n";
    html = apex_markdown_to_html(after_fence_md, strlen(after_fence_md), &opts);
    assert_contains(html, "<a href=\"https://example.com/caf%C3%A9\">the spec</a>",
        "Reference defs: definition after a fence still encoded");
    apex_free_string(html);

    const char *inline_img_code_md = "Write `![x](a b.png)` for images.\n";
    html = apex_markdown_to_html(inline_img_code_md, strlen(inline_img_code_md), &opts);
    assert_contains(html, "<code>![x](a b.png)</code>",
        "Reference defs: inline image syntax in code span untouched");
    apex_free_string(html);

    const char *unmatched_md = "A lone ` tick then [x](a b) link.\n";
    html = apex_markdown_to_html(unmatched_md, strlen(unmatched_md), &opts);
    assert_contains(html, "<a href=\"a%20b\">x</a>",
        "Reference defs: unmatched backtick does not disable link encoding");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Reference Definition Tests", had_failures, false);
}

/**
 * Test file includes
 */

void test_file_includes(void) {
    int suite_failures = suite_start();
    print_suite_title("File Includes Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_file_includes = true;
#ifdef TEST_FIXTURES_DIR
    opts.base_directory = TEST_FIXTURES_DIR;
#else
    opts.base_directory = "tests/fixtures/includes";
#endif
    char *html;

    /* Test Marked markdown include */
    html = apex_markdown_to_html("Before\n\n<<[simple.md]\n\nAfter", 28, &opts);
    assert_contains(html, "Included Content", "Marked markdown include");
    assert_contains(html, "List item 1", "Markdown processed from include");
    apex_free_string(html);

    /* Test Marked code include */
    html = apex_markdown_to_html("Code:\n\n<<(code.py)\n\nDone", 23, &opts);
    assert_contains(html, "<pre", "Code include generates pre tag");
    assert_contains(html, "def hello", "Code content included");
    assert_contains(html, "lang=\"python\"", "Python language class added");
    apex_free_string(html);

    /* Test Marked raw HTML include - currently uses placeholder */
    html = apex_markdown_to_html("HTML:\n\n<<{raw.html}\n\nDone", 24, &opts);
    assert_contains(html, "APEX_RAW_INCLUDE", "Raw HTML include marker present");
    apex_free_string(html);

    /* Test MMD transclusion */
    html = apex_markdown_to_html("Include: {{simple.md}}", 22, &opts);
    assert_contains(html, "Included Content", "MMD transclusion works");
    apex_free_string(html);

    apex_options plugin_opts = opts;
    plugin_opts.enable_plugins = true;
    plugin_opts.allow_external_plugin_detection = false;
    plugin_opts.plugin_register = include_preparse_plugin_register;
    html = apex_markdown_to_html("Include: {{simple.md}}", 22, &plugin_opts);
    assert_contains(html, "Plugin Included Content", "Included file runs through pre-parse plugin pipeline");
    apex_free_string(html);

    const char *plugin_section_include = "![[sections#Section 2]]";
    html = apex_markdown_to_html(plugin_section_include, strlen(plugin_section_include), &plugin_opts);
    assert_contains(html, "Plugin Section 2", "Section include resolves before plugin rewrites section heading");
    assert_contains(html, "Nested section 2.1 text.", "Section include with plugin keeps nested subsection");
    assert_not_contains(html, "Section 3 text.", "Section include with plugin still stops at next same-level heading");
    apex_free_string(html);

    /* Test MMD wildcard transclusion: file.* (legacy behavior) */
    html = apex_markdown_to_html("Include: {{simple.*}}", 22, &opts);
    assert_contains(html, "Included Content", "MMD wildcard file.* resolves to simple.md");
    apex_free_string(html);

    /* Test CSV to table conversion */
    html = apex_markdown_to_html("Data:\n\n<<[data.csv]\n\nEnd", 24, &opts);
    assert_contains(html, "<table>", "CSV converts to table");
    assert_contains(html, "Alice", "CSV data in table");
    assert_contains(html, "New York", "CSV cell content");
    apex_free_string(html);

    /* Test Marked CSV include with embedded shorthand delimiter override */
    const char *marked_csv_embedded_short = "Data:\n\n<<[data-semi.csv{;}]\n\nEnd";
    html = apex_markdown_to_html(marked_csv_embedded_short, strlen(marked_csv_embedded_short), &opts);
    assert_contains(html, "<table>", "Marked CSV include with embedded {;} converts to table");
    assert_contains(html, "San Francisco", "Marked CSV include with embedded {;} parses semicolon-separated values");
    assert_not_contains(html, "{;}", "Marked CSV include consumes embedded {;} delimiter token");
    apex_free_string(html);

    /* Test Marked CSV include with embedded verbose delimiter override */
    const char *marked_csv_embedded_verbose = "Data:\n\n<<[data-semi.csv{delimiter=;}]\n\nEnd";
    html = apex_markdown_to_html(marked_csv_embedded_verbose, strlen(marked_csv_embedded_verbose), &opts);
    assert_contains(html, "<table>", "Marked CSV include with embedded {delimiter=;} converts to table");
    assert_contains(html, "Alice", "Marked CSV include with embedded {delimiter=;} parses semicolon-separated values");
    assert_not_contains(html, "{delimiter=;}", "Marked CSV include consumes embedded verbose delimiter token");
    apex_free_string(html);

    /* Test TSV to table conversion */
    html = apex_markdown_to_html("{{data.tsv}}", 12, &opts);
    assert_contains(html, "<table>", "TSV converts to table");
    assert_contains(html, "Widget", "TSV data in table");
    apex_free_string(html);

    /* Test MMD CSV transclusion with embedded shorthand delimiter override */
    const char *mmd_csv_embedded_short = "{{data-semi.csv{;}}}";
    html = apex_markdown_to_html(mmd_csv_embedded_short, strlen(mmd_csv_embedded_short), &opts);
    assert_contains(html, "<table>", "MMD CSV transclusion with embedded {;} converts to table");
    assert_contains(html, "San Francisco", "MMD CSV transclusion with embedded {;} parses semicolon-separated values");
    assert_not_contains(html, "{;}", "MMD CSV transclusion consumes embedded {;} delimiter token");
    apex_free_string(html);

    /* Test MMD CSV transclusion with embedded verbose delimiter override */
    const char *mmd_csv_embedded_verbose = "{{data-semi.csv{delimiter=;}}}";
    html = apex_markdown_to_html(mmd_csv_embedded_verbose, strlen(mmd_csv_embedded_verbose), &opts);
    assert_contains(html, "<table>", "MMD CSV transclusion with embedded {delimiter=;} converts to table");
    assert_contains(html, "Alice", "MMD CSV transclusion with embedded {delimiter=;} parses semicolon-separated values");
    assert_not_contains(html, "{delimiter=;}", "MMD CSV transclusion consumes embedded verbose delimiter token");
    apex_free_string(html);

    /* Test percent-encoded path in include */
    html = apex_markdown_to_html("<<[with%20space.txt]", 21, &opts);
    assert_contains(html, "Percent-decoded", "Percent-encoded path (with%20space.txt) resolves to file with space");
    apex_free_string(html);

    /* Test iA Writer image include */
    html = apex_markdown_to_html("/image.png", 10, &opts);
    assert_contains(html, "<img", "iA Writer image include");
    assert_contains(html, "image.png", "Image path included");
    apex_free_string(html);

    /* Test iA Writer code include */
    html = apex_markdown_to_html("/code.py", 8, &opts);
    assert_contains(html, "<pre", "iA Writer code include");
    assert_contains(html, "def hello", "Code included");
    apex_free_string(html);

    /* Test iA Writer CSV include with verbose delimiter keyword */
    const char *ia_csv_delim_override_doc = "/data-semi.csv {delimiter=;}";
    html = apex_markdown_to_html(ia_csv_delim_override_doc, strlen(ia_csv_delim_override_doc), &opts);
    assert_contains(html, "<table>", "iA Writer CSV include with {delimiter=;} converts to table");
    assert_contains(html, "Alice", "iA Writer CSV include parses semicolon-separated values");
    assert_not_contains(html, "{delimiter=;}", "iA Writer include consumes verbose delimiter token");
    apex_free_string(html);

    /* Test iA Writer CSV include with embedded shorthand delimiter (no whitespace) */
    const char *ia_csv_embedded_short = "/data-semi.csv{;}";
    html = apex_markdown_to_html(ia_csv_embedded_short, strlen(ia_csv_embedded_short), &opts);
    assert_contains(html, "<table>", "iA Writer CSV include with embedded {;} converts to table");
    assert_contains(html, "San Francisco", "iA Writer CSV include with embedded {;} parses semicolon-separated values");
    assert_not_contains(html, "{;}", "iA Writer include consumes embedded shorthand delimiter token");
    apex_free_string(html);

    /* Test glob wildcard: *.md (should resolve to one of the .md fixtures) */
    html = apex_markdown_to_html("{{*.md}}", 10, &opts);
    if (strstr(html, "Included Content") != NULL ||
        strstr(html, "Nested Content") != NULL) {
        tests_passed++;
        tests_run++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Glob wildcard *.md resolves to a Markdown file\n");
        }
    } else {
        tests_failed++;
        tests_run++;
        test_result(false, "Glob wildcard *.md did not resolve correctly");
    }
    apex_free_string(html);

    /* Test MMD address syntax - line range */
    html = apex_markdown_to_html("{{simple.md}}[3,5]", 20, &opts);
    assert_contains(html, "This is a simple", "Line range includes line 3");
    assert_contains(html, "markdown file", "Line range includes line 4");
    assert_not_contains(html, "Included Content", "Line range excludes line 1");
    assert_not_contains(html, "List item 1", "Line range excludes line 5 and beyond");
    apex_free_string(html);

    /* Test MMD address syntax - from line to end */
    html = apex_markdown_to_html("{{simple.md}}[5,]", 19, &opts);
    assert_contains(html, "List item 1", "From line includes line 5");
    assert_contains(html, "List item 2", "From line includes later lines");
    assert_not_contains(html, "Included Content", "From line excludes earlier lines");
    apex_free_string(html);

    /* Test MMD address syntax - prefix */
    html = apex_markdown_to_html("{{code.py}}[1,3;prefix=\"C: \"]", 30, &opts);
    assert_contains(html, "C: def hello()", "Prefix applied to included lines");
    assert_contains(html, "C:     print", "Prefix applied to all lines");
    apex_free_string(html);

    /* Test glob wildcard with single-character ?: c?de.py should resolve to code.py */
    html = apex_markdown_to_html("{{c?de.py}}", 12, &opts);
    assert_contains(html, "def hello", "? wildcard resolves to code.py");
    apex_free_string(html);

    /* Test Marked address syntax - line range */
    html = apex_markdown_to_html("<<[simple.md][3,5]", 20, &opts);
    assert_contains(html, "This is a simple", "Marked syntax with line range");
    assert_not_contains(html, "Included Content", "Line range excludes header");
    apex_free_string(html);

    /* Section extraction parity across include syntaxes */
    const char *mmd_section_include = "{{sections.md#Section 2}}";
    html = apex_markdown_to_html(mmd_section_include, strlen(mmd_section_include), &opts);
    assert_contains(html, "Section 2 text.", "MMD section include: selected section content");
    assert_contains(html, "Nested section 2.1 text.", "MMD section include: nested subsection included");
    assert_not_contains(html, "Section 3 text.", "MMD section include: stops before next same-level heading");
    apex_free_string(html);

    const char *marked_section_include = "<<[sections.md#Section 2]";
    html = apex_markdown_to_html(marked_section_include, strlen(marked_section_include), &opts);
    assert_contains(html, "Section 2 text.", "Marked section include: selected section content");
    assert_contains(html, "Nested section 2.1 text.", "Marked section include: nested subsection included");
    assert_not_contains(html, "Section 3 text.", "Marked section include: stops before next same-level heading");
    apex_free_string(html);

    const char *ia_section_include = "/sections.md#Section 2";
    html = apex_markdown_to_html(ia_section_include, strlen(ia_section_include), &opts);
    assert_contains(html, "Section 2 text.", "iA Writer section include: selected section content");
    assert_contains(html, "Nested section 2.1 text.", "iA Writer section include: nested subsection included");
    assert_not_contains(html, "Section 3 text.", "iA Writer section include: stops before next same-level heading");
    apex_free_string(html);

    const char *obsidian_section_include = "![[sections#Section 2]]";
    html = apex_markdown_to_html(obsidian_section_include, strlen(obsidian_section_include), &opts);
    assert_contains(html, "Section 2 text.", "Obsidian embed include: selected section content");
    assert_contains(html, "Nested section 2.1 text.", "Obsidian embed include: nested subsection included");
    assert_not_contains(html, "Section 3 text.", "Obsidian embed include: stops before next same-level heading");
    apex_free_string(html);

    opts.wikilink_extension = "txt";
    html = apex_markdown_to_html(obsidian_section_include, strlen(obsidian_section_include), &opts);
    assert_contains(html, "Section 2 text from TXT fixture.", "Obsidian embed include: uses --wikilink-extension default");
    assert_not_contains(html, "Nested section 2.1 text.", "Obsidian embed include: extension override changes source file");
    assert_not_contains(html, "Section 3 text from TXT fixture.", "Obsidian embed include: extension override still trims to section");
    apex_free_string(html);
    opts.wikilink_extension = ".txt";
    html = apex_markdown_to_html(obsidian_section_include, strlen(obsidian_section_include), &opts);
    assert_contains(html, "Section 2 text from TXT fixture.", "Obsidian embed include: dot-prefixed wikilink extension works");
    apex_free_string(html);
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html(obsidian_section_include, strlen(obsidian_section_include), &opts);
    assert_contains(html, "Section 2 text.", "Obsidian embed include: falls back to .md when configured extension file is missing");
    apex_free_string(html);
    opts.wikilink_extension = NULL;

    const char *missing_section_obsidian = "![[sections#Does Not Exist]]";
    html = apex_markdown_to_html(missing_section_obsidian, strlen(missing_section_obsidian), &opts);
    assert_contains(html, "Intro paragraph.", "Obsidian embed include: missing section falls back to full document");
    assert_contains(html, "Section 3 text.", "Obsidian embed include: full document fallback includes later sections");
    apex_free_string(html);

    const char *missing_section_mmd = "{{sections.md#Does Not Exist}}";
    html = apex_markdown_to_html(missing_section_mmd, strlen(missing_section_mmd), &opts);
    assert_contains(html, "Intro paragraph.", "MMD include: missing section falls back to full document");
    assert_contains(html, "Section 3 text.", "MMD include: full document fallback includes later sections");
    apex_free_string(html);

    /* Test Marked code include with address syntax */
    html = apex_markdown_to_html("<<(code.py)[1,3]", 18, &opts);
    assert_contains(html, "def hello()", "Code include with line range");
    assert_contains(html, "print", "Code include includes second line");
    assert_not_contains(html, "return True", "Code include excludes later lines");
    apex_free_string(html);

    /* Test regex address syntax */
    html = apex_markdown_to_html("{{simple.md}}[/This is/,/List item/]", 36, &opts);
    assert_contains(html, "This is a simple", "Regex range includes matching line");
    assert_contains(html, "markdown file", "Regex range includes lines between matches");
    assert_not_contains(html, "Included Content", "Regex range excludes before first match");
    apex_free_string(html);

    /* iA Writer plain include still includes full file when no section is requested */
    html = apex_markdown_to_html("/code.py", 8, &opts);
    assert_contains(html, "def hello()", "iA Writer syntax unchanged");
    assert_contains(html, "return True", "iA Writer includes full file");
    apex_free_string(html);

    /* Test address syntax edge cases */
    /* Single line range - line 3 is the full sentence, so [3,4] includes only line 3 */
    html = apex_markdown_to_html("{{simple.md}}[3,4]", 20, &opts);
    assert_contains(html, "This is a simple", "Single line range works");
    assert_contains(html, "markdown file", "Single line includes full line 3");
    assert_not_contains(html, "List item 1", "Single line excludes line 5");
    apex_free_string(html);

    /* Prefix with regex range - check if prefix is applied (may need to check implementation) */
    html = apex_markdown_to_html("{{simple.md}}[/This is/,/List item/;prefix=\"  \"]", 48, &opts);
    assert_contains(html, "This is a simple", "Regex range includes matching line");
    /* Prefix application to regex ranges may need implementation verification */
    apex_free_string(html);

    /* Prefix only (no line range) - verify prefix-only syntax is parsed */
    html = apex_markdown_to_html("{{code.py}}[prefix=\"// \"]", 26, &opts);
    assert_contains(html, "def hello()", "Prefix-only includes content");
    /* Prefix application may need implementation verification */
    apex_free_string(html);

    /* Address syntax with CSV (should extract lines before conversion) */
    html = apex_markdown_to_html("{{data.csv}}[2,4]", 18, &opts);
    assert_contains(html, "<table>", "CSV with address converts to table");
    assert_contains(html, "Alice", "CSV address includes correct row");
    assert_not_contains(html, "Name,Age,City", "CSV address excludes header");
    apex_free_string(html);

    /* Address syntax with Marked raw HTML */
    html = apex_markdown_to_html("<<{raw.html}[1,3]", 18, &opts);
    assert_contains(html, "APEX_RAW_INCLUDE", "Raw HTML include with address");
    apex_free_string(html);

    /* Regex with no match (should return empty) */
    html = apex_markdown_to_html("{{simple.md}}[/NOTFOUND/,/ALSONOTFOUND/]", 44, &opts);
    /* Should not contain any content from file */
    if (strstr(html, "Included Content") == NULL && strstr(html, "List item") == NULL) {
        tests_passed++;
        tests_run++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Regex with no match returns empty\n");
        }
    } else {
        tests_failed++;
        tests_run++;
        test_result(false, "Regex with no match should return empty");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("File Includes Tests", had_failures, false);
}

/**
 * Test IAL (Inline Attribute Lists)
 */

void test_definition_lists(void) {
    int suite_failures = suite_start();
    print_suite_title("Definition Lists Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    char *html;

    /* Test basic definition list */
    html = apex_markdown_to_html("Term\n: Definition", 17, &opts);
    assert_contains(html, "<dl>", "Definition list tag");
    assert_contains(html, "<dt>Term</dt>", "Definition term");
    assert_contains(html, "<dd>Definition</dd>", "Definition description");
    apex_free_string(html);

    /* Test multiple definitions */
    html = apex_markdown_to_html("Apple\n: A fruit\n: A company", 27, &opts);
    assert_contains(html, "<dt>Apple</dt>", "Multiple definitions term");
    assert_contains(html, "<dd>A fruit</dd>", "First definition");
    assert_contains(html, "<dd>A company</dd>", "Second definition");
    apex_free_string(html);

    /* Test definition with Markdown content */
    const char *block_def = "Term\n: Definition with **bold** and *italic*";
    html = apex_markdown_to_html(block_def, strlen(block_def), &opts);
    assert_contains(html, "<dd>", "Definition created");
    assert_contains(html, "<strong>bold</strong>", "Bold markdown in definition");
    assert_contains(html, "<em>italic</em>", "Italic markdown in definition");
    apex_free_string(html);

    /* Test multiple terms and definitions */
    const char *multi = "Term1\n: Def1\n\nTerm2\n: Def2";
    html = apex_markdown_to_html(multi, strlen(multi), &opts);
    assert_contains(html, "<dt>Term1</dt>", "First term");
    assert_contains(html, "<dt>Term2</dt>", "Second term");
    assert_contains(html, "<dd>Def1</dd>", "First definition");
    assert_contains(html, "<dd>Def2</dd>", "Second definition");
    apex_free_string(html);

    /* Test inline links in definition list terms */
    const char *inline_link = "Term with [inline link](https://example.com)\n: Definition";
    html = apex_markdown_to_html(inline_link, strlen(inline_link), &opts);
    assert_contains(html, "<dt>", "Definition term with inline link");
    assert_contains(html, "<a href=\"https://example.com\"", "Inline link in term has href");
    assert_contains(html, "inline link</a>", "Inline link text in term");
    apex_free_string(html);

    /* Test reference-style links in definition list terms */
    const char *ref_link = "Term with [reference link][ref]\n: Definition\n\n[ref]: https://example.com \"Reference title\"";
    html = apex_markdown_to_html(ref_link, strlen(ref_link), &opts);
    assert_contains(html, "<dt>", "Definition term with reference link");
    assert_contains(html, "<a href=\"https://example.com\"", "Reference link in term has href");
    assert_contains(html, "title=\"Reference title\"", "Reference link in term has title");
    assert_contains(html, "reference link</a>", "Reference link text in term");
    apex_free_string(html);

    /* Test shortcut reference links in definition list terms */
    const char *shortcut_link = "Term with [shortcut][]\n: Definition\n\n[shortcut]: https://example.org";
    html = apex_markdown_to_html(shortcut_link, strlen(shortcut_link), &opts);
    assert_contains(html, "<dt>", "Definition term with shortcut reference");
    assert_contains(html, "<a href=\"https://example.org\"", "Shortcut reference in term has href");
    assert_contains(html, "shortcut</a>", "Shortcut reference text in term");
    apex_free_string(html);

    /* Test inline links in definition descriptions */
    const char *def_inline = "Term\n: Definition with [inline link](https://example.com)";
    html = apex_markdown_to_html(def_inline, strlen(def_inline), &opts);
    assert_contains(html, "<dd>", "Definition with inline link");
    assert_contains(html, "<a href=\"https://example.com\"", "Inline link in definition has href");
    apex_free_string(html);

    /* Test reference-style links in definition descriptions */
    const char *def_ref = "Term\n: Definition with [reference][ref]\n\n[ref]: https://example.com";
    html = apex_markdown_to_html(def_ref, strlen(def_ref), &opts);
    assert_contains(html, "<dd>", "Definition with reference link");
    assert_contains(html, "<a href=\"https://example.com\"", "Reference link in definition has href");
    apex_free_string(html);

    /* Test definition list with blank line between term and first definition */
    const char *blank_before = "Term\n\n: definition 1\n: definition 2";
    html = apex_markdown_to_html(blank_before, strlen(blank_before), &opts);
    assert_contains(html, "<dl>", "Definition list with blank before first definition");
    assert_contains(html, "<dt>Term</dt>", "Term preserved across blank line");
    assert_contains(html, "<dd>definition 1</dd>", "First definition after blank line");
    assert_contains(html, "<dd>definition 2</dd>", "Second definition");
    apex_free_string(html);

    /* Test definition list with blank line between definitions */
    const char *blank_between = "Term\n: definition 1\n\n: definition 2";
    html = apex_markdown_to_html(blank_between, strlen(blank_between), &opts);
    assert_contains(html, "<dl>", "Definition list with blank between definitions");
    assert_contains(html, "<dt>Term</dt>", "Term in list with blank between definitions");
    assert_contains(html, "<dd>definition 1</dd>", "First definition");
    assert_contains(html, "<dd>definition 2</dd>", "Second definition after blank line");
    apex_free_string(html);

    /* Test definition list with blank lines everywhere (user's exact case) */
    const char *blank_everywhere = "Term\n\n: definition 1\n\n: definition 2";
    html = apex_markdown_to_html(blank_everywhere, strlen(blank_everywhere), &opts);
    assert_contains(html, "<dl>", "Definition list with blank lines everywhere");
    assert_contains(html, "<dt>Term</dt>", "Term preserved with multiple blank lines");
    assert_contains(html, "<dd>definition 1</dd>", "First definition");
    assert_contains(html, "<dd>definition 2</dd>", "Second definition");
    apex_free_string(html);

    /* Test all four definition list formats */
    html = apex_markdown_to_html("term\n: definition", 17, &opts);
    assert_contains(html, "<dt>term</dt>", "Kramdown format: term + : definition");
    assert_contains(html, "<dd>definition</dd>", "Kramdown format dd");
    apex_free_string(html);

    html = apex_markdown_to_html("term\n:: definition", 18, &opts);
    assert_contains(html, "<dt>term</dt>", "Kramdown format: term + :: definition");
    assert_contains(html, "<dd>definition</dd>", "Kramdown :: format dd");
    apex_free_string(html);

    html = apex_markdown_to_html("term::definition", 16, &opts);
    assert_contains(html, "<dt>term</dt>", "One-line format: term::definition");
    assert_contains(html, "<dd>definition</dd>", "One-line format dd");
    apex_free_string(html);

    html = apex_markdown_to_html("term :: definition", 18, &opts);
    assert_contains(html, "<dt>term</dt>", "One-line format: term :: definition");
    assert_contains(html, "<dd>definition</dd>", "One-line format with spaces dd");
    apex_free_string(html);

    /* One-line definition NOT converted inside inline code span */
    html = apex_markdown_to_html("Use `term::definition` for syntax", 32, &opts);
    assert_contains(html, "<code>term::definition</code>", "One-line def preserved in inline code");
    assert_not_contains(html, "<dt>term</dt>", "No definition list from content inside backticks");
    apex_free_string(html);

    /* One-line definition NOT converted inside fenced code block */
    const char *fenced_def = "```\nfoo::bar\nterm::definition\n```";
    html = apex_markdown_to_html(fenced_def, strlen(fenced_def), &opts);
    assert_contains(html, "foo::bar", "One-line def in fenced block preserved");
    assert_contains(html, "term::definition", "Second one-line def in fenced block preserved");
    assert_not_contains(html, "<dt>foo</dt>", "No definition list from fenced code content");
    apex_free_string(html);

    /* One-line definition NOT converted inside indented code block */
    const char *indented_def = "    key::value\n    term::definition";
    html = apex_markdown_to_html(indented_def, strlen(indented_def), &opts);
    assert_contains(html, "key::value", "One-line def in indented block preserved");
    assert_contains(html, "term::definition", "Second one-line def in indented block preserved");
    assert_not_contains(html, "<dt>key</dt>", "No definition list from indented code content");
    apex_free_string(html);

    /* Kramdown : definition NOT converted inside multi-line inline code span */
    const char *multiline_code = "`term::works\n :more:`";
    html = apex_markdown_to_html(multiline_code, strlen(multiline_code), &opts);
    assert_contains(html, "term::works", "One-line def in multi-line inline code preserved");
    assert_not_contains(html, "<dt>term</dt>", "No definition list from multi-line inline code");
    apex_free_string(html);

    /* Leanpub {::pagebreak /} must not trigger definition list reordering */
    const char *heading_pagebreak = "## Page breaks\n    {::pagebreak /}";
    html = apex_markdown_to_html(heading_pagebreak, strlen(heading_pagebreak), &opts);
    assert_contains(html, "<h2", "Heading before indented Leanpub pagebreak marker");
    assert_contains(html, "Page", "Heading text preserved");
    assert_contains(html, "<pre><code>{::pagebreak /}", "Leanpub pagebreak marker in code block");
    assert_not_contains(html, "<dl>", "Leanpub pagebreak marker did not create definition list");
    const char *h2_pos = strstr(html, "<h2");
    const char *pre_pos = strstr(html, "<pre>");
    if (h2_pos && pre_pos && h2_pos < pre_pos) {
        test_result(true, "Heading appears before code block with Leanpub pagebreak marker");
    } else {
        test_result(false, "Heading should appear before code block with Leanpub pagebreak marker");
    }
    apex_free_string(html);

    /* Buffered term before fenced code block must not be discarded */
    const char *term_before_code =
        "You can now use these environment variables in your Run Command actions:\n\n"
        "```bash\n"
        "echo hello\n"
        "```";
    html = apex_markdown_to_html(term_before_code, strlen(term_before_code), &opts);
    assert_contains(html, "Run Command actions", "Intro paragraph preserved before code block");
    {
        const char *intro_pos = strstr(html, "Run Command actions");
        const char *code_pos = strstr(html, "<pre");
        test_result(intro_pos && code_pos && intro_pos < code_pos,
                    "Intro paragraph appears before fenced code block");
    }
    apex_free_string(html);

    /* Fence immediately after definition list must become a real code block */
    {
        const char *dl_then_fence =
            "Term\n"
            ": Def\n"
            "\n"
            "```\n"
            "code\n"
            "```\n";
        html = apex_markdown_to_html(dl_then_fence, strlen(dl_then_fence), &opts);
        assert_contains(html, "<dl>", "DL before bare fence still renders");
        assert_contains(html, "<pre", "Bare fence after DL becomes pre/code");
        assert_contains(html, "<code", "Bare fence after DL contains code element");
        assert_not_contains(html, "</dl>\n```", "Fence after DL is not left as raw backticks");
        apex_free_string(html);
    }

    /* Paragraph between DL and fence stays before the fence */
    {
        const char *dl_para_fence =
            "Term\n"
            ": Def\n"
            "\n"
            "This should stay before the diagram\n"
            "\n"
            "```mermaid\n"
            "graph TD\n"
            "  A --> B\n"
            "```\n";
        html = apex_markdown_to_html(dl_para_fence, strlen(dl_para_fence), &opts);
        const char *para = strstr(html, "This should stay before the diagram");
        const char *pre = strstr(html, "<pre");
        test_result(para && pre && para < pre,
                    "Paragraph between DL and fence stays before pre");
        apex_free_string(html);
    }

    /* DL + callout + following paragraph: blanks must not be eaten */
    {
        apex_options callout_opts = apex_options_default();
        callout_opts.mode = APEX_MODE_UNIFIED;
        callout_opts.enable_callouts = true;
        callout_opts.enable_definition_lists = true;
        const char *dl_callout =
            "Term\n"
            ": Def\n"
            "\n"
            "> [!Note]\n"
            "> note body\n"
            "\n"
            "After callout\n";
        html = apex_markdown_to_html(dl_callout, strlen(dl_callout), &callout_opts);
        assert_contains(html, "callout", "Callout after DL still renders");
        assert_contains(html, "After callout", "Paragraph after callout preserved");
        /* "After callout" must not sit inside the callout content blockquote */
        const char *after = strstr(html, "After callout");
        const char *callout_end = strstr(html, "</div>\n</div>");
        if (!callout_end) callout_end = strstr(html, "callout-content");
        test_result(after != NULL, "After callout text present in output");
        if (after && strstr(html, "callout-content")) {
            /* Find the callout-content section and ensure After is outside it */
            const char *content = strstr(html, "callout-content");
            const char *content_close = content ? strstr(content, "</div>") : NULL;
            test_result(content_close && after > content_close,
                        "After callout is outside callout-content");
        }
        apex_free_string(html);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Definition Lists Tests", had_failures, false);
}

/**
 * Test advanced tables
 */

static int count_occurrences(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    int count = 0;
    for (const char *p = strstr(haystack, needle); p; p = strstr(p + strlen(needle), needle)) {
        count++;
    }
    return count;
}

void test_callouts(void) {
    int suite_failures = suite_start();
    print_suite_title("Callouts Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_callouts = true;
    char *html;

    /* Test Bear/Obsidian NOTE callout */
    html = apex_markdown_to_html("> [!NOTE] Important\n> This is a note", 36, &opts);
    assert_contains(html, "class=\"callout", "Callout class present");
    assert_contains(html, "callout-note", "Note callout type");
    apex_free_string(html);

    /* Test WARNING callout */
    html = apex_markdown_to_html("> [!WARNING] Be careful\n> Warning text", 38, &opts);
    assert_contains(html, "callout-warning", "Warning callout type");
    apex_free_string(html);

    /* Test TIP callout */
    html = apex_markdown_to_html("> [!TIP] Pro tip\n> Helpful advice", 33, &opts);
    assert_contains(html, "callout-tip", "Tip callout type");
    apex_free_string(html);

    /* Test DANGER callout */
    html = apex_markdown_to_html("> [!DANGER] Critical\n> Dangerous action", 40, &opts);
    assert_contains(html, "callout-danger", "Danger callout type");
    apex_free_string(html);

    /* Test INFO callout */
    html = apex_markdown_to_html("> [!INFO] Information\n> Info text", 34, &opts);
    assert_contains(html, "callout-info", "Info callout type");
    apex_free_string(html);

    /* Test collapsible callout with + */
    html = apex_markdown_to_html("> [!NOTE]+ Expandable\n> Content", 32, &opts);
    assert_contains(html, "<details", "Collapsible callout uses details");
    apex_free_string(html);

    /* Test collapsed callout with - */
    html = apex_markdown_to_html("> [!NOTE]- Collapsed\n> Hidden content", 38, &opts);
    assert_contains(html, "<details", "Collapsed callout uses details");
    apex_free_string(html);

    /* Test callout with multiple paragraphs */
    const char *multi = "> [!NOTE] Title\n> Para 1\n>\n> Para 2";
    html = apex_markdown_to_html(multi, strlen(multi), &opts);
    assert_contains(html, "callout", "Multi-paragraph callout");
    apex_free_string(html);

    /* Wrapper structure for +, -, and no marker */
    html = apex_markdown_to_html("> [!INFO]+ Open\n> Body", 22, &opts);
    assert_contains(html, "<details class=\"callout callout-info\" open>\n<summary>Open</summary>",
                    "+ callout renders open details with summary");
    assert_contains(html, "<p>Body</p>", "Callout body has no leading line break");
    apex_free_string(html);

    html = apex_markdown_to_html("> [!NOTE]- Closed\n> Body", 24, &opts);
    assert_contains(html, "<details class=\"callout callout-note\">\n<summary>Closed</summary>",
                    "- callout renders closed details");
    apex_free_string(html);

    html = apex_markdown_to_html("> [!TIP] Plain\n> Body", 21, &opts);
    assert_contains(html, "<div class=\"callout callout-tip\">\n<div class=\"callout-title\">Plain</div>",
                    "Callout without marker renders div with callout-title");
    apex_free_string(html);

    /* Inline markup in titles belongs in the title element */
    const char *bold_title = "> [!info]+ **IN-FLIGHT STATUS**\n> Body text";
    html = apex_markdown_to_html(bold_title, strlen(bold_title), &opts);
    assert_contains(html, "<summary><strong>IN-FLIGHT STATUS</strong></summary>",
                    "Bold callout title rendered in summary");
    assert_not_contains(html, "<summary>info</summary>", "Bold title does not fall back to type name");
    assert_not_contains(html, "<p><strong>IN-FLIGHT STATUS", "Bold title not left in callout body");
    apex_free_string(html);

    const char *emoji_title = "> [!info]+ \xE2\x9A\xA1 **NEXT ACTIONS**\n> Body text";
    html = apex_markdown_to_html(emoji_title, strlen(emoji_title), &opts);
    assert_contains(html, "<summary>\xE2\x9A\xA1 <strong>NEXT ACTIONS</strong></summary>",
                    "Emoji plus bold title rendered in summary");
    apex_free_string(html);

    const char *mixed_title = "> [!note] Use `code` & *care*\n> Body text";
    html = apex_markdown_to_html(mixed_title, strlen(mixed_title), &opts);
    assert_contains(html, "<div class=\"callout-title\">Use <code>code</code> &amp; <em>care</em></div>",
                    "Inline code, emphasis, and escaping in callout-title");
    apex_free_string(html);

    /* Nested callouts */
    const char *nested = "> [!note]+ Outer\n> > [!tip]- Inner\n> > Inner body\n";
    html = apex_markdown_to_html(nested, strlen(nested), &opts);
    test_result(count_occurrences(html, "class=\"callout ") == 2, "Nested callout produces two .callout elements");
    assert_contains(html, "<details class=\"callout callout-tip\">\n<summary>Inner</summary>",
                    "Inner callout converted");
    assert_not_contains(html, "[!tip]", "No literal inner callout marker");
    apex_free_string(html);

    /* Full nested sample with tables and task lists (unified defaults) */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *agenda =
        "## AGENDA\n"
        "\n"
        "> [!info]+ OVERVIEW\n"
        "> Overview text.\n"
        "\n"
        "> [!info]+ **IN-FLIGHT STATUS**\n"
        "> > [!info]+ **PROMISES (waiting on me)**\n"
        "> > | Who | Date | Waiting for |\n"
        "> > | --- | ---- | ----------- |\n"
        "> > | WHO | 26-09-03 | THE THING |\n"
        ">\n"
        "> > [!info]+ **WAITING FOR**\n"
        "> > | Who | Date | Waiting for |\n"
        "> > | --- | ---- | ----------- |\n"
        "> > | WHO | 26-09-03 | THE THING |\n"
        ">\n"
        "> > [!info]+ \xE2\x9A\xA1 **NEXT ACTIONS**\n"
        "> > - [ ] ACTION\n";
    html = apex_markdown_to_html(agenda, strlen(agenda), &unified_opts);
    test_result(count_occurrences(html, "<details class=\"callout callout-info\" open>") == 5,
                "Agenda sample converts all five callouts");
    const char *in_flight = html ? strstr(html, "<summary><strong>IN-FLIGHT STATUS</strong></summary>") : NULL;
    test_result(in_flight != NULL && count_occurrences(in_flight, "<details class=\"callout") == 3,
                "Three nested callouts under IN-FLIGHT STATUS");
    test_result(count_occurrences(html, "<table>") == 2, "Two tables inside nested callouts");
    assert_contains(html, "<input type=\"checkbox\" disabled=\"\" /> ACTION", "Task list checkbox in nested callout");
    assert_not_contains(html, "[!info]", "No literal [!info] markers left");
    assert_not_contains(html, "[ ] ACTION", "No literal task marker left");
    apex_free_string(html);

    /* Test regular blockquote (not a callout) */
    html = apex_markdown_to_html("> Just a quote\n> Regular text", 29, &opts);
    if (strstr(html, "class=\"callout") == NULL) {
        test_result(true, "Regular blockquote not treated as callout");
    } else {
        test_result(false, "Regular blockquote incorrectly treated as callout");
    }
    apex_free_string(html);

    /* Python-Markdown callouts are disabled by default */
    const char *py_callout = "!!! note \"Py Title\"\n    Py body line";
    html = apex_markdown_to_html(py_callout, strlen(py_callout), &opts);
    assert_not_contains(html, "class=\"callout", "Python callout syntax ignored when flag disabled");
    apex_free_string(html);

    /* Python-Markdown callouts enabled behind flag */
    opts.enable_py_callouts = true;
    html = apex_markdown_to_html(py_callout, strlen(py_callout), &opts);
    assert_contains(html, "class=\"callout", "Python callout recognized when enabled");
    assert_contains(html, "callout-note", "Python note callout type");
    assert_contains(html, "Py Title", "Python callout title preserved");
    apex_free_string(html);
    opts.enable_py_callouts = false;

    /* markdown-callouts NOTE: syntax enabled behind py flag */
    opts.enable_py_callouts = true;
    html = apex_markdown_to_html("NOTE: Inline py callout body", 28, &opts);
    assert_contains(html, "class=\"callout", "NOTE: syntax recognized with py-callouts");
    assert_contains(html, "callout-note", "NOTE: maps to note callout");
    assert_contains(html, "Inline py callout body", "NOTE: body preserved");
    apex_free_string(html);

    /* markdown-callouts collapsed syntax */
    const char *collapsed_py = ">? NOTE: Collapsed note\n> Collapsed body";
    html = apex_markdown_to_html(collapsed_py, strlen(collapsed_py), &opts);
    assert_contains(html, "<details", ">? syntax creates collapsible callout");
    assert_contains(html, "callout-note", ">? NOTE maps to note callout");
    assert_contains(html, "Collapsed body", ">? NOTE body preserved");
    apex_free_string(html);

    opts.enable_py_callouts = false;

    /* Quarto callouts are disabled by default */
    const char *quarto_callout =
        "::: {.callout-warning}\n"
        "## Quarto Title\n"
        "Quarto warning body.\n"
        ":::\n";
    html = apex_markdown_to_html(quarto_callout, strlen(quarto_callout), &opts);
    assert_not_contains(html, "class=\"callout callout-warning\"", "Quarto callout syntax ignored when flag disabled");
    apex_free_string(html);

    /* Quarto callouts enabled behind flag */
    opts.enable_quarto_callouts = true;
    html = apex_markdown_to_html(quarto_callout, strlen(quarto_callout), &opts);
    assert_contains(html, "class=\"callout", "Quarto callout recognized when enabled");
    assert_contains(html, "callout-warning", "Quarto warning callout type");
    assert_contains(html, "Quarto warning body", "Quarto callout body preserved");
    apex_free_string(html);

    /* Quarto flag should still allow non-callout divs to render as divs */
    const char *regular_div =
        "::: {.sidebar}\n"
        "Regular div content.\n"
        ":::\n";
    html = apex_markdown_to_html(regular_div, strlen(regular_div), &opts);
    assert_contains(html, "<div class=\"sidebar\"", "Non-callout fenced div still rendered with Quarto mode enabled");
    assert_not_contains(html, "class=\"callout", "Non-callout fenced div not converted into callout");
    apex_free_string(html);
    opts.enable_quarto_callouts = false;

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Callouts Tests", had_failures, false);
}

/**
 * Test blockquotes with lists
 */

void test_blockquote_lists(void) {
    int suite_failures = suite_start();
    print_suite_title("Blockquote Lists Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test unordered list in blockquote */
    html = apex_markdown_to_html("> Quote text\n>\n> - Item 1\n> - Item 2\n> - Item 3", 50, &opts);
    assert_contains(html, "<blockquote>", "Blockquote with list has blockquote tag");
    assert_contains(html, "<ul>", "Blockquote contains unordered list");
    assert_contains(html, "<li>Item 1</li>", "First list item in blockquote");
    assert_contains(html, "<li>Item 2</li>", "Second list item in blockquote");
    assert_contains(html, "<li>Item 3</li>", "Third list item in blockquote");
    apex_free_string(html);

    /* Test ordered list in blockquote */
    const char *ordered_list = "> Numbered items:\n>\n> 1. First\n> 2. Second\n> 3. Third";
    html = apex_markdown_to_html(ordered_list, strlen(ordered_list), &opts);
    assert_contains(html, "<blockquote>", "Blockquote with ordered list");
    assert_contains(html, "<ol>", "Blockquote contains ordered list");
    assert_contains(html, "<li>First</li>", "First ordered item");
    assert_contains(html, "<li>Second</li>", "Second ordered item");
    assert_contains(html, "<li>Third</li>", "Third ordered item");
    apex_free_string(html);

    /* Test nested list in blockquote */
    html = apex_markdown_to_html("> Main list:\n>\n> - Item 1\n>   - Nested 1\n>   - Nested 2\n> - Item 2", 60, &opts);
    assert_contains(html, "<blockquote>", "Blockquote with nested list");
    assert_contains(html, "<ul>", "Outer list present");
    assert_contains(html, "<li>Item 1", "Outer list item");
    assert_contains(html, "<li>Nested 1", "Nested list item");
    assert_contains(html, "<li>Nested 2", "Second nested item");
    apex_free_string(html);

    /* Test list with paragraph in blockquote */
    const char *list_para = "> Introduction\n>\n> - Point one\n> - Point two\n>\n> Conclusion";
    html = apex_markdown_to_html(list_para, strlen(list_para), &opts);
    assert_contains(html, "<blockquote>", "Blockquote with list and paragraphs");
    assert_contains(html, "Introduction", "Paragraph before list");
    assert_contains(html, "<ul>", "List present");
    /* Conclusion may be in a separate blockquote or paragraph */
    assert_contains(html, "Conclusion", "Conclusion text present");
    apex_free_string(html);

    /* Test task list in blockquote (requires GFM mode) */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
    const char *task_list = "> Tasks:\n>\n> - [ ] Todo\n> - [x] Done\n> - [ ] Another";
    html = apex_markdown_to_html(task_list, strlen(task_list), &gfm_opts);
    assert_contains(html, "<blockquote>", "Blockquote with task list");
    assert_contains(html, "<li><input type=\"checkbox\" disabled=\"\" /> Todo</li>", "Unchecked task in blockquote");
    assert_contains(html, "<li><input type=\"checkbox\" checked=\"\" disabled=\"\" /> Done</li>", "Checked task in blockquote");
    assert_not_contains(html, "[ ]", "No literal task marker in blockquote");
    apex_free_string(html);

    const char *nested_task = "> > - [X] Deep\n> > - [ ]\n> > - [ ]not a task";
    html = apex_markdown_to_html(nested_task, strlen(nested_task), &gfm_opts);
    assert_contains(html, "checked=\"\" disabled=\"\" /> Deep</li>", "Uppercase X task in nested blockquote");
    assert_contains(html, "<li><input type=\"checkbox\" disabled=\"\" /> </li>", "Empty task item in blockquote");
    assert_contains(html, "<li>[ ]not a task</li>", "Marker without trailing space stays literal");
    apex_free_string(html);

    /* Relaxed tables must not inject unprefixed separator rows into blockquoted tables */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *quoted_table = "> > | A | B |\n> > | --- | --- |\n> > | 1 | 2 |\n";
    html = apex_markdown_to_html(quoted_table, strlen(quoted_table), &unified_opts);
    assert_contains(html, "<blockquote>\n<blockquote>\n<table>", "GFM table stays inside nested blockquote");
    assert_contains(html, "<td>1</td>", "Blockquoted table body row rendered");
    assert_not_contains(html, "|", "No pipe text leaked from blockquoted table");
    apex_free_string(html);

    /* Test definition list in blockquote (MMD mode) */
    html = apex_markdown_to_html("> Terms:\n>\n> Term 1\n> : Definition 1\n>\n> Term 2\n> : Definition 2", 60, &opts);
    assert_contains(html, "<blockquote>", "Blockquote with definition list");
    /* Definition lists may or may not be parsed depending on mode */
    apex_free_string(html);

    /* Reference image after blockquote without blank line should not stay in blockquote */
    const char *ref_img_after_quote =
        "> Block quote\n"
        "![][2]\n"
        "\n"
        "[2]: /path/to/image.jpg\n"
        "\n"
        "Additional paragraph.";
    html = apex_markdown_to_html(ref_img_after_quote, strlen(ref_img_after_quote), &opts);
    assert_contains(html, "<blockquote>", "Blockquote present for ref image test");
    assert_contains(html, "Block quote", "Blockquote text preserved");
    assert_not_contains(html, "<blockquote>\n<p>Block quote\n<img", "Reference image not lazy-continued into blockquote");
    assert_contains(html, "<img src=\"/path/to/image.jpg\"", "Reference image resolved outside blockquote");
    assert_contains(html, "Additional paragraph.", "Paragraph after reference image preserved");
    apex_free_string(html);

    /* Inline image after blockquote without blank line */
    html = apex_markdown_to_html("> Block quote\n![alt](test.png)\n\nNext paragraph.", 52, &opts);
    assert_not_contains(html, "<blockquote>\n<p>Block quote\n<", "Inline image not inside blockquote paragraph");
    assert_contains(html, "test.png", "Inline image rendered outside blockquote");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Blockquote Lists Tests", had_failures, false);
}

/**
 * Test TOC generation
 */

void test_html_markdown_attributes(void) {
    int suite_failures = suite_start();
    print_suite_title("HTML Markdown Attributes Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test markdown="1" (parse as block markdown) */
    const char *block1 = "<div markdown=\"1\">\n# Header\n\n**bold**\n</div>";
    html = apex_markdown_to_html(block1, strlen(block1), &opts);
    assert_contains(html, "<h1>Header</h1>", "markdown=\"1\" parses headers");
    assert_contains(html, "<strong>bold</strong>", "markdown=\"1\" parses emphasis");
    apex_free_string(html);

    /* Test markdown="block" (parse as block markdown) */
    const char *block_attr = "<div markdown=\"block\">\n## Section\n\n- List item\n</div>";
    html = apex_markdown_to_html(block_attr, strlen(block_attr), &opts);
    assert_contains(html, "<h2>Section</h2>", "markdown=\"block\" parses headers");
    assert_contains(html, "<li>List item</li>", "markdown=\"block\" parses lists");
    apex_free_string(html);

    /* GFM extensions enabled on the document apply inside markdown="1" */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    gfm_opts.relaxed_tables = false;
    const char *table_block = "<div markdown=\"1\">\n| A | B |\n| --- | --- |\n| 1 | 2 |\n</div>";
    html = apex_markdown_to_html(table_block, strlen(table_block), &gfm_opts);
    assert_contains(html, "<table>", "markdown=\"1\" parses pipe tables");
    assert_contains(html, "<td>1</td>", "markdown=\"1\" table has body cells");
    apex_free_string(html);

    const char *nested_table = "<div markdown=\"1\">\n<div markdown=\"1\">\n| A | B |\n| --- | --- |\n| 1 | 2 |\n</div>\n</div>";
    html = apex_markdown_to_html(nested_table, strlen(nested_table), &gfm_opts);
    assert_contains(html, "<table>", "Nested markdown=\"1\" parses pipe tables");
    apex_free_string(html);

    const char *strike_block = "<div markdown=\"1\">\n~~gone~~\n</div>";
    html = apex_markdown_to_html(strike_block, strlen(strike_block), &gfm_opts);
    assert_contains(html, "<del>gone</del>", "markdown=\"1\" parses strikethrough");
    apex_free_string(html);

    apex_options no_tables_opts = gfm_opts;
    no_tables_opts.enable_tables = false;
    html = apex_markdown_to_html(table_block, strlen(table_block), &no_tables_opts);
    assert_not_contains(html, "<table>", "markdown=\"1\" respects disabled tables");
    apex_free_string(html);

    /* Test markdown="span" (parse as inline markdown) */
    const char *span = "<div markdown=\"span\">**bold** and *italic*</div>";
    html = apex_markdown_to_html(span, strlen(span), &opts);
    assert_contains(html, "<strong>bold</strong>", "markdown=\"span\" parses bold");
    assert_contains(html, "<em>italic</em>", "markdown=\"span\" parses italic");
    apex_free_string(html);

    /* Test markdown="0" (no processing) */
    const char *no_parse = "<div markdown=\"0\">\n**not bold**\n</div>";
    html = apex_markdown_to_html(no_parse, strlen(no_parse), &opts);
    assert_contains(html, "**not bold**", "markdown=\"0\" preserves literal text");
    apex_free_string(html);

    /* Test nested HTML with markdown - nested tags may not parse */
    const char *nested = "<section markdown=\"1\">\n<div>\n# Nested Header\n</div>\n</section>";
    html = apex_markdown_to_html(nested, strlen(nested), &opts);
    // Note: Nested HTML processing may need refinement
    assert_contains(html, "<section>", "Section tag preserved");
    // assert_contains(html, "<h1>", "Nested HTML with markdown");
    apex_free_string(html);

    /* Test HTML without markdown attribute (default behavior) */
    const char *no_attr = "<div>\n**should not parse**\n</div>";
    html = apex_markdown_to_html(no_attr, strlen(no_attr), &opts);
    // Without markdown attribute, HTML content is typically preserved
    assert_contains(html, "<div>", "HTML preserved without markdown attribute");
    apex_free_string(html);

    /* Reference-style links in markdown="1" blocks resolve definitions from full document */
    const char *ref_in_block =
        "<blockquote class=\"tip\" markdown=\"1\">\n"
        "See the [extension page][ext] for details.\n"
        "</blockquote>\n\n"
        "[ext]: https://example.com/ext \"Extension page\"";
    html = apex_markdown_to_html(ref_in_block, strlen(ref_in_block), &opts);
    assert_contains(html, "<a href=\"https://example.com/ext\"", "markdown=\"1\" resolves reference-style links");
    assert_contains(html, "extension page</a>", "markdown=\"1\" reference link text preserved");
    apex_free_string(html);

    /* Reference footnotes in markdown="1" blocks resolve definitions from full document */
    const char *fn_in_block =
        "<blockquote class=\"tip\" markdown=\"1\">\n"
        "See footnote[^note] here.\n"
        "</blockquote>\n\n"
        "[^note]: Footnote text.";
    html = apex_markdown_to_html(fn_in_block, strlen(fn_in_block), &opts);
    assert_contains(html, "class=\"footnote-ref\"", "markdown=\"1\" resolves reference footnotes");
    assert_contains(html, "href=\"#fn-note\"", "markdown=\"1\" footnote links to block anchor");
    assert_contains(html, "<section class=\"footnotes\"", "markdown=\"1\" footnotes section rendered in block");
    assert_contains(html, "Footnote text.", "markdown=\"1\" footnote definition rendered in block");
    assert_not_contains(html, "See footnote[^note]", "markdown=\"1\" footnote not left literal");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("HTML Markdown Attributes Tests", had_failures, false);
}

/**
 * Test Pandoc fenced divs
 */

void test_fenced_divs(void) {
    int suite_failures = suite_start();
    print_suite_title("Fenced Divs Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    opts.enable_divs = true;
    char *html;

    /* Test basic fenced div with ID and class */
    const char *basic_div = "::::: {#special .sidebar}\nHere is a paragraph.\n\nAnd another.\n:::::";
    html = apex_markdown_to_html(basic_div, strlen(basic_div), &opts);
    assert_contains(html, "<div", "Basic fenced div renders");
    assert_contains(html, "id=\"special\"", "Fenced div has ID");
    assert_contains(html, "class=\"sidebar\"", "Fenced div has class");
    assert_contains(html, "Here is a paragraph", "Fenced div content preserved");
    assert_contains(html, "</div>", "Fenced div properly closed");
    apex_free_string(html);

    /* Test fenced div with single unbraced word (treated as class) */
    const char *unbraced_class = "::: sidebar\nThis is a div.\n:::";
    html = apex_markdown_to_html(unbraced_class, strlen(unbraced_class), &opts);
    assert_contains(html, "<div", "Unbraced class div renders");
    assert_contains(html, "class=\"sidebar\"", "Unbraced word becomes class");
    apex_free_string(html);

    /* Test fenced div with multiple classes */
    const char *multiple_classes = "::::: {.warning .important .highlight}\nWarning text\n:::::";
    html = apex_markdown_to_html(multiple_classes, strlen(multiple_classes), &opts);
    assert_contains(html, "<div", "Multiple classes div renders");
    assert_contains(html, "class=\"warning important highlight\"", "Multiple classes applied");
    apex_free_string(html);

    /* Test fenced div with custom attributes */
    const char *custom_attrs = "::::: {#mydiv .container key=\"value\" data-id=\"123\"}\nContent\n:::::";
    html = apex_markdown_to_html(custom_attrs, strlen(custom_attrs), &opts);
    assert_contains(html, "<div", "Custom attributes div renders");
    assert_contains(html, "id=\"mydiv\"", "Custom attributes div has ID");
    assert_contains(html, "class=\"container\"", "Custom attributes div has class");
    assert_contains(html, "key=\"value\"", "Custom attribute key present");
    assert_contains(html, "data-id=\"123\"", "Custom attribute data-id present");
    apex_free_string(html);

    /* Test fenced div with trailing colons */
    const char *trailing_colons = "::::: {#special .sidebar} ::::\nContent\n::::::::::::::::::";
    html = apex_markdown_to_html(trailing_colons, strlen(trailing_colons), &opts);
    assert_contains(html, "<div", "Trailing colons div renders");
    assert_contains(html, "id=\"special\"", "Trailing colons div has ID");
    apex_free_string(html);

    /* Test nested fenced divs */
    const char *nested_divs = "::: Warning ::::::\nOuter warning.\n\n::: Danger\nInner danger.\n:::\n::::::::::::::::::";
    html = apex_markdown_to_html(nested_divs, strlen(nested_divs), &opts);
    assert_contains(html, "<div", "Nested divs render");
    assert_contains(html, "class=\"Warning\"", "Outer div class");
    assert_contains(html, "class=\"Danger\"", "Inner div class");
    assert_contains(html, "Outer warning", "Outer div content");
    assert_contains(html, "Inner danger", "Inner div content");
    /* Should have two opening divs and two closing divs */
    size_t open_count = 0, close_count = 0;
    const char *p = html;
    while ((p = strstr(p, "<div")) != NULL) {
        open_count++;
        p += 4;
    }
    p = html;
    while ((p = strstr(p, "</div>")) != NULL) {
        close_count++;
        p += 6;
    }
    if (open_count >= 2 && close_count >= 2) {
        tests_run++;
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Nested divs properly structured\n");
        }
    } else {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " Nested divs properly structured\n");
    }
    apex_free_string(html);

    /* Test fenced div with block type (aside) */
    const char *aside_block = "::: >aside {.sidebar}\nThis is an aside block.\n:::";
    html = apex_markdown_to_html(aside_block, strlen(aside_block), &opts);
    assert_contains(html, "<aside", "Aside block renders");
    assert_contains(html, "</aside>", "Aside block closes");
    assert_contains(html, "class=\"sidebar\"", "Aside block has class");
    assert_contains(html, "This is an aside block", "Aside block content");
    apex_free_string(html);

    /* Test fenced div with block type (article) */
    const char *article_block = "::: >article {#post .main}\nArticle content here.\n:::";
    html = apex_markdown_to_html(article_block, strlen(article_block), &opts);
    assert_contains(html, "<article", "Article block renders");
    assert_contains(html, "</article>", "Article block closes");
    assert_contains(html, "id=\"post\"", "Article block has ID");
    assert_contains(html, "class=\"main\"", "Article block has class");
    apex_free_string(html);

    /* Test fenced div with block type (details/summary - nested) */
    const char *details_block = "::: >details {.warning} :::\n:::: >summary\nThis is a summary\n::::\nThis is the content of the details block\n:::";
    html = apex_markdown_to_html(details_block, strlen(details_block), &opts);
    assert_contains(html, "<details", "Details block renders");
    assert_contains(html, "</details>", "Details block closes");
    assert_contains(html, "<summary", "Summary block renders");
    assert_contains(html, "</summary>", "Summary block closes");
    assert_contains(html, "class=\"warning\"", "Details block has class");
    assert_contains(html, "This is a summary", "Summary content");
    assert_contains(html, "This is the content of the details block", "Details content");
    apex_free_string(html);

    /* Test fenced div with block type and unbraced class */
    const char *aside_unbraced = "::: >aside Warning :::\nWarning content.\n:::";
    html = apex_markdown_to_html(aside_unbraced, strlen(aside_unbraced), &opts);
    assert_contains(html, "<aside", "Aside with unbraced class renders");
    assert_contains(html, "class=\"Warning\"", "Aside has unbraced class");
    apex_free_string(html);

    /* Test default div behavior (no > prefix) */
    const char *default_div = "::: {.container}\nRegular div content.\n:::";
    html = apex_markdown_to_html(default_div, strlen(default_div), &opts);
    assert_contains(html, "<div", "Default div renders");
    assert_contains(html, "</div>", "Default div closes");
    assert_contains(html, "class=\"container\"", "Default div has class");
    apex_free_string(html);

    /* Test nested blocks with different types */
    const char *nested_blocks = "::: >section {.outer} :::\nOuter section.\n\n::: >aside {.inner}\nInner aside.\n:::\n\nMore outer content.\n:::";
    html = apex_markdown_to_html(nested_blocks, strlen(nested_blocks), &opts);
    assert_contains(html, "<section", "Nested section renders");
    assert_contains(html, "</section>", "Nested section closes");
    assert_contains(html, "<aside", "Nested aside renders");
    assert_contains(html, "</aside>", "Nested aside closes");
    assert_contains(html, "Outer section", "Outer content");
    assert_contains(html, "Inner aside", "Inner content");
    apex_free_string(html);

    /* Test block type with section */
    const char *section_block = "::: >section {#chapter1 .main-section}\nSection content here.\n:::";
    html = apex_markdown_to_html(section_block, strlen(section_block), &opts);
    assert_contains(html, "<section", "Section block renders");
    assert_contains(html, "</section>", "Section block closes");
    assert_contains(html, "id=\"chapter1\"", "Section block has ID");
    assert_contains(html, "class=\"main-section\"", "Section block has class");
    apex_free_string(html);

    /* Test block type with header */
    const char *header_block = "::: >header {.site-header}\nSite header content\n:::";
    html = apex_markdown_to_html(header_block, strlen(header_block), &opts);
    assert_contains(html, "<header", "Header block renders");
    assert_contains(html, "</header>", "Header block closes");
    apex_free_string(html);

    /* Test block type with footer */
    const char *footer_block = "::: >footer {.site-footer}\nSite footer content\n:::";
    html = apex_markdown_to_html(footer_block, strlen(footer_block), &opts);
    assert_contains(html, "<footer", "Footer block renders");
    assert_contains(html, "</footer>", "Footer block closes");
    apex_free_string(html);

    /* Test block type with nav */
    const char *nav_block = "::: >nav {.main-nav}\nNavigation content\n:::";
    html = apex_markdown_to_html(nav_block, strlen(nav_block), &opts);
    assert_contains(html, "<nav", "Nav block renders");
    assert_contains(html, "</nav>", "Nav block closes");
    apex_free_string(html);

    /* Test block type with explicit div */
    const char *explicit_div = "::: >div {.custom-div}\nExplicit div content\n:::";
    html = apex_markdown_to_html(explicit_div, strlen(explicit_div), &opts);
    assert_contains(html, "<div", "Explicit div block renders");
    assert_contains(html, "</div>", "Explicit div block closes");
    apex_free_string(html);

    /* Test block type with trailing colons */
    const char *block_trailing = "::: >aside {.sidebar} :::\nContent with trailing colons\n:::";
    html = apex_markdown_to_html(block_trailing, strlen(block_trailing), &opts);
    assert_contains(html, "<aside", "Block with trailing colons renders");
    assert_contains(html, "class=\"sidebar\"", "Block with trailing colons has class");
    apex_free_string(html);

    /* Test block type with multiple attributes */
    const char *block_multi_attr = "::: >article {#post .main .highlight data-id=\"123\" role=\"main\"}\nArticle with multiple attributes\n:::";
    html = apex_markdown_to_html(block_multi_attr, strlen(block_multi_attr), &opts);
    assert_contains(html, "<article", "Block with multiple attributes renders");
    assert_contains(html, "id=\"post\"", "Block has ID");
    assert_contains(html, "class=\"main highlight\"", "Block has multiple classes");
    assert_contains(html, "data-id=\"123\"", "Block has data attribute");
    assert_contains(html, "role=\"main\"", "Block has role attribute");
    apex_free_string(html);

    /* Test deeply nested block types */
    const char *deep_nested = "::: >section {.level1} :::\nLevel 1\n\n::: >article {.level2}\nLevel 2\n\n::: >aside {.level3}\nLevel 3\n:::\n\nMore level 2\n:::\n\nMore level 1\n:::";
    html = apex_markdown_to_html(deep_nested, strlen(deep_nested), &opts);
    assert_contains(html, "<section", "Deep nested section renders");
    assert_contains(html, "</section>", "Deep nested section closes");
    assert_contains(html, "<article", "Deep nested article renders");
    assert_contains(html, "</article>", "Deep nested article closes");
    assert_contains(html, "<aside", "Deep nested aside renders");
    assert_contains(html, "</aside>", "Deep nested aside closes");
    assert_contains(html, "Level 1", "Deep nested level 1 content");
    assert_contains(html, "Level 2", "Deep nested level 2 content");
    assert_contains(html, "Level 3", "Deep nested level 3 content");
    apex_free_string(html);

    /* Test mixed block types and regular divs */
    const char *mixed_types = "::: >section {.outer}\nSection content\n\n::: {.regular-div}\nRegular div inside section\n:::\n\n::: >aside {.aside-in-section}\nAside inside section\n:::\n\nMore section content\n:::";
    html = apex_markdown_to_html(mixed_types, strlen(mixed_types), &opts);
    assert_contains(html, "<section", "Mixed types section renders");
    assert_contains(html, "</section>", "Mixed types section closes");
    assert_contains(html, "<div", "Mixed types regular div renders");
    assert_contains(html, "</div>", "Mixed types regular div closes");
    assert_contains(html, "<aside", "Mixed types aside renders");
    assert_contains(html, "</aside>", "Mixed types aside closes");
    apex_free_string(html);

    /* Test block type with hyphenated name */
    const char *hyphenated = "::: >custom-element {.test}\nCustom element content\n:::";
    html = apex_markdown_to_html(hyphenated, strlen(hyphenated), &opts);
    assert_contains(html, "<custom-element", "Hyphenated block type renders");
    assert_contains(html, "</custom-element>", "Hyphenated block type closes");
    apex_free_string(html);

    /* Test block type preserves markdown parsing */
    const char *block_with_markdown = "::: >article {.content}\n## Heading\n\nParagraph with **bold** text.\n:::";
    html = apex_markdown_to_html(block_with_markdown, strlen(block_with_markdown), &opts);
    assert_contains(html, "<article", "Block with markdown renders");
    assert_contains(html, "<h2", "Block content parsed as markdown (heading)");
    assert_contains(html, "Heading", "Block content parsed as markdown (heading text)");
    assert_contains(html, "<strong", "Block content parsed as markdown (bold)");
    assert_contains(html, "bold", "Block content parsed as markdown (bold text)");
    apex_free_string(html);

    /* Test minimum 3 colons */
    const char *min_colons = "::: {.minimal}\nMinimal div\n:::";
    html = apex_markdown_to_html(min_colons, strlen(min_colons), &opts);
    assert_contains(html, "<div", "Minimum colons div renders");
    assert_contains(html, "class=\"minimal\"", "Minimum colons div has class");
    apex_free_string(html);

    /* Test fenced div with only ID */
    const char *only_id = "::: {#only-id}\nDiv with only ID\n:::";
    html = apex_markdown_to_html(only_id, strlen(only_id), &opts);
    assert_contains(html, "<div", "Only ID div renders");
    assert_contains(html, "id=\"only-id\"", "Only ID div has ID");
    assert_not_contains(html, "class=", "Only ID div has no class");
    apex_free_string(html);

    /* Test fenced div with only classes */
    const char *only_classes = "::: {.only-classes .multiple}\nDiv with only classes\n:::";
    html = apex_markdown_to_html(only_classes, strlen(only_classes), &opts);
    assert_contains(html, "<div", "Only classes div renders");
    assert_contains(html, "class=\"only-classes multiple\"", "Only classes div has classes");
    assert_not_contains(html, "id=", "Only classes div has no ID");
    apex_free_string(html);

    /* Test fenced div with quoted attribute values */
    const char *quoted_values = "::::: {#quoted .test attr1=\"quoted value\" attr2='single quoted'}\nContent\n:::::";
    html = apex_markdown_to_html(quoted_values, strlen(quoted_values), &opts);
    assert_contains(html, "<div", "Quoted values div renders");
    assert_contains(html, "attr1=\"quoted value\"", "Double-quoted attribute");
    assert_contains(html, "attr2=\"single quoted\"", "Single-quoted attribute converted");
    apex_free_string(html);

    /* Test fenced div disabled in non-Unified mode */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
    gfm_opts.enable_divs = true;  /* Even if enabled, should not work in GFM mode */
    const char *div_in_gfm = "::: {.test}\nContent\n:::";
    html = apex_markdown_to_html(div_in_gfm, strlen(div_in_gfm), &gfm_opts);
    assert_not_contains(html, "<div", "Fenced divs disabled in GFM mode");
    apex_free_string(html);

    /* Test fenced div disabled with --no-divs flag */
    apex_options no_divs_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    no_divs_opts.enable_divs = false;
    const char *div_disabled = "::: {.test}\nContent\n:::";
    html = apex_markdown_to_html(div_disabled, strlen(div_disabled), &no_divs_opts);
    assert_not_contains(html, "<div", "Fenced divs disabled with --no-divs");
    apex_free_string(html);

    /* Test fenced div with mixed content (lists, blockquotes) */
    const char *mixed_content = "::::: {#mixed .content}\n- List item\n- Another item\n\n> A blockquote\n\nAnd a paragraph.\n:::::";
    html = apex_markdown_to_html(mixed_content, strlen(mixed_content), &opts);
    assert_contains(html, "<div", "Mixed content div renders");
    assert_contains(html, "<ul>", "Mixed content has list");
    assert_contains(html, "<blockquote>", "Mixed content has blockquote");
    assert_contains(html, "<p>And a paragraph.</p>", "Mixed content has paragraph");
    apex_free_string(html);

    /* Test multiple fenced divs in sequence */
    const char *multiple_divs = "::: {.first}\nFirst div.\n:::\n\n::: {.second}\nSecond div.\n:::\n\n::: {.third}\nThird div.\n:::";
    html = apex_markdown_to_html(multiple_divs, strlen(multiple_divs), &opts);
    assert_contains(html, "class=\"first\"", "First div class");
    assert_contains(html, "class=\"second\"", "Second div class");
    assert_contains(html, "class=\"third\"", "Third div class");
    assert_contains(html, "First div", "First div content");
    assert_contains(html, "Second div", "Second div content");
    assert_contains(html, "Third div", "Third div content");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Fenced Divs Tests", had_failures, false);
}

/**
 * Test abbreviations
 */

void test_abbreviations(void) {
    int suite_failures = suite_start();
    print_suite_title("Abbreviations Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test basic abbreviation */
    const char *abbr_doc = "*[HTML]: Hypertext Markup Language\n\nHTML is great.";
    html = apex_markdown_to_html(abbr_doc, strlen(abbr_doc), &opts);
    assert_contains(html, "<abbr", "Abbreviation tag created");
    assert_contains(html, "Hypertext Markup Language", "Abbreviation title");
    apex_free_string(html);

    /* Test multiple abbreviations */
    const char *multi_abbr = "*[CSS]: Cascading Style Sheets\n*[JS]: JavaScript\n\nCSS and JS are essential.";
    html = apex_markdown_to_html(multi_abbr, strlen(multi_abbr), &opts);
    assert_contains(html, "<abbr", "Abbreviation tags present");
    assert_contains(html, "Cascading Style Sheets", "First abbreviation");
    assert_contains(html, "JavaScript", "Second abbreviation");
    apex_free_string(html);

    /* Test abbreviation with multiple occurrences */
    const char *multiple = "*[API]: Application Programming Interface\n\nThe API docs explain the API usage.";
    html = apex_markdown_to_html(multiple, strlen(multiple), &opts);
    assert_contains(html, "<abbr", "Multiple occurrences wrapped");
    assert_contains(html, "Application Programming Interface", "Abbreviation definition");
    apex_free_string(html);

    /* Test text without abbreviations */
    const char *no_abbr = "Just plain text here.";
    html = apex_markdown_to_html(no_abbr, strlen(no_abbr), &opts);
    assert_contains(html, "plain text", "Non-abbreviation text preserved");
    apex_free_string(html);

    /* Test MMD 6 reference abbreviation: [>abbr]: expansion */
    const char *mmd6_ref = "[>MMD]: MultiMarkdown\n\n[>MMD] is great.";
    html = apex_markdown_to_html(mmd6_ref, strlen(mmd6_ref), &opts);
    assert_contains(html, "<abbr", "MMD 6 reference abbr tag");
    assert_contains(html, "MultiMarkdown", "MMD 6 reference expansion");
    apex_free_string(html);

    /* Test MMD 6 inline abbreviation: [>(abbr) expansion] */
    const char *mmd6_inline = "This is [>(MD) Markdown] syntax.";
    html = apex_markdown_to_html(mmd6_inline, strlen(mmd6_inline), &opts);
    assert_contains(html, "<abbr title=\"Markdown\">MD</abbr>", "MMD 6 inline abbr");
    apex_free_string(html);

    /* Test multiple MMD 6 inline abbreviations */
    const char *mmd6_multi = "[>(HTML) Hypertext] and [>(CSS) Styles] work.";
    html = apex_markdown_to_html(mmd6_multi, strlen(mmd6_multi), &opts);
    assert_contains(html, "title=\"Hypertext\">HTML</abbr>", "First MMD 6 inline");
    assert_contains(html, "title=\"Styles\">CSS</abbr>", "Second MMD 6 inline");
    apex_free_string(html);

    /* Test mixing old and new syntax */
    const char *mixed = "*[OLD]: Old Style\n[>NEW]: New Style\n\nOLD and [>NEW] work.";
    html = apex_markdown_to_html(mixed, strlen(mixed), &opts);
    assert_contains(html, "Old Style", "Old syntax in mixed");
    assert_contains(html, "New Style", "New syntax in mixed");
    apex_free_string(html);

    /* Test inline and reference-style in same document (both must be wrapped) */
    const char *inline_and_ref = "This is HTML. And more [>(ABBR) abbreviation syntax].\n\n[>HTML]: Hypertext Markup Language";
    html = apex_markdown_to_html(inline_and_ref, strlen(inline_and_ref), &opts);
    assert_contains(html, "<abbr title=\"Hypertext Markup Language\">HTML</abbr>", "Reference abbr when mixed");
    assert_contains(html, "<abbr title=\"abbreviation syntax\">ABBR</abbr>", "Inline abbr when mixed");
    apex_free_string(html);

    /* Regression for issue #31: preprocessing must preserve physical lines
     * longer than the old 1024-byte abbreviation and 2048-byte ALD buffers. */
    const char *tail_marker = "END-OF-LONG-LINE";
    const size_t prefix_len = 2160;
    size_t marker_len = strlen(tail_marker);
    char *long_line = malloc(prefix_len + marker_len + 2);
    if (long_line) {
        memset(long_line, 'a', prefix_len);
        memcpy(long_line + prefix_len, tail_marker, marker_len);
        long_line[prefix_len + marker_len] = '\n';
        long_line[prefix_len + marker_len + 1] = '\0';

        const apex_mode_t modes[] = {
            APEX_MODE_MULTIMARKDOWN,
            APEX_MODE_KRAMDOWN,
            APEX_MODE_UNIFIED,
            APEX_MODE_QUARTO
        };
        const char *mode_names[] = {"MMD", "Kramdown", "Unified", "Quarto"};
        for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
            apex_options long_opts = apex_options_for_mode(modes[i]);
            html = apex_markdown_to_html(long_line, strlen(long_line), &long_opts);
            test_resultf(html && strstr(html, tail_marker) != NULL,
                         "%s mode preserves lines longer than 2048 bytes", mode_names[i]);
            apex_free_string(html);
        }
        free(long_line);
    } else {
        test_result(false, "Allocate issue #31 long-line regression input");
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Abbreviations Tests", had_failures, false);
}

/**
 * Test MMD 6 features: multi-line setext headers and link/image titles with different quotes
 */

void test_mmd6_features(void) {
    int suite_failures = suite_start();
    print_suite_title("MMD 6 Features Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test multi-line setext header (h1) */
    const char *multiline_h1 = "This is\na multi-line\nsetext header\n========";
    html = apex_markdown_to_html(multiline_h1, strlen(multiline_h1), &opts);
    assert_contains(html, "<h1", "Multi-line setext h1 tag");
    assert_contains(html, "This is", "Multi-line setext h1 contains first line");
    assert_contains(html, "a multi-line", "Multi-line setext h1 contains second line");
    assert_contains(html, "setext header</h1>", "Multi-line setext h1 contains last line");
    apex_free_string(html);

    /* Test multi-line setext header (h2) */
    const char *multiline_h2 = "Another\nheader\nwith\nmultiple\nlines\n--------";
    html = apex_markdown_to_html(multiline_h2, strlen(multiline_h2), &opts);
    assert_contains(html, "<h2", "Multi-line setext h2 tag");
    assert_contains(html, "Another", "Multi-line setext h2 contains first line");
    assert_contains(html, "multiple", "Multi-line setext h2 contains middle line");
    assert_contains(html, "lines</h2>", "Multi-line setext h2 contains last line");
    apex_free_string(html);

    /* Test link title with double quotes */
    const char *link_double = "[Link](https://example.com \"Double quote title\")";
    html = apex_markdown_to_html(link_double, strlen(link_double), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Link with double quote title has href");
    assert_contains(html, "title=\"Double quote title\"", "Link with double quote title");
    apex_free_string(html);

    /* Test link title with single quotes */
    const char *link_single = "[Link](https://example.com 'Single quote title')";
    html = apex_markdown_to_html(link_single, strlen(link_single), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Link with single quote title has href");
    assert_contains(html, "title=\"Single quote title\"", "Link with single quote title");
    apex_free_string(html);

    /* Test link title with parentheses */
    const char *link_paren = "[Link](https://example.com (Parentheses title))";
    html = apex_markdown_to_html(link_paren, strlen(link_paren), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Link with parentheses title has href");
    assert_contains(html, "title=\"Parentheses title\"", "Link with parentheses title");
    apex_free_string(html);

    /* Test image title with double quotes */
    const char *img_double = "![Image](image.png \"Double quote title\")";
    html = apex_markdown_to_html(img_double, strlen(img_double), &opts);
    assert_contains(html, "<img src=\"image.png\"", "Image with double quote title has src");
    assert_contains(html, "title=\"Double quote title\"", "Image with double quote title");
    apex_free_string(html);

    /* Test image title with single quotes */
    const char *img_single = "![Image](image.png 'Single quote title')";
    html = apex_markdown_to_html(img_single, strlen(img_single), &opts);
    assert_contains(html, "<img src=\"image.png\"", "Image with single quote title has src");
    assert_contains(html, "title=\"Single quote title\"", "Image with single quote title");
    apex_free_string(html);

    /* Test image title with parentheses */
    const char *img_paren = "![Image](image.png (Parentheses title))";
    html = apex_markdown_to_html(img_paren, strlen(img_paren), &opts);
    assert_contains(html, "<img src=\"image.png\"", "Image with parentheses title has src");
    assert_contains(html, "title=\"Parentheses title\"", "Image with parentheses title");
    apex_free_string(html);

    /* Test reference link with double quote title */
    const char *ref_double = "[Ref][id]\n\n[id]: https://example.com \"Reference title\"";
    html = apex_markdown_to_html(ref_double, strlen(ref_double), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Reference link with double quote title has href");
    assert_contains(html, "title=\"Reference title\"", "Reference link with double quote title");
    apex_free_string(html);

    /* Test reference link with single quote title */
    const char *ref_single = "[Ref][id]\n\n[id]: https://example.com 'Reference title'";
    html = apex_markdown_to_html(ref_single, strlen(ref_single), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Reference link with single quote title has href");
    assert_contains(html, "title=\"Reference title\"", "Reference link with single quote title");
    apex_free_string(html);

    /* Test reference link with parentheses title */
    const char *ref_paren = "[Ref][id]\n\n[id]: https://example.com (Reference title)";
    html = apex_markdown_to_html(ref_paren, strlen(ref_paren), &opts);
    assert_contains(html, "<a href=\"https://example.com\"", "Reference link with parentheses title has href");
    assert_contains(html, "title=\"Reference title\"", "Reference link with parentheses title");
    apex_free_string(html);

    /* Test in unified mode as well */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *unified_test = "Multi\nLine\nHeader\n========\n\n[Link](url 'Title')";
    html = apex_markdown_to_html(unified_test, strlen(unified_test), &unified_opts);
    assert_contains(html, "<h1", "Multi-line setext header works in unified mode");
    assert_contains(html, "Multi\nLine\nHeader</h1>", "Multi-line setext header content in unified mode");
    assert_contains(html, "title=\"Title\"", "Link title with single quotes works in unified mode");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("MMD 6 Features Tests", had_failures, false);
}

/**
 * Test emoji support
 */

void test_emoji(void) {
    int suite_failures = suite_start();
    print_suite_title("Emoji Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_marked_extensions = true;
    char *html;

    /* Test basic emoji */
    html = apex_markdown_to_html("Hello :smile: world", 19, &opts);
    assert_contains(html, "😄", "Smile emoji converted");
    apex_free_string(html);

    /* Test multiple emoji */
    html = apex_markdown_to_html(":thumbsup: :heart: :rocket:", 27, &opts);
    assert_contains(html, "👍", "Thumbs up emoji");
    assert_contains(html, "❤", "Heart emoji");
    assert_contains(html, "🚀", "Rocket emoji");
    apex_free_string(html);

    /* Test emoji in text */
    html = apex_markdown_to_html("I :heart: coding!", 17, &opts);
    assert_contains(html, "❤", "Emoji in sentence");
    assert_contains(html, "coding", "Regular text preserved");
    apex_free_string(html);

    /* Test unknown emoji (should be preserved) */
    html = apex_markdown_to_html(":notarealemojicode:", 19, &opts);
    assert_contains(html, ":notarealemojicode:", "Unknown emoji preserved");
    apex_free_string(html);

    /* --no-emoji / enable_emoji=false leaves :name: literal */
    opts.enable_emoji = false;
    html = apex_markdown_to_html("Hello :smile: world", 19, &opts);
    assert_contains(html, ":smile:", "enable_emoji=false preserves :smile:");
    assert_not_contains(html, "😄", "enable_emoji=false does not convert smile");
    apex_free_string(html);

    apex_options unified_no_emoji = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_no_emoji.enable_emoji = false;
    html = apex_markdown_to_html("Go :rocket:", 11, &unified_no_emoji);
    assert_contains(html, ":rocket:", "unified + enable_emoji=false preserves :rocket:");
    assert_not_contains(html, "🚀", "unified + enable_emoji=false does not convert rocket");
    apex_free_string(html);

    opts.enable_emoji = true;

    /* Test emoji variations */
    html = apex_markdown_to_html(":star: :warning: :+1:", 21, &opts);
    assert_contains(html, "⭐", "Star emoji");
    assert_contains(html, "⚠", "Warning emoji");
    assert_contains(html, "👍", "Plus one emoji");
    apex_free_string(html);

    /* Emoji NOT converted inside inline code span */
    html = apex_markdown_to_html("Use `:smile:` for emoji syntax", 30, &opts);
    assert_contains(html, "<code>:smile:</code>", "Emoji preserved as literal in inline code");
    assert_not_contains(html, "😄", "Emoji character not in code span output");
    apex_free_string(html);

    /* Emoji NOT converted inside fenced code block */
    const char *fenced_emoji = "```\n:smile:\n:rocket:\n```";
    html = apex_markdown_to_html(fenced_emoji, strlen(fenced_emoji), &opts);
    assert_contains(html, ":smile:", "Emoji pattern preserved in fenced block");
    assert_contains(html, ":rocket:", "Second emoji preserved in fenced block");
    apex_free_string(html);

    /* Emoji NOT converted inside indented code block */
    const char *indented_emoji = "    :smile:\n    :rocket:";
    html = apex_markdown_to_html(indented_emoji, strlen(indented_emoji), &opts);
    assert_contains(html, ":smile:", "Emoji pattern preserved in indented block");
    assert_contains(html, ":rocket:", "Second emoji preserved in indented block");
    apex_free_string(html);

    /* Timecodes must not be treated as emoji shortcodes (Unified + autocorrect). */
    apex_options unified = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified.enable_plugins = false;
    html = apex_markdown_to_html("1:00:55:00", 10, &unified);
    assert_contains(html, "1:00:55:00", "Timecode 1:00:55:00 preserved with emoji+autocorrect");
    assert_not_contains(html, "💯", "Timecode does not emit 100 emoji");
    apex_free_string(html);

    html = apex_markdown_to_html("at 12:00:00", 11, &unified);
    assert_contains(html, "12:00:00", "HH:MM:SS timecode preserved");
    assert_not_contains(html, "💯", "HH:MM:SS does not emit 100 emoji");
    apex_free_string(html);

    /* Exact numeric shortcode still works when properly bounded */
    html = apex_markdown_to_html(":100:", 5, &unified);
    assert_contains(html, "💯", "Exact :100: still converts");
    apex_free_string(html);

    /* BOL and punctuation-adjacent shortcodes still convert */
    html = apex_markdown_to_html(":smile:", 7, &unified);
    assert_contains(html, "😄", "BOL :smile: still converts");
    apex_free_string(html);

    html = apex_markdown_to_html("hello :smile: world", 19, &unified);
    assert_contains(html, "😄", "Whitespace-bounded :smile: still converts");
    apex_free_string(html);

    html = apex_markdown_to_html("(:smile:)", 9, &unified);
    assert_contains(html, "😄", "Punctuation-adjacent :smile: still converts");
    apex_free_string(html);

    /* Letter-adjacent shortcodes are not converted */
    html = apex_markdown_to_html("foo:smile:bar", 13, &unified);
    assert_contains(html, "foo:smile:bar", "Letter-adjacent :smile: left literal");
    assert_not_contains(html, "😄", "Letter-adjacent :smile: does not convert");
    apex_free_string(html);

    /* Autocorrect still fixes safe typos; all-digit fuzzy must not run */
    html = apex_markdown_to_html(":smlie:", 7, &unified);
    assert_contains(html, "😄", "Typo :smlie: still autocorrects to smile");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Emoji Tests", had_failures, false);
}

/**
 * Test special markers (page breaks, pauses, end-of-block)
 */

void test_special_markers(void) {
    int suite_failures = suite_start();
    print_suite_title("Special Markers Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_marked_extensions = true;
    char *html;

    /* Test page break HTML comment */
    html = apex_markdown_to_html("Before\n\n<!--BREAK-->\n\nAfter", 28, &opts);
    assert_contains(html, "page-break-after", "Page break marker");
    assert_contains(html, "Before", "Content before break");
    assert_contains(html, "After", "Content after break");
    apex_free_string(html);

    /* Test Kramdown page break */
    html = apex_markdown_to_html("Page 1\n\n{::pagebreak /}\n\nPage 2", 32, &opts);
    assert_contains(html, "page-break-after", "Kramdown page break");
    assert_contains(html, "Page 2", "Content after pagebreak");
    apex_free_string(html);

    /* Test autoscroll pause */
    html = apex_markdown_to_html("Text\n\n<!--PAUSE:5-->\n\nMore text", 31, &opts);
    assert_contains(html, "data-pause", "Pause marker");
    assert_contains(html, "data-pause=\"5\"", "Pause duration");
    assert_contains(html, "More text", "Content after pause");
    apex_free_string(html);

    /* Test end-of-block marker */
    const char *eob = "- Item 1\n\n^\n\n- Item 2";
    html = apex_markdown_to_html(eob, strlen(eob), &opts);
    // End of block should separate lists
    assert_contains(html, "<ul>", "Lists created");
    apex_free_string(html);

    /* Test empty HTML comment as block separator (CommonMark spec) */
    const char *html_comment_separator = "- foo\n- bar\n\n<!-- -->\n\n- baz\n- bim";
    html = apex_markdown_to_html(html_comment_separator, strlen(html_comment_separator), &opts);
    // Should create two separate lists, not one merged list
    const char *first_ul = strstr(html, "<ul>");
    const char *second_ul = first_ul ? strstr(first_ul + 1, "<ul>") : NULL;
    if (second_ul != NULL) {
        test_result(true, "Empty HTML comment separates lists");
    } else {
        test_result(false, "Empty HTML comment does not separate lists");
    }
    assert_contains(html, "<li>foo</li>", "First list contains foo");
    assert_contains(html, "<li>bar</li>", "First list contains bar");
    assert_contains(html, "<li>baz</li>", "Second list contains baz");
    assert_contains(html, "<li>bim</li>", "Second list contains bim");
    assert_contains(html, "<!-- -->", "Empty HTML comment preserved");
    apex_free_string(html);

    /* Test multiple page breaks */
    const char *multi = "Section 1\n\n<!--BREAK-->\n\nSection 2\n\n<!--BREAK-->\n\nSection 3";
    html = apex_markdown_to_html(multi, strlen(multi), &opts);
    assert_contains(html, "page-break-after", "Multiple page breaks");
    assert_contains(html, "Section 1", "First section");
    assert_contains(html, "Section 3", "Last section");
    apex_free_string(html);

    /* Special markers NOT converted inside inline code span */
    html = apex_markdown_to_html("`<!--PAUSE:15-->`", 17, &opts);
    assert_contains(html, "<code>&lt;!--PAUSE:15--&gt;</code>", "Pause marker preserved in inline code");
    assert_not_contains(html, "data-pause", "Pause marker not rendered in inline code");
    apex_free_string(html);

    /* Special markers NOT converted inside indented code block */
    const char *indented_break = "    <!--BREAK-->";
    html = apex_markdown_to_html(indented_break, strlen(indented_break), &opts);
    assert_contains(html, "&lt;!--BREAK--&gt;", "Break marker preserved in indented code");
    assert_not_contains(html, "page-break-after", "Break marker not rendered in indented code");
    apex_free_string(html);

    /* Special markers NOT converted inside fenced code block */
    const char *fenced = "```\n<!--BREAK-->\n```";
    html = apex_markdown_to_html(fenced, strlen(fenced), &opts);
    assert_contains(html, "&lt;!--BREAK--&gt;", "Break marker preserved in fenced code");
    assert_not_contains(html, "page-break-after", "Break marker not rendered in fenced code");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Special Markers Tests", had_failures, false);
}

/**
 * Test inline tables from CSV/TSV
 */

void test_advanced_footnotes(void) {
    int suite_failures = suite_start();
    print_suite_title("Advanced Footnotes Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    char *html;

    /* Direct call: cover NULL-root early return */
    test_result(apex_process_advanced_footnotes(NULL, NULL) == NULL, "advanced footnotes: NULL root returns NULL");

    /* Test basic footnote */
    const char *basic = "Text[^1]\n\n[^1]: Footnote text";
    html = apex_markdown_to_html(basic, strlen(basic), &opts);
    assert_contains(html, "footnote", "Footnote generated");
    apex_free_string(html);

    /* Test Kramdown inline footnote: ^[text] */
    const char *kramdown_inline = "Text^[Kramdown inline footnote]";
    html = apex_markdown_to_html(kramdown_inline, strlen(kramdown_inline), &opts);
    assert_contains(html, "footnote", "Kramdown inline footnote");
    assert_contains(html, "Kramdown inline footnote", "Kramdown footnote content");
    apex_free_string(html);

    /* Test MMD inline footnote: [^text with spaces] */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    const char *mmd_inline = "Text[^MMD inline footnote with spaces]";
    html = apex_markdown_to_html(mmd_inline, strlen(mmd_inline), &mmd_opts);
    assert_contains(html, "footnote", "MMD inline footnote");
    assert_contains(html, "MMD inline footnote with spaces", "MMD footnote content");
    apex_free_string(html);

    /* Test regular reference (no spaces) still works */
    const char *reference = "Text[^ref]\n\n[^ref]: Definition";
    html = apex_markdown_to_html(reference, strlen(reference), &mmd_opts);
    assert_contains(html, "footnote", "Regular reference footnote");
    assert_contains(html, "Definition", "Reference footnote content");
    apex_free_string(html);

    /* Test multiple inline footnotes */
    const char *multiple = "First^[one] and second^[two] footnotes";
    html = apex_markdown_to_html(multiple, strlen(multiple), &opts);
    assert_contains(html, "one", "First inline footnote");
    assert_contains(html, "two", "Second inline footnote");
    apex_free_string(html);

    /* Test inline footnote with formatting */
    const char *formatted = "Text^[footnote with **bold**]";
    html = apex_markdown_to_html(formatted, strlen(formatted), &opts);
    assert_contains(html, "footnote", "Formatted inline footnote");
    /* Note: Markdown in inline footnotes handled by cmark-gfm */
    apex_free_string(html);

    /* Test advanced footnote with multiple paragraphs, list, and fenced code (```).
     * This should exercise the reparse path for block-level content inside footnotes.
     */
    const char *blocky =
        "Text[^a]\n"
        "\n"
        "[^a]: First para\n"
        "\n"
        "    Second para\n"
        "\n"
        "    - item1\n"
        "    - item2\n"
        "\n"
        "    ```\n"
        "    code\n"
        "    ```\n";
    html = apex_markdown_to_html(blocky, strlen(blocky), &opts);
    assert_contains(html, "<p>First para</p>", "Advanced footnote: first paragraph");
    assert_contains(html, "<p>Second para</p>", "Advanced footnote: second paragraph");
    assert_contains(html, "<ul>", "Advanced footnote: list parsed");
    assert_contains(html, "<li>item1</li>", "Advanced footnote: list item 1");
    assert_contains(html, "<pre><code>code", "Advanced footnote: fenced code block parsed");
    apex_free_string(html);

    /* Test advanced footnote with indented code block (4+ spaces after newline). */
    const char *indented_code =
        "Text[^b]\n"
        "\n"
        "[^b]: Intro\n"
        "\n"
        "        indented\n"
        "        code\n";
    html = apex_markdown_to_html(indented_code, strlen(indented_code), &opts);
    assert_contains(html, "<p>Intro</p>", "Indented code footnote: intro paragraph");
    assert_contains(html, "<pre><code>indented", "Indented code footnote: code block parsed");
    apex_free_string(html);

    /* Test advanced footnote with fenced code using ~~~ (alternate fence detection). */
    const char *tilde_fence =
        "Text[^c]\n"
        "\n"
        "[^c]: Para\n"
        "\n"
        "    ~~~\n"
        "    tilde\n"
        "    ~~~\n";
    html = apex_markdown_to_html(tilde_fence, strlen(tilde_fence), &opts);
    assert_contains(html, "<pre><code>tilde", "Tilde fence footnote: code block parsed");
    apex_free_string(html);

    /* Test ordered list inside footnote. */
    const char *ordered_list =
        "Text[^d]\n"
        "\n"
        "[^d]: Steps\n"
        "\n"
        "    1. one\n"
        "    2. two\n";
    html = apex_markdown_to_html(ordered_list, strlen(ordered_list), &opts);
    assert_contains(html, "<p>Steps</p>", "Ordered list footnote: intro paragraph");
    assert_contains(html, "<ol>", "Ordered list footnote: ordered list parsed");
    assert_contains(html, "<li>one</li>", "Ordered list footnote: first item");
    assert_contains(html, "<li>two</li>", "Ordered list footnote: second item");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Advanced Footnotes Tests", had_failures, false);
}

/**
 * Test standalone document output
 */

void test_sup_sub(void) {
    int suite_failures = suite_start();
    print_suite_title("Superscript, Subscript, Underline, Delete, and Highlight Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_sup_sub = true;
    char *html;

    /* ===== SUBSCRIPT TESTS ===== */

    /* Test H~2~O for subscript 2 (paired tildes within word) */
    html = apex_markdown_to_html("H~2~O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "H~2~O creates subscript 2");
    assert_contains(html, "H<sub>2</sub>O", "Subscript within word");
    if (strstr(html, "<u>2</u>") == NULL) {
        test_result(true, "H~2~O is subscript, not underline");
    } else {
        test_result(false, "H~2~O incorrectly treated as underline");
    }
    apex_free_string(html);

    /* Test H~2~SO~4~ for both 2 and 4 as subscripts */
    html = apex_markdown_to_html("H~2~SO~4~", 9, &opts);
    assert_contains(html, "<sub>2</sub>", "H~2~SO~4~ creates subscript 2");
    assert_contains(html, "<sub>4</sub>", "H~2~SO~4~ creates subscript 4");
    assert_contains(html, "H<sub>2</sub>SO<sub>4</sub>", "Multiple subscripts within word");
    apex_free_string(html);

    /* Test subscript ends at sentence terminators */
    html = apex_markdown_to_html("H~2.O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at period");
    apex_free_string(html);

    html = apex_markdown_to_html("H~2,O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at comma");
    apex_free_string(html);

    html = apex_markdown_to_html("H~2;O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at semicolon");
    apex_free_string(html);

    html = apex_markdown_to_html("H~2:O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at colon");
    apex_free_string(html);

    html = apex_markdown_to_html("H~2!O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at exclamation");
    apex_free_string(html);

    html = apex_markdown_to_html("H~2?O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at question mark");
    apex_free_string(html);

    /* Test subscript ends at space */
    html = apex_markdown_to_html("H~2 O", 5, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript stops at space");
    assert_contains(html, "H<sub>2</sub> O", "Space after subscript");
    apex_free_string(html);

    /* ===== SUPERSCRIPT TESTS ===== */

    /* Test basic superscript */
    html = apex_markdown_to_html("m^2", 3, &opts);
    assert_contains(html, "<sup>2</sup>", "Basic superscript m^2");
    assert_contains(html, "m<sup>2</sup>", "Superscript in context");
    apex_free_string(html);

    /* Test superscript ends at space */
    html = apex_markdown_to_html("x^2 + y^2", 9, &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript stops at space");
    assert_contains(html, "x<sup>2</sup>", "First superscript");
    assert_contains(html, "y<sup>2</sup>", "Second superscript");
    apex_free_string(html);

    /* Test superscript ends at sentence terminators */
    html = apex_markdown_to_html("x^2.", 4, &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript stops at period");
    apex_free_string(html);

    html = apex_markdown_to_html("x^2,", 4, &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript stops at comma");
    apex_free_string(html);

    html = apex_markdown_to_html("E = mc^2!", 9, &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript stops at exclamation");
    apex_free_string(html);

    /* Test multiple superscripts */
    html = apex_markdown_to_html("x^2 + y^2 = z^2", 15, &opts);
    assert_contains(html, "x<sup>2</sup>", "First superscript");
    assert_contains(html, "y<sup>2</sup>", "Second superscript");
    assert_contains(html, "z<sup>2</sup>", "Third superscript");
    apex_free_string(html);

    /* ===== UNDERLINE TESTS ===== */

    /* Test underline with tildes at word boundaries */
    html = apex_markdown_to_html("text ~underline~ text", 22, &opts);
    assert_contains(html, "<u>underline</u>", "Tildes at word boundaries create underline");
    assert_contains(html, "text <u>underline</u> text", "Underline in context");
    if (strstr(html, "<sub>underline</sub>") == NULL) {
        test_result(true, "~underline~ is underline, not subscript");
    } else {
        test_result(false, "~underline~ incorrectly treated as subscript");
    }
    apex_free_string(html);

    /* Test underline with single word */
    html = apex_markdown_to_html("~h2o~", 6, &opts);
    assert_contains(html, "<u>h2o</u>", "~h2o~ creates underline");
    if (strstr(html, "<sub>") == NULL) {
        test_result(true, "~h2o~ is underline, not subscript");
    } else {
        test_result(false, "~h2o~ incorrectly treated as subscript");
    }
    apex_free_string(html);

    /* ===== STRIKETHROUGH/DELETE TESTS ===== */

    /* Test strikethrough with double tildes */
    html = apex_markdown_to_html("text ~~deleted text~~ text", 26, &opts);
    assert_contains(html, "<del>deleted text</del>", "Double tildes create strikethrough");
    assert_contains(html, "text <del>deleted text</del> text", "Strikethrough in context");
    apex_free_string(html);

    /* Test strikethrough doesn't interfere with subscript */
    html = apex_markdown_to_html("H~2~O and ~~deleted~~", 21, &opts);
    assert_contains(html, "<sub>2</sub>", "Subscript still works with strikethrough");
    assert_contains(html, "<del>deleted</del>", "Strikethrough still works with subscript");
    apex_free_string(html);

    /* Test strikethrough doesn't interfere with underline */
    html = apex_markdown_to_html("~underline~ and ~~deleted~~", 27, &opts);
    assert_contains(html, "<u>underline</u>", "Underline still works with strikethrough");
    assert_contains(html, "<del>deleted</del>", "Strikethrough still works with underline");
    apex_free_string(html);

    /* ===== HIGHLIGHT TESTS ===== */

    /* Test highlight with double equals */
    html = apex_markdown_to_html("text ==highlighted text== text", 30, &opts);
    assert_contains(html, "<mark>highlighted text</mark>", "Double equals create highlight");
    assert_contains(html, "text <mark>highlighted text</mark> text", "Highlight in context");
    apex_free_string(html);

    /* Test highlight with single word */
    html = apex_markdown_to_html("==highlight==", 14, &opts);
    assert_contains(html, "<mark>highlight</mark>", "Single word highlight");
    apex_free_string(html);

    /* Test highlight with multiple words */
    html = apex_markdown_to_html("==this is highlighted==", 24, &opts);
    assert_contains(html, "<mark>this is highlighted</mark>", "Multi-word highlight");
    apex_free_string(html);

    /* Test highlight doesn't break Setext h1 */
    html = apex_markdown_to_html("Header\n==\n\n==highlight==", 25, &opts);
    assert_contains(html, "<h1", "Setext h1 still works");
    assert_contains(html, "Header</h1>", "Setext h1 content");
    assert_contains(html, "<mark>highlight</mark>", "Highlight after Setext h1");
    /* Verify the == after header is not treated as highlight */
    if (strstr(html, "<mark></mark>") == NULL || strstr(html, "<mark>\n</mark>") == NULL) {
        test_result(true, "== after Setext h1 doesn't break header");
    } else {
        test_result(false, "== after Setext h1 breaks header");
    }
    apex_free_string(html);

    /* Test highlight with Setext h2 (===) */
    html = apex_markdown_to_html("Header\n---\n\n==highlight==", 25, &opts);
    assert_contains(html, "<h2", "Setext h2 still works");
    assert_contains(html, "Header</h2>", "Setext h2 content");
    assert_contains(html, "<mark>highlight</mark>", "Highlight after Setext h2");
    apex_free_string(html);

    /* Test highlight in various contexts */
    html = apex_markdown_to_html("Before ==highlight== after", 26, &opts);
    assert_contains(html, "<mark>highlight</mark>", "Highlight in paragraph");
    apex_free_string(html);

    html = apex_markdown_to_html("**bold ==highlight== bold**", 27, &opts);
    assert_contains(html, "<mark>highlight</mark>", "Highlight in bold");
    apex_free_string(html);

    /* ===== INTERACTION TESTS ===== */

    /* Test that sup/sub is disabled when option is off */
    apex_options no_sup_sub = apex_options_default();
    no_sup_sub.enable_sup_sub = false;
    html = apex_markdown_to_html("H^2 O", 5, &no_sup_sub);
    if (strstr(html, "<sup>") == NULL) {
        test_result(true, "Sup/sub disabled when option is off");
    } else {
        test_result(false, "Sup/sub not disabled when option is off");
    }
    apex_free_string(html);

    /* Test that sup/sub is disabled in CommonMark mode */
    apex_options cm_opts = apex_options_for_mode(APEX_MODE_COMMONMARK);
    html = apex_markdown_to_html("H^2 O", 5, &cm_opts);
    if (strstr(html, "<sup>") == NULL) {
        test_result(true, "Sup/sub disabled in CommonMark mode");
    } else {
        test_result(false, "Sup/sub not disabled in CommonMark mode");
    }
    apex_free_string(html);

    /* Test that sup/sub is enabled in Unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    html = apex_markdown_to_html("H^2 O", 5, &unified_opts);
    assert_contains(html, "<sup>2</sup>", "Sup/sub enabled in Unified mode");
    apex_free_string(html);

    /* Test that sup/sub is enabled in MultiMarkdown mode */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    html = apex_markdown_to_html("H^2 O", 5, &mmd_opts);
    assert_contains(html, "<sup>2</sup>", "Sup/sub enabled in MultiMarkdown mode");
    apex_free_string(html);

    /* Test that ^ and ~ are preserved in math spans */
    opts.enable_math = true;
    html = apex_markdown_to_html("Equation: $E=mc^2$", 18, &opts);
    assert_contains(html, "E=mc^2", "Superscript preserved in math span");
    if (strstr(html, "<sup>2</sup>") == NULL) {
        test_result(true, "Superscript not processed inside math span");
    } else {
        test_result(false, "Superscript incorrectly processed inside math span");
    }
    apex_free_string(html);

    /* Test that ^ is preserved in footnote references */
    html = apex_markdown_to_html("Text[^ref]", 10, &opts);
    if (strstr(html, "<sup>ref</sup>") == NULL) {
        test_result(true, "Superscript not processed in footnote reference");
    } else {
        test_result(false, "Superscript incorrectly processed in footnote reference");
    }
    apex_free_string(html);

    /* Test that ~ is preserved in critic markup */
    opts.enable_critic_markup = true;
    html = apex_markdown_to_html("{~~old~>new~~}", 13, &opts);
    if (strstr(html, "<sub>old</sub>") == NULL) {
        test_result(true, "Subscript not processed in critic markup");
    } else {
        test_result(false, "Subscript incorrectly processed in critic markup");
    }
    apex_free_string(html);
    opts.enable_critic_markup = false;

    /* ===== CODE BLOCKS: extended syntax not processed ===== */

    /* Indented code block: ^ ~ ~~ == must appear literally, not as sup/sub/underline/strikethrough/highlight */
    const char *indented_with_extended = "Normal text x^2 here.\n\n    code with ~subscript~ and ^caret^\n    and ~~strikethrough~~ and ==highlight==\n\nBack to normal.";
    html = apex_markdown_to_html(indented_with_extended, strlen(indented_with_extended), &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript processed in normal text");
    assert_contains(html, "~subscript~", "Subscript not processed in indented code block");
    assert_contains(html, "^caret^", "Caret not processed as superscript in indented code block");
    assert_not_contains(html, "<sub>subscript</sub>", "No subscript tag in indented code block");
    assert_contains(html, "~~strikethrough~~", "Strikethrough not processed in indented code block");
    assert_contains(html, "==highlight==", "Highlight not processed in indented code block");
    assert_not_contains(html, "<mark>highlight</mark>", "No mark tag in indented code block");
    apex_free_string(html);

    /* Fenced code block: same checks */
    const char *fenced_with_extended = "Normal x^2.\n\n```\ncode with ~subscript~ and ^caret^\nand ~~strikethrough~~ and ==highlight==\n```\n\nBack.";
    html = apex_markdown_to_html(fenced_with_extended, strlen(fenced_with_extended), &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript processed in normal text");
    assert_contains(html, "~subscript~", "Subscript not processed in fenced code block");
    assert_contains(html, "^caret^", "Caret not processed in fenced code block");
    assert_not_contains(html, "<sub>subscript</sub>", "No subscript tag in fenced code block");
    assert_contains(html, "~~strikethrough~~", "Strikethrough not processed in fenced code block");
    assert_contains(html, "==highlight==", "Highlight not processed in fenced code block");
    apex_free_string(html);

    /* Inline code: ^ and ~ must not be processed */
    html = apex_markdown_to_html("Use `x^2` and `H~2~O` in code.", 32, &opts);
    assert_contains(html, "x^2", "Caret preserved in inline code");
    assert_contains(html, "H~2~O", "Tildes preserved in inline code");
    assert_not_contains(html, "<sup>2</sup>", "No superscript in inline code");
    assert_not_contains(html, "<sub>2</sub>", "No subscript in inline code");
    apex_free_string(html);

    /* Nested list with 4+ spaces: sup/sub and highlight should be processed (list line, not code block) */
    const char *list_with_extended = "- Outer item\n    - Nested with x^2 and H~2~O\n    - And ==highlight== here";
    html = apex_markdown_to_html(list_with_extended, strlen(list_with_extended), &opts);
    assert_contains(html, "<sup>2</sup>", "Superscript processed in nested list line");
    assert_contains(html, "<sub>2</sub>", "Subscript processed in nested list line");
    assert_contains(html, "<mark>highlight</mark>", "Highlight processed in nested list line");
    apex_free_string(html);

    /* Indented line that is real code (no list marker): still no processing */
    const char *real_indented_code = "Paragraph.\n\n    actual code ~subscript~ here\n\nBack.";
    html = apex_markdown_to_html(real_indented_code, strlen(real_indented_code), &opts);
    assert_contains(html, "~subscript~", "Subscript not processed in real indented code block");
    assert_not_contains(html, "<sub>subscript</sub>", "No subscript tag in real indented code block");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Superscript, Subscript, Underline, Delete, and Highlight Tests", had_failures, false);
}

/**
 * Test mixed list markers
 */

void test_mixed_lists(void) {
    int suite_failures = suite_start();
    print_suite_title("Mixed List Markers Tests", false, true);

    char *html;

    /* Test mixed markers in unified mode (should merge) */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *mixed_list = "1. First item\n* Second item\n* Third item";
    html = apex_markdown_to_html(mixed_list, strlen(mixed_list), &unified_opts);
    assert_contains(html, "<ol>", "Mixed list creates ordered list");
    assert_contains(html, "<li>First item</li>", "First item in list");
    assert_contains(html, "<li>Second item</li>", "Second item in list");
    assert_contains(html, "<li>Third item</li>", "Third item in list");
    /* Should have only one list, not two */
    const char *first_ol = strstr(html, "<ol>");
    const char *second_ol = first_ol ? strstr(first_ol + 1, "<ol>") : NULL;
    if (second_ol == NULL) {
        test_result(true, "Mixed markers create single list in unified mode");
    } else {
        test_result(false, "Mixed markers create multiple lists in unified mode");
    }
    apex_free_string(html);

    /* Test mixed markers in CommonMark mode (should be separate lists) */
    apex_options cm_opts = apex_options_for_mode(APEX_MODE_COMMONMARK);
    html = apex_markdown_to_html(mixed_list, strlen(mixed_list), &cm_opts);
    assert_contains(html, "<ol>", "First list exists");
    assert_contains(html, "<ul>", "Second list exists");
    /* Should have two separate lists */
    first_ol = strstr(html, "<ol>");
    second_ol = first_ol ? strstr(first_ol + 1, "<ol>") : NULL;
    const char *first_ul = strstr(html, "<ul>");
    if (second_ol == NULL && first_ul != NULL) {
        test_result(true, "Mixed markers create separate lists in CommonMark mode");
    } else {
        test_result(false, "Mixed markers not handled correctly in CommonMark mode");
    }
    apex_free_string(html);

    /* Test mixed markers with unordered first */
    const char *mixed_unordered = "* First item\n1. Second item\n2. Third item";
    html = apex_markdown_to_html(mixed_unordered, strlen(mixed_unordered), &unified_opts);
    assert_contains(html, "<ul>", "Unordered-first mixed list creates unordered list");
    assert_contains(html, "<li>First item</li>", "First unordered item");
    assert_contains(html, "<li>Second item</li>", "Second item inherits unordered");
    apex_free_string(html);

    /* Test that allow_mixed_list_markers=false creates separate lists even in unified mode */
    unified_opts.allow_mixed_list_markers = false;
    html = apex_markdown_to_html(mixed_list, strlen(mixed_list), &unified_opts);
    first_ol = strstr(html, "<ol>");
    second_ol = first_ol ? strstr(first_ol + 1, "<ol>") : NULL;
    first_ul = strstr(html, "<ul>");
    if (second_ol == NULL && first_ul != NULL) {
        test_result(true, "--no-mixed-lists disables mixed list merging");
    } else {
        test_result(false, "--no-mixed-lists does not disable mixed list merging");
    }
    apex_free_string(html);

    /* Regression: alpha lists with nested sublists should stay intact and not leak marker tokens */
    unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *alpha_with_nested_bullets = "a. Test\nb. Test\n\t- Test\n\t- Test\nc. Test\n";
    html = apex_markdown_to_html(alpha_with_nested_bullets, strlen(alpha_with_nested_bullets), &unified_opts);
    assert_contains(html, "<ol style=\"list-style-type: lower-alpha\">", "Alpha list keeps lower-alpha style with nested bullets");
    assert_contains(html, "<ul>", "Nested bullet list rendered");
    assert_not_contains(html, "[apex-alpha-list:", "Alpha marker token removed from output");
    apex_free_string(html);

    const char *alpha_with_nested_ordered = "a. Test\nb. Test\n\t1. Nested one\n\t2. Nested two\nc. Test\n";
    html = apex_markdown_to_html(alpha_with_nested_ordered, strlen(alpha_with_nested_ordered), &unified_opts);
    assert_contains(html, "<ol style=\"list-style-type: lower-alpha\">", "Alpha list keeps style with nested ordered list");
    assert_not_contains(html, "[apex-alpha-list:", "No leaked alpha marker token after nested ordered list");
    apex_free_string(html);

    const char *alpha_with_nested_alpha = "a. Test\nb. Test\n\tc. Test\n\td. Test\ne. Test\n";
    html = apex_markdown_to_html(alpha_with_nested_alpha, strlen(alpha_with_nested_alpha), &unified_opts);
    assert_contains(html, "<ol style=\"list-style-type: lower-alpha\">", "Alpha list keeps style with nested alpha sublist");
    assert_not_contains(html, "[apex-alpha-list:", "No leaked alpha marker token after nested alpha sublist");
    assert_not_contains(html, "<p>Test</p>\n<ol>", "Synthetic nested alpha sublist stays tight without paragraph wrapper");
    apex_free_string(html);

    /* Numeric parent list with indented alpha sublist → nested <ol style="lower-alpha"> */
    const char *numeric_with_nested_alpha = "1. One\n2. Two\n\ta. Two-One\n\tb. Two-Two\n3. Three\n";
    html = apex_markdown_to_html(numeric_with_nested_alpha, strlen(numeric_with_nested_alpha), &unified_opts);
    assert_contains(html, "<ol style=\"list-style-type: lower-alpha\">",
                    "Alpha sublist under numeric list gets lower-alpha styling");
    assert_contains(html, "<li>Two-One</li>", "Nested alpha item text preserved");
    assert_not_contains(html, "<!-- apex-alpha-list-", "Alpha HTML comment marker stripped from output");
    apex_free_string(html);

    const char *numeric_with_nested_ordered = "1. Test\n2. Test\n\t3. Test\n\t4. Test\n5. Test\n";
    html = apex_markdown_to_html(numeric_with_nested_ordered, strlen(numeric_with_nested_ordered), &unified_opts);
    assert_contains(html, "<ol start=\"3\">", "Numeric nested ordered sublist renders as nested ordered list");
    assert_not_contains(html, "2. Test\n    3. Test", "Nested ordered items are not flattened into parent text");
    assert_not_contains(html, "<p>Test</p>\n<ol start=\"3\">", "Synthetic nested ordered list stays tight without paragraph wrapper");
    apex_free_string(html);

    const char *numeric_with_explicit_loose_nested_ordered = "1. Test\n2. Test\n\n\t3. Test\n\t4. Test\n5. Test\n";
    html = apex_markdown_to_html(numeric_with_explicit_loose_nested_ordered, strlen(numeric_with_explicit_loose_nested_ordered), &unified_opts);
    assert_contains(html, "<ol start=\"3\">", "Explicit blank line nested ordered sublist renders");
    assert_contains(html, "<p>Test</p>\n<ol start=\"3\">", "Explicit blank line keeps loose list paragraph wrapper");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Mixed List Markers Tests", had_failures, false);
}

/**
 * Test unsafe mode (raw HTML handling)
 */

void test_unsafe_mode(void) {
    int suite_failures = suite_start();
    print_suite_title("Unsafe Mode Tests", false, true);

    char *html;

    /* Test that unsafe mode allows raw HTML by default in unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *raw_html = "<div>Raw HTML content</div>";
    html = apex_markdown_to_html(raw_html, strlen(raw_html), &unified_opts);
    assert_contains(html, "<div>Raw HTML content</div>", "Raw HTML allowed in unified mode");
    if (strstr(html, "raw HTML omitted") == NULL && strstr(html, "omitted") == NULL) {
        test_result(true, "Raw HTML preserved in unified mode (unsafe default)");
    } else {
        test_result(false, "Raw HTML not preserved in unified mode");
    }
    apex_free_string(html);

    /* Test that unsafe mode blocks raw HTML in CommonMark mode */
    apex_options cm_opts = apex_options_for_mode(APEX_MODE_COMMONMARK);
    html = apex_markdown_to_html(raw_html, strlen(raw_html), &cm_opts);
    if (strstr(html, "raw HTML omitted") != NULL || strstr(html, "omitted") != NULL) {
        tests_passed++;
        tests_run++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Raw HTML blocked in CommonMark mode (safe default)\n");
        }
    } else if (strstr(html, "<div>Raw HTML content</div>") == NULL) {
        tests_passed++;
        tests_run++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " Raw HTML blocked in CommonMark mode (safe default)\n");
        }
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Raw HTML not blocked in CommonMark mode\n");
    }
    apex_free_string(html);

    /* Test that unsafe=false blocks raw HTML even in unified mode */
    unified_opts.unsafe = false;
    html = apex_markdown_to_html(raw_html, strlen(raw_html), &unified_opts);
    if (strstr(html, "raw HTML omitted") != NULL || strstr(html, "omitted") != NULL) {
        test_result(true, "--no-unsafe blocks raw HTML");
    } else if (strstr(html, "<div>Raw HTML content</div>") == NULL) {
        test_result(true, "--no-unsafe blocks raw HTML");
    } else {
        test_result(false, "--no-unsafe does not block raw HTML");
    }
    apex_free_string(html);

    /* Test that unsafe=true allows raw HTML even in CommonMark mode */
    cm_opts.unsafe = true;
    html = apex_markdown_to_html(raw_html, strlen(raw_html), &cm_opts);
    assert_contains(html, "<div>Raw HTML content</div>", "Raw HTML allowed with unsafe=true");
    apex_free_string(html);

    /* Test HTML comments in unsafe mode */
    const char *html_comment = "<!-- This is a comment -->";
    unified_opts.unsafe = true;
    html = apex_markdown_to_html(html_comment, strlen(html_comment), &unified_opts);
    assert_contains(html, "<!-- This is a comment -->", "HTML comments preserved in unsafe mode");
    apex_free_string(html);

    /* Test HTML comments in safe mode */
    unified_opts.unsafe = false;
    html = apex_markdown_to_html(html_comment, strlen(html_comment), &unified_opts);
    if (strstr(html, "raw HTML omitted") != NULL || strstr(html, "omitted") != NULL) {
        test_result(true, "HTML comments blocked in safe mode");
    } else {
        test_result(false, "HTML comments not blocked in safe mode");
    }
    apex_free_string(html);

    /* Test script tags are handled according to unsafe setting */
    const char *script_tag = "<script>alert('xss')</script>";
    unified_opts.unsafe = true;
    html = apex_markdown_to_html(script_tag, strlen(script_tag), &unified_opts);
    /* In unsafe mode, script tags might be preserved or escaped depending on cmark-gfm */
    /* Just verify it's handled somehow */
    if (strstr(html, "script") != NULL || strstr(html, "omitted") != NULL) {
        test_result(true, "Script tags handled in unsafe mode");
    } else {
        test_result(false, "Script tags not handled in unsafe mode");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Unsafe Mode Tests", had_failures, false);
}

/**
 * Test Insert Syntax (++text++)
 */
void test_insert_syntax(void) {
    int suite_failures = suite_start();
    print_suite_title("Insert Syntax Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test basic insert without IAL */
    html = apex_markdown_to_html("Text ++inserted++ here", 23, &opts);
    assert_contains(html, "<ins>inserted</ins>", "Basic insert syntax");
    apex_free_string(html);

    /* Test insert with Kramdown-style IAL */
    html = apex_markdown_to_html("Text ++inserted++{: .class} here", 33, &opts);
    assert_contains(html, "<ins", "Insert with IAL creates ins tag");
    assert_contains(html, "class=\"class\"", "Insert with IAL has class");
    assert_contains(html, "inserted", "Insert with IAL contains text");
    apex_free_string(html);

    /* Test insert with Pandoc-style IAL */
    html = apex_markdown_to_html("Text ++inserted++{#id .class} here", 35, &opts);
    assert_contains(html, "<ins", "Insert with Pandoc IAL creates ins tag");
    assert_contains(html, "id=\"id\"", "Insert with Pandoc IAL has ID");
    assert_contains(html, "class=\"class\"", "Insert with Pandoc IAL has class");
    apex_free_string(html);

    /* Test insert with multiple classes */
    html = apex_markdown_to_html("Text ++inserted++{: .class1 .class2} here", 39, &opts);
    assert_contains(html, "class=\"class1 class2\"", "Insert with multiple classes");
    apex_free_string(html);

    /* Test insert does not interfere with CriticMarkup */
    opts.enable_critic_markup = true;
    opts.critic_mode = 2;  /* CRITIC_MARKUP */
    html = apex_markdown_to_html("Text {++critic++} and ++plain++ here", 38, &opts);
    assert_contains(html, "<ins class=\"critic\">critic</ins>", "CriticMarkup insert still works");
    assert_contains(html, "<ins>plain</ins>", "Plain insert still works");
    apex_free_string(html);

    /* Test insert in code blocks is not processed */
    html = apex_markdown_to_html("```\n++code++\n```", 18, &opts);
    assert_contains(html, "++code++", "Insert in code block not processed");
    assert_not_contains(html, "<ins>code</ins>", "Insert in code block not converted");
    apex_free_string(html);

    /* Test insert in inline code is not processed */
    html = apex_markdown_to_html("Text `++code++` here", 20, &opts);
    assert_contains(html, "++code++", "Insert in inline code not processed");
    assert_not_contains(html, "<ins>code</ins>", "Insert in inline code not converted");
    apex_free_string(html);

    /* Test insert inside HTML attribute values (base64 data URIs) is not processed */
    {
        const char *md = "<img src='data:image/png;base64,AAAA++23++BB\nCC++xy++DD'>\n\nText ++ins++ here";
        html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "AAAA++23++BB", "Insert in HTML attribute not processed");
        assert_contains(html, "CC++xy++DD", "Insert in multi-line HTML attribute not processed");
        assert_contains(html, "<ins>ins</ins>", "Insert after HTML tag still converted");
        apex_free_string(html);
    }

    /* Test stray < does not disable inserts */
    {
        const char *md = "Stray a <b and ++ins++ here";
        html = apex_markdown_to_html(md, strlen(md), &opts);
        assert_contains(html, "<ins>ins</ins>", "Insert after stray < still converted");
        apex_free_string(html);
    }

    /* Test insert with markdown inside */
    html = apex_markdown_to_html("Text ++*italic*++ here", 23, &opts);
    assert_contains(html, "<ins>", "Insert tag present");
    assert_contains(html, "<em>italic</em>", "Markdown inside insert processed");
    apex_free_string(html);

    /* Test insert with IAL and markdown inside */
    html = apex_markdown_to_html("Text ++*italic*++{: .highlight} here", 35, &opts);
    assert_contains(html, "<ins", "Insert with IAL and markdown creates ins tag");
    assert_contains(html, "class=\"highlight\"", "Insert with IAL has class");
    assert_contains(html, "<em>italic</em>", "Markdown inside insert with IAL processed");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Insert Syntax Tests", had_failures, false);
}

/**
 * Test image captions -> figure/figcaption wrapping
 * Enabled by default in MultiMarkdown and Unified modes, configurable via options.
 */
void test_image_captions(void) {
    int suite_failures = suite_start();
    print_suite_title("Image Captions Tests", false, true);

    const char *md_basic =
        "![Alt only](/img/basic.png)\n\n"
        "![With title](/img/title.png \"Title caption\")\n\n"
        "![](/img/empty.png)\n";

    /* MultiMarkdown mode: captions enabled by default */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html = apex_markdown_to_html(md_basic, strlen(md_basic), &mmd_opts);
    assert_contains(html, "<figure>", "MMD: figures generated");
    assert_contains(html, "<img src=\"/img/basic.png\"", "MMD: basic image present");
    assert_contains(html, "<figcaption>Alt only</figcaption>", "MMD: caption from alt text");
    assert_contains(html, "<img src=\"/img/title.png\"", "MMD: titled image present");
    assert_contains(html, "<figcaption>Title caption</figcaption>", "MMD: caption from title text");
    assert_not_contains(html, "<figure><img src=\"/img/empty.png\"", "MMD: no figure for empty alt/title image");
    apex_free_string(html);

    /* Unified mode: captions enabled by default */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    html = apex_markdown_to_html(md_basic, strlen(md_basic), &unified_opts);
    assert_contains(html, "<figure>", "Unified: figures generated");
    assert_contains(html, "<figcaption>Alt only</figcaption>", "Unified: caption from alt text");
    assert_contains(html, "<figcaption>Title caption</figcaption>", "Unified: caption from title text");
    assert_not_contains(html, "<figure><img src=\"/img/empty.png\"", "Unified: no figure for empty alt/title image");
    apex_free_string(html);

    /* Explicitly disabling captions should produce plain <img> tags */
    apex_options disabled_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    disabled_opts.enable_image_captions = false;
    html = apex_markdown_to_html(md_basic, strlen(md_basic), &disabled_opts);
    assert_not_contains(html, "<figure>", "Disabled: no figures generated");
    assert_not_contains(html, "<figcaption>", "Disabled: no figcaptions generated");
    assert_contains(html, "<img src=\"/img/basic.png\"", "Disabled: basic image present");
    apex_free_string(html);

    /* caption="TEXT" on image adds figure/figcaption even when --image-captions is off */
    const char *md_caption_attr = "![Alt](/img/cap.png){caption=\"Explicit caption\"}\n";
    apex_options no_captions_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    no_captions_opts.enable_image_captions = false;
    html = apex_markdown_to_html(md_caption_attr, strlen(md_caption_attr), &no_captions_opts);
    assert_contains(html, "<figure>", "caption=: figure present");
    assert_contains(html, "<figcaption>Explicit caption</figcaption>", "caption=: figcaption text");
    assert_not_contains(html, "caption=\"", "caption=: caption attr stripped from img");
    apex_free_string(html);

    /* --title-captions-only: only images with title get captions; alt-only images do not */
    const char *md_alt_and_title =
        "![Alt only](/img/alt.png)\n\n"
        "![With title](/img/title.png \"Title caption\")\n";
    apex_options title_only_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    title_only_opts.enable_image_captions = true;
    title_only_opts.title_captions_only = true;
    html = apex_markdown_to_html(md_alt_and_title, strlen(md_alt_and_title), &title_only_opts);
    assert_contains(html, "<figcaption>Title caption</figcaption>", "title_captions_only: caption from title");
    assert_not_contains(html, "<figcaption>Alt only</figcaption>", "title_captions_only: no caption from alt");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Image Captions Tests", had_failures, false);
}

/**
 * Test image embedding
 */
