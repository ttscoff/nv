/**
 * Metadata Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/metadata.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

void test_metadata(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test YAML metadata with variables */
    const char *yaml_doc = "---\ntitle: Test Doc\nauthor: John\n---\n\n# [%title]\n\nBy [%author]";
    html = apex_markdown_to_html(yaml_doc, strlen(yaml_doc), &opts);
    assert_contains(html, "<h1", "YAML metadata variable in header");
    assert_contains(html, "Test Doc</h1>", "YAML metadata variable content");
    assert_contains(html, "By John", "YAML metadata variable in text");
    apex_free_string(html);

    /* Test YAML arrays + nested mappings (libyaml flattening) and end marker '...'. */
    const char *yaml_nested =
        "---\n"
        "tags:\n"
        "  - one\n"
        "  - two\n"
        "nested:\n"
        "  a: 1\n"
        "  b:\n"
        "    - x\n"
        "    - y\n"
        "...\n"
        "\n"
        "Tags: [%tags]\n"
        "Nested a: [%nested.a]\n"
        "Nested b: [%nested.b]\n"
        "Nested b0: [%nested.b.0]\n";
    html = apex_markdown_to_html(yaml_nested, strlen(yaml_nested), &opts);
    assert_contains(html, "Tags: one, two", "YAML array flattened to joined string");
    assert_contains(html, "Nested a: 1", "YAML nested mapping flattened (nested.a)");
    assert_contains(html, "Nested b: x, y", "YAML nested sequence flattened (nested.b)");
    /* Note: scalar-only sequences are normalized to a joined string; index keys are not generated. */
    assert_contains(html, "Nested b0: [%nested.b.0]", "YAML scalar sequence does not generate index keys");
    apex_free_string(html);

    /* Test YAML sequence containing a nested mapping: scalar items join, mapping items flatten with index keys. */
    const char *yaml_seq_map =
        "---\n"
        "arr:\n"
        "  - one\n"
        "  - k: v\n"
        "---\n"
        "\n"
        "Arr: [%arr]\n"
        "Arr0: [%arr.0]\n"
        "Arr1.k: [%arr.1.k]\n";
    html = apex_markdown_to_html(yaml_seq_map, strlen(yaml_seq_map), &opts);
    /* Note: mixed scalar+mapping sequences fall back to indexed keys (no joined base key). */
    assert_contains(html, "Arr: [%arr]", "YAML mixed sequence: base key not generated");
    assert_contains(html, "Arr0: one", "YAML sequence scalar index flattened (arr.0)");
    assert_contains(html, "Arr1.k: v", "YAML sequence mapping flattened with index key (arr.1.k)");
    apex_free_string(html);

    /* Test MMD metadata */
    const char *mmd_doc = "Title: My Title\n\n# [%Title]";
    html = apex_markdown_to_html(mmd_doc, strlen(mmd_doc), &opts);
    assert_contains(html, "<h1", "MMD metadata variable");
    assert_contains(html, "My Title</h1>", "MMD metadata variable content");
    apex_free_string(html);

    /* Test Pandoc metadata */
    const char *pandoc_doc = "% The Title\n% The Author\n\n# [%title]";
    html = apex_markdown_to_html(pandoc_doc, strlen(pandoc_doc), &opts);
    assert_contains(html, "<h1", "Pandoc metadata variable");
    assert_contains(html, "The Title</h1>", "Pandoc metadata variable content");
    apex_free_string(html);

    /* Test that list items with colons are not treated as metadata in unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    const char *list_with_colon_doc = "## A Header\n\n- Foo: Bar\n- Another item\n- Third item";
    html = apex_markdown_to_html(list_with_colon_doc, strlen(list_with_colon_doc), &unified_opts);
    assert_contains(html, "<h2", "List with colon: header is rendered");
    assert_contains(html, "A Header</h2>", "List with colon: header content");
    assert_contains(html, "<ul>", "List with colon: unordered list rendered");
    assert_contains(html, "<li>Foo: Bar</li>", "List with colon: first item with colon rendered");
    assert_contains(html, "<li>Another item</li>", "List with colon: second item rendered");
    assert_contains(html, "<li>Third item</li>", "List with colon: third item rendered");
    apex_free_string(html);

    /* H1 then a paragraph with ": " must not strip the paragraph as MMD metadata */
    const char *h1_colon_para =
        "# My Title\n"
        "\n"
        "This is a note: it has a colon in the first sentence.\n"
        "\n"
        "More content.\n";
    html = apex_markdown_to_html(h1_colon_para, strlen(h1_colon_para), &unified_opts);
    assert_contains(html, "<h1", "H1+colon para: heading rendered");
    assert_contains(html, "My Title", "H1+colon para: heading text preserved");
    assert_contains(html, "This is a note: it has a colon", "H1+colon para: paragraph preserved");
    assert_contains(html, "More content.", "H1+colon para: following paragraph preserved");
    apex_free_string(html);

    /* Leading blank line disables undelimited MMD metadata */
    const char *leading_blank_meta =
        "\n"
        "Title: Should Not Strip\n"
        "\n"
        "# Body\n";
    html = apex_markdown_to_html(leading_blank_meta, strlen(leading_blank_meta), &opts);
    assert_contains(html, "Title: Should Not Strip", "Leading blank: metadata-like line kept as content");
    assert_contains(html, "Body", "Leading blank: body heading rendered");
    apex_free_string(html);

    /* Valid single short MMD key/value still works */
    const char *single_mmd = "Title: Only Title\n\n# [%Title]\n";
    html = apex_markdown_to_html(single_mmd, strlen(single_mmd), &opts);
    assert_contains(html, "Only Title</h1>", "Single short MMD metadata still works");
    assert_not_contains(html, "Title: Only Title", "Single short MMD metadata is stripped");
    apex_free_string(html);

    /* Keys with punctuation other than space/-/_ are not metadata */
    const char *punct_key =
        "Hello, world: not metadata\n"
        "\n"
        "# Body\n";
    html = apex_markdown_to_html(punct_key, strlen(punct_key), &opts);
    assert_contains(html, "Hello, world: not metadata", "Punctuation in key keeps line as content");
    apex_free_string(html);

    /* Single metadata-like line longer than 100 chars is treated as content */
    {
        char long_doc[256];
        char key[41];
        char val[71];
        memset(key, 'a', 40); key[40] = '\0';
        memset(val, 'b', 70); val[70] = '\0';
        /* "aaaa...: bbbb..." is > 100 chars on the first line */
        snprintf(long_doc, sizeof(long_doc), "%s: %s\n\n# Body\n", key, val);
        assert_option_bool(strlen(key) + 2 + strlen(val) > 100, true,
                           "long-line fixture first line is >100 chars");
        html = apex_markdown_to_html(long_doc, strlen(long_doc), &opts);
        assert_contains(html, key, "Long single colon-line kept as content (key)");
        assert_contains(html, "Body", "Long single colon-line: body heading rendered");
        apex_free_string(html);
    }

    /* Multiple short metadata lines still work (including keys with spaces/-/_) */
    const char *multi_mmd =
        "Title: Multi Title\n"
        "Base Header Level: 2\n"
        "css-file: style.css\n"
        "\n"
        "# Section\n";
    html = apex_markdown_to_html(multi_mmd, strlen(multi_mmd), &opts);
    assert_not_contains(html, "Title: Multi Title", "Multi MMD: title stripped");
    assert_not_contains(html, "Base Header Level:", "Multi MMD: spaced key stripped");
    assert_contains(html, "Section", "Multi MMD: body remains");
    apex_free_string(html);

    /* No space after colon is not metadata (e.g. bare URLs / ratios) */
    const char *no_space_after_colon =
        "https://example.com/path\n"
        "\n"
        "# Body\n";
    html = apex_markdown_to_html(no_space_after_colon, strlen(no_space_after_colon), &opts);
    assert_contains(html, "https://example.com/path", "No space after colon: URL kept as content");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Tests", had_failures, false);
}

/**
 * YAML serialization helpers used by the CLI
 */
void test_metadata_yaml_emit(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata YAML emit Tests", false, true);

    apex_metadata_item *m = apex_parse_command_metadata("title=Hello World");
    assert(m != NULL);
    FILE *fp = tmpfile();
    assert(fp != NULL);
    apex_metadata_fprint_yaml_document(fp, m);
    rewind(fp);
    char buf[800];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    assert_contains(buf, "---", "yaml document has markers");
    assert_contains(buf, "title:", "yaml title key");
    assert_contains(buf, "Hello World", "yaml title value");
    apex_free_metadata(m);

    apex_metadata_item *a = apex_parse_command_metadata("x=1");
    apex_metadata_item *b = apex_parse_command_metadata("x=2");
    apex_metadata_item *mg = apex_merge_metadata(a, b, NULL);
    apex_free_metadata(a);
    apex_free_metadata(b);
    fp = tmpfile();
    apex_metadata_fprint_yaml_mapping(fp, mg);
    rewind(fp);
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    assert_contains(buf, "x: 2", "merge: later metadata wins for yaml mapping");
    apex_free_metadata(mg);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata YAML emit Tests", had_failures, false);
}

static void assert_meta_value(apex_metadata_item *m, const char *key, const char *expected,
                              const char *label) {
    const char *v = apex_metadata_get(m, key);
    test_resultf(v && strcmp(v, expected) == 0, "%s: %s = [%s] (got [%s])",
                 label, key, expected, v ? v : "(null)");
}

static void assert_quoted_scalars(apex_metadata_item *m, const char *label) {
    assert_meta_value(m, "b", "it's", label);
    assert_meta_value(m, "d", "tab\there", label);
    assert_meta_value(m, "q", "say \"hi\"", label);
    assert_meta_value(m, "s", "back\\slash", label);
    assert_meta_value(m, "n", "line\nbreak", label);
    assert_meta_value(m, "u", "caf\xC3\xA9", label);
    assert_meta_value(m, "c", "x", label);
    assert_meta_value(m, "p", "plain value", label);
    assert_meta_value(m, "sp", "  padded  ", label);
}

/* Write contents to a new temp file; path_template may end with a suffix of
 * suffix_len characters after the XXXXXX. Returns 0 on success. */
static int write_temp_file_suffix(char *path_template, int suffix_len, const char *contents) {
    int fd = mkstemps(path_template, suffix_len);
    if (fd < 0) return -1;
    size_t len = strlen(contents);
    ssize_t written = write(fd, contents, len);
    close(fd);
    return written == (ssize_t)len ? 0 : -1;
}

static int write_temp_file(char *path_template, const char *contents) {
    return write_temp_file_suffix(path_template, 0, contents);
}

/* Load a metadata file, capturing anything written to stderr into err_buf. */
static apex_metadata_item *load_metadata_capturing_stderr(const char *path, char *err_buf, size_t err_size) {
    err_buf[0] = '\0';
    char err_path[] = "/tmp/apex_test_stderr_XXXXXX";
    int err_fd = mkstemp(err_path);
    if (err_fd < 0) return apex_load_metadata_from_file(path);
    int saved_stderr = dup(STDERR_FILENO);
    fflush(stderr);
    dup2(err_fd, STDERR_FILENO);

    apex_metadata_item *m = apex_load_metadata_from_file(path);

    fflush(stderr);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    lseek(err_fd, 0, SEEK_SET);
    ssize_t n = read(err_fd, err_buf, err_size - 1);
    err_buf[n > 0 ? n : 0] = '\0';
    close(err_fd);
    unlink(err_path);
    return m;
}

/**
 * The simple parser used without libyaml (or when libyaml rejects the input)
 * must unquote scalars the same way libyaml does.
 */
void test_metadata_yaml_fallback(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata YAML fallback Tests", false, true);

    const char *scalars =
        "b: 'it''s'\n"
        "d: \"tab\\there\"\n"
        "q: \"say \\\"hi\\\"\"\n"
        "s: \"back\\\\slash\"\n"
        "n: \"line\\nbreak\"\n"
        "u: \"caf\\u00e9\"\n"
        "c: 'x' # comment\n"
        "p: plain value\n"
        "sp: \"  padded  \"\n";
    /* libyaml rejects a plain scalar containing ": ", forcing the fallback */
    const char *rejected_line = "description: Wrap :emoji: markers\n";

    char doc[1024];
    char *ptr;
    apex_metadata_item *m;

    snprintf(doc, sizeof(doc), "---\n%s---\nBody\n", scalars);
    ptr = doc;
    m = apex_extract_metadata(&ptr);
    assert_quoted_scalars(m, "front matter");
    apex_free_metadata(m);

    snprintf(doc, sizeof(doc), "---\n%s%s---\nBody\n", rejected_line, scalars);
    ptr = doc;
    m = apex_extract_metadata(&ptr);
    assert_quoted_scalars(m, "fallback front matter");
    assert_meta_value(m, "description", "Wrap :emoji: markers", "fallback front matter");
    apex_free_metadata(m);

    /* Rendered: the substituted value, not the escaped source */
    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    opts.enable_smart_typography = false;
    const char *render_doc =
        "---\ndescription: Wrap :emoji: markers\ntitle: 'Brett''s Notes'\n---\n[%title]\n";
    char *html = apex_markdown_to_html(render_doc, strlen(render_doc), &opts);
    assert_contains(html, "Brett's Notes", "fallback: '' renders as a single quote");
    assert_not_contains(html, "Brett''s", "fallback: '' is not left doubled");
    apex_free_string(html);

    /* Plugin manifests load through the same fallback; \" must not survive */
    char manifest_path[] = "/tmp/apex_test_manifest_XXXXXX";
    const char *manifest =
        "---\n"
        "id: test\n"
        "description: Wrap :emoji: markers\n"
        "phase: post_render\n"
        "pattern: \"(:[0-9]+:)\"\n"
        "replacement: \"<span class=\\\"emoji\\\">$1</span>\"\n"
        "---\n";
    char err_buf[512];
    if (write_temp_file(manifest_path, manifest) == 0) {
        m = load_metadata_capturing_stderr(manifest_path, err_buf, sizeof(err_buf));
        assert_meta_value(m, "replacement", "<span class=\"emoji\">$1</span>", "fallback manifest");
        assert_meta_value(m, "pattern", "(:[0-9]+:)", "fallback manifest");
        assert_contains(err_buf, "Warning: YAML error in", "rejected manifest prints a warning");
        assert_contains(err_buf, manifest_path, "warning names the manifest file");
        assert_contains(err_buf, "mapping values are not allowed", "warning includes libyaml's problem");
        apex_free_metadata(m);
        unlink(manifest_path);
    } else {
        test_result(false, "fallback manifest: could not write temp file");
    }

    /* .yml files are YAML even without --- markers */
    char yml_path[] = "/tmp/apex_test_meta_XXXXXX.yml";
    if (write_temp_file_suffix(yml_path, 4, "# comment: not a key\na: \"x\"\nb: 'it''s'\n") == 0) {
        m = load_metadata_capturing_stderr(yml_path, err_buf, sizeof(err_buf));
        assert_meta_value(m, "a", "x", "undelimited .yml");
        assert_meta_value(m, "b", "it's", "undelimited .yml");
        test_result(apex_metadata_get(m, "# comment") == NULL, "undelimited .yml: comment is not a key");
        test_result(err_buf[0] == '\0', "undelimited valid .yml prints no warning");
        apex_free_metadata(m);
        unlink(yml_path);
    } else {
        test_result(false, "undelimited .yml: could not write temp file");
    }

    char yaml_path[] = "/tmp/apex_test_meta_XXXXXX.yaml";
    if (write_temp_file_suffix(yaml_path, 5,
                               "# comment: not a key\ndescription: Wrap :emoji: markers\na: \"x\"") == 0) {
        m = load_metadata_capturing_stderr(yaml_path, err_buf, sizeof(err_buf));
        assert_meta_value(m, "a", "x", "undelimited .yaml fallback");
        assert_meta_value(m, "description", "Wrap :emoji: markers", "undelimited .yaml fallback");
        test_result(apex_metadata_get(m, "# comment") == NULL, "undelimited .yaml fallback: comment is not a key");
        assert_contains(err_buf, "(line 2,", "undelimited .yaml warning reports the file's line number");
        apex_free_metadata(m);
        unlink(yaml_path);
    } else {
        test_result(false, "undelimited .yaml: could not write temp file");
    }

    char txt_path[] = "/tmp/apex_test_meta_XXXXXX.txt";
    if (write_temp_file_suffix(txt_path, 4, "a: \"x\"\n") == 0) {
        m = apex_load_metadata_from_file(txt_path);
        assert_meta_value(m, "a", "\"x\"", "undelimited non-YAML file stays MultiMarkdown");
        apex_free_metadata(m);
        unlink(txt_path);
    } else {
        test_result(false, "undelimited .txt: could not write temp file");
    }

    /* A NULL list in the middle must not stop the merge */
    apex_metadata_item *global = apex_parse_command_metadata("a=global,keep=1");
    apex_metadata_item *file = apex_parse_command_metadata("a=file");
    apex_metadata_item *lists[] = { global, NULL, file };
    apex_metadata_item *merged = apex_merge_metadata_lists(lists, 3);
    assert_meta_value(merged, "a", "file", "merge lists past NULL");
    assert_meta_value(merged, "keep", "1", "merge lists past NULL");
    apex_free_metadata(merged);
    apex_free_metadata(global);
    apex_free_metadata(file);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata YAML fallback Tests", had_failures, false);
}

/**
 * Test MultiMarkdown metadata keys
 */

void test_mmd_metadata_keys(void) {
    int suite_failures = suite_start();
    print_suite_title("MultiMarkdown Metadata Keys Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    char *html;

    /* Test Base Header Level */
    const char *base_header_doc = "Base Header Level: 2\n\n# Header 1\n## Header 2";
    html = apex_markdown_to_html(base_header_doc, strlen(base_header_doc), &opts);
    assert_contains(html, "<h2", "Base Header Level: h1 becomes h2");
    assert_contains(html, "Header 1</h2>", "Base Header Level: h1 content in h2 tag");
    assert_contains(html, "<h3", "Base Header Level: h2 becomes h3");
    assert_contains(html, "Header 2</h3>", "Base Header Level: h2 content in h3 tag");
    apex_free_string(html);

    /* Test HTML Header Level (format-specific) */
    const char *html_header_level_doc = "HTML Header Level: 3\n\n# Header 1";
    html = apex_markdown_to_html(html_header_level_doc, strlen(html_header_level_doc), &opts);
    assert_contains(html, "<h3", "HTML Header Level: h1 becomes h3");
    assert_contains(html, "Header 1</h3>", "HTML Header Level: h1 content in h3 tag");
    apex_free_string(html);

    /* Test Language metadata in standalone document */
    opts.standalone = true;
    const char *language_doc = "Language: fr\n\n# Bonjour";
    html = apex_markdown_to_html(language_doc, strlen(language_doc), &opts);
    assert_contains(html, "<html lang=\"fr\">", "Language metadata sets HTML lang attribute");
    apex_free_string(html);

    /* Test Quotes Language - French (requires smart typography) */
    opts.standalone = false;
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *quotes_fr_doc = "Quotes Language: french\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(quotes_fr_doc, strlen(quotes_fr_doc), &opts);
    assert_contains(html, "&laquo;&nbsp;", "Quotes Language: French opening quote");
    assert_contains(html, "&nbsp;&raquo;", "Quotes Language: French closing quote");
    apex_free_string(html);

    /* Test Quotes Language - German */
    const char *quotes_de_doc = "Quotes Language: german\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(quotes_de_doc, strlen(quotes_de_doc), &opts);
    assert_contains(html, "&bdquo;", "Quotes Language: German opening quote");
    assert_contains(html, "&ldquo;", "Quotes Language: German closing quote");
    apex_free_string(html);

    /* Test Quotes Language fallback to Language */
    opts.standalone = true;
    const char *lang_fallback_doc = "Language: fr\n\nHe said \"hello\" to me.";
    html = apex_markdown_to_html(lang_fallback_doc, strlen(lang_fallback_doc), &opts);
    assert_contains(html, "<html lang=\"fr\">", "Language metadata sets HTML lang");
    /* Quotes should also use French since Quotes Language not specified */
    assert_contains(html, "&laquo;&nbsp;", "Quotes Language falls back to Language");
    apex_free_string(html);

    /* Test CSS metadata in standalone document */
    opts.standalone = true;
    const char *css_doc = "CSS: styles.css\n\n# Test";
    html = apex_markdown_to_html(css_doc, strlen(css_doc), &opts);
    assert_contains(html, "<link rel=\"stylesheet\" href=\"styles.css\">", "CSS metadata adds stylesheet link");
    assert_not_contains(html, "<style>", "CSS metadata: no default styles when CSS specified");
    apex_free_string(html);

    /* Test CSS metadata: default styles when no CSS */
    const char *no_css_doc = "Title: Test\n\n# Content";
    html = apex_markdown_to_html(no_css_doc, strlen(no_css_doc), &opts);
    assert_contains(html, "<style>", "No CSS metadata: default styles included");
    apex_free_string(html);

    /* Test HTML Header metadata */
    const char *html_header_doc = "HTML Header: <script src=\"mathjax.js\"></script>\n\n# Test";
    html = apex_markdown_to_html(html_header_doc, strlen(html_header_doc), &opts);
    assert_contains(html, "<script src=\"mathjax.js\"></script>", "HTML Header metadata inserted in head");
    assert_contains(html, "</head>", "HTML Header metadata before </head>");
    apex_free_string(html);

    /* Test HTML Footer metadata */
    const char *html_footer_doc = "HTML Footer: <script>init();</script>\n\n# Test";
    html = apex_markdown_to_html(html_footer_doc, strlen(html_footer_doc), &opts);
    assert_contains(html, "<script>init();</script>", "HTML Footer metadata inserted before </body>");
    assert_contains(html, "</body>", "HTML Footer metadata before </body>");
    apex_free_string(html);

    /* Test generic metadata tags in standalone HTML head */
    opts.standalone = true;
    const char *head_meta_doc =
        "Title: This is the Title\n"
        "Author: That would be me\n"
        "Date: March 9, 2026\n\n"
        "# Test";
    html = apex_markdown_to_html(head_meta_doc, strlen(head_meta_doc), &opts);
    assert_contains(html, "<meta name=\"Author\" content=\"That would be me\"/>", "Author metadata emitted as meta tag");
    assert_contains(html, "<meta name=\"Date\" content=\"March 9, 2026\"/>", "Date metadata emitted as meta tag");
    apex_free_string(html);

    /* Test generic metadata tags in standalone HTML head (Unified mode) */
    apex_options unified_head_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_head_opts.standalone = true;
    html = apex_markdown_to_html(head_meta_doc, strlen(head_meta_doc), &unified_head_opts);
    assert_contains(html, "<meta name=\"Author\" content=\"That would be me\"/>", "Unified: author metadata emitted as meta tag");
    assert_contains(html, "<meta name=\"Date\" content=\"March 9, 2026\"/>", "Unified: date metadata emitted as meta tag");
    apex_free_string(html);

    /* Test generic metadata tags in standalone HTML head (Kramdown mode) */
    apex_options kramdown_head_opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    kramdown_head_opts.standalone = true;
    html = apex_markdown_to_html(head_meta_doc, strlen(head_meta_doc), &kramdown_head_opts);
    assert_contains(html, "<meta name=\"Author\" content=\"That would be me\"/>", "Kramdown: author metadata emitted as meta tag");
    assert_contains(html, "<meta name=\"Date\" content=\"March 9, 2026\"/>", "Kramdown: date metadata emitted as meta tag");
    apex_free_string(html);

    /* Test normalized key matching (spaces removed, case-insensitive) */
    opts.standalone = false;
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *normalized_doc = "quoteslanguage: french\nbaseheaderlevel: 2\n\n# Header\nHe said \"hello\".";
    html = apex_markdown_to_html(normalized_doc, strlen(normalized_doc), &opts);
    assert_contains(html, "<h2", "Normalized key: baseheaderlevel works");
    assert_contains(html, "&laquo;&nbsp;", "Normalized key: quoteslanguage works");
    apex_free_string(html);

    /* Test case-insensitive matching */
    opts.enable_smart_typography = true;  /* Ensure smart typography is enabled */
    const char *case_doc = "QUOTES LANGUAGE: german\nBASE HEADER LEVEL: 3\n\n# Header\nHe said \"hello\".";
    html = apex_markdown_to_html(case_doc, strlen(case_doc), &opts);
    assert_contains(html, "<h3", "Case-insensitive: BASE HEADER LEVEL works");
    assert_contains(html, "&bdquo;", "Case-insensitive: QUOTES LANGUAGE works");
    apex_free_string(html);

    /* MMD delimiter block compatibility: opening/closing with repeated chars */
    opts.standalone = true;
    const char *mmd_delimited_doc =
        "----\n"
        "Title: Delimited Title\n"
        "Author: Delimited Author\n"
        "......\n"
        "\n"
        "# Body";
    html = apex_markdown_to_html(mmd_delimited_doc, strlen(mmd_delimited_doc), &opts);
    assert_contains(html, "<meta name=\"Author\" content=\"Delimited Author\"/>", "MMD delimiter block: author parsed");
    assert_not_contains(html, "......", "MMD delimiter block: dot closer not rendered in body");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("MultiMarkdown Metadata Keys Tests", had_failures, false);
}

/**
 * Test metadata transforms
 */

void test_metadata_transforms(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Transforms Tests", false, true);

    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Test basic transforms: upper */
    const char *upper_doc = "---\ntitle: hello world\n---\n\n# [%title:upper]";
    html = apex_markdown_to_html(upper_doc, strlen(upper_doc), &opts);
    assert_contains(html, "HELLO WORLD</h1>", "upper transform");
    apex_free_string(html);

    /* Test basic transforms: lower */
    const char *lower_doc = "---\ntitle: HELLO WORLD\n---\n\n# [%title:lower]";
    html = apex_markdown_to_html(lower_doc, strlen(lower_doc), &opts);
    assert_contains(html, "hello world</h1>", "lower transform");
    apex_free_string(html);

    /* Test basic transforms: title */
    const char *title_doc = "---\ntitle: hello world\n---\n\n# [%title:title]";
    html = apex_markdown_to_html(title_doc, strlen(title_doc), &opts);
    assert_contains(html, "Hello World</h1>", "title transform");
    apex_free_string(html);

    /* Test basic transforms: capitalize */
    const char *capitalize_doc = "---\ntitle: hello world\n---\n\n# [%title:capitalize]";
    html = apex_markdown_to_html(capitalize_doc, strlen(capitalize_doc), &opts);
    assert_contains(html, "Hello world</h1>", "capitalize transform");
    apex_free_string(html);

    /* Test basic transforms: trim */
    const char *trim_doc = "---\ntitle: \"  hello world  \"\n---\n\n# [%title:trim]";
    html = apex_markdown_to_html(trim_doc, strlen(trim_doc), &opts);
    assert_contains(html, "hello world</h1>", "trim transform");
    apex_free_string(html);

    /* Test slug transform */
    const char *slug_doc = "---\ntitle: My Great Post!\n---\n\n[%title:slug]";
    html = apex_markdown_to_html(slug_doc, strlen(slug_doc), &opts);
    assert_contains(html, "my-great-post", "slug transform");
    apex_free_string(html);

    /* Test replace transform (simple) */
    const char *replace_doc = "---\nurl: http://example.com\n---\n\n[%url:replace(http:,https:)]";
    html = apex_markdown_to_html(replace_doc, strlen(replace_doc), &opts);
    assert_contains(html, "https://example.com", "replace transform");
    apex_free_string(html);

    /* Test replace transform (regex) - use simple pattern without brackets first */
    const char *regex_doc = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:123,N)]";
    html = apex_markdown_to_html(regex_doc, strlen(regex_doc), &opts);
    assert_contains(html, "Hello N World", "replace with regex");
    apex_free_string(html);

    /* Test regex with character class [0-9]+ */
    const char *regex_doc2 = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:[0-9]+,N)]";
    html = apex_markdown_to_html(regex_doc2, strlen(regex_doc2), &opts);
    assert_contains(html, "Hello N World", "replace with regex pattern with brackets");
    apex_free_string(html);

    /* Test regex with simpler pattern that definitely works */
    const char *regex_doc3 = "---\ntext: Hello 123 World\n---\n\n[%text:replace(regex:12,N)]";
    html = apex_markdown_to_html(regex_doc3, strlen(regex_doc3), &opts);
    assert_contains(html, "Hello N3 World", "replace with regex simple pattern");
    apex_free_string(html);

    /* Test substring transform */
    const char *substr_doc = "---\ntitle: Hello World\n---\n\n[%title:substr(0,5)]";
    html = apex_markdown_to_html(substr_doc, strlen(substr_doc), &opts);
    assert_contains(html, "Hello", "substring transform");
    apex_free_string(html);

    /* Test truncate transform - note: smart typography may convert ... to … */
    const char *truncate_doc = "---\ntitle: This is a very long title\n---\n\n[%title:truncate(15,...)]";
    html = apex_markdown_to_html(truncate_doc, strlen(truncate_doc), &opts);
    /* Check for either ... or … (smart typography ellipsis) */
    if (strstr(html, "This is a very...") || strstr(html, "This is a very…") || strstr(html, "This is a ve")) {
        test_result(true, "truncate transform");
    } else {
        test_result(false, "truncate transform failed");
    }
    apex_free_string(html);

    /* Test default transform */
    const char *default_doc = "---\ndesc: \"\"\n---\n\n[%desc:default(No description)]";
    html = apex_markdown_to_html(default_doc, strlen(default_doc), &opts);
    assert_contains(html, "No description", "default transform with empty value");
    apex_free_string(html);

    /* Test default transform with non-empty value */
    const char *default_nonempty_doc = "---\ndesc: Has value\n---\n\n[%desc:default(No description)]";
    html = apex_markdown_to_html(default_nonempty_doc, strlen(default_nonempty_doc), &opts);
    assert_contains(html, "Has value", "default transform preserves non-empty");
    apex_free_string(html);

    /* Test html_escape transform */
    const char *escape_doc = "---\ntitle: A & B\n---\n\n[%title:html_escape]";
    html = apex_markdown_to_html(escape_doc, strlen(escape_doc), &opts);
    assert_contains(html, "&amp;", "html_escape transform");
    apex_free_string(html);

    /* Test basename transform */
    const char *basename_doc = "---\nimage: /path/to/image.jpg\n---\n\n[%image:basename]";
    html = apex_markdown_to_html(basename_doc, strlen(basename_doc), &opts);
    assert_contains(html, "image.jpg", "basename transform");
    apex_free_string(html);

    /* Test urlencode transform */
    const char *urlencode_doc = "---\nsearch: hello world\n---\n\n[%search:urlencode]";
    html = apex_markdown_to_html(urlencode_doc, strlen(urlencode_doc), &opts);
    assert_contains(html, "hello%20world", "urlencode transform");
    apex_free_string(html);

    /* Test urldecode transform */
    const char *urldecode_doc = "---\nsearch: hello%20world\n---\n\n[%search:urldecode]";
    html = apex_markdown_to_html(urldecode_doc, strlen(urldecode_doc), &opts);
    assert_contains(html, "hello world", "urldecode transform");
    apex_free_string(html);

    /* Test prefix transform */
    const char *prefix_doc = "---\nurl: example.com\n---\n\n[%url:prefix(https://)]";
    html = apex_markdown_to_html(prefix_doc, strlen(prefix_doc), &opts);
    assert_contains(html, "https://example.com", "prefix transform");
    apex_free_string(html);

    /* Test suffix transform */
    const char *suffix_doc = "---\ntitle: Hello\n---\n\n[%title:suffix(!)]";
    html = apex_markdown_to_html(suffix_doc, strlen(suffix_doc), &opts);
    assert_contains(html, "Hello!", "suffix transform");
    apex_free_string(html);

    /* Test remove transform */
    const char *remove_doc = "---\ntitle: Hello'World\n---\n\n[%title:remove(')]";
    html = apex_markdown_to_html(remove_doc, strlen(remove_doc), &opts);
    assert_contains(html, "HelloWorld", "remove transform");
    apex_free_string(html);

    /* Test repeat transform - escape the result to avoid markdown HR interpretation */
    const char *repeat_doc = "---\nsep: -\n---\n\n`[%sep:repeat(3)]`";
    html = apex_markdown_to_html(repeat_doc, strlen(repeat_doc), &opts);
    /* Check inside code span to avoid HR interpretation */
    assert_contains(html, "<code>---</code>", "repeat transform");
    apex_free_string(html);

    /* Test reverse transform */
    const char *reverse_doc = "---\ntext: Hello\n---\n\n[%text:reverse]";
    html = apex_markdown_to_html(reverse_doc, strlen(reverse_doc), &opts);
    assert_contains(html, "olleH", "reverse transform");
    apex_free_string(html);

    /* Test format transform */
    const char *format_doc = "---\nprice: 42.5\n---\n\n[%price:format($%.2f)]";
    html = apex_markdown_to_html(format_doc, strlen(format_doc), &opts);
    assert_contains(html, "$42.50", "format transform");
    apex_free_string(html);

    /* Test length transform */
    const char *length_doc = "---\ntext: Hello\n---\n\n[%text:length]";
    html = apex_markdown_to_html(length_doc, strlen(length_doc), &opts);
    assert_contains(html, "5", "length transform");
    apex_free_string(html);

    /* Test pad transform */
    const char *pad_doc = "---\nnumber: 42\n---\n\n[%number:pad(5,0)]";
    html = apex_markdown_to_html(pad_doc, strlen(pad_doc), &opts);
    assert_contains(html, "00042", "pad transform");
    apex_free_string(html);

    /* Test contains transform */
    const char *contains_doc = "---\ntags: javascript,html,css\n---\n\n[%tags:contains(javascript)]";
    html = apex_markdown_to_html(contains_doc, strlen(contains_doc), &opts);
    assert_contains(html, "true", "contains transform");
    apex_free_string(html);

    /* Test array transforms: split */
    const char *split_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):first]";
    html = apex_markdown_to_html(split_doc, strlen(split_doc), &opts);
    assert_contains(html, "tag1", "split and first transforms");
    apex_free_string(html);

    /* Test array transforms: join */
    const char *join_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):join( | )]";
    html = apex_markdown_to_html(join_doc, strlen(join_doc), &opts);
    assert_contains(html, "tag1 | tag2 | tag3", "split and join transforms");
    apex_free_string(html);

    /* Test array transforms: last */
    const char *last_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):last]";
    html = apex_markdown_to_html(last_doc, strlen(last_doc), &opts);
    assert_contains(html, "tag3", "last transform");
    apex_free_string(html);

    /* Test array transforms: slice */
    const char *slice_doc = "---\ntags: tag1,tag2,tag3\n---\n\n[%tags:split(,):slice(0,2):join(,)]";
    html = apex_markdown_to_html(slice_doc, strlen(slice_doc), &opts);
    assert_contains(html, "tag1,tag2", "slice transform");
    apex_free_string(html);

    /* Test slice with string (character-by-character) */
    const char *slice_str_doc = "---\ntext: Hello\n---\n\n[%text:slice(0,5)]";
    html = apex_markdown_to_html(slice_str_doc, strlen(slice_str_doc), &opts);
    assert_contains(html, "Hello", "slice transform on string");
    apex_free_string(html);

    /* Test strftime transform */
    const char *strftime_doc = "---\ndate: 2024-03-15\n---\n\n[%date:strftime(%Y)]";
    html = apex_markdown_to_html(strftime_doc, strlen(strftime_doc), &opts);
    assert_contains(html, "2024", "strftime transform");
    apex_free_string(html);

    /* Test transform chaining */
    const char *chain_doc = "---\ntitle: hello world\n---\n\n# [%title:title:split( ):first]";
    html = apex_markdown_to_html(chain_doc, strlen(chain_doc), &opts);
    assert_contains(html, "Hello</h1>", "transform chaining");
    apex_free_string(html);

    /* Test transform chaining with date */
    const char *date_chain_doc = "---\ndate: 2024-03-15 14:30\n---\n\n[%date:strftime(%Y)]";
    html = apex_markdown_to_html(date_chain_doc, strlen(date_chain_doc), &opts);
    assert_contains(html, "2024", "strftime with time");
    apex_free_string(html);

    /* Test that transforms are disabled when flag is off */
    apex_options no_transforms = apex_options_for_mode(APEX_MODE_UNIFIED);
    no_transforms.enable_metadata_transforms = false;
    const char *disabled_doc = "---\ntitle: Hello\n---\n\n[%title:upper]";
    html = apex_markdown_to_html(disabled_doc, strlen(disabled_doc), &no_transforms);
    /* Should keep the transform syntax verbatim or use simple replacement */
    if (strstr(html, "[%title:upper]") != NULL || strstr(html, "Hello") != NULL) {
        test_result(true, "Transforms disabled when flag is off");
    } else {
        test_result(false, "Transforms not disabled when flag is off");
    }
    apex_free_string(html);

    /* Test that transforms are disabled in non-unified modes by default */
    apex_options mmd_opts = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
    html = apex_markdown_to_html(disabled_doc, strlen(disabled_doc), &mmd_opts);
    if (strstr(html, "[%title:upper]") != NULL || strstr(html, "Hello") != NULL) {
        test_result(true, "Transforms disabled in MMD mode by default");
    } else {
        test_result(false, "Transforms incorrectly enabled in MMD mode");
    }
    apex_free_string(html);

    /* Test that simple [%key] still works with transforms enabled */
    const char *simple_doc = "---\ntitle: Hello\n---\n\n[%title]";
    html = apex_markdown_to_html(simple_doc, strlen(simple_doc), &opts);
    assert_contains(html, "Hello", "Simple metadata replacement still works");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Transforms Tests", had_failures, false);
}

/**
 * Test wiki links
 */

void test_metadata_control_options(void) {
    int suite_failures = suite_start();
    print_suite_title("Metadata Control of Options Tests", false, true);

    /* Test boolean options via metadata */
    apex_options opts = apex_options_default();
    opts.enable_indices = true;  /* Start with indices enabled */
    opts.enable_wiki_links = false;  /* Start with wikilinks disabled */

    /* Create metadata with boolean options */
    apex_metadata_item *metadata = NULL;
    apex_metadata_item *item;

    /* Test indices: false */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("indices");
    item->value = strdup("false");
    item->next = metadata;
    metadata = item;

    /* Test wikilinks: true */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    /* Test pretty: yes */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("pretty");
    item->value = strdup("yes");
    item->next = metadata;
    metadata = item;

    /* Test standalone: 1 */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("standalone");
    item->value = strdup("1");
    item->next = metadata;
    metadata = item;

    /* Apply metadata */
    apex_apply_metadata_to_options(metadata, &opts);

    /* Verify boolean options */
    assert_option_bool(opts.enable_indices, false, "indices: false sets enable_indices to false");
    assert_option_bool(opts.enable_wiki_links, true, "wikilinks: true sets enable_wiki_links to true");
    assert_option_bool(opts.pretty, true, "pretty: yes sets pretty to true");
    assert_option_bool(opts.standalone, true, "standalone: 1 sets standalone to true");

    /* Clean up */
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test string options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("title");
    item->value = strdup("My Test Document");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("csl");
    item->value = strdup("apa.csl");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("id-format");
    item->value = strdup("mmd");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("toc-min-max");
    item->value = strdup("2,4");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_string(opts.document_title, "My Test Document", "title sets document_title");
    assert_option_string(opts.csl_file, "apa.csl", "csl sets csl_file");
    assert_option_bool(opts.id_format == 1, true, "id-format: mmd sets id_format to 1 (MMD)");
    assert_option_bool(opts.toc_min == 2, true, "toc-min-max: 2,4 sets toc_min to 2");
    assert_option_bool(opts.toc_max == 4, true, "toc-min-max: 2,4 sets toc_max to 4");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test mode option (should reset options) */
    opts = apex_options_default();
    opts.enable_indices = true;
    opts.enable_wiki_links = true;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("mode");
    item->value = strdup("gfm");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.mode == APEX_MODE_GFM, true, "mode: gfm sets mode to GFM");
    /* After mode reset, wikilinks should still be applied */
    assert_option_bool(opts.enable_wiki_links, true, "wikilinks applied after mode reset");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test case-insensitive boolean values */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("indices");
    item->value = strdup("TRUE");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("wikilinks");
    item->value = strdup("FALSE");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.enable_indices, true, "indices: TRUE (uppercase) sets enable_indices to true");
    assert_option_bool(opts.enable_wiki_links, false, "wikilinks: FALSE (uppercase) sets enable_wiki_links to false");

    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test more boolean options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("relaxed-tables");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("link-citations");
    item->value = strdup("yes");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("suppress-bibliography");
    item->value = strdup("1");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);

    assert_option_bool(opts.relaxed_tables, true, "relaxed-tables: true sets relaxed_tables");
    assert_option_bool(opts.link_citations, true, "link-citations: yes sets link_citations");
    assert_option_bool(opts.suppress_bibliography, true, "suppress-bibliography: 1 sets suppress_bibliography");

    apex_free_metadata(metadata);

    /* Test public API for loading and applying metadata from a file */
#ifdef TEST_FIXTURES_DIR
    opts = apex_options_default();
    opts.enable_indices = true;
    char public_metadata_file_path[512];
    snprintf(public_metadata_file_path, sizeof(public_metadata_file_path), "%s/metadata_options.yml", TEST_FIXTURES_DIR);

    assert_option_bool(apex_options_apply_meta_file(NULL, public_metadata_file_path), false,
                       "metadata file API rejects NULL options");
    assert_option_bool(apex_options_apply_meta_file(&opts, NULL), false,
                       "metadata file API rejects NULL path");
    assert_option_bool(apex_options_apply_meta_file(&opts, ""), false,
                       "metadata file API rejects empty path");
    assert_option_bool(apex_options_apply_meta_file(&opts, "/no/such/apex-metadata-file.yml"), false,
                       "metadata file API rejects missing file");
    assert_option_bool(opts.enable_indices, true,
                       "metadata file API leaves options unchanged on failure");
    assert_option_bool(apex_options_apply_meta_file(&opts, public_metadata_file_path), true,
                       "metadata file API loads a valid file");
    assert_option_bool(opts.enable_indices, false,
                       "metadata file API applies boolean options");
    assert_option_string(opts.csl_file, "test.csl",
                         "metadata file API owns csl after returning");
    assert_option_string(opts.document_title, "Test Document from File",
                         "metadata file API owns title after returning");
#endif

    /* Test loading metadata from file */
#ifdef TEST_FIXTURES_DIR
    opts = apex_options_default();
    char metadata_file_path[512];
    snprintf(metadata_file_path, sizeof(metadata_file_path), "%s/metadata_options.yml", TEST_FIXTURES_DIR);
    apex_metadata_item *file_metadata = apex_load_metadata_from_file(metadata_file_path);
    if (file_metadata) {
        apex_apply_metadata_to_options(file_metadata, &opts);

        assert_option_bool(opts.enable_indices, false, "metadata file: indices: false");
        assert_option_bool(opts.enable_wiki_links, true, "metadata file: wikilinks: true");
        assert_option_bool(opts.pretty, true, "metadata file: pretty: true");
        assert_option_bool(opts.standalone, true, "metadata file: standalone: true");
        assert_option_string(opts.document_title, "Test Document from File", "metadata file: title");
        assert_option_string(opts.csl_file, "test.csl", "metadata file: csl");
        assert_option_bool(opts.id_format == 2, true, "metadata file: id-format: kramdown sets id_format to 2");
        assert_option_bool(opts.link_citations, true, "metadata file: link-citations: true");
        assert_option_bool(opts.suppress_bibliography, false, "metadata file: suppress-bibliography: false");

        apex_free_metadata(file_metadata);
    } else {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " metadata file: Failed to load metadata_options.yml\n");
    }
#endif

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Metadata Control of Options Tests", had_failures, false);
}

/**
 * Test syntax highlighting options via metadata
 */
void test_syntax_highlight_options(void) {
    int suite_failures = suite_start();
    print_suite_title("Syntax Highlighting Options Tests", false, true);

    apex_options opts;
    apex_metadata_item *metadata = NULL;
    apex_metadata_item *item;

    /* Test code-highlight option: pygments (full name) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pygments");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: pygments sets code_highlighter");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: skylighting (full name) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("skylighting");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: skylighting sets code_highlighter");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: p (abbreviation for pygments) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("p");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: p (abbreviation) sets pygments");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: s (abbreviation for skylighting) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("s");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: s (abbreviation) sets skylighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: pyg (abbreviation for pygments) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pyg");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "code-highlight: pyg (abbreviation) sets pygments");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: sky (abbreviation for skylighting) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("sky");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "skylighting", "code-highlight: sky (abbreviation) sets skylighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: shiki (full name) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("shiki");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "shiki", "code-highlight: shiki sets code_highlighter");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: sh (abbreviation for shiki) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("sh");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "shiki", "code-highlight: sh (abbreviation) sets shiki");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: false disables */
    opts = apex_options_default();
    opts.code_highlighter = "pygments";  /* Start with it enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_highlighter == NULL, true, "code-highlight: false disables highlighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-highlight option: none disables */
    opts = apex_options_default();
    opts.code_highlighter = "pygments";  /* Start with it enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("none");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_highlighter == NULL, true, "code-highlight: none disables highlighting");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-line-numbers option: true */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, true, "code-line-numbers: true enables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code-line-numbers option: false */
    opts = apex_options_default();
    opts.code_line_numbers = true;  /* Start enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, false, "code-line-numbers: false disables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* paginate: symbols */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("paginate");
    item->value = strdup("symbols");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.paginate, true, "paginate: symbols enables paginate");
    assert_option_bool(opts.paginate_symbols, true, "paginate: symbols enables paginate_symbols");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test code_line_numbers option with underscore (alternate key format) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code_line_numbers");
    item->value = strdup("yes");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.code_line_numbers, true, "code_line_numbers (underscore): yes enables line numbers");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight-language-only option: true */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("true");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, true, "highlight-language-only: true enables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight-language-only option: false */
    opts = apex_options_default();
    opts.highlight_language_only = true;  /* Start enabled */
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("false");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, false, "highlight-language-only: false disables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test highlight_language_only option with underscore (alternate key format) */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight_language_only");
    item->value = strdup("1");
    item->next = NULL;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_bool(opts.highlight_language_only, true, "highlight_language_only (underscore): 1 enables language-only mode");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test combined syntax highlighting options */
    opts = apex_options_default();
    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-highlight");
    item->value = strdup("pygments");
    item->next = NULL;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("code-line-numbers");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    item = malloc(sizeof(apex_metadata_item));
    item->key = strdup("highlight-language-only");
    item->value = strdup("true");
    item->next = metadata;
    metadata = item;

    apex_apply_metadata_to_options(metadata, &opts);
    assert_option_string(opts.code_highlighter, "pygments", "Combined: code-highlight set");
    assert_option_bool(opts.code_line_numbers, true, "Combined: code-line-numbers set");
    assert_option_bool(opts.highlight_language_only, true, "Combined: highlight-language-only set");
    apex_free_metadata(metadata);
    metadata = NULL;

    /* Test via YAML front matter in document */
    apex_options yaml_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    char *html;

    /* Note: We can't fully test external highlighting without the tools installed,
     * but we can verify that the options are parsed from YAML metadata correctly.
     * The actual highlighting would require pygments/skylighting to be installed. */

    /* Test that code blocks are rendered when no highlighting tool is available */
    const char *code_doc = "---\ncode-highlight: pygments\n---\n\n```python\nprint('hello')\n```";
    html = apex_markdown_to_html(code_doc, strlen(code_doc), &yaml_opts);
    /* Should have code block regardless of whether pygments is available */
    assert_contains(html, "<pre", "Code block has pre tag");
    assert_contains(html, "<code", "Code block has code tag");
    assert_contains(html, "print", "Code content preserved");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Syntax Highlighting Options Tests", had_failures, false);
}

/**
 * Test ARIA labels and accessibility attributes
 */
