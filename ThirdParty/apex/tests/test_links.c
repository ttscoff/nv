/**
 * Links Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/wiki_links.h"
#include <string.h>

void test_wiki_links(void) {
    int suite_failures = suite_start();
    print_suite_title("Wiki Links Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_wiki_links = true;
    char *html;

    /* Test basic wiki link */
    html = apex_markdown_to_html("[[Page]]", 8, &opts);
    assert_contains(html, "<a href=\"Page\">Page</a>", "Basic wiki link");
    apex_free_string(html);

    /* Test wiki links are NOT processed when feature is disabled */
    {
        apex_options disabled = apex_options_default();
        disabled.enable_wiki_links = false;
        html = apex_markdown_to_html("[[Page]]", 8, &disabled);
        assert_contains(html, "[[Page]]", "Wiki link literal preserved when disabled");
        assert_not_contains(html, "<a href=", "No link generated when wiki links disabled");
        apex_free_string(html);
    }

    /* Test wiki link with display text */
    html = apex_markdown_to_html("[[Page|Display]]", 16, &opts);
    assert_contains(html, "<a href=\"Page\">Display</a>", "Wiki link with display");
    apex_free_string(html);

    /* Test wiki link with section */
    html = apex_markdown_to_html("[[Page#Section]]", 16, &opts);
    assert_contains(html, "#Section", "Wiki link with section");
    apex_free_string(html);

    /* Test wiki link with section AND display text: [[Page#Sec|Display]] */
    html = apex_markdown_to_html("[[Page#Sec|Display]]", 20, &opts);
    assert_contains(html, "<a href=\"Page#Sec\">Display</a>", "Wiki link with section and display text");
    apex_free_string(html);

    /* Test multiple wiki links in one text node (prefix + between + tail handling) */
    html = apex_markdown_to_html("A [[One]] and [[Two|2]] end", strlen("A [[One]] and [[Two|2]] end"), &opts);
    assert_contains(html, "A ", "Multiple links: prefix preserved");
    assert_contains(html, "<a href=\"One\">One</a>", "Multiple links: first converted");
    assert_contains(html, "<a href=\"Two\">2</a>", "Multiple links: second converted");
    assert_contains(html, " end", "Multiple links: trailing text preserved");
    apex_free_string(html);

    /* Test adjacent wiki links */
    html = apex_markdown_to_html("[[A]][[B]]", strlen("[[A]][[B]]"), &opts);
    assert_contains(html, "<a href=\"A\">A</a>", "Adjacent links: first");
    assert_contains(html, "<a href=\"B\">B</a>", "Adjacent links: second");
    apex_free_string(html);

    /* Test malformed wiki link with no closing marker (should remain literal) */
    html = apex_markdown_to_html("Start [[Broken", strlen("Start [[Broken"), &opts);
    assert_contains(html, "[[Broken", "Malformed wiki link (no close) preserved as text");
    assert_not_contains(html, "<a href=", "Malformed wiki link (no close) does not create link");
    apex_free_string(html);

    /* Test empty wiki link content: [[]] should remain literal */
    html = apex_markdown_to_html("[[]]", strlen("[[]]"), &opts);
    assert_contains(html, "[[]]", "Empty wiki link preserved as text");
    assert_not_contains(html, "<a href=", "Empty wiki link does not create link");
    apex_free_string(html);

    /* Test space mode: dash (default) */
    opts.wikilink_space = 0;  /* dash */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page\">Home Page</a>", "Wiki link space mode: dash");
    apex_free_string(html);

    /* Test space mode: none */
    opts.wikilink_space = 1;  /* none */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"HomePage\">Home Page</a>", "Wiki link space mode: none");
    apex_free_string(html);

    /* Test space mode: underscore */
    opts.wikilink_space = 2;  /* underscore */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home_Page\">Home Page</a>", "Wiki link space mode: underscore");
    apex_free_string(html);

    /* Test space mode: space (URL-encoded as %20) */
    opts.wikilink_space = 3;  /* space */
    opts.wikilink_extension = NULL;  /* Reset extension */
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home%20Page\">Home Page</a>", "Wiki link space mode: space (URL-encoded)");
    apex_free_string(html);

    /* Test extension without leading dot */
    opts.wikilink_space = 0;  /* dash (default) */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page.html\">Home Page</a>", "Wiki link with extension (no leading dot)");
    apex_free_string(html);

    /* Test extension with leading dot */
    opts.wikilink_extension = ".html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home-Page.html\">Home Page</a>", "Wiki link with extension (with leading dot)");
    apex_free_string(html);

    /* Test extension with section */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page#Section]]", 21, &opts);
    assert_contains(html, "<a href=\"Home-Page.html#Section\">Home Page</a>", "Wiki link with extension and section");
    apex_free_string(html);

    /* Test extension with display text */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 0;  /* dash */
        test_opts.wikilink_extension = "html";
        html = apex_markdown_to_html("[[Home Page|Main]]", 18, &test_opts);
        assert_contains(html, "<a href=\"Home-Page.html\">Main</a>", "Wiki link with extension and display text");
        apex_free_string(html);
    }

    /* Test space mode none with extension */
    opts.wikilink_space = 1;  /* none */
    opts.wikilink_extension = "md";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"HomePage.md\">Home Page</a>", "Wiki link space mode none with extension");
    apex_free_string(html);

    /* Test space mode underscore with extension */
    opts.wikilink_space = 2;  /* underscore */
    opts.wikilink_extension = "html";
    html = apex_markdown_to_html("[[Home Page]]", 13, &opts);
    assert_contains(html, "<a href=\"Home_Page.html\">Home Page</a>", "Wiki link space mode underscore with extension");
    apex_free_string(html);

    /* Test multiple spaces with dash mode */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 0;  /* dash */
        test_opts.wikilink_extension = NULL;
        html = apex_markdown_to_html("[[My Home Page]]", 16, &test_opts);
        assert_contains(html, "<a href=\"My-Home-Page\">My Home Page</a>", "Wiki link multiple spaces with dash");
        apex_free_string(html);
    }

    /* Test multiple spaces with none mode */
    {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = 1;  /* none */
        test_opts.wikilink_extension = NULL;
        html = apex_markdown_to_html("[[My Home Page]]", 16, &test_opts);
        assert_contains(html, "<a href=\"MyHomePage\">My Home Page</a>", "Wiki link multiple spaces with none");
        apex_free_string(html);
    }

    struct {
        const char *md;
        size_t md_len;
        int wikilink_space;
        const char *wikilink_extension;
        const char *expect;
        const char *desc;
    } sanitize_tests[] = {
        /* basic sanitization */
        { "[[Mork💓Mindy]]", 17, WIKILINK_SPACE_DASH, NULL, "href=\"mork-mindy\"", "Sanitize emoji in the middle of words" },
        { "[[UPPERCASE Page]]", 18, WIKILINK_SPACE_DASH, NULL, "<a href=\"uppercase-page\">UPPERCASE Page</a>", "Sanitize lowercases" },
        { "[[Hello!! World!!]]", 19, WIKILINK_SPACE_DASH, NULL, "<a href=\"hello-world\">Hello!! World!!</a>", "Sanitize replaces non-alnum" },
        { "[[Hello   World]]", 17, WIKILINK_SPACE_DASH, NULL, "<a href=\"hello-world\">Hello   World</a>", "Sanitize removes duplicate dashes" },
        { "[[???Hello World???]]", 21, WIKILINK_SPACE_DASH, NULL, "href=\"hello-world\"", "Sanitize removes leading/trailing" },
        { "[[Hello World]]", 15, WIKILINK_SPACE_DASH, "html", "<a href=\"hello-world.html\">Hello World</a>", "Sanitize with extension" },
        { "[[path/to/FILE.MD]]", 19, WIKILINK_SPACE_DASH, NULL, "href=\"path/to/file.md\"", "Sanitize preserves slashes and periods" },
        { "[[My Page Name|Click Here]]", 27, WIKILINK_SPACE_DASH, NULL, "<a href=\"my-page-name\">Click Here</a>", "Sanitize with display text" },
        /* apostrophes and quotes always removed */
        { "[[O'Brien's Page]]", 18, WIKILINK_SPACE_DASH, NULL, "href=\"obriens-page\"", "Sanitize removes apostrophes" },
        { "[[Abso\\`fricking´lutely]]", 27, WIKILINK_SPACE_DASH, NULL, "href=\"absofrickinglutely\"", "Sanitize removes ascii fancy apostrophes" },
        { "[[Abso‘fricking’lutely]]", 28, WIKILINK_SPACE_DASH, NULL, "href=\"absofrickinglutely\"", "Sanitize removes unicode fancy apostrophes" },
        { "[[Abso\"fricking\"lutely]]", 28, WIKILINK_SPACE_DASH, NULL, "href=\"absofrickinglutely\"", "Sanitize removes quotes" },
        { "[[Abso“fricking”lutely]]", 28, WIKILINK_SPACE_DASH, NULL, "href=\"absofrickinglutely\"", "Sanitize removes unicode fancy quotes" },
        /* different space modes */
        { "[[Hello World]]", 15, WIKILINK_SPACE_UNDERSCORE, NULL, "<a href=\"hello_world\">Hello World</a>", "Sanitize with underscore mode" },
        { "[[Hello World!!!]]", 18, WIKILINK_SPACE_NONE, NULL, "<a href=\"helloworld\">Hello World!!!</a>", "Sanitize with none mode" },
        /* unicode accents */
        { "[[L" "\xC3\xA9" "on]]", 22, WIKILINK_SPACE_DASH, NULL, "<a href=\"leon\"", "Accents removed NFC: lowercase" },
        { "[[L" "\xC3\x89" "ON]]", 22, WIKILINK_SPACE_DASH, NULL, "<a href=\"leon\"", "Accents removed NFC: uppercase" },
        { "[[L" "\x65\xCC\x81" "on]]", 22, WIKILINK_SPACE_DASH, NULL, "<a href=\"leon\"", "Accents removed NFD: lowercase" },
        { "[[L" "\x45\xCC\x81" "ON]]", 22, WIKILINK_SPACE_DASH, NULL, "<a href=\"leon\"", "Accents removed NFD: uppercase" },
        /* ligatures: æ (U+00E6) = 0xC3 0xA6, Æ (U+00C6) = 0xC3 0x86, ß (U+00DF) = 0xC3 0x9F*/
        { "[[" "\xC3\xA6" "on]]", 9, WIKILINK_SPACE_DASH, NULL, "<a href=\"aeon\"", "Ligature ae lowercase" },
        { "[[" "\xC3\x86" "ON]]", 9, WIKILINK_SPACE_DASH, NULL, "<a href=\"aeon\"", "Ligature ae uppercase" },
        { "[[Stra" "\xC3\x9F" "e]]", 12, WIKILINK_SPACE_DASH, NULL, "<a href=\"strasse\"", "Ligature ss" },
    };
    for (size_t i = 0; i < sizeof(sanitize_tests)/sizeof(sanitize_tests[0]); ++i) {
        apex_options test_opts = apex_options_default();
        test_opts.enable_wiki_links = true;
        test_opts.wikilink_space = sanitize_tests[i].wikilink_space;
        test_opts.wikilink_sanitize = true;
        test_opts.wikilink_extension = sanitize_tests[i].wikilink_extension;
        html = apex_markdown_to_html(
            sanitize_tests[i].md,
            sanitize_tests[i].md_len,
            &test_opts
        );
        assert_contains(html, sanitize_tests[i].expect, sanitize_tests[i].desc);
        apex_free_string(html);
    }

    /* Reset options */
    opts.wikilink_extension = NULL;
    opts.wikilink_space = 0;  /* dash (default) */

    /* Direct calls for coverage: create_wiki_links_extension returns NULL (postprocess-only) */
    test_result(create_wiki_links_extension() == NULL, "create_wiki_links_extension returns NULL (postprocess-only)");

    /* Direct calls for coverage: wiki_links_set_config (no-op unless ext+config provided) */
    {
        wiki_link_config cfg;
        cfg.base_path = "/wiki/";
        cfg.extension = ".html";
        cfg.space_mode = WIKILINK_SPACE_DASH;

        cmark_syntax_extension *ext = cmark_syntax_extension_new("dummy_wiki");
        wiki_links_set_config(ext, &cfg);
        cmark_syntax_extension_free(cmark_get_default_mem_allocator(), ext);
        test_result(true, "wiki_links_set_config called with extension and config");
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Wiki Links Tests", had_failures, false);
}

/**
 * Test math support
 */

void test_image_embedding(void) {
    int suite_failures = suite_start();
    print_suite_title("Image Embedding Tests", false, true);

    apex_options opts = apex_options_default();
    char *html;

    /* Test local image embedding */
    opts.embed_images = true;
    opts.base_directory = TEST_FIXTURES_DIR;
    const char *local_image_md = "![Test Image](test_image.png)";
    html = apex_markdown_to_html(local_image_md, strlen(local_image_md), &opts);
    assert_contains(html, "<img", "Local image generates img tag");
    assert_contains(html, "data:image/png;base64,", "Local image embedded as base64 data URL");
    assert_not_contains(html, "test_image.png", "Local image path replaced with data URL");
    apex_free_string(html);

    /* Test that local images are not embedded when flag is off */
    opts.embed_images = false;
    html = apex_markdown_to_html(local_image_md, strlen(local_image_md), &opts);
    assert_contains(html, "<img", "Local image generates img tag");
    assert_contains(html, "test_image.png", "Local image path preserved when embedding disabled");
    assert_not_contains(html, "data:image/png;base64,", "Local image not embedded when flag is off");
    apex_free_string(html);

    /* Test that remote images are not embedded (only local images supported) */
    opts.embed_images = true;
    const char *remote_image_md = "![Remote Image](https://fastly.picsum.photos/id/973/300/300.jpg?hmac=KKNEjIQImwiXzi0Xly-dB7LhYl5SX5koiFRx0HiSUmA)";
    html = apex_markdown_to_html(remote_image_md, strlen(remote_image_md), &opts);
    assert_contains(html, "<img", "Remote image generates img tag");
    assert_contains(html, "fastly.picsum.photos", "Remote image URL preserved (only local images are embedded)");
    assert_not_contains(html, "data:image/", "Remote image not embedded");
    apex_free_string(html);

    /* Test that already-embedded data URLs are not processed again */
    opts.embed_images = true;
    const char *data_url_md = "![Already Embedded](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==)";
    html = apex_markdown_to_html(data_url_md, strlen(data_url_md), &opts);
    assert_contains(html, "data:image/png;base64,", "Data URL preserved");
    /* Should only appear once (not duplicated) */
    const char *first = strstr(html, "data:image/png;base64,");
    const char *second = first ? strstr(first + 1, "data:image/png;base64,") : NULL;
    if (first && !second) {
        test_result(true, "Data URL not processed again");
    } else {
        test_result(false, "Data URL was processed again");
    }
    apex_free_string(html);

    /* Test base_directory for relative path resolution */
    opts.embed_images = true;
    opts.base_directory = NULL;  /* No base directory */
    const char *relative_image_md = "![Relative Image](test_image.png)";
    html = apex_markdown_to_html(relative_image_md, strlen(relative_image_md), &opts);
    /* Without base_directory, relative path might not be found, so it won't be embedded */
    assert_contains(html, "test_image.png", "Relative image path preserved when base_directory not set");
    apex_free_string(html);

    /* Test with base_directory set */
    opts.base_directory = TEST_FIXTURES_DIR;
    html = apex_markdown_to_html(relative_image_md, strlen(relative_image_md), &opts);
    assert_contains(html, "data:image/png;base64,", "Relative image embedded when base_directory is set");
    assert_not_contains(html, "test_image.png", "Relative image path replaced with data URL when base_directory set");
    apex_free_string(html);

    /* Test that absolute paths work regardless of base_directory */
    opts.base_directory = "/nonexistent/path";
    char abs_path[512];
    snprintf(abs_path, sizeof(abs_path), "![Absolute Image](%s/test_image.png)", TEST_FIXTURES_DIR);
    html = apex_markdown_to_html(abs_path, strlen(abs_path), &opts);
    assert_contains(html, "data:image/png;base64,", "Absolute path image embedded regardless of base_directory");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Image Embedding Tests", had_failures, false);
}

/**
 * Test image width/height attribute conversion
 */

void test_image_width_height_conversion(void) {
    int suite_failures = suite_start();
    print_suite_title("Image Width/Height Conversion Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Test 1: Percentage width → style attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=50% }", strlen("![](img.jpg){ width=50% }"), &opts);
    assert_contains(html, "style=\"width: 50%\"", "Percentage width converted to style");
    assert_not_contains(html, "width=\"50%\"", "Percentage not in width attribute");
    apex_free_string(html);

    /* Test 2: Pixel width → integer width attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=300px }", strlen("![](img.jpg){ width=300px }"), &opts);
    assert_contains(html, "width=\"300\"", "Pixel width converted to integer");
    assert_not_contains(html, "width=\"300px\"", "px suffix removed");
    assert_not_contains(html, "style=\"width: 300px\"", "Pixel not in style");
    apex_free_string(html);

    /* Test 3: Bare integer width → width attribute */
    html = apex_markdown_to_html("![](img.jpg){ width=300 }", strlen("![](img.jpg){ width=300 }"), &opts);
    assert_contains(html, "width=\"300\"", "Bare integer width preserved");
    apex_free_string(html);

    /* Test 4: Percentage height → style attribute */
    html = apex_markdown_to_html("![](img.jpg){ height=75% }", strlen("![](img.jpg){ height=75% }"), &opts);
    assert_contains(html, "style=\"height: 75%\"", "Percentage height converted to style");
    apex_free_string(html);

    /* Test 5: Pixel height → integer height attribute */
    html = apex_markdown_to_html("![](img.jpg){ height=200px }", strlen("![](img.jpg){ height=200px }"), &opts);
    assert_contains(html, "height=\"200\"", "Pixel height converted to integer");
    assert_not_contains(html, "height=\"200px\"", "px suffix removed");
    apex_free_string(html);

    /* Test 6: Both width and height with percentages → style */
    html = apex_markdown_to_html("![](img.jpg){ width=50% height=75% }", strlen("![](img.jpg){ width=50% height=75% }"), &opts);
    assert_contains(html, "style=\"width: 50%; height: 75%\"", "Both percentages in style");
    apex_free_string(html);

    /* Test 7: Mixed - pixel width, percentage height */
    html = apex_markdown_to_html("![](img.jpg){ width=300px height=50% }", strlen("![](img.jpg){ width=300px height=50% }"), &opts);
    assert_contains(html, "width=\"300\"", "Pixel width as attribute");
    assert_contains(html, "style=\"height: 50%\"", "Percentage height in style");
    apex_free_string(html);

    /* Test 8: Mixed - percentage width, pixel height */
    html = apex_markdown_to_html("![](img.jpg){ width=50% height=200px }", strlen("![](img.jpg){ width=50% height=200px }"), &opts);
    assert_contains(html, "height=\"200\"", "Pixel height as attribute");
    assert_contains(html, "style=\"width: 50%\"", "Percentage width in style");
    apex_free_string(html);

    /* Test 9: Other units (em, rem) → style */
    html = apex_markdown_to_html("![](img.jpg){ width=5em height=10rem }", strlen("![](img.jpg){ width=5em height=10rem }"), &opts);
    assert_contains(html, "style=\"width: 5em; height: 10rem\"", "Other units in style");
    apex_free_string(html);

    /* Test 10: Decimal pixel value → style */
    html = apex_markdown_to_html("![](img.jpg){ width=100.5px }", strlen("![](img.jpg){ width=100.5px }"), &opts);
    assert_contains(html, "style=\"width: 100.5px\"", "Decimal pixel in style");
    assert_not_contains(html, "width=\"100.5\"", "Decimal pixel not as attribute");
    apex_free_string(html);

    /* Test 11: Viewport units → style */
    html = apex_markdown_to_html("![](img.jpg){ width=50vw height=30vh }", strlen("![](img.jpg){ width=50vw height=30vh }"), &opts);
    assert_contains(html, "style=\"width: 50vw; height: 30vh\"", "Viewport units in style");
    apex_free_string(html);

    /* Test 12: Inline image with IAL and percentage */
    html = apex_markdown_to_html("![alt](img.jpg){ width=75% }", strlen("![alt](img.jpg){ width=75% }"), &opts);
    assert_contains(html, "style=\"width: 75%\"", "Inline image percentage in style");
    apex_free_string(html);

    /* Test 13: Mixed with other attributes (ID, class) */
    html = apex_markdown_to_html("![test](img.jpg){#test .image width=250px height=80% }", strlen("![test](img.jpg){#test .image width=250px height=80% }"), &opts);
    assert_contains(html, "id=\"test\"", "ID preserved");
    assert_contains(html, "class=\"image\"", "Class preserved");
    assert_contains(html, "width=\"250\"", "Pixel width as attribute");
    assert_contains(html, "style=\"height: 80%\"", "Percentage height in style");
    apex_free_string(html);

    /* Test 14: Zero pixel value */
    html = apex_markdown_to_html("![](img.jpg){ width=0px }", strlen("![](img.jpg){ width=0px }"), &opts);
    assert_contains(html, "width=\"0\"", "Zero pixel converted to integer");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Image Width/Height Conversion Tests", had_failures, false);
}

/**
 * Test indices (mmark and TextIndex syntax)
 */
