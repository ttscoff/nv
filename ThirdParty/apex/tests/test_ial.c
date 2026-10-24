/**
 * Ial Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include <string.h>

void test_ial(void) {
    int suite_failures = suite_start();
    print_suite_title("IAL Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    char *html;

    /* Test block IAL with ID */
    html = apex_markdown_to_html("# Header\n{: #custom-id}", 24, &opts);
    assert_contains(html, "id=\"custom-id\"", "Block IAL ID");
    apex_free_string(html);

    /* Test block IAL with class (requires blank line in standard Kramdown) */
    html = apex_markdown_to_html("Paragraph\n\n{: .important}", 25, &opts);
    assert_contains(html, "class=\"important\"", "Block IAL class");
    apex_free_string(html);

    /* Test block IAL with multiple classes */
    html = apex_markdown_to_html("Text\n\n{: .class1 .class2}", 26, &opts);
    assert_contains(html, "class=\"class1 class2\"", "Block IAL multiple classes");
    apex_free_string(html);

    /* Test block IAL with ID and class */
    html = apex_markdown_to_html("## Header 2\n{: #myid .myclass}", 31, &opts);
    assert_contains(html, "id=\"myid\"", "Block IAL ID with class");
    assert_contains(html, "class=\"myclass\"", "Block IAL class with ID");
    apex_free_string(html);

    /* Test block IAL with custom attributes - skip for now (complex quoting) */
    // html = apex_markdown_to_html("Para\n{: data-value=\"test\"}", 27, &opts);
    // assert_contains(html, "data-value=\"test\"", "Block IAL custom attribute");
    // apex_free_string(html);

    /* Test ALD (Attribute List Definition) - needs debugging */
    // const char *ald_doc = "{:ref: #special .highlight}\n\nParagraph 1\n{:ref}\n\nParagraph 2\n{:ref}";
    // html = apex_markdown_to_html(ald_doc, strlen(ald_doc), &opts);
    // assert_contains(html, "id=\"special\"", "ALD reference applied");
    // assert_contains(html, "class=\"highlight\"", "ALD class applied");
    // apex_free_string(html);

    /* Test list item IAL - needs debugging */
    // html = apex_markdown_to_html("- Item 1\n{: .special}\n- Item 2", 31, &opts);
    // assert_contains(html, "class=\"special\"", "List item IAL");
    // apex_free_string(html);

    /* Test inline IAL on links */
    const char *link_ial = "Here's a [link](https://example.com){:.button} with text after.";
    html = apex_markdown_to_html(link_ial, strlen(link_ial), &opts);
    assert_contains(html, "class=\"button\"", "Inline IAL on link");
    assert_contains(html, "with text after", "Text after IAL preserved");
    assert_not_contains(html, "{:.button}", "IAL removed from output");
    apex_free_string(html);

    /* Test inline IAL on links with duplicate URLs */
    const char *dup_urls = "First [link](https://example.com) and [second](https://example.com){:.special} link.";
    html = apex_markdown_to_html(dup_urls, strlen(dup_urls), &opts);
    assert_contains(html, "<a href=\"https://example.com\">link</a>", "First link without class");
    assert_contains(html, "class=\"special\"", "Second link with class");
    assert_not_contains(html, "{:.special}", "IAL removed from output");
    apex_free_string(html);

    /* Test Pandoc-style IAL (no colon) for block-level elements */
    html = apex_markdown_to_html("# Header\n{#pandoc-id .pandoc-class}", 35, &opts);
    assert_contains(html, "id=\"pandoc-id\"", "Pandoc-style block IAL ID");
    assert_contains(html, "class=\"pandoc-class\"", "Pandoc-style block IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL for paragraphs */
    html = apex_markdown_to_html("Paragraph text\n\n{#para-id .para-class}", 40, &opts);
    assert_contains(html, "id=\"para-id\"", "Pandoc-style paragraph IAL ID");
    assert_contains(html, "class=\"para-class\"", "Pandoc-style paragraph IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL for inline elements */
    const char *pandoc_inline = "Here's a [link](url){#link-id .link-class} with Pandoc IAL.";
    html = apex_markdown_to_html(pandoc_inline, strlen(pandoc_inline), &opts);
    assert_contains(html, "id=\"link-id\"", "Pandoc-style inline IAL ID");
    assert_contains(html, "class=\"link-class\"", "Pandoc-style inline IAL class");
    apex_free_string(html);

    /* Test Pandoc-style IAL with multiple classes */
    html = apex_markdown_to_html("## Heading\n{#heading-id .class1 .class2}", 40, &opts);
    assert_contains(html, "id=\"heading-id\"", "Pandoc-style IAL with multiple classes - ID");
    assert_contains(html, "class=\"class1 class2\"", "Pandoc-style IAL with multiple classes");
    apex_free_string(html);

    /* Test inline IAL on strong/emph */
    html = apex_markdown_to_html("This is **bold**{:.bold-style} text.", 40, &opts);
    assert_contains(html, "class=\"bold-style\"", "Inline IAL on strong");
    assert_contains(html, "<strong", "Strong tag present");
    assert_not_contains(html, "{:.bold-style}", "IAL removed from output");
    apex_free_string(html);

    html = apex_markdown_to_html("This is *italic*{:.italic-style} text.", 42, &opts);
    assert_contains(html, "class=\"italic-style\"", "Inline IAL on emphasis");
    assert_contains(html, "<em", "Em tag present");
    assert_not_contains(html, "{:.italic-style}", "IAL removed from output");
    apex_free_string(html);

    /* Test inline IAL on code */
    html = apex_markdown_to_html("Use `code`{:.code-inline} here.", 34, &opts);
    assert_contains(html, "class=\"code-inline\"", "Inline IAL on code");
    assert_contains(html, "<code", "Code tag present");
    assert_not_contains(html, "{:.code-inline}", "IAL removed from output");
    apex_free_string(html);

    /* Test multiple inline IALs in same paragraph */
    const char *multi_ial = "[link](url){:.link} and **bold**{:.bold} and *em*{:.em}.";
    html = apex_markdown_to_html(multi_ial, strlen(multi_ial), &opts);
    assert_contains(html, "class=\"link\"", "First IAL applied");
    assert_contains(html, "class=\"bold\"", "Second IAL applied");
    assert_contains(html, "class=\"em\"", "Third IAL applied");
    apex_free_string(html);

    /* Test inline IAL with multiple classes */
    const char *multi_class = "[link](url){:.primary .large .button} text.";
    html = apex_markdown_to_html(multi_class, strlen(multi_class), &opts);
    assert_contains(html, "class=\"primary large button\"", "Multiple classes in inline IAL");
    apex_free_string(html);

    /* Test inline IAL with ID and classes */
    const char *id_class = "**bold**{:#bold-id .highlight .important}";
    html = apex_markdown_to_html(id_class, strlen(id_class), &opts);
    assert_contains(html, "id=\"bold-id\"", "Inline IAL with ID");
    assert_contains(html, "class=\"highlight important\"", "Inline IAL ID with classes");
    apex_free_string(html);

    /* Test inline IAL at end of paragraph (should still work) */
    const char *end_ial = "End with [link](url){:.end-link}.";
    html = apex_markdown_to_html(end_ial, strlen(end_ial), &opts);
    assert_contains(html, "class=\"end-link\"", "Inline IAL at paragraph end");
    apex_free_string(html);

    /* Test inline IAL with whitespace (spaces around IAL) */
    const char *spaced_ial = "[link](url){: .spaced-class } text.";
    html = apex_markdown_to_html(spaced_ial, strlen(spaced_ial), &opts);
    assert_contains(html, "class=\"spaced-class\"", "Inline IAL with spaces");
    apex_free_string(html);

    /* Test block IAL on paragraph when IAL is on same line (no blank line) - span IAL path */
    const char *same_line_ial = "Lead paragraph text.\n{: .lead }";
    html = apex_markdown_to_html(same_line_ial, strlen(same_line_ial), &opts);
    assert_contains(html, "class=\"lead\"", "Paragraph IAL same line (no blank)");
    assert_not_contains(html, "{: .lead }", "IAL removed from output");
    apex_free_string(html);

    /* Test block IAL with raw div (simulates fenced div output: two paragraphs, second is IAL) */
    apex_options opts_unified = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *raw_div_ial = "<div class=\"section\">\n\nOpening paragraph here.\n\n{: .lead }\n\n</div>";
    html = apex_markdown_to_html(raw_div_ial, strlen(raw_div_ial), &opts_unified);
    assert_contains(html, "class=\"lead\"", "Block IAL with raw div: .lead on paragraph");
    assert_not_contains(html, "<p>{: .lead }</p>", "Block IAL with raw div: IAL paragraph removed");
    apex_free_string(html);

    /* Test block IAL inside fenced div (Unified mode: divs + IAL preprocess) */
    const char *fenced_ial = "::: section\nOpening paragraph here.\n{: .lead }\n:::";
    html = apex_markdown_to_html(fenced_ial, strlen(fenced_ial), &opts_unified);
    assert_contains(html, "class=\"section\"", "Fenced div IAL: section div present");
    assert_contains(html, "class=\"lead\"", "Fenced div IAL: .lead on paragraph");
    assert_not_contains(html, "<p>{: .lead }</p>", "Fenced div IAL: IAL paragraph removed");
    apex_free_string(html);

    /* An IAL on the line after a block attaches to that block. */
    const char *list_ial = "- one\n- two\n{: .items}\n";
    html = apex_markdown_to_html(list_ial, strlen(list_ial), &opts_unified);
    assert_contains(html, "<ul class=\"items\">", "IAL after a list attaches to the list");
    assert_not_contains(html, "{:", "IAL after a list is removed");
    apex_free_string(html);

    const char *hr_ial = "---\n{: .rule}\n";
    html = apex_markdown_to_html(hr_ial, strlen(hr_ial), &opts_unified);
    assert_contains(html, "<hr class=\"rule\"", "IAL after a thematic break attaches to the rule");
    assert_not_contains(html, "{:", "IAL after a thematic break is removed");
    apex_free_string(html);

    const char *deflist_ial = "Term\n: definition\n{: .gloss}\n";
    html = apex_markdown_to_html(deflist_ial, strlen(deflist_ial), &opts_unified);
    assert_contains(html, "<dl class=\"gloss\">", "IAL after a definition list attaches to the list");
    assert_not_contains(html, "{:", "IAL after a definition list is removed");
    apex_free_string(html);

    const char *div_ial = "::: section\nInside\n:::\n{: .wrap}\n";
    html = apex_markdown_to_html(div_ial, strlen(div_ial), &opts_unified);
    assert_contains(html, "class=\"section wrap\"", "IAL after a fenced div joins its class");
    assert_not_contains(html, "{:", "IAL after a fenced div is removed");
    apex_free_string(html);

    const char *html_ial = "<div>raw</div>\n{: .raw}\n";
    html = apex_markdown_to_html(html_ial, strlen(html_ial), &opts_unified);
    assert_contains(html, "<div class=\"raw\">", "IAL after raw HTML attaches to the element");
    assert_not_contains(html, "{:", "IAL after raw HTML is removed");
    apex_free_string(html);

    const char *two_raw = "<div>a</div>\n{: .a}\n\n<div>b</div>\n{: .b}\n";
    html = apex_markdown_to_html(two_raw, strlen(two_raw), &opts_unified);
    assert_contains(html, "<div class=\"a\">", "IAL on the first raw element");
    assert_contains(html, "<div class=\"b\">", "IAL on the second raw element");
    assert_not_contains(html, "{:", "Both raw-element IALs are removed");
    apex_free_string(html);

    const char *second_dl = "Term\n: definition\n\nOther\n: def\n{: .gloss}\n";
    html = apex_markdown_to_html(second_dl, strlen(second_dl), &opts_unified);
    assert_contains(html, "<dl class=\"gloss\">", "IAL after the second definition list");
    assert_contains(html, "<dl>\n<dt>Term</dt>", "First definition list stays plain");
    apex_free_string(html);

    const char *quote_ial = "> quoted\n> {: .note}\n";
    html = apex_markdown_to_html(quote_ial, strlen(quote_ial), &opts_unified);
    assert_contains(html, "<blockquote>\n<p class=\"note\">quoted</p>\n</blockquote>", "IAL on the next blockquote line attaches to the paragraph");
    assert_not_contains(html, "{:", "IAL inside a blockquote is removed");
    apex_free_string(html);

    const char *quote_last = "> one\n>\n> two\n> {: .note}\n";
    html = apex_markdown_to_html(quote_last, strlen(quote_last), &opts_unified);
    assert_contains(html, "<p>one</p>", "Earlier quoted paragraph stays plain");
    assert_contains(html, "<p class=\"note\">two</p>", "Inside IAL attaches to the paragraph it follows");
    assert_not_contains(html, "<blockquote class=", "Inside IAL leaves the blockquote unclassed");
    apex_free_string(html);

    const char *quote_out = "> quoted\n{: .note}\n";
    html = apex_markdown_to_html(quote_out, strlen(quote_out), &opts_unified);
    assert_contains(html, "<blockquote class=\"note\">\n<p>quoted</p>\n</blockquote>", "IAL after a blockquote attaches to the blockquote");
    assert_not_contains(html, "{:", "IAL after a blockquote is removed");
    apex_free_string(html);

    const char *quote_multiline = "> A stupid quote\n{: .stupid }\n\n> A stupid paragraph\n> inside a quote\n> {: .silly }\n{: .dumb }\n";
    html = apex_markdown_to_html(quote_multiline, strlen(quote_multiline), &opts_unified);
    assert_contains(html, "<blockquote class=\"stupid\">", "IAL after a quote attaches to that blockquote");
    assert_contains(html, "<p class=\"silly\">A stupid paragraph\ninside a quote</p>", "Inside IAL on a multiline quote attaches to the paragraph");
    assert_contains(html, "<blockquote class=\"dumb\">", "IAL after the second quote attaches to that blockquote");
    assert_not_contains(html, "{:", "Quote IALs are removed");
    apex_free_string(html);

    const char *item_ial = "- A blockquote line{: .vapid}\n{: .stupid}\n";
    html = apex_markdown_to_html(item_ial, strlen(item_ial), &opts_unified);
    assert_contains(html, "<ul class=\"stupid\">", "IAL after a list attaches to the list");
    assert_contains(html, "<li class=\"vapid\">A blockquote line</li>", "Same-line IAL attaches to the list item");
    assert_not_contains(html, "{:", "List item IAL is removed");
    apex_free_string(html);

    const char *item_later = "- first\n\n  second{: .note}\n";
    html = apex_markdown_to_html(item_later, strlen(item_later), &opts_unified);
    assert_contains(html, "<p class=\"note\">second</p>", "IAL on a later item paragraph stays on that paragraph");
    assert_not_contains(html, "<li class=\"note\">", "Later item paragraph IAL leaves the item unclassed");
    apex_free_string(html);

    const char *quote_same = "> quoted {: .note}\n";
    html = apex_markdown_to_html(quote_same, strlen(quote_same), &opts_unified);
    assert_contains(html, "<p class=\"note\">quoted</p>", "Same-line IAL inside a blockquote attaches to the paragraph");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("IAL Tests", had_failures, false);
}

/**
 * Test bracketed spans [text]{IAL}
 */

void test_bracketed_spans(void) {
    int suite_failures = suite_start();
    print_suite_title("Bracketed Spans Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_spans = true;
    char *html;

    /* Test basic bracketed span with class */
    const char *basic_span = "This is [some text]{.class} with a span.";
    html = apex_markdown_to_html(basic_span, strlen(basic_span), &opts);
    assert_contains(html, "<span", "Bracketed span creates span tag");
    assert_contains(html, "class=\"class\"", "Bracketed span has class");
    assert_contains(html, "some text", "Bracketed span contains text");
    assert_not_contains(html, "[some text]{.class}", "Bracketed span syntax removed");
    apex_free_string(html);

    /* Test bracketed span with ID */
    const char *span_with_id = "This is [text]{#my-id} with an ID.";
    html = apex_markdown_to_html(span_with_id, strlen(span_with_id), &opts);
    assert_contains(html, "id=\"my-id\"", "Bracketed span has ID");
    apex_free_string(html);

    /* Test bracketed span with multiple attributes */
    const char *span_multi = "This is [text]{#id .class1 .class2 key=\"value\"} with multiple attributes.";
    html = apex_markdown_to_html(span_multi, strlen(span_multi), &opts);
    assert_contains(html, "id=\"id\"", "Bracketed span has ID");
    assert_contains(html, "class=\"class1 class2\"", "Bracketed span has multiple classes");
    assert_contains(html, "key=\"value\"", "Bracketed span has custom attribute");
    apex_free_string(html);

    /* Test bracketed span with markdown inside */
    const char *span_markdown = "This is [some *text*]{.highlight} with markdown.";
    html = apex_markdown_to_html(span_markdown, strlen(span_markdown), &opts);
    assert_contains(html, "<em>text</em>", "Bracketed span processes markdown inside");
    assert_contains(html, "class=\"highlight\"", "Bracketed span with markdown has class");
    apex_free_string(html);

    /* Test reference link takes precedence over span */
    const char *ref_link = "This is [a link] that should be a link.\n\n[a link]: https://example.com";
    html = apex_markdown_to_html(ref_link, strlen(ref_link), &opts);
    assert_contains(html, "<a href", "Reference link creates link tag");
    assert_contains(html, "https://example.com", "Reference link has correct URL");
    assert_not_contains(html, "<span", "Reference link does not create span");
    apex_free_string(html);

    /* Test reference link with different ID */
    const char *ref_link2 = "This is [another link][ref] that should be a link.\n\n[ref]: https://example.org";
    html = apex_markdown_to_html(ref_link2, strlen(ref_link2), &opts);
    assert_contains(html, "<a href", "Reference link with ID creates link tag");
    assert_contains(html, "https://example.org", "Reference link with ID has correct URL");
    assert_not_contains(html, "<span", "Reference link with ID does not create span");
    apex_free_string(html);

    /* Test mixed reference link and span */
    const char *mixed = "This has [a link][ref] and [a span]{.span-class}.\n\n[ref]: https://example.com";
    html = apex_markdown_to_html(mixed, strlen(mixed), &opts);
    assert_contains(html, "<a href", "Mixed content has link");
    assert_contains(html, "<span", "Mixed content has span");
    assert_contains(html, "class=\"span-class\"", "Mixed content span has class");
    apex_free_string(html);

    /* Test bracketed span with bold markdown */
    const char *span_bold = "This is [**bold text**]{.bold} in a span.";
    html = apex_markdown_to_html(span_bold, strlen(span_bold), &opts);
    assert_contains(html, "<strong>bold text</strong>", "Bracketed span processes bold markdown");
    apex_free_string(html);

    /* Test bracketed span with code markdown */
    const char *span_code = "This is [`code`]{.code-style} in a span.";
    html = apex_markdown_to_html(span_code, strlen(span_code), &opts);
    assert_contains(html, "<code>code</code>", "Bracketed span processes code markdown");
    apex_free_string(html);

    /* Test bracketed span with nested brackets */
    const char *span_nested = "This is [Text with [nested brackets]]{.nested} that should work.";
    html = apex_markdown_to_html(span_nested, strlen(span_nested), &opts);
    assert_contains(html, "<span", "Bracketed span with nested brackets creates span");
    assert_contains(html, "class=\"nested\"", "Bracketed span with nested brackets has class");
    assert_contains(html, "Text with [nested brackets]", "Bracketed span with nested brackets contains full text");
    assert_not_contains(html, "[Text with [nested brackets]]{.nested}", "Bracketed span syntax removed");
    apex_free_string(html);

    /* Class-only span must not produce <spanclass=...> (space before class attribute) */
    const char *class_only = "[smallcaps text]{.smallcaps}";
    html = apex_markdown_to_html(class_only, strlen(class_only), &opts);
    assert_contains(html, "<span class=\"smallcaps\">", "Class-only bracketed span has valid class attribute");
    assert_not_contains(html, "<spanclass", "Class-only bracketed span does not merge tag and class");
    apex_free_string(html);

    /* Test that spans are disabled when flag is off */
    apex_options no_spans = apex_options_default();
    no_spans.enable_spans = false;
    const char *span_disabled = "This is [text]{.class} that should NOT be a span.";
    html = apex_markdown_to_html(span_disabled, strlen(span_disabled), &no_spans);
    /* When spans are disabled, [text]{.class} should remain as-is or be treated differently */
    /* For now, we'll just check that it doesn't create a span when disabled */
    if (strstr(html, "<span") == NULL) {
        test_result(true, "Bracketed spans disabled when flag is off");
    } else {
        test_result(false, "Bracketed spans incorrectly enabled when flag is off");
    }
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Bracketed Spans Tests", had_failures, false);
}

/**
 * Test definition lists
 */
