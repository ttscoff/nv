/**
 * Tables Tests
 */

#include "test_helpers.h"
#include "apex/apex.h"
#include "../src/extensions/relaxed_tables.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

void test_advanced_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Advanced Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;  /* Use standard GFM table syntax for these tests */
    char *html;

    /* Test table with caption before table */
    const char *caption_table = "[Table Caption]\n\n| H1 | H2 |\n|----|----|"
                                "\n| C1 | C2 |";
    html = apex_markdown_to_html(caption_table, strlen(caption_table), &opts);
    assert_contains(html, "<table", "Caption table renders");
    assert_contains(html, "<figure", "Caption table wrapped in figure");
    assert_contains(html, "<figcaption>", "Caption has figcaption tag");
    assert_contains(html, "Table Caption", "Caption text is present");
    assert_contains(html, "</figure>", "Caption figure is closed");
    apex_free_string(html);

    /* Test table with caption after table */
    const char *caption_table_after = "| H1 | H2 |\n|----|----|"
                                     "\n| C1 | C2 |\n\n[Table Caption After]";
    html = apex_markdown_to_html(caption_table_after, strlen(caption_table_after), &opts);
    assert_contains(html, "<table", "Caption table after renders");
    assert_contains(html, "<figure", "Caption table after wrapped in figure");
    assert_contains(html, "Table Caption After", "Caption text after is present");
    apex_free_string(html);

    /* Regression: footnote definitions must not be interpreted as table captions.
     * Repro from issue #20: inline table followed by [^1]: ...
     */
    apex_options inline_table_opts = apex_options_default();
    inline_table_opts.enable_marked_extensions = true;
    const char *inline_table_footnote_caption_regression =
        "Lorem ipsum dolor sit amet[^1], fabulas propriae signiferumque in ius.\n\n"
        "<<[Table.csv{;}]\n\n"
        "[^1]: Lorem is a good guy\n";
    html = apex_markdown_to_html(inline_table_footnote_caption_regression,
                                 strlen(inline_table_footnote_caption_regression),
                                 &inline_table_opts);
    assert_not_contains(html, "<figcaption>^1</figcaption>",
                        "Footnote definition is not treated as table caption");
    apex_free_string(html);

    const char *inline_table_linkdef_caption_regression =
        "Lorem ipsum dolor sit amet [link-ref].\n\n"
        "<<[Table.csv{;}]\n\n"
        "[link-ref]: https://example.com\n";
    html = apex_markdown_to_html(inline_table_linkdef_caption_regression,
                                 strlen(inline_table_linkdef_caption_regression),
                                 &inline_table_opts);
    assert_not_contains(html, "<figcaption>link-ref</figcaption>",
                        "Link definition is not treated as table caption");
    apex_free_string(html);

    const char *gfm_table_footnote_caption_regression =
        "Lorem ipsum dolor sit amet[^1], fabulas propriae signiferumque in ius.\n\n"
        "| Lorem         | Dolor   | Sit     |\n"
        "| ------------- | ------- | ------- |\n"
        "| Amet          | Fabulas | Propiae |\n"
        "| Signiferumque | In      | Ius     |\n\n"
        "[^1]: Lorem is a good guy\n";
    html = apex_markdown_to_html(gfm_table_footnote_caption_regression,
                                 strlen(gfm_table_footnote_caption_regression),
                                 &opts);
    assert_contains(html, "<table", "GFM table renders in footnote-caption regression");
    assert_not_contains(html, "<figcaption>^1</figcaption>",
                        "Footnote definition after GFM table is not treated as caption");
    apex_free_string(html);

    /* Regression (#22): caption from inclusion-table section must not leak to nearby tables
     * that do not define their own caption.
     */
    const char *inclusion_caption_leak_regression =
        "**First table**\n\n"
        "| Lorem         | Dolor   | Sit     |\n"
        "| ------------- | ------- | ------- |\n"
        "| Amet          | Fabulas | Propiae |\n"
        "| Signiferumque | In      | Ius     |\n"
        ": My little table caption\n\n"
        "**Second table**\n\n"
        "| Lorem         | Dolor   | Sit     |\n"
        "| ------------- | ------- | ------- |\n"
        "| Amet          | Fabulas | Propiae |\n"
        "| Signiferumque | In      | Ius     |\n\n"
        "**Inclusion Table**\n\n"
        "<<[Table.csv]\n"
        ": Caption for inclusion table\n\n"
        "**Third table**\n\n"
        "| Lorem         | Dolor   | Sit     |\n"
        "| ------------- | ------- | ------- |\n"
        "| Amet          | Fabulas | Propiae |\n"
        "| Signiferumque | In      | Ius     |\n";
    html = apex_markdown_to_html(inclusion_caption_leak_regression,
                                 strlen(inclusion_caption_leak_regression),
                                 &inline_table_opts);
    assert_contains(html, "My little table caption",
                    "Own caption remains attached to first table");
    assert_not_contains(html, "data-caption=\"Caption for inclusion table\"",
                        "Inclusion-table caption does not leak into other table attributes");
    assert_not_contains(html, "<figcaption>Caption for inclusion table</figcaption>",
                        "Inclusion-table caption does not leak into other table figure captions");
    apex_free_string(html);

    /* Regression (#20 comment): <<[file] include must not treat following
     * definition lines as include addresses or table captions.
     */
    const char *include_csv_path = "/tmp/apex_issue20_include_table.csv";
    FILE *include_csv = fopen(include_csv_path, "w");
    if (include_csv) {
        fputs("H1,H2\nA,B\n", include_csv);
        fclose(include_csv);
    }

    char include_table_footnote_def_regression[512];
    snprintf(include_table_footnote_def_regression,
             sizeof(include_table_footnote_def_regression),
             "<<[%s]\n\n[^1]: Some Footnote Text\n",
             include_csv_path);
    html = apex_markdown_to_html(include_table_footnote_def_regression,
                                 strlen(include_table_footnote_def_regression),
                                 &inline_table_opts);
    assert_contains(html, "<table", "Include-table with real CSV renders");
    assert_not_contains(html, "<figcaption>Some Footnote Text</figcaption>",
                        "Footnote definition after <<[file] is not treated as caption");
    apex_free_string(html);

    char include_table_link_def_regression[512];
    snprintf(include_table_link_def_regression,
             sizeof(include_table_link_def_regression),
             "<<[%s]\n\n[link-ref]: https://example.com\n",
             include_csv_path);
    html = apex_markdown_to_html(include_table_link_def_regression,
                                 strlen(include_table_link_def_regression),
                                 &inline_table_opts);
    assert_contains(html, "<table", "Include-table with link definition renders");
    assert_not_contains(html, "<figcaption>https://example.com</figcaption>",
                        "Link definition after <<[file] is not treated as caption");
    apex_free_string(html);

    remove(include_csv_path);

    /* Test rowspan with ^^ */
    const char *rowspan_table = "| H1 | H2 |\n|----|----|"
                                "\n| A  | B  |"
                                "\n| ^^ | C  |";
    html = apex_markdown_to_html(rowspan_table, strlen(rowspan_table), &opts);
    assert_contains(html, "rowspan", "Rowspan attribute added");
    assert_contains(html, "<td rowspan=\"2\">A</td>", "Rowspan applied to first cell content");
    apex_free_string(html);

    /* Test tfoot support using === separator row.
     * The === row itself should be removed, and subsequent rows should be wrapped in <tfoot>.
     */
    const char *tfoot_table =
        "| H1 | H2 |\n"
        "|----|----|\n"
        "| A  | B  |\n"
        "| C  | D  |\n"
        "| E  | F  |\n"
        "| === | === |\n"
        "| F1 | F2 |\n";
    /* tfoot detection/mapping is more reliable with relaxed tables enabled (unified default). */
    apex_options tfoot_opts = opts;
    tfoot_opts.relaxed_tables = true;
    html = apex_markdown_to_html(tfoot_table, strlen(tfoot_table), &tfoot_opts);
    assert_contains(html, "<tfoot>", "Tfoot: footer section opened");
    assert_contains(html, "F1", "Tfoot: footer content present");
    assert_not_contains(html, "===", "Tfoot: === marker row removed");
    apex_free_string(html);

    /* Test colspan with consecutive pipes (|||) */
    const char *colspan_table = "| H1 | H2 | H3 |\n|----|----|----|"
                                "\n| A  |||"
                                "\n| B  | C  | D  |";
    html = apex_markdown_to_html(colspan_table, strlen(colspan_table), &opts);
    assert_contains(html, "colspan", "Colspan attribute added");
    /* A should span all three columns in the first data row */
    assert_contains(html, "<td colspan=\"3\">A</td>", "Colspan applied to first row A spanning 3 columns with consecutive pipes");
    apex_free_string(html);

    /* Test colspan only when cell contains nothing but << and optional whitespace.
     * A cell with "**<<**" or "raw <<" must NOT be colspan; only bare "<<" (or " << ") is.
     */
    const char *colspan_only_bare =
        "| H1 | H2 | H3 |\n"
        "|----|----|----|\n"
        "| A  | B  | << |\n"
        "| C  | D  | E |\n";
    html = apex_markdown_to_html(colspan_only_bare, strlen(colspan_only_bare), &opts);
    assert_contains(html, "colspan", "Colspan only when cell is just << (and optional whitespace)");
    apex_free_string(html);

    const char *header_colspan_ltlt =
        "| A | Wide | << | D |\n"
        "| --- | --- | --- | --- |\n"
        "| a | b | c | d |\n";
    html = apex_markdown_to_html(header_colspan_ltlt, strlen(header_colspan_ltlt), &opts);
    assert_contains(html, "<th>A</th>", "Header colspan (<<): cell before span kept");
    assert_contains(html, "<th colspan=\"2\">Wide</th>", "Header colspan (<<): spanning cell kept with colspan");
    assert_contains(html, "<th>D</th>", "Header colspan (<<): cell after span kept");
    assert_not_contains(html, "&lt;&lt;", "Header colspan (<<): marker cell removed");
    apex_free_string(html);

    const char *header_colspan_pipes =
        "| Wide || C |\n"
        "| --- | --- | --- |\n"
        "| a | b | c |\n";
    html = apex_markdown_to_html(header_colspan_pipes, strlen(header_colspan_pipes), &opts);
    assert_contains(html, "<th colspan=\"2\">Wide</th>", "Header colspan (||): spanning cell kept with colspan");
    assert_contains(html, "<th>C</th>", "Header colspan (||): following cell kept");
    apex_free_string(html);

    const char *header_then_rowspan =
        "| A | B |\n"
        "| --- | --- |\n"
        "| x | y |\n"
        "| ^^ | z |\n";
    html = apex_markdown_to_html(header_then_rowspan, strlen(header_then_rowspan), &opts);
    assert_contains(html, "<th>A</th>", "Rowspan below header: header cell A kept");
    assert_contains(html, "<th>B</th>", "Rowspan below header: header cell B kept");
    assert_contains(html, "rowspan=\"2\"", "Rowspan below header: rowspan applied");
    apex_free_string(html);

    const char *inline_markers =
        "| H1 | H2 |\n"
        "| --- | --- |\n"
        "| x | a << b |\n"
        "| y | c ^^ d |\n";
    html = apex_markdown_to_html(inline_markers, strlen(inline_markers), &opts);
    assert_contains(html, "<td>a &lt;&lt; b</td>", "Text containing << is not a colspan marker");
    assert_contains(html, "<td>c ^^ d</td>", "Text containing ^^ is not a rowspan marker");
    apex_free_string(html);

    /* Test that empty cells with whitespace do NOT create colspan */
    const char *empty_cells_table = "| H1 | H2 | H3 |\n|----|----|----|"
                                    "\n| A  |    |    |"
                                    "\n| B  | C  | D  |";
    html = apex_markdown_to_html(empty_cells_table, strlen(empty_cells_table), &opts);
    assert_contains(html, "<td>A</td>", "Empty cells table: A cell present");
    assert_contains(html, "<td></td>", "Empty cells table: empty cell present (not merged)");
    assert_not_contains(html, "colspan", "Empty cells table: no colspan attribute (empty cells don't create colspan)");
    apex_free_string(html);

    /* Blank cells use spaced pipes (| |); colspan only for consecutive pipes (||).
     * A later full-width colspan row must not steal blanks from earlier indent rows. */
    const char *blank_then_colspan =
        "|:-|:-|:-|:-|:-|\n"
        "|a|b|c|d|e|\n"
        "| | |x|y|z|\n"
        "| | |p|q|r|\n"
        "|full|||||\n";
    html = apex_markdown_to_html(blank_then_colspan, strlen(blank_then_colspan), &opts);
    assert_contains(html, "<td colspan=\"5\">full</td>", "Blank then colspan: full-width colspan on last row");
    assert_contains(html, ">x</td>", "Blank then colspan: indent cell x present");
    assert_contains(html, ">p</td>", "Blank then colspan: indent cell p present");
    /* First indent row should keep two leading blank cells, not a colspan on an empty cell */
    assert_not_contains(html, "<td colspan=\"5\"></td>", "Blank then colspan: no empty-cell colspan on indent rows");
    assert_not_contains(html, "<th colspan=\"5\"></th>", "Blank then colspan: no empty th colspan on indent rows");
    apex_free_string(html);

    /* Spaced empty cell between content stays a blank column, not a span */
    const char *spaced_blank_between =
        "| H1 | H2 | H3 |\n"
        "|----|----|----|\n"
        "| A | | B |\n";
    html = apex_markdown_to_html(spaced_blank_between, strlen(spaced_blank_between), &opts);
    assert_contains(html, "<td>A</td>", "Spaced blank between: A present");
    assert_contains(html, "<td></td>", "Spaced blank between: blank cell present");
    assert_contains(html, "<td>B</td>", "Spaced blank between: B present");
    assert_not_contains(html, "colspan", "Spaced blank between: no colspan from | |");
    apex_free_string(html);

    /* Test dash-only separator row removal (rows containing only — should be removed). */
    const char *dash_row =
        "| H1 | H2 |\n"
        "|----|----|\n"
        "| A  | B  |\n"
        "| —  | —  |\n"
        "| C  | D  |\n";
    html = apex_markdown_to_html(dash_row, strlen(dash_row), &opts);
    assert_contains(html, "<td>A</td>", "Dash row: first row present");
    assert_contains(html, "<td>C</td>", "Dash row: row after dash separator present");
    assert_not_contains(html, "<td>—</td>", "Dash row: em-dash-only row removed");
    apex_free_string(html);

    /* Test per-cell alignment marker parsing in cell content:
     * these markers should be stripped from the rendered cell content.
     * (Alignment styling is handled in HTML postprocessing; here we assert the markers don't leak.)
     */
    apex_options cell_align_opts = opts;
    cell_align_opts.enable_emoji_autocorrect = false; /* Avoid :x: / :X: emoji autocorrect interference */
    const char *cell_align =
        "| H1 | H2 | H3 |\n"
        "|----|----|----|\n"
        "| :L | :X: | R: |\n";
    html = apex_markdown_to_html(cell_align, strlen(cell_align), &cell_align_opts);
    assert_not_contains(html, ":L", "Cell alignment marker: leading colon stripped");
    assert_not_contains(html, ":X:", "Cell alignment marker: both colons stripped");
    assert_not_contains(html, "R:", "Cell alignment marker: trailing colon stripped");
    assert_contains(html, "<td>L</td>", "Cell alignment marker: left content preserved");
    assert_contains(html, "<td>X</td>", "Cell alignment marker: center content preserved");
    assert_contains(html, "<td>R</td>", "Cell alignment marker: right content preserved");
    apex_free_string(html);

    /* Test per-cell alignment using colons */
    const char *align_table = "| h1  |  h2   | h3  |\n"
                              "| --- | :---: | --- |\n"
                              "| d1  |  d2   | d3  |";
    html = apex_markdown_to_html(align_table, strlen(align_table), &opts);
    /* cmark-gfm uses align=\"left|center|right\" attributes rather than inline styles */
    assert_contains(html, "<th>h1</th>", "Left-aligned header from colon pattern");
    /* Accept either align="center" or style="text-align: center" */
    bool has_align = strstr(html, "<th align=\"center\">h2</th") != NULL;
    bool has_style = strstr(html, "<th style=\"text-align: center\">h2</th") != NULL;
    if (has_align || has_style) {
        test_result(true, "Center-aligned header from colon pattern");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Center-aligned header from colon pattern\n");
        printf("  Looking for: <th align=\"center\">h2</th> or <th style=\"text-align: center\">h2</th>\n");
        printf("  In:          %s\n", html);
    }
    apex_free_string(html);

    /* Test basic table (ensure we didn't break existing functionality) */
    const char *basic_table = "| H1 | H2 |\n|-----|-----|\n| C1 | C2 |";
    html = apex_markdown_to_html(basic_table, strlen(basic_table), &opts);
    assert_contains(html, "<table>", "Basic table still works");
    assert_contains(html, "<th>H1</th>", "Table header");
    assert_contains(html, "<td>C1</td>", "Table cell");
    apex_free_string(html);

    /* Test row header column when first header cell is empty */
    const char *row_header_table =
        "|   | H1 | H2 |\n"
        "|----|----|----|\n"
        "| Row 1 | A1 | B1 |\n"
        "| Row 2 | A2 | B2 |";
    html = apex_markdown_to_html(row_header_table, strlen(row_header_table), &opts);
    assert_contains(html, "<table>", "Row-header table renders");
    /* Check for th with scope="row" - the first column cells in tbody should be converted to row headers */
    /* Check for the pattern: <th scope="row"> followed by Row 1 or Row 2 */
    bool has_row_scope_1 = (strstr(html, "<th scope=\"row\">Row 1</th>") != NULL) ||
                           (strstr(html, "scope=\"row\">Row 1") != NULL);
    bool has_row_scope_2 = (strstr(html, "<th scope=\"row\">Row 2</th>") != NULL) ||
                           (strstr(html, "scope=\"row\">Row 2") != NULL);
    if (has_row_scope_1 && has_row_scope_2) {
        test_result(true, "Row-header table: first row header cell");
        test_result(true, "Row-header table: second row header cell");
    } else {
        tests_failed += 2;
        tests_run += 2;
        printf(COLOR_RED "✗" COLOR_RESET " Row-header table: first row header cell\n");
        printf(COLOR_RED "✗" COLOR_RESET " Row-header table: second row header cell\n");
        printf("  Looking for: <th scope=\"row\">Row 1</th> and <th scope=\"row\">Row 2</th>\n");
        printf("  In:          %s\n", html);
    }
    assert_contains(html, "<td>A1</td>", "Row-header table: body cell A1");
    apex_free_string(html);

    /* Test table followed by paragraph (regression: last row should not become paragraph) */
    const char *table_with_text = "| H1 | H2 |\n|-----|-----|\n| C1 | C2 |\n| C3 | C4 |\n\nText after.";
    html = apex_markdown_to_html(table_with_text, strlen(table_with_text), &opts);
    assert_contains(html, "<td>C3</td>", "Last table row C3 in table");
    assert_contains(html, "<td>C4</td>", "Last table row C4 in table");
    assert_contains(html, "</table>\n<p>Text after.</p>", "Table properly closed before paragraph");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table */
    const char *colon_caption_before = ": My Table\n\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before, strlen(colon_caption_before), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table has figcaption tag");
    assert_contains(html, "My Table", ": Caption before table text is present");
    assert_contains(html, "<table", "Table with : Caption before renders");
    assert_not_contains(html, "<p>: My Table</p>", ": Caption before table paragraph removed");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table with IAL */
    const char *colon_caption_before_ial = ": My Table {#table-id .highlight}\n\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before_ial, strlen(colon_caption_before_ial), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table with IAL has figcaption tag");
    assert_contains(html, "My Table", ": Caption before table with IAL text is present");
    assert_contains(html, "id=\"table-id\"", ": Caption before table IAL ID applied");
    assert_contains(html, "class=\"highlight\"", ": Caption before table IAL class applied");
    assert_not_contains(html, "<p>: My Table", ": Caption before table with IAL paragraph removed");
    apex_free_string(html);

    /* Test : Caption syntax BEFORE table without blank line */
    const char *colon_caption_before_no_blank = ": Caption No Blank\n| Col1 | Col2 |\n|------|------|\n| A    | B    |";
    html = apex_markdown_to_html(colon_caption_before_no_blank, strlen(colon_caption_before_no_blank), &opts);
    assert_contains(html, "<figcaption>", ": Caption before table no blank has figcaption tag");
    assert_contains(html, "Caption No Blank", ": Caption before table no blank text is present");
    assert_contains(html, "<table", "Table with : Caption before no blank renders");
    apex_free_string(html);

    /* Test Pandoc-style table caption with : Caption syntax AFTER table */
    const char *pandoc_caption = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table";
    html = apex_markdown_to_html(pandoc_caption, strlen(pandoc_caption), &opts);
    assert_contains(html, "<figcaption>", "Pandoc caption has figcaption tag");
    assert_contains(html, "Key value table", "Pandoc caption text is present");
    assert_contains(html, "<table", "Table with Pandoc caption renders");
    apex_free_string(html);

    /* Test Pandoc-style table caption with IAL attributes (Kramdown format) */
    const char *pandoc_caption_ial_kramdown = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table {: #table-id .testing}";
    html = apex_markdown_to_html(pandoc_caption_ial_kramdown, strlen(pandoc_caption_ial_kramdown), &opts);
    assert_contains(html, "<table", "Table with Pandoc caption and IAL renders");
    assert_contains(html, "id=\"table-id\"", "Table IAL ID from caption applied");
    assert_contains(html, "class=\"testing\"", "Table IAL class from caption applied");
    assert_contains(html, "Key value table", "Caption text is present");
    apex_free_string(html);

    /* Test Pandoc-style table caption with IAL attributes (Pandoc format) */
    const char *pandoc_caption_ial_pandoc = "| Key | Value |\n| --- | :---: |\n| one |   1   |\n| two |   2   |\n\n: Key value table {#table-id-2 .testing-2}";
    html = apex_markdown_to_html(pandoc_caption_ial_pandoc, strlen(pandoc_caption_ial_pandoc), &opts);
    assert_contains(html, "<table", "Table with Pandoc caption and Pandoc IAL renders");
    assert_contains(html, "id=\"table-id-2\"", "Table Pandoc IAL ID from caption applied");
    assert_contains(html, "class=\"testing-2\"", "Table Pandoc IAL class from caption applied");
    assert_contains(html, "Key value table", "Caption text is present");
    apex_free_string(html);

    /* Test table with IAL applied directly (not via caption) */
    const char *table_with_direct_ial = "| H1 | H2 |\n|----|----|\n| C1 | C2 |\n{: #direct-table .direct-class}";
    html = apex_markdown_to_html(table_with_direct_ial, strlen(table_with_direct_ial), &opts);
    assert_contains(html, "<table", "Table with direct IAL renders");
    assert_contains(html, "id=\"direct-table\"", "Direct table IAL ID applied");
    assert_contains(html, "class=\"direct-class\"", "Direct table IAL class applied");
    apex_free_string(html);

    /* Test table caption before table with IAL */
    const char *caption_before_ial = "[Caption Before]\n\n| H1 | H2 |\n|----|----|\n| C1 | C2 |\n{: #before-table .before-class}";
    html = apex_markdown_to_html(caption_before_ial, strlen(caption_before_ial), &opts);
    assert_contains(html, "<table", "Table with caption before and IAL renders");
    assert_contains(html, "Caption Before", "Caption text before table");
    assert_contains(html, "id=\"before-table\"", "Table IAL ID with caption before");
    assert_contains(html, "class=\"before-class\"", "Table IAL class with caption before");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Advanced Tables Tests", had_failures, false);
}

/**
 * Test relaxed tables (tables without separator rows)
 */

void test_relaxed_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Relaxed Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = true;
    char *html;

    /* Test basic relaxed table (2 rows, no separator) */
    const char *relaxed_table = "A | B\n1 | 2";
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &opts);
    assert_contains(html, "<table>", "Relaxed table renders");
    assert_contains(html, "<tbody>", "Relaxed table has tbody");
    assert_contains(html, "<tr>", "Relaxed table has rows");
    assert_contains(html, "<td>A</td>", "First cell A");
    assert_contains(html, "<td>B</td>", "First cell B");
    assert_contains(html, "<td>1</td>", "Second cell 1");
    assert_contains(html, "<td>2</td>", "Second cell 2");
    /* Should NOT have a header row */
    if (strstr(html, "<thead>") == NULL && strstr(html, "<th>") == NULL) {
        test_result(true, "Relaxed table has no header row");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Relaxed table incorrectly has header row\n");
    }
    apex_free_string(html);

    /* Test relaxed table with 3 rows */
    const char *relaxed_table3 = "A | B\n1 | 2\n3 | 4";
    html = apex_markdown_to_html(relaxed_table3, strlen(relaxed_table3), &opts);
    assert_contains(html, "<table>", "Relaxed table with 3 rows renders");
    assert_contains(html, "<td>3</td>", "Third row cell 3");
    assert_contains(html, "<td>4</td>", "Third row cell 4");
    apex_free_string(html);

    /* Test relaxed table stops at blank line */
    const char *relaxed_table_blank = "A | B\n1 | 2\n\nParagraph text";
    html = apex_markdown_to_html(relaxed_table_blank, strlen(relaxed_table_blank), &opts);
    assert_contains(html, "<table>", "Relaxed table before blank line");
    assert_contains(html, "<p>Paragraph text</p>", "Paragraph after blank line");
    apex_free_string(html);

    /* Test relaxed table with leading pipe */
    const char *relaxed_table_leading = "| A | B |\n| 1 | 2 |";
    html = apex_markdown_to_html(relaxed_table_leading, strlen(relaxed_table_leading), &opts);
    assert_contains(html, "<table>", "Relaxed table with leading pipe renders");
    assert_contains(html, "<td>A</td>", "Cell A with leading pipe");
    apex_free_string(html);

    /* Pipes inside [[wiki|links]] are not cell separators */
    apex_options wiki_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    wiki_opts.relaxed_tables = true;
    const char *wiki_list = "* A [[Page/Sub|Label]]\n* B [[Other|Label2]]\n\tpara\n";
    html = apex_markdown_to_html(wiki_list, strlen(wiki_list), &wiki_opts);
    assert_not_contains(html, "<table>", "Wiki link pipes in list items do not make a table");
    assert_not_contains(html, "|</li>", "Wiki link pipes do not inject a separator row");
    apex_free_string(html);

    const char *wiki_lines = "See [[A|B]] here\nAnd [[C|D]] there";
    html = apex_markdown_to_html(wiki_lines, strlen(wiki_lines), &wiki_opts);
    assert_not_contains(html, "<table>", "Wiki link pipes in paragraph lines do not make a table");
    apex_free_string(html);

    const char *wiki_cells = "[[Page]] | two\n[[Other]] | four";
    html = apex_markdown_to_html(wiki_cells, strlen(wiki_cells), &wiki_opts);
    assert_contains(html, "<table>", "Relaxed table with wiki links in cells still renders");
    apex_free_string(html);

    /* Test that relaxed tables are disabled by default in GFM mode */
    apex_options gfm_opts = apex_options_for_mode(APEX_MODE_GFM);
    gfm_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &gfm_opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Relaxed tables disabled in GFM mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly enabled in GFM mode");
    }
    apex_free_string(html);

    /* Test that relaxed tables are enabled by default in Kramdown mode */
    apex_options kramdown_opts = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    kramdown_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &kramdown_opts);
    if (strstr(html, "<table>") != NULL) {
        test_result(true, "Relaxed tables enabled in Kramdown mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly disabled in Kramdown mode");
    }
    apex_free_string(html);

    /* Test that relaxed tables are enabled by default in Unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    unified_opts.enable_tables = true;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &unified_opts);
    if (strstr(html, "<table>") != NULL) {
        test_result(true, "Relaxed tables enabled in Unified mode by default");
    } else {
        test_result(false, "Relaxed tables incorrectly disabled in Unified mode");
    }
    apex_free_string(html);

    /* Test that --no-relaxed-tables disables it even in Kramdown mode */
    apex_options no_relaxed = apex_options_for_mode(APEX_MODE_KRAMDOWN);
    no_relaxed.enable_tables = true;
    no_relaxed.relaxed_tables = false;
    html = apex_markdown_to_html(relaxed_table, strlen(relaxed_table), &no_relaxed);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "--no-relaxed-tables disables relaxed tables");
    } else {
        test_result(false, "--no-relaxed-tables did not disable relaxed tables");
    }
    apex_free_string(html);

    /* Test that single row with pipe is not treated as table */
    const char *single_row = "A | B";
    html = apex_markdown_to_html(single_row, strlen(single_row), &opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Single row is not treated as table");
    } else {
        test_result(false, "Single row incorrectly treated as table");
    }
    apex_free_string(html);

    /* Test that rows with different column counts are not treated as table */
    const char *mismatched = "A | B\n1 | 2 | 3";
    html = apex_markdown_to_html(mismatched, strlen(mismatched), &opts);
    if (strstr(html, "<table>") == NULL) {
        test_result(true, "Mismatched column counts are not treated as table");
    } else {
        test_result(false, "Mismatched column counts incorrectly treated as table");
    }
    apex_free_string(html);

    /* Pipes inside fenced code blocks must not trigger relaxed table separators */
    const char *fenced_code_with_pipes =
        "```bash\n"
        "update_widgets() {\n"
        "  echo foo | sed 's/x/y/' | awk '{print $1}'\n"
        "}\n"
        "```";
    html = apex_markdown_to_html(fenced_code_with_pipes, strlen(fenced_code_with_pipes), &opts);
    assert_contains(html, "<code", "Fenced code block with pipes renders as code");
    assert_contains(html, "echo foo | sed", "Code block content with pipes preserved");
    if (strstr(html, "---|---|---|") == NULL) {
        test_result(true, "No relaxed-table separator injected inside fenced code");
    } else {
        test_result(false, "Relaxed-table separator was injected inside fenced code");
    }
    apex_free_string(html);

    /* Mermaid edge labels |Yes| / |No| must not get a GFM separator injected */
    {
        apex_options unified = apex_options_default();
        unified.mode = APEX_MODE_UNIFIED;
        unified.relaxed_tables = true;
        unified.enable_tables = true;
        const char *mermaid_pipes =
            "```mermaid\n"
            "graph TD\n"
            "  A -->|Yes| B\n"
            "  A -->|No| C\n"
            "```\n";
        html = apex_markdown_to_html(mermaid_pipes, strlen(mermaid_pipes), &unified);
        assert_contains(html, "lang=\"mermaid\"", "Mermaid fence keeps language");
        assert_contains(html, "|Yes|", "Mermaid Yes edge label preserved");
        assert_contains(html, "|No|", "Mermaid No edge label preserved");
        assert_not_contains(html, "---|---|---", "No relaxed-table separator inside Mermaid fence");
        apex_free_string(html);
    }

    /* Regression: GFM tables with 2+ body rows must not get a second separator
     * injected after the first body row (that broke following tables, especially
     * when a later row used || colspan syntax). */
    {
        apex_options unified = apex_options_for_mode(APEX_MODE_UNIFIED);
        unified.enable_plugins = false;
        const char *multi_table =
            "| COL1 | COL2 |\n"
            "| ---- | ---- |\n"
            "| CELL | CELL |\n"
            "| BLAH | BLOP |\n"
            "| CELL | CELL |\n"
            "| test | test |\n"
            "[Caption]\n"
            "\n"
            "Table 2:\n"
            "\n"
            "| COL1 | COL2 | COL3 |\n"
            "| ---- | ---- | ---- |\n"
            "| CELL || CELL |\n"
            "\n"
            "Table 3:\n"
            "\n"
            "| COL1 | COL2 | COL3 | COL4 |\n"
            "| ---- | ---- | ---- | ---- |\n"
            "| CELL | CELL | CELL | CELL |\n";
        html = apex_markdown_to_html(multi_table, strlen(multi_table), &unified);
        assert_contains(html, "colspan", "Multi-row table then colspan row still merges");
        assert_contains(html, "<td>CELL</td>", "Table 3 body cells render");
        assert_not_contains(html, "<p>| CELL", "Following table body rows are not leaked as paragraphs");
        assert_contains(html, "Caption", "First table caption still present");
        apex_free_string(html);

        /* Preprocess must not rewrite already-separated GFM tables */
        const char *gfm_two_body =
            "| H1 | H2 |\n"
            "| ---- | ---- |\n"
            "| A | B |\n"
            "| C | D |\n";
        char *pre = apex_process_relaxed_tables(gfm_two_body);
        if (pre == NULL) {
            test_result(true, "GFM table with separator is left unchanged by relaxed preprocess");
        } else {
            test_result(false, "GFM table with separator was rewritten by relaxed preprocess");
            free(pre);
        }
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Relaxed Tables Tests", had_failures, false);
}

/**
 * Test combine-like behavior for GitBook SUMMARY.md via core API
 * (indirectly validates that include expansion and ordering work).
 */

void test_comprehensive_table_features(void) {
    int suite_failures = suite_start();
    print_suite_title("Comprehensive Test File Table Features", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    char *html = NULL;

    /* Read comprehensive_test.md file */
    FILE *f = fopen("tests/fixtures/comprehensive_test.md", "r");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " comprehensive_test.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: Caption before table with IAL should render correctly */
    /* The caption "Employee Performance Q4 2025" should appear in figcaption, not as a paragraph */
    /* Note: Caption may appear as <p> if not properly detected, so check for either format */
    bool has_figcaption = strstr(html, "<figcaption>Employee Performance Q4 2025</figcaption>") != NULL;
    bool has_caption_para = strstr(html, "<p>[Employee Performance Q4 2025]</p>") != NULL;
    if (has_figcaption) {
        test_result(true, "Caption appears in figcaption tag");
        /* If in figcaption, it should NOT be in paragraph */
        if (!has_caption_para) {
            test_result(true, "Caption paragraph removed (no duplicate)");
        } else {
            tests_failed++;
            tests_run++;
            printf(COLOR_RED "✗" COLOR_RESET " Caption paragraph removed (no duplicate)\n");
            printf("  Caption found in both figcaption and paragraph\n");
        }
    } else if (has_caption_para) {
        /* Caption not in figcaption - this might be expected if caption detection isn't working */
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Caption appears in figcaption tag\n");
        printf("  Caption found as paragraph instead of figcaption\n");
    } else {
        /* Caption not found at all */
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Caption appears in figcaption tag\n");
        printf("  Caption 'Employee Performance Q4 2025' not found in output\n");
    }

    /* Test 3: Rowspan should be applied correctly - Engineering rowspan="2" */
    /* Note: Rowspan detection may require the ^^ marker to be in the correct cell position */
    assert_contains(html, "rowspan=\"2\"", "Rowspan attribute present");
    /* Check for Engineering with rowspan - may have alignment attribute */
    bool engineering_has_rowspan = strstr(html, "<td rowspan=\"2\">Engineering</td>") != NULL ||
                                   strstr(html, "<td align=\"right\" rowspan=\"2\">Engineering</td>") != NULL ||
                                   strstr(html, "rowspan=\"2\">Engineering") != NULL;
    if (engineering_has_rowspan) {
        test_result(true, "Engineering has rowspan=2");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Engineering has rowspan=2\n");
        printf("  Looking for: <td rowspan=\"2\">Engineering</td> or <td align=\"right\" rowspan=\"2\">Engineering</td>\n");
        printf("  In:          %s\n", html);
    }

    /* Test 4: Rowspan should be applied correctly - Sales rowspan="2" */
    /* Check for Sales with rowspan - may have alignment attribute */
    bool sales_has_rowspan = strstr(html, "<td rowspan=\"2\">Sales</td>") != NULL ||
                             strstr(html, "<td align=\"right\" rowspan=\"2\">Sales</td>") != NULL ||
                             strstr(html, "rowspan=\"2\">Sales") != NULL;
    if (sales_has_rowspan) {
        test_result(true, "Sales has rowspan=2");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Sales has rowspan=2\n");
        printf("  Looking for: <td rowspan=\"2\">Sales</td> or <td align=\"right\" rowspan=\"2\">Sales</td>\n");
        printf("  In:          %s\n", html);
    }

    /* Test 5: Table should be wrapped in figure tag */
    assert_contains(html, "<figure class=\"table-figure\">", "Table wrapped in figure with class");

    /* Test 6: Empty cells are preserved (Absent cell followed by empty cells) */
    /* The comprehensive test shows: | Marketing | Charlie | Absent | | | 92.00 | */
    /* This means: Marketing, Charlie, Absent, then empty cells, then 92.00 */
    /* Empty cells with whitespace should NOT create colspan, they should remain as empty cells */
    assert_contains(html, "<td>Absent</td>", "Absent cell present");
    /* Check for empty cell after Absent - empty cells should remain as <td></td> not merged */
    bool has_empty_after_absent = strstr(html, "<td>Absent</td><td></td>") != NULL ||
                                   strstr(html, "<td>Absent</td>\n<td></td>") != NULL ||
                                   (strstr(html, "<td>Absent</td>") != NULL && strstr(html, "<td></td>") != NULL);
    if (has_empty_after_absent) {
        test_result(true, "Empty cell present in table");
    } else {
        tests_failed++;
        tests_run++;
        printf(COLOR_RED "✗" COLOR_RESET " Empty cell present in table\n");
        printf("  Looking for: <td>Absent</td> followed by <td></td> (empty cells don't create colspan)\n");
        printf("  In:          %s\n", html);
    }

    /* Test 7: Table structure should be correct - key rows present */
    assert_contains(html, "<td>Alice</td>", "Alice row present");
    assert_contains(html, "<td>Bob</td>", "Bob row present");
    assert_contains(html, "<td>Charlie</td>", "Charlie row present");
    assert_contains(html, "<td>Diana</td>", "Diana row present");
    /* Eve is in the last row with rowspan */
    assert_contains(html, "Eve", "Eve row present");

    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Comprehensive Test File Table Features", had_failures, false);
}

/**
 * Test that table parsing works correctly when file doesn't end with a newline.
 * This ensures the last row of the first table is properly parsed and not rendered as text.
 */
void test_table_no_trailing_newline(void) {
    int suite_failures = suite_start();
    print_suite_title("Table No Trailing Newline Test", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;
    char *html = NULL;

    /* Read table_no_trailing_newline.md file (which intentionally doesn't end with newline) */
    FILE *f = fopen("tests/fixtures/tables/table_no_trailing_newline.md", "r");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Verify file doesn't end with newline (key part of the test) */
    if (bytes_read > 0 && (markdown[bytes_read - 1] == '\n' || markdown[bytes_read - 1] == '\r')) {
        free(markdown);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: File ends with newline (should not)\n");
        return;
    }

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_no_trailing_newline.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: First table's last row should be parsed correctly - A and B should be in table cells */
    assert_contains(html, "<td>A</td>", "First table last row: A cell parsed correctly");
    assert_contains(html, "<td>B</td>", "First table last row: B cell parsed correctly");

    /* Test 2: The last row should NOT appear as raw text like "| A | B |" */
    assert_not_contains(html, "<p>| A    | B    |</p>", "First table last row: not rendered as paragraph text");
    assert_not_contains(html, "<p>| A | B |</p>", "First table last row: not rendered as paragraph text (alternate format)");

    /* Test 3: First table should be properly closed before the caption */
    assert_contains(html, "</table>", "First table properly closed");
    assert_contains(html, "<figcaption>", "Caption present after first table");

    /* Test 4: Second table should also be parsed correctly */
    assert_contains(html, "<table", "Second table renders");
    assert_contains(html, "My Table", "Caption text present");

    /* Test 5: Both tables should have their data rows */
    /* Count occurrences of table cells - should have at least 4 (2 tables * 2 cells each) */
    int td_count = 0;
    const char *td_pos = html;
    while ((td_pos = strstr(td_pos, "<td>")) != NULL) {
        td_count++;
        td_pos += 4;
    }
    if (td_count >= 4) {
        test_resultf(true, "Both tables have all data rows parsed (found %d cells)", td_count);
    } else {
        test_resultf(false, "Both tables should have all data rows (found %d cells, expected at least 4)", td_count);
    }

    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Table No Trailing Newline Test", had_failures, false);
}

/**
 * Test that table parsing works correctly when file uses CR line endings.
 * This ensures Table: Caption syntax is processed correctly with CR line endings.
 */
void test_table_cr_line_endings(void) {
    int suite_failures = suite_start();
    print_suite_title("Table CR Line Endings Test", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.relaxed_tables = false;
    char *html = NULL;

    /* Read table_cr_line_endings.md file (which intentionally uses CR line endings) */
    FILE *f = fopen("tests/fixtures/tables/table_cr_line_endings.md", "rb");
    if (!f) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Could not open file\n");
        return;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content as binary to preserve CR characters */
    char *markdown = (char *)malloc(file_size + 1);
    if (!markdown) {
        fclose(f);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Memory allocation failed\n");
        return;
    }

    size_t bytes_read = fread(markdown, 1, file_size, f);
    markdown[bytes_read] = '\0';
    fclose(f);

    /* Verify file uses CR line endings (key part of the test) */
    bool has_cr = false;
    for (size_t i = 0; i < bytes_read; i++) {
        if (markdown[i] == '\r') has_cr = true;
    }
    if (!has_cr) {
        free(markdown);
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: File does not use CR line endings (should not)\n");
        return;
    }

    /* Convert to HTML */
    html = apex_markdown_to_html(markdown, bytes_read, &opts);
    free(markdown);

    if (!html) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " table_cr_line_endings.md: Failed to convert to HTML\n");
        return;
    }

    /* Test 1: First table should be parsed correctly */
    assert_contains(html, "<td>A</td>", "CR line endings: First table last row A cell parsed correctly");
    assert_contains(html, "<td>B</td>", "CR line endings: First table last row B cell parsed correctly");

    /* Test 2: Table: Caption should be processed correctly (not rendered as paragraph) */
    assert_contains(html, "<figcaption>", "CR line endings: Table: Caption has figcaption tag");
    assert_contains(html, "My Table", "CR line endings: Table: Caption text is present");
    assert_contains(html, "table-id2", "CR line endings: Table: Caption IAL ID applied");

    /* Test 3: Table: Caption should NOT appear as raw text like "<p>Table: My Table" */
    assert_not_contains(html, "<p>Table: My Table", "CR line endings: Table: Caption not rendered as paragraph text");

    /* Test 4: : Caption syntax should also work with CR line endings */
    assert_contains(html, "table-id", "CR line endings: : Caption IAL ID applied");

    /* Test 5: Both tables should be properly rendered */
    int table_count = 0;
    const char *table_pos = html;
    while ((table_pos = strstr(table_pos, "<table")) != NULL) {
        table_count++;
        table_pos += 6;
    }
    if (table_count >= 2) {
        test_resultf(true, "CR line endings: Both tables rendered correctly (found %d tables)", table_count);
    } else {
        test_resultf(false, "CR line endings: Both tables should be rendered (found %d tables, expected at least 2)", table_count);
    }

    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Table CR Line Endings Test", had_failures, false);
}

/**
 * Test callouts (Bear/Obsidian/Xcode)
 */

void test_inline_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Inline Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_marked_extensions = true;
    char *html;

    /* ```table fence with CSV data */
    const char *csv_table =
        "```table\n"
        "header 1,header 2,header 3\n"
        "data 1,data 2,data 3\n"
        ",,data 2c\n"
        "```\n";
    html = apex_markdown_to_html(csv_table, strlen(csv_table), &opts);
    assert_contains(html, "<table>", "CSV table fence: table element");
    assert_contains(html, "<th>header 1</th>", "CSV table fence: header 1");
    assert_contains(html, "<th>header 2</th>", "CSV table fence: header 2");
    assert_contains(html, "<th>header 3</th>", "CSV table fence: header 3");
    assert_contains(html, "<td>data 1</td>", "CSV table fence: first data cell");
    assert_contains(html, "<td>data 2c</td>", "CSV table fence: continued cell");
    apex_free_string(html);

    /* ```table fence with CSV data and alignment keywords */
    const char *csv_align =
        "```table\n"
        "H1,H2,H3\n"
        "left,center,right\n"
        "a,b,c\n"
        "```\n";
    html = apex_markdown_to_html(csv_align, strlen(csv_align), &opts);
    assert_contains(html, "<table>", "CSV table with alignment: table element");
    /* Be conservative about HTML structure: just verify content appears in a table */
    assert_contains(html, "H1", "CSV table with alignment: header text H1 present");
    assert_contains(html, "H2", "CSV table with alignment: header text H2 present");
    assert_contains(html, "H3", "CSV table with alignment: header text H3 present");
    assert_contains(html, "a", "CSV table with alignment: data 'a' present");
    apex_free_string(html);

    /* ```table fence with Markdown-style alignment (:--, --:, :--:) */
    const char *csv_align_colon_dash =
        "```table\n"
        "A,B,C\n"
        ":--,--:,:--:\n"
        "x,y,z\n"
        "```\n";
    html = apex_markdown_to_html(csv_align_colon_dash, strlen(csv_align_colon_dash), &opts);
    assert_contains(html, "<table>", "CSV table colon-dash alignment: table element");
    assert_contains(html, "A", "CSV table colon-dash alignment: header A");
    assert_contains(html, "x", "CSV table colon-dash alignment: data x");
    apex_free_string(html);

    /* ```table fence with no explicit alignment row: should also be headless */
    const char *csv_no_align =
        "```table\n"
        "r1c1,r1c2,r1c3\n"
        "r2c1,r2c2,r2c3\n"
        "```\n";
    html = apex_markdown_to_html(csv_no_align, strlen(csv_no_align), &opts);
    assert_contains(html, "<table>", "CSV table no-align: table element");
    assert_contains(html, "r1c1", "CSV table no-align: first row content present");
    assert_contains(html, "r2c1", "CSV table no-align: second row content present");
    apex_free_string(html);

    /* ```table fence with TSV data (real tabs) */
    const char *tsv_table =
        "```table\n"
        "col1\tcol2\tcol3\n"
        "val1\tval2\tval3\n"
        "```\n";
    html = apex_markdown_to_html(tsv_table, strlen(tsv_table), &opts);
    assert_contains(html, "<table>", "TSV table fence: table element");
    assert_contains(html, "col1", "TSV table fence: header col1 text");
    assert_contains(html, "col2", "TSV table fence: header col2 text");
    assert_contains(html, "col3", "TSV table fence: header col3 text");
    assert_contains(html, "val1", "TSV table fence: first data value");
    apex_free_string(html);

    /* ```table fence with no delimiter: should remain a code block */
    const char *no_delim =
        "```table\n"
        "this has no delimiters\n"
        "on the second line\n"
        "```\n";
    html = apex_markdown_to_html(no_delim, strlen(no_delim), &opts);
    assert_contains(html, "<pre lang=\"table\"><code>", "No-delim table fence: rendered as code block");
    assert_contains(html, "this has no delimiters", "No-delim table fence: content preserved");
    apex_free_string(html);

    /* <!--TABLE--> with CSV data */
    const char *csv_marker =
        "<!--TABLE-->\n"
        "one,two,three\n"
        "four,five,six\n"
        "\n";
    html = apex_markdown_to_html(csv_marker, strlen(csv_marker), &opts);
    assert_contains(html, "<table>", "CSV TABLE marker: table element");
    assert_contains(html, "one", "CSV TABLE marker: header text");
    assert_contains(html, "four", "CSV TABLE marker: data value");
    apex_free_string(html);

    /* <!--TABLE--> with TSV data (real tabs) */
    const char *tsv_marker =
        "<!--TABLE-->\n"
        "alpha\tbeta\tgamma\n"
        "delta\tepsilon\tzeta\n"
        "\n";
    html = apex_markdown_to_html(tsv_marker, strlen(tsv_marker), &opts);
    assert_contains(html, "<table>", "TSV TABLE marker: table element");
    assert_contains(html, "alpha", "TSV TABLE marker: header text");
    assert_contains(html, "delta", "TSV TABLE marker: data value");
    apex_free_string(html);

    /* <!--TABLE--> with no following data: comment should be preserved */
    const char *empty_marker =
        "Before\n\n"
        "<!--TABLE-->\n"
        "\n"
        "After\n";
    html = apex_markdown_to_html(empty_marker, strlen(empty_marker), &opts);
    assert_contains(html, "Before", "Empty TABLE marker: before text preserved");
    assert_contains(html, "<!--TABLE-->", "Empty TABLE marker: comment preserved");
    assert_contains(html, "After", "Empty TABLE marker: after text preserved");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Inline Tables Tests", had_failures, false);
}

void test_grid_tables(void) {
    int suite_failures = suite_start();
    print_suite_title("Grid Tables Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_tables = true;
    opts.enable_grid_tables = true;
    opts.relaxed_tables = false;
    char *html;

    /* Test basic grid table */
    const char *basic_grid = "+---+---+\n"
                             "| H1 | H2 |\n"
                             "+===+===+\n"
                             "| C1 | C2 |\n"
                             "+---+---+";
    html = apex_markdown_to_html(basic_grid, strlen(basic_grid), &opts);
    assert_contains(html, "<table", "Basic grid table renders");
    assert_contains(html, "<th>H1</th>", "Grid table header");
    assert_contains(html, "<td>C1</td>", "Grid table cell");
    apex_free_string(html);

    /* Test grid table with header separator */
    const char *header_sep_grid = "+---+---+\n"
                                   "| H1 | H2 |\n"
                                   "+===+===+\n"
                                   "| C1 | C2 |\n"
                                   "+---+---+";
    html = apex_markdown_to_html(header_sep_grid, strlen(header_sep_grid), &opts);
    assert_contains(html, "<table", "Grid table with header separator renders");
    assert_contains(html, "<th>H1</th>", "Header separator table has header");
    apex_free_string(html);

    /* Test grid table with alignment */
    const char *aligned_grid = "+:---+---+---:+\n"
                                "| C  | L  |  R |\n"
                                "+===+===+===+\n"
                                "| A  | B  |  C |\n"
                                "+---+---+---+";
    html = apex_markdown_to_html(aligned_grid, strlen(aligned_grid), &opts);
    assert_contains(html, "<table", "Aligned grid table renders");
    apex_free_string(html);

    /* Test grid table with footer separator */
    const char *footer_grid = "+---+---+\n"
                              "| H1 | H2 |\n"
                              "+---+---+\n"
                              "| C1 | C2 |\n"
                              "+===+===+\n"
                              "| F1 | F2 |\n"
                              "+---+---+";
    html = apex_markdown_to_html(footer_grid, strlen(footer_grid), &opts);
    assert_contains(html, "<table", "Grid table with footer renders");
    apex_free_string(html);

    /* Test multi-line cells in grid table */
    const char *multiline_grid = "+---+---+\n"
                                  "| H1 | H2 |\n"
                                  "+===+===+\n"
                                  "| Line 1 | Cell 2 |\n"
                                  "| Line 2 |        |\n"
                                  "+---+---+";
    html = apex_markdown_to_html(multiline_grid, strlen(multiline_grid), &opts);
    assert_contains(html, "<table", "Multi-line grid table renders");
    assert_contains(html, "Line 1", "Multi-line cell content preserved");
    assert_contains(html, "Line 2", "Multi-line cell second line preserved");
    apex_free_string(html);

    /* Test grid table with caption (before) */
    const char *caption_before = "[Grid Table Caption]\n\n"
                                 "+---+---+\n"
                                 "| H1 | H2 |\n"
                                 "+===+===+\n"
                                 "| C1 | C2 |\n"
                                 "+---+---+";
    html = apex_markdown_to_html(caption_before, strlen(caption_before), &opts);
    assert_contains(html, "<table", "Grid table with caption before renders");
    assert_contains(html, "<figure", "Grid table with caption wrapped in figure");
    assert_contains(html, "Grid Table Caption", "Caption text present");
    apex_free_string(html);

    /* Test grid table with caption (after) */
    const char *caption_after = "+---+---+\n"
                                "| H1 | H2 |\n"
                                "+===+===+\n"
                                "| C1 | C2 |\n"
                                "+---+---+\n\n"
                                "[Grid Table Caption After]";
    html = apex_markdown_to_html(caption_after, strlen(caption_after), &opts);
    assert_contains(html, "<table", "Grid table with caption after renders");
    assert_contains(html, "Grid Table Caption After", "Caption text after present");
    apex_free_string(html);

    /* Test grid table in code block (should not be parsed) */
    const char *code_block_grid = "```\n"
                                  "+---+---+\n"
                                  "| H1 | H2 |\n"
                                  "+===+===+\n"
                                  "| C1 | C2 |\n"
                                  "+---+---+\n"
                                  "```";
    html = apex_markdown_to_html(code_block_grid, strlen(code_block_grid), &opts);
    assert_not_contains(html, "<table", "Grid table in code block not parsed");
    assert_contains(html, "<code", "Code block preserved");
    apex_free_string(html);

    /* Test mixed grid and pipe tables */
    const char *mixed_tables = "+---+---+\n"
                               "| G1 | G2 |\n"
                               "+===+===+\n"
                               "| GC1 | GC2 |\n"
                               "+---+---+\n\n"
                               "| P1 | P2 |\n"
                               "|----|----|\n"
                               "| PC1 | PC2 |";
    html = apex_markdown_to_html(mixed_tables, strlen(mixed_tables), &opts);
    assert_contains(html, "<table", "Mixed tables render");
    assert_contains(html, "G1", "Grid table content present");
    assert_contains(html, "P1", "Pipe table content present");
    apex_free_string(html);

    /* Test grid table disabled */
    opts.enable_grid_tables = false;
    html = apex_markdown_to_html(basic_grid, strlen(basic_grid), &opts);
    apex_free_string(html);

    /* Complex colspan row spanning full table width (no closing table separator) */
    opts.enable_grid_tables = true;
    opts.enable_tables = true;
    const char *colspan_grid = "+---+---+\n"
                               "| H1 | H2 |\n"
                               "+===+===+\n"
                               "| C1 | C2 |\n"
                               "+---+---+\n"
                               "| Spans both columns here             |\n";
    html = apex_markdown_to_html(colspan_grid, strlen(colspan_grid), &opts);
    assert_contains(html, "colspan=\"2\"", "Colspan row spans both columns");
    assert_contains(html, "Spans both columns", "Colspan cell content preserved");
    apex_free_string(html);

    /* Nested grid inside colspan cell stays in one cell */
    const char *nested_in_colspan = "+---+---+\n"
                                    "| A | B |\n"
                                    "+===+===+\n"
                                    "| Wide cell                           |\n"
                                    "+---+---+---+\n"
                                    "| n1 | n2 | n3 |\n"
                                    "+---+---+---+";
    html = apex_markdown_to_html(nested_in_colspan, strlen(nested_in_colspan), &opts);
    assert_contains(html, "colspan=\"2\"", "Nested grid parent has colspan");
    assert_contains(html, "Wide cell", "Wide cell text preserved");
    assert_not_contains(html, "<tr>\n<td>n1</td>\n<td>n2</td>", "Nested grid not separate top-level row");
    apex_free_string(html);

    /* Fixture table 1: colspan + nested grid stays inside the table, not a stray paragraph */
    const char *fixture_colspan_nested = "+-------------------+-------------------+\n"
        "| Grid Tables       | Are Beautiful     |\n"
        "+===================+===================+\n"
        "| Easy to read      | In code and docs  |\n"
        "+-------------------+-------------------+\n"
        "| Exceptionally flexible and powerful   |\n"
        "+-------+-------+-------+-------+-------+\n"
        "| Col 1 | Col 2 | Col 3 | Col 4 | Col 5 |\n"
        "+-------+-------+-------+-------+-------+\n";
    html = apex_markdown_to_html(fixture_colspan_nested, strlen(fixture_colspan_nested), &opts);
    assert_contains(html, "colspan=\"2\"", "Fixture colspan row in table");
    assert_contains(html, "Exceptionally flexible and powerful", "Fixture colspan text in output");
    assert_not_contains(html, "</table>\n<p>|", "Colspan row not leaked as paragraph after table");
    assert_contains(html, "Col 1", "Nested grid Col 1 inside colspan cell");
    assert_contains(html, "Col 5", "Nested grid Col 5 inside colspan cell");
    apex_free_string(html);

    /* Partial separator table (fixture table 2 pattern) */
    const char *partial_grid = "+---------------------+----------+\n"
        "| Property            | Earth    |\n"
        "+=============+=======+==========+\n"
        "|             | min   | -89.2 °C |\n"
        "| Temperature +-------+----------+\n"
        "| 1961-1990   | mean  | 14 °C    |\n"
        "|             +-------+----------+\n"
        "|             | min   | 56.7 °C  |\n"
        "+-------------+-------+----------+\n";
    html = apex_markdown_to_html(partial_grid, strlen(partial_grid), &opts);
    assert_contains(html, "colspan=\"2\"", "Partial table Property spans two columns");
    assert_contains(html, "rowspan=\"3\"", "Partial table Temperature rowspan");
    assert_contains(html, "14 °C", "Partial table mean temperature");
    apex_free_string(html);

    /* Multiline list cells */
    const char *list_cells = "+---+---+---+\n"
                             "| F | P | Notes |\n"
                             "+===+===+===+\n"
                             "| Ban | $1 | - one |\n"
                             "|     |    | - two |\n"
                             "+---+---+---+";
    html = apex_markdown_to_html(list_cells, strlen(list_cells), &opts);
    assert_contains(html, "<ul>", "List cell renders as ul");
    assert_contains(html, "<li>one</li>", "List item one in cell");
    assert_contains(html, "<li>two</li>", "List item two in cell");
    assert_contains(html, "<table", "List cells table renders");
    apex_free_string(html);

    /* Plus-prefixed prose lines are not mistaken for grid tables */
    const char *plus_prose = "+ Item one\n+ Item two\n\nParagraph after.";
    html = apex_markdown_to_html(plus_prose, strlen(plus_prose), &opts);
    assert_contains(html, "Item one", "Plus prose lines preserved as list items");
    assert_contains(html, "Paragraph after", "Text after plus lines preserved");
    assert_not_contains(html, "<table", "Plus prose is not converted to a table");
    apex_free_string(html);

    /* Block with +---+ lines but no valid grid content is preserved on failure */
    const char *invalid_grid = "+---+\nThis line has no pipes\n+---+";
    html = apex_markdown_to_html(invalid_grid, strlen(invalid_grid), &opts);
    assert_contains(html, "This line has no pipes", "Invalid grid block content preserved");
    apex_free_string(html);

    /* Grid tables disabled by default in unified mode */
    apex_options unified_opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    html = apex_markdown_to_html(basic_grid, strlen(basic_grid), &unified_opts);
    assert_not_contains(html, "<th>H1</th>", "Grid tables off by default in unified mode");
    apex_free_string(html);

    /* Pandoc-style simple grid (manual example pattern) */
    const char *pandoc_simple = "+---------------+---------------+\n"
        "| Fruit         | Price         |\n"
        "+:==============+==============:+\n"
        "| Bananas       | $1.34         |\n"
        "| Oranges       | $2.10         |\n"
        "+---------------+---------------+\n";
    html = apex_markdown_to_html(pandoc_simple, strlen(pandoc_simple), &opts);
    assert_contains(html, "Bananas", "Pandoc-style aligned grid renders");
    assert_contains(html, "$1.34", "Pandoc-style grid cell content");
    assert_contains(html, "<table", "Pandoc-style grid produces table");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Grid Tables Tests", had_failures, false);
}

/**
 * Test advanced footnotes
 */
