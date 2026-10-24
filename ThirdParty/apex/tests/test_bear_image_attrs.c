#include "test_helpers.h"
#include "apex/apex.h"
#include "extensions/bear_image_attrs.h"

#include <string.h>

static const char *bear_value(const apex_bear_image_attrs *attrs,
                              const char *key) {
    for (size_t i = 0; i < attrs->count; i++) {
        if (strcmp(attrs->items[i].key, key) == 0) {
            return attrs->items[i].value;
        }
    }
    return NULL;
}

static char *render_bear(
    const char *markdown, apex_mode_t mode, bool unsafe) {
    apex_options opts = apex_options_for_mode(mode);
    opts.unsafe = unsafe;
    return apex_markdown_to_html(markdown, strlen(markdown), &opts);
}

static void test_bear_inline_modes(void) {
    const char *md =
        "![](emperor-1.jpg)<!-- {\"width\":259} -->";
    const apex_mode_t modes[] = {
        APEX_MODE_COMMONMARK,
        APEX_MODE_GFM,
        APEX_MODE_MULTIMARKDOWN,
        APEX_MODE_UNIFIED
    };

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        char *html = render_bear(md, modes[i], true);
        assert_contains(html, "width=\"259\"", "Bear width is applied");
        assert_contains(
            html,
            "<!-- {\"width\":259} -->",
            "Bear comment is preserved");
        apex_free_string(html);
    }

    char *safe = render_bear(md, APEX_MODE_COMMONMARK, false);
    assert_contains(safe, "width=\"259\"", "Safe mode applies Bear width");
    assert_contains(
        safe, "raw HTML omitted", "Safe mode keeps raw HTML policy");
    apex_free_string(safe);

    char *spaced = render_bear(
        "![](x.jpg) \t<!-- {\"width\":\"50%\",\"height\":\"20px\"} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        spaced, "height=\"20\"", "Pixel height uses an HTML attribute");
    assert_contains(
        spaced, "style=\"width: 50%\"", "Percent width uses style");
    apex_free_string(spaced);

    char *escaped = render_bear(
        "![](x.jpg)<!-- {\"title\":\"x\\\" onerror=\\\"boom\"} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        escaped,
        "title=\"x&quot; onerror=&quot;boom\"",
        "Bear values are HTML attribute escaped");
    assert_not_contains(
        escaped, " onerror=\"boom\"", "Bear values cannot inject attributes");
    apex_free_string(escaped);

    char *kramdown = render_bear(
        "![](x.jpg)<!-- {\"width\":259} -->", APEX_MODE_KRAMDOWN, true);
    assert_not_contains(
        kramdown,
        "width=\"259\"",
        "Kramdown does not apply Bear metadata");
    apex_free_string(kramdown);
}

static void test_bear_reference_definitions(void) {
    const char *md =
        "![One][Emperor]\n\n"
        "![Emperor][]\n\n"
        "![Emperor]\n\n"
        "[ emperor ]: emperor.jpg "
        "<!-- {\"width\":259,\"title\":\"Ruler\"} -->";
    char *html = render_bear(md, APEX_MODE_UNIFIED, true);

    const char *cursor = html;
    int widths = 0;
    while ((cursor = strstr(cursor, "width=\"259\"")) != NULL) {
        widths++;
        cursor += strlen("width=\"259\"");
    }
    test_result(widths == 3, "Definition metadata applies to every use");
    assert_contains(
        html,
        "<!-- {\"width\":259,\"title\":\"Ruler\"} -->",
        "Definition comment is preserved once");

    cursor = html;
    int comments = 0;
    while ((cursor = strstr(cursor, "<!-- {\"width\":259")) != NULL) {
        comments++;
        cursor++;
    }
    test_result(comments == 1, "Definition comment renders exactly once");
    apex_free_string(html);

    html = render_bear(
        "![Crown][EMPEROR   OF ROME]\n\n"
        "[ emperor of  rome ]: emperor.jpg "
        "<!-- {\"height\":100} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        html,
        "height=\"100\"",
        "Reference matching normalizes case and internal whitespace");
    apex_free_string(html);

    html = render_bear(
        "![Use][ref]\n\n"
        "[ref]: pic.jpg junk <!-- {\"width\":259} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_not_contains(
        html,
        "width=\"259\"",
        "Non-adjacent definition comment does not apply metadata");
    apex_free_string(html);

    html = render_bear(
        "![Use][ref]\n\n"
        "[ref]: pic.jpg <!-- {\"width\":259} -->\n"
        "After paragraph.",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        html, "width=\"259\"", "Definition before text applies metadata");
    assert_contains(
        html,
        "<p>After paragraph.</p>",
        "Removed definition keeps its line ending");
    apex_free_string(html);

    html = render_bear(
        "![Use][ref]\n\n"
        "[ref]: pic.jpg \"MD Title\" "
        "<!-- {\"title\":\"Bear Title\",\"width\":120} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_contains(
        html,
        "title=\"Bear Title\"",
        "Bear title overrides the definition title");
    assert_not_contains(
        html,
        "title=\"MD Title\"",
        "Definition title does not shadow the Bear title");
    apex_free_string(html);
}

static void test_bear_reference_overrides(void) {
    const char *md =
        "![Default][shared]\n\n"
        "![Wide][shared]<!-- {\"width\":320,\"title\":\"Wide\"} -->\n\n"
        "![Default again][shared]\n\n"
        "[shared]: shared.jpg <!-- {\"width\":200,\"height\":100} -->";
    char *html = render_bear(md, APEX_MODE_UNIFIED, true);

    const char *cursor = html;
    int defaults = 0;
    while ((cursor = strstr(cursor, "width=\"200\"")) != NULL) {
        defaults++;
        cursor += strlen("width=\"200\"");
    }
    test_result(defaults == 2, "Sibling references retain definition width");
    test_result(
        strstr(html, "width=\"320\"") != NULL,
        "Per-use metadata overrides the definition width");
    assert_contains(
        html, "title=\"Wide\"", "Per-use title is applied");

    cursor = html;
    int inherited_heights = 0;
    while ((cursor = strstr(cursor, "height=\"100\"")) != NULL) {
        inherited_heights++;
        cursor += strlen("height=\"100\"");
    }
    test_result(
        inherited_heights == 3,
        "Per-use metadata inherits the definition height");

    assert_contains(
        html,
        "<!-- {\"width\":320,\"title\":\"Wide\"} -->",
        "Per-use comment is preserved");
    apex_free_string(html);

    char *unresolved = render_bear(
        "![Missing][none]<!-- {\"width\":999} -->",
        APEX_MODE_UNIFIED,
        true);
    assert_not_contains(
        unresolved, "<img", "Unresolved reference remains unresolved");
    assert_contains(
        unresolved,
        "<!-- {\"width\":999} -->",
        "Unresolved metadata comment is preserved");
    apex_free_string(unresolved);
}

void test_bear_image_attributes(void) {
    int failures = suite_start();
    print_suite_title("Bear Image Attribute Tests", false, true);

    const char *input = "<!-- {\"width\":259,\"title\":\"A & B\"} -->";
    const char *end = input + strlen(input);
    const char *comment_end = NULL;
    apex_bear_image_attrs attrs = {0};

    bool ok = apex_parse_bear_image_comment(
        input, end, &comment_end, &attrs);
    test_result(ok, "Bear parser accepts a flat JSON object");
    assert_option_string(
        bear_value(&attrs, "width"), "259", "Numeric width is normalized");
    assert_option_string(
        bear_value(&attrs, "title"), "A & B", "String value is decoded");
    test_result(comment_end == end, "Parser returns the comment end");
    apex_free_bear_image_attrs(&attrs);

    const char *unsafe =
        "<!-- {\"width\":259,\"onclick\":\"alert(1)\","
        "\"data-x\":\"bad\"} -->";
    end = unsafe + strlen(unsafe);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        unsafe, end, &comment_end, &attrs);
    test_result(ok, "Unsupported keys do not invalidate metadata");
    test_result(
        attrs.count == 1, "Only allowlisted attributes are returned");
    apex_free_bear_image_attrs(&attrs);

    const char *nested =
        "<!-- {\"width\":259,\"title\":{\"nested\":true}} -->";
    end = nested + strlen(nested);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        nested, end, &comment_end, &attrs);
    test_result(ok, "Unsupported nested values are skipped");
    test_result(attrs.count == 1, "Supported sibling value is retained");
    apex_free_bear_image_attrs(&attrs);

    const char *bad = "<!-- {\"width\":259,} -->";
    end = bad + strlen(bad);
    attrs = (apex_bear_image_attrs){0};
    test_result(
        !apex_parse_bear_image_comment(
            bad, end, &comment_end, &attrs),
        "Malformed JSON is rejected");

    /* Reusing a populated result for a failing parse must clean it. */
    const char *reused = "<!-- {\"width\":259,\"title\":\"A\"} -->";
    end = reused + strlen(reused);
    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        reused, end, &comment_end, &attrs);
    test_result(ok && attrs.count == 2, "Reused struct starts populated");

    const char *too_short = "<!-- x";
    end = too_short + strlen(too_short);
    ok = apex_parse_bear_image_comment(
        too_short, end, &comment_end, &attrs);
    test_result(!ok, "Too-short comment fails on a reused struct");
    test_result(
        attrs.count == 0, "Early failure leaves the reused struct empty");

    attrs = (apex_bear_image_attrs){0};
    ok = apex_parse_bear_image_comment(
        reused, end = reused + strlen(reused), &comment_end, &attrs);
    test_result(ok && attrs.count == 2, "Struct repopulated for reuse");

    const char *unterminated = "<!-- {\"width\":259}";
    end = unterminated + strlen(unterminated);
    ok = apex_parse_bear_image_comment(
        unterminated, end, &comment_end, &attrs);
    test_result(!ok, "Unterminated comment fails on a reused struct");
    test_result(
        attrs.count == 0,
        "Missing terminator leaves the reused struct empty");
    apex_free_bear_image_attrs(&attrs);

    test_bear_inline_modes();
    test_bear_reference_definitions();
    test_bear_reference_overrides();

    bool had_failures = suite_end(failures);
    print_suite_title("Bear Image Attribute Tests", had_failures, false);
}
