/**
 * Advanced Tables Extension for Apex
 * Implementation
 *
 * Postprocessing approach to add table enhancements:
 * - Column spans (empty cells merge with previous)
 * - Row spans (^^ marker merges with cell above)
 * - Table captions (paragraph before/after table with [Caption] format)
 * - Multi-line support (future)
 */

#include "advanced_tables.h"
#include "parser.h"
#include "node.h"
#include "render.h"
#include "table.h"
#include "ial.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

/* Global flag for per-cell alignment (set when extension is created) */
static bool g_per_cell_alignment = false;

/* Placeholder for escaped \<< (literal <<). Must match table.c. No underscore so inline parser doesn't treat as emphasis. */
static const unsigned char ESCAPED_LTLT_PLACEHOLDER[] = "APEXLTLT";
#define ESCAPED_LTLT_PLACEHOLDER_LEN 8

/**
 * Recursively collect all text from a node into a buffer.
 * Caller must free the returned string.
 */
static char *get_node_full_text(cmark_node *node) {
    if (!node) return NULL;

    /* Collect literal from TEXT, CODE, HTML_INLINE so "raw <<" isn't seen as just "<<" */
    cmark_node_type t = cmark_node_get_type(node);
    if (t == CMARK_NODE_TEXT || t == CMARK_NODE_CODE || t == CMARK_NODE_HTML_INLINE) {
        const char *lit = cmark_node_get_literal(node);
        return lit ? strdup(lit) : NULL;
    }

    size_t cap = 64;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t len = 0;

    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        char *part = get_node_full_text(child);
        if (part) {
            size_t part_len = strlen(part);
            while (len + part_len + 1 > cap) {
                cap *= 2;
                char *new_buf = realloc(buf, cap);
                if (!new_buf) {
                    free(part);
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            memcpy(buf + len, part, part_len + 1);
            len += part_len;
            free(part);
        }
        child = cmark_node_next(child);
    }
    return buf;
}

/** Count TEXT/CODE/HTML_INLINE nodes under n that have literal content. */
static int count_literal_nodes(cmark_node *n) {
    int count = 0;
    cmark_node_type t = cmark_node_get_type(n);
    if (t == CMARK_NODE_TEXT || t == CMARK_NODE_CODE || t == CMARK_NODE_HTML_INLINE) {
        const char *lit = cmark_node_get_literal(n);
        return (lit && lit[0]) ? 1 : 0;
    }
    for (cmark_node *c = cmark_node_first_child(n); c; c = cmark_node_next(c))
        count += count_literal_nodes(c);
    return count;
}

/**
 * Check if a cell should span (contains << marker).
 * Only treat as colspan when the cell contains NOTHING but "<<" and optional whitespace.
 * Any other character (e.g. "raw <<", "**<<**", "x <<") must NOT be interpreted as colspan.
 * Content that is \<< (escaped) is replaced at parse time with a placeholder; we do not treat it as colspan.
 * Prefer raw string content when available so we see the actual characters; otherwise use parsed full text.
 * raw_content_override: if non-NULL, use this as the raw content (e.g. captured before user_data was overwritten).
 */
static bool is_colspan_cell(cmark_node *cell, const char *raw_content_override) {
    if (!cell) return false;

    char *full_text = get_node_full_text(cell);
    if (!full_text) return false;

    /* Use raw content when available so "raw <<" or "**<<**" etc. are not falsely treated as colspan.
     * Prefer raw_content_override (captured before process_cell_alignment overwrote user_data). */
    const char *raw = raw_content_override;
    if (!raw || !raw[0]) {
        void *ud = cmark_node_get_user_data(cell);
        if (ud) {
            char *s = (char *)ud;
            if (s[0] && !strstr(s, "colspan") && !strstr(s, "rowspan") && !strstr(s, "data-"))
                raw = s;
        }
    }
    if (!raw || !raw[0])
        raw = cmark_node_get_string_content(cell);
    if (!raw || !raw[0]) {
        cmark_node *first = cmark_node_first_child(cell);
        if (first && !cmark_node_next(first) &&
            cmark_node_get_type(first) == CMARK_NODE_PARAGRAPH)
            raw = cmark_node_get_string_content(first);
    }
    if (raw && raw[0]) {
        free(full_text);
        full_text = strdup(raw);
        if (!full_text) return false;
    } else {
        /* No raw content (cleared after parse). If the cell has more than one literal-bearing node
         * (TEXT/CODE/HTML_INLINE), we had e.g. "raw <<" so do not colspan. */
        if (count_literal_nodes(cell) > 1) {
            free(full_text);
            return false;
        }
    }

    const char *start = full_text;
    const char *end = full_text + strlen(full_text);
    if (start == end) {
        free(full_text);
        return false;
    }
    end--; /* last char */

    /* Trim leading whitespace */
    while (start <= end && isspace((unsigned char)*start)) start++;
    /* Trim trailing whitespace */
    while (end >= start && isspace((unsigned char)*end)) end--;

    size_t len = (end >= start) ? (size_t)(end - start + 1) : 0;

    bool result = false;
    /* Only treat as colspan when the ENTIRE cell content is exactly "<<". */
    if (len == 2 && start[0] == '<' && start[1] == '<') {
        result = true;  /* Exactly << → colspan */
    }
    /* Escaped \<< is replaced with placeholder; do not treat as colspan */
    if (len == ESCAPED_LTLT_PLACEHOLDER_LEN &&
        memcmp(start, ESCAPED_LTLT_PLACEHOLDER, ESCAPED_LTLT_PLACEHOLDER_LEN) == 0) {
        result = false;
    }
    /* "raw <<" or any other content that contains << but isn't exactly << → not colspan */
    if (len > 2 && strstr(full_text, "<<") != NULL) {
        result = false;
    }

    /* If we would treat as colspan but node's raw content buffer (string_content) has
     * more than "<<", prefer it so "raw <<" isn't wrongly merged (e.g. if user_data was lost). */
    if (result) {
        const char *raw_buf = cmark_node_get_string_content(cell);
        if (raw_buf && raw_buf[0]) {
            const char *r = raw_buf;
            const char *r_end = r + strlen(r);
            if (r_end > r) r_end--;
            while (r <= r_end && isspace((unsigned char)*r)) r++;
            while (r_end >= r && isspace((unsigned char)*r_end)) r_end--;
            if (r_end >= r && (size_t)(r_end - r + 1) > 2)
                result = false;
        }
    }

    free(full_text);
    return result;
}

/**
 * Check if a cell should rowspan (contains ^^ marker)
 */
static bool is_rowspan_cell(cmark_node *cell) {
    if (!cell) return false;

    cmark_node *child = cmark_node_first_child(cell);
    if (!child || cmark_node_get_type(child) != CMARK_NODE_TEXT) return false;

    const char *text = cmark_node_get_literal(child);
    if (!text) return false;

    /* Trim and check for ^^ */
    while (*text && isspace((unsigned char)*text)) text++;

    if (text[0] == '^' && text[1] == '^') {
        const char *rest = text + 2;
        while (*rest && isspace((unsigned char)*rest)) rest++;
        return (*rest == '\0'); /* Just ^^ */
    }

    return false;
}

/**
 * Process cell alignment markers (: at start/end) and strip them from content.
 * Returns alignment type: "left", "right", "center", or NULL for default.
 * Modifies the cell's text content by removing the colons.
 */
static const char *process_cell_alignment(cmark_node *cell) {
    if (!cell) return NULL;

    /* Recursively find all text nodes in the cell */
    cmark_node *text_node = NULL;

    /* Try to find the first text node */
    cmark_node *child = cmark_node_first_child(cell);
    while (child && !text_node) {
        if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
            text_node = child;
            break;
        }
        /* Check nested nodes (paragraphs, etc.) */
        cmark_node *nested = cmark_node_first_child(child);
        while (nested && !text_node) {
            if (cmark_node_get_type(nested) == CMARK_NODE_TEXT) {
                text_node = nested;
                break;
            }
            nested = cmark_node_next(nested);
        }
        child = cmark_node_next(child);
    }

    if (!text_node) return NULL;

    const char *original_text = cmark_node_get_literal(text_node);
    if (!original_text) return NULL;

    /* Check for alignment markers */
    const char *text = original_text;
    const char *start = text;
    const char *end = text + strlen(text) - 1;

    /* Trim leading whitespace */
    while (start <= end && isspace((unsigned char)*start)) start++;
    /* Trim trailing whitespace */
    while (end >= start && isspace((unsigned char)*end)) end--;

    if (start > end) return NULL; /* Empty after trimming */

    bool has_leading_colon = (*start == ':');
    bool has_trailing_colon = (*end == ':');

    if (!has_leading_colon && !has_trailing_colon) {
        return NULL; /* No alignment markers */
    }

    /* Determine alignment */
    const char *align = NULL;
    if (has_leading_colon && has_trailing_colon) {
        align = "center";
    } else if (has_leading_colon) {
        align = "left";
    } else if (has_trailing_colon) {
        align = "right";
    }

    /* Strip the colons from the text */
    const char *new_start = has_leading_colon ? start + 1 : start;
    const char *new_end = has_trailing_colon ? end - 1 : end;

    /* Also preserve leading/trailing whitespace that was there originally */
    const char *original_start = text;
    const char *original_end = text + strlen(text) - 1;

    /* Find where the original leading whitespace ends */
    const char *leading_ws_end = original_start;
    while (leading_ws_end < start && isspace((unsigned char)*leading_ws_end)) {
        leading_ws_end++;
    }

    /* Find where the original trailing whitespace starts */
    const char *trailing_ws_start = original_end;
    while (trailing_ws_start > end && isspace((unsigned char)*trailing_ws_start)) {
        trailing_ws_start--;
    }

    /* Build the new text: leading ws + content (without colons) + trailing ws */
    size_t leading_ws_len = leading_ws_end - original_start;
    size_t content_len = (new_end >= new_start) ? (new_end - new_start + 1) : 0;
    size_t trailing_ws_len = original_end - trailing_ws_start;

    size_t new_len = leading_ws_len + content_len + trailing_ws_len;
    char *new_text = malloc(new_len + 1);
    if (!new_text) return align; /* Return alignment even if we can't modify text */

    char *write = new_text;
    /* Copy leading whitespace */
    memcpy(write, original_start, leading_ws_len);
    write += leading_ws_len;
    /* Copy content without colons */
    if (content_len > 0) {
        memcpy(write, new_start, content_len);
        write += content_len;
    }
    /* Copy trailing whitespace */
    if (trailing_ws_len > 0) {
        memcpy(write, trailing_ws_start + 1, trailing_ws_len);
        write += trailing_ws_len;
    }
    *write = '\0';

    /* Update the text node */
    cmark_node_set_literal(text_node, new_text);
    free(new_text);

    return align;
}

/**
 * Check if a row should be in tfoot (contains === markers)
 */
static bool is_tfoot_row(cmark_node *row) {
    if (!row || cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) return false;

    cmark_node *cell = cmark_node_first_child(row);
    bool has_equals = false;

    while (cell) {
        if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) {
            cmark_node *text_node = cmark_node_first_child(cell);
            if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(text_node);
                if (text) {
                    /* Trim whitespace */
                    while (*text && isspace((unsigned char)*text)) text++;
                    /* Check if it's only === (three or more equals) */
                    if (*text == '=' && text[1] == '=' && text[2] == '=') {
                        const char *after = text + 3;
                        while (*after == '=') after++; /* Allow more equals */
                        while (*after && isspace((unsigned char)*after)) after++;
                        if (*after == '\0') {
                            has_equals = true;
                        }
                    }
                }
            }
        }
        cell = cmark_node_next(cell);
    }

    return has_equals;
}

/**
 * Check if a row contains only a caption marker (last row with [Caption])
 * Note: This detection works when captions are parsed as table rows, but currently
 * captions immediately following tables without a blank line are not reliably detected
 * because cmark-gfm parses them as table rows which interferes with detection.
 */
static bool is_caption_row(cmark_node *row) {
    if (!row || cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) return false;

    /* Check if this row has a cell with [Caption] format */
    /* It might be a single cell, or a cell that spans columns */
    cmark_node *cell = cmark_node_first_child(row);
    int cell_count = 0;
    bool has_caption = false;
    int caption_cell_count = 0;

    while (cell) {
        if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) {
            cell_count++;
            cmark_node *text_node = cmark_node_first_child(cell);
            if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(text_node);
                if (text && text[0] == '[') {
                    const char *end = strchr(text + 1, ']');
                    if (end) {
                        const char *after = end + 1;
                        while (*after && isspace((unsigned char)*after)) after++;
                        if (*after == '\0') {
                            has_caption = true;
                            caption_cell_count++;
                        }
                    }
                }
            }
        }
        cell = cmark_node_next(cell);
    }

    /* Caption row: has caption text, and either:
     * - Single cell with caption, OR
     * - All cells contain caption text (shouldn't happen, but be safe) */
    return has_caption && (cell_count == 1 || caption_cell_count == cell_count);
}

/**
 * Add colspan/rowspan attributes to table cells
 * This modifies the AST by setting user_data with HTML attributes
 */
static void process_table_spans(cmark_node *table) {
    if (!table || cmark_node_get_type(table) != CMARK_NODE_TABLE) return;

    /* Walk through table rows - start from first TABLE_ROW node */
    cmark_node *row = cmark_node_first_child(table);
    while (row && cmark_node_get_type(row) != CMARK_NODE_TABLE_ROW) {
        row = cmark_node_next(row); /* Skip non-row nodes */
    }

    cmark_node *prev_row = NULL;
    bool is_first_row = true; /* Track header row */
    bool in_tfoot_section = false; /* Track if we've entered tfoot section */

    /* Track active rowspan cells per column (inspired by Jekyll Spaceship).
     * active_rowspan[col] points to the cell node that's currently being rowspanned in that column.
     * When we see a ^^ cell, we merge it with the active cell for that column.
     * This persists across rows - when a regular cell appears, it becomes the new active cell.
     * Initialize all to NULL at the start of each table. */
    cmark_node *active_rowspan[50] = {NULL};  /* One per column, max 50 columns, persists across rows */

    while (row) {
        if (cmark_node_get_type(row) == CMARK_NODE_TABLE_ROW) {
            /* Process span for header row too - don't skip it */
            if (is_first_row) {
                is_first_row = false;
            }

            /* Check if this row is a tfoot row (contains ===) */
            /* Once we encounter a tfoot row, all subsequent rows are in tfoot */
            if (is_tfoot_row(row)) {
                /* This is the === row itself - mark it as tfoot and set flag */
                in_tfoot_section = true;
                char *existing = (char *)cmark_node_get_user_data(row);
                if (existing) free(existing);
                cmark_node_set_user_data(row, strdup(" data-tfoot=\"true\""));
            } else if (in_tfoot_section) {
                /* We've already encountered the === row, so mark this row as tfoot */
                char *existing = (char *)cmark_node_get_user_data(row);
                if (existing) free(existing);
                cmark_node_set_user_data(row, strdup(" data-tfoot=\"true\""));
            }

            /* If this row is the === separator row, mark its cells for removal.
             * We no longer skip processing for tfoot rows entirely, so that colspan
             * logic can still run on footer rows. Rowspan logic will simply not
             * trigger unless ^^ markers are present. */
            if (is_tfoot_row(row)) {
                cmark_node *cell = cmark_node_first_child(row);
                while (cell) {
                    if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) {
                        cmark_node *text_node = cmark_node_first_child(cell);
                        if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                            const char *text = cmark_node_get_literal(text_node);
                            if (text) {
                                /* Trim whitespace */
                                while (*text && isspace((unsigned char)*text)) text++;
                                /* Check if it's === */
                                if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                    const char *after = text + 3;
                                    while (*after == '=') after++; /* Allow more equals */
                                    while (*after && isspace((unsigned char)*after)) after++;
                                    if (*after == '\0') {
                                        /* This is a === cell - mark for removal */
                                        char *cell_attrs = (char *)cmark_node_get_user_data(cell);
                                        if (cell_attrs) free(cell_attrs);
                                        cmark_node_set_user_data(cell, strdup(" data-remove=\"true\""));
                                    }
                                }
                            }
                        }
                    }
                    cell = cmark_node_next(cell);
                }
            }

            /* Check if this row only contains '—' cells (separator row or empty row) */
            cmark_node *check_cell = cmark_node_first_child(row);
            bool all_dash = true;
            bool has_cells = false;
            while (check_cell) {
                if (cmark_node_get_type(check_cell) == CMARK_NODE_TABLE_CELL) {
                    has_cells = true;
                    cmark_node *check_text = cmark_node_first_child(check_cell);
                    if (check_text && cmark_node_get_type(check_text) == CMARK_NODE_TEXT) {
                        const char *check_content = cmark_node_get_literal(check_text);
                        if (check_content && strcmp(check_content, "—") != 0) {
                            all_dash = false;
                            break;
                        }
                    } else {
                        /* Empty cell, check if it's really empty */
                        if (cmark_node_first_child(check_cell)) {
                            all_dash = false;
                            break;
                        }
                    }
                }
                check_cell = cmark_node_next(check_cell);
            }
            /* Skip rows that only contain '—' characters (alignment/separator rows) */
            if (has_cells && all_dash) {
                /* Mark the entire row for removal in HTML output */
                cmark_node *dash_cell = cmark_node_first_child(row);
                while (dash_cell) {
                    if (cmark_node_get_type(dash_cell) == CMARK_NODE_TABLE_CELL) {
                        char *existing = (char *)cmark_node_get_user_data(dash_cell);
                        if (existing) free(existing);
                        cmark_node_set_user_data(dash_cell, strdup(" data-remove=\"true\""));
                    }
                    dash_cell = cmark_node_next(dash_cell);
                }
                /* Don't update prev_row when skipping - keep it pointing to the previous valid row */
                row = cmark_node_next(row);
                continue;
            }

            cmark_node *cell = cmark_node_first_child(row);
            cmark_node *prev_cell = NULL;  /* Always reset to NULL at start of each row */
            int col_index = 0;

            /* Note: We don't initialize active_rowspan here because it persists across rows.
             * When we process the first data row, active_rowspan will be NULL for all columns,
             * so ^^ cells will find cells from prev_row and set them as active.
             * Regular cells in subsequent rows will update active_rowspan as they're processed. */

            while (cell) {
                if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) {
                    /* Capture raw cell content before process_cell_alignment may overwrite user_data.
                     * Table parser stores raw content in user_data; we need it for colspan check
                     * so "raw <<" is not wrongly treated as colspan. */
                    const char *raw_content = NULL;
                    {
                        void *ud = cmark_node_get_user_data(cell);
                        if (ud) {
                            char *s = (char *)ud;
                            if (s[0] && !strstr(s, "colspan") && !strstr(s, "rowspan") && !strstr(s, "data-"))
                                raw_content = s;
                        }
                    }

                    /* Process per-cell alignment markers (:) BEFORE colspan/rowspan processing
                     * so that alignment is preserved when cells are merged. */
                    if (g_per_cell_alignment) {
                        char *cell_attrs_check = (char *)cmark_node_get_user_data(cell);
                        if (!cell_attrs_check || !strstr(cell_attrs_check, "data-remove")) {
                            const char *align = process_cell_alignment(cell);
                            if (align) {
                            /* Add style attribute for alignment */
                            char *existing_attrs = (char *)cmark_node_get_user_data(cell);
                            char new_attrs[256];

                            if (existing_attrs && strlen(existing_attrs) > 0) {
                                snprintf(new_attrs, sizeof(new_attrs), "%s style=\"text-align: %s\"", existing_attrs, align);
                            } else {
                                snprintf(new_attrs, sizeof(new_attrs), " style=\"text-align: %s\"", align);
                            }

                            if (existing_attrs) free(existing_attrs);
                            cmark_node_set_user_data(cell, strdup(new_attrs));
                            }
                        }
                    }

                            /* Check for colspan (pass raw_content so we see "raw <<" etc. before user_data was overwritten) */
                    bool is_colspan = is_colspan_cell(cell, raw_content);
                    /* Final safeguard: if we would merge, re-check parsed content length.
                     * If the cell has multiple literal nodes (e.g. "raw " + "<<") the full text
                     * is longer than "<<"; do not merge so "raw <<" stays a normal cell. */
                    if (is_colspan) {
                        char *full = get_node_full_text(cell);
                        if (full) {
                            const char *p = full;
                            while (*p && isspace((unsigned char)*p)) p++;
                            const char *q = p + strlen(p);
                            if (q > p) q--;
                            while (q >= p && isspace((unsigned char)*q)) q--;
                            if (q >= p && (size_t)(q - p + 1) > 2)
                                is_colspan = false;
                            free(full);
                        }
                    }
                    if (is_colspan) {
                        /* Only process colspan if we have a previous cell in the SAME ROW */
                        if (!prev_cell || cmark_node_parent(prev_cell) != row) {
                            /* No previous cell in same row, can't do colspan.
                             * This happens for:
                             * 1. First cell in a row (prev_cell is NULL)
                             * 2. First non-empty cell after removed cells (prev_cell is from previous row)
                             * In these cases, mark empty cells for removal. */
                            cmark_node *child = cmark_node_first_child(cell);
                            if (child == NULL) {
                                /* Empty cell with no previous cell - mark for removal */
                                char *existing = (char *)cmark_node_get_user_data(cell);
                                if (existing) free(existing);
                                cmark_node_set_user_data(cell, strdup(" data-remove=\"true\""));
                            }
                            prev_cell = cell;
                            col_index++;
                            cell = cmark_node_next(cell);
                            continue;
                        }
                        /* Find the first non-empty cell going backwards (skip cells marked for removal) */
                        cmark_node *target_cell = prev_cell;
                        while (target_cell) {
                            /* Verify target_cell is in the same row */
                            if (cmark_node_parent(target_cell) != row) {
                                break; /* target_cell is not in the same row, stop */
                            }
                            char *target_attrs = (char *)cmark_node_get_user_data(target_cell);
                            /* Skip cells marked for removal */
                            if (!target_attrs || !strstr(target_attrs, "data-remove")) {
                                break; /* Found a real cell */
                            }
                            /* Move to previous cell */
                            cmark_node *prev = cmark_node_previous(target_cell);
                            while (prev && cmark_node_get_type(prev) != CMARK_NODE_TABLE_CELL) {
                                prev = cmark_node_previous(prev);
                            }
                            target_cell = prev;
                        }

                        if (target_cell && cmark_node_parent(target_cell) == row) {
                            /* Merge empty cells with the previous cell to create colspan.
                             * This handles both:
                             * - Consecutive empty cells (like |||) merging together
                             * - Empty cells after a content cell (like | header |||) merging with the content cell
                             *
                             * However, we need to be careful: a single empty cell between two content cells
                             * (like | Absent | | 92.00 |) should NOT merge, as that's just a missing value.
                             *
                             * The distinction: if the target_cell is empty, we're merging consecutive empty cells.
                             * If the target_cell has content, we're merging an empty cell with content (colspan).
                             * Both cases are valid for creating colspan. */

                            /* Check if target_cell is empty (has no content at all).
                             * IMPORTANT: We only merge empty cells that are part of consecutive empty cells (|||).
                             * Empty cells with whitespace between pipes (|    |) should NOT be merged. */
                            cmark_node *target_child = cmark_node_first_child(target_cell);
                            bool target_is_empty = (target_child == NULL);

                            /* Check if the next cell (after current) has content.
                             * If target has content AND next also has content, don't merge (isolated empty cell).
                             * Example: | Absent | | 92.00 | - empty cell between two content cells should NOT merge.
                             * But if target has content and next is empty or end-of-row, merge (empty cells after content).
                             * Example: | header ||| - empty cells after content should merge. */
                            cmark_node *next_cell = cmark_node_next(cell);
                            while (next_cell && cmark_node_get_type(next_cell) != CMARK_NODE_TABLE_CELL) {
                                next_cell = cmark_node_next(next_cell);
                            }
                            bool next_has_content = false;
                            if (next_cell && cmark_node_get_type(next_cell) == CMARK_NODE_TABLE_CELL) {
                                cmark_node *next_child = cmark_node_first_child(next_cell);
                                next_has_content = (next_child != NULL);
                            }

                            /* Merge if:
                             * 1. Current cell has << marker (explicit colspan marker, always merge), OR
                             * 2. Target is empty AND next is also empty (consecutive empty cells from |||), OR
                             * 3. Target has content AND next is empty/end (empty cells after content from | header |||)
                             *
                             * Do NOT merge if:
                             * - Target has content AND next also has content (isolated empty cell like | A | | B |)
                             * - Target is empty but next has content (empty cell before content, like | | A |) */
                            /* For << markers (explicit colspan), always merge regardless of other conditions */
                            bool should_merge = is_colspan ||
                                (target_is_empty && !next_has_content) ||
                                (!target_is_empty && !next_has_content);

                            if (should_merge) {
                                /* Target cell is empty or has << marker - merge them (colspan) */
                                /* Get or create colspan attribute */
                                char *prev_attrs = (char *)cmark_node_get_user_data(target_cell);
                                int current_colspan = 1;

                                if (prev_attrs && strstr(prev_attrs, "colspan=")) {
                                    sscanf(strstr(prev_attrs, "colspan="), "colspan=\"%d\"", &current_colspan);
                                }

                                /* Extract style attribute if it exists */
                                char style_attr[256] = "";
                                if (prev_attrs && strstr(prev_attrs, "style=")) {
                                    const char *style_start = strstr(prev_attrs, "style=");
                                    const char *style_end = style_start;
                                    /* Find the opening quote after style= */
                                    while (*style_end && *style_end != '"') style_end++;
                                    if (*style_end == '"') {
                                        style_end++; /* Skip opening quote */
                                        /* Find the closing quote */
                                        while (*style_end && *style_end != '"') style_end++;
                                        if (*style_end == '"') {
                                            style_end++; /* Include the closing quote */
                                            /* Now find the end: space or end of string */
                                            const char *attr_end = style_end;
                                            while (*attr_end && *attr_end != ' ' && *attr_end != '\t') attr_end++;
                                            size_t style_len = attr_end - style_start;
                                            if (style_len < sizeof(style_attr)) {
                                                strncpy(style_attr, style_start, style_len);
                                                style_attr[style_len] = '\0';
                                            }
                                        }
                                    }
                                }

                                /* Build new attributes - combine style (if any) and colspan */
                                char new_attrs[512];
                                if (style_attr[0] != '\0') {
                                    snprintf(new_attrs, sizeof(new_attrs), " %s colspan=\"%d\"", style_attr, current_colspan + 1);
                                } else {
                                    snprintf(new_attrs, sizeof(new_attrs), " colspan=\"%d\"", current_colspan + 1);
                                }

                                /* Free old user_data before setting new */
                                if (prev_attrs) free(prev_attrs);
                                cmark_node_set_user_data(target_cell, strdup(new_attrs));

                                /* Mark current cell for removal */
                                cmark_node_set_user_data(cell, strdup(" data-remove=\"true\""));
                            }
                        }
                    }
                    /* Check for rowspan */
                    else if (is_rowspan_cell(cell) && col_index < 50) {
                        /* Use Jekyll Spaceship approach: merge with active rowspan cell for this column.
                         * If there's an active rowspan cell, increment its rowspan.
                         * Otherwise, find the cell in the previous row and make it active. */
                        cmark_node *target_cell = active_rowspan[col_index];

                        /* If no active cell, find one in the previous row */
                        if (!target_cell && prev_row) {
                            cmark_node *candidate = cmark_node_first_child(prev_row);
                            int prev_col = 0;

                            /* Find cell at col_index in the previous row */
                            while (candidate) {
                                if (cmark_node_get_type(candidate) == CMARK_NODE_TABLE_CELL) {
                                    if (prev_col == col_index) {
                                        /* Check if this cell is marked for removal */
                                        char *cand_attrs = (char *)cmark_node_get_user_data(candidate);
                                        if (!cand_attrs || !strstr(cand_attrs, "data-remove")) {
                                            /* Found a real cell (not marked for removal) at this column index */
                                            target_cell = candidate;
                                            active_rowspan[col_index] = candidate;  /* Make it active */
                                        }
                                        break;
                                    }
                                    prev_col++;
                                }
                                candidate = cmark_node_next(candidate);
                            }
                        }

                        if (target_cell && cmark_node_get_type(target_cell) == CMARK_NODE_TABLE_CELL) {
                            /* Get or create rowspan attribute */
                            char *prev_attrs = (char *)cmark_node_get_user_data(target_cell);
                            int current_rowspan = 1;

                            if (prev_attrs && strstr(prev_attrs, "rowspan=")) {
                                sscanf(strstr(prev_attrs, "rowspan="), "rowspan=\"%d\"", &current_rowspan);
                            }

                            /* Increment rowspan - append only if prev_attrs looks like HTML attributes
                             * (contains '='). The table parser may store raw cell content in user_data;
                             * we must not prepend that to rowspan or we get <td Engineering rowspan="2">. */
                            char new_attrs[256];
                            bool prev_looks_like_attrs = (prev_attrs && strchr(prev_attrs, '=') != NULL);
                            if (prev_looks_like_attrs && !strstr(prev_attrs, "rowspan=")) {
                                snprintf(new_attrs, sizeof(new_attrs), "%s rowspan=\"%d\"", prev_attrs, current_rowspan + 1);
                            } else {
                                snprintf(new_attrs, sizeof(new_attrs), " rowspan=\"%d\"", current_rowspan + 1);
                            }
                            /* Free old user_data before setting new */
                            if (prev_attrs) free(prev_attrs);
                            cmark_node_set_user_data(target_cell, strdup(new_attrs));

                        }
                        /* Always mark rowspan cell for removal, even if target not found */
                        char *existing = (char *)cmark_node_get_user_data(cell);
                        if (existing) free(existing);
                        cmark_node_set_user_data(cell, strdup(" data-remove=\"true\""));
                    }

                    /* If this is a regular cell (not rowspan), it becomes the new active cell for this column.
                     * This happens AFTER processing rowspan cells, so regular cells replace the previous active cell.
                     * This is the Jekyll Spaceship approach: regular cells become the new active cells.
                     *
                     * IMPORTANT: We set this AFTER processing the cell, so that when we process ^^ cells
                     * in the NEXT row, they will use the correct active cell from THIS row. */
                    if (!is_rowspan_cell(cell)) {
                        char *cell_attrs = (char *)cmark_node_get_user_data(cell);
                        if (!cell_attrs || !strstr(cell_attrs, "data-remove")) {
                            /* This is a regular cell, so it becomes the new active cell for this column */
                            active_rowspan[col_index] = cell;
                        }
                    }

                    prev_cell = cell;
                    col_index++;
                }
                cell = cmark_node_next(cell);
            }

            /* Update prev_row after processing this row */
            prev_row = row;
        }
        row = cmark_node_next(row);
    }
}

/**
 * Check if a paragraph is a table caption ([Caption Text])
 */
/**
 * Check if a paragraph is adjacent to a table (before or after)
 */
static bool is_adjacent_to_table(cmark_node *para) {
    if (!para) return false;

    /* Check previous sibling, skipping blank paragraphs */
    cmark_node *prev = cmark_node_previous(para);
    while (prev) {
        cmark_node_type prev_type = cmark_node_get_type(prev);
        if (prev_type == CMARK_NODE_TABLE) {
            return true;
        } else if (prev_type == CMARK_NODE_PARAGRAPH) {
            /* Check if paragraph is blank */
            cmark_node *child = cmark_node_first_child(prev);
            bool is_blank = true;
            if (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(child);
                if (text) {
                    const char *p = text;
                    while (*p) {
                        if (!isspace((unsigned char)*p)) {
                            is_blank = false;
                            break;
                        }
                        p++;
                    }
                }
            } else if (child) {
                is_blank = false; /* Has non-text content */
            }
            if (!is_blank) {
                break; /* Found non-blank paragraph, stop looking */
            }
            prev = cmark_node_previous(prev); /* Skip blank paragraph */
        } else {
            break; /* Not a table or paragraph, stop looking */
        }
    }

    /* Check next sibling, skipping blank paragraphs */
    cmark_node *next = cmark_node_next(para);
    while (next) {
        cmark_node_type next_type = cmark_node_get_type(next);
        if (next_type == CMARK_NODE_TABLE) {
            return true;
        } else if (next_type == CMARK_NODE_PARAGRAPH) {
            /* Check if paragraph is blank */
            cmark_node *child = cmark_node_first_child(next);
            bool is_blank = true;
            if (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                const char *text = cmark_node_get_literal(child);
                if (text) {
                    const char *p = text;
                    while (*p) {
                        if (!isspace((unsigned char)*p)) {
                            is_blank = false;
                            break;
                        }
                        p++;
                    }
                }
            } else if (child) {
                is_blank = false; /* Has non-text content */
            }
            if (!is_blank) {
                break; /* Found non-blank paragraph, stop looking */
            }
            next = cmark_node_next(next); /* Skip blank paragraph */
        } else {
            break; /* Not a table or paragraph, stop looking */
        }
    }

    return false;
}

/**
 * Parse IAL attributes from text (supports both {: ...} and {#id .class} formats)
 * Returns attributes structure or NULL if no IAL found
 */
static apex_attributes *parse_ial_from_text(const char *text) {
    if (!text) return NULL;

    /* Look for IAL pattern: {: ... } or {#id .class} */
    /* Search from the end backwards to find the last IAL (in case there are multiple) */
    const char *ial_start = NULL;

    /* Find the last opening brace that looks like an IAL */
    const char *p = text;
    const char *last_ial = NULL;

    while (*p) {
        if (*p == '{') {
            /* Check if this looks like an IAL */
            if (p[1] == ':') {
                /* Kramdown format: {: ... } */
                last_ial = p + 2; /* Skip {: */
            } else if (p[1] == '#' || p[1] == '.') {
                /* Pandoc format: {#id .class} */
                last_ial = p + 1; /* Skip { */
            }
        }
        p++;
    }

    if (last_ial) {
        ial_start = last_ial;
    }

    if (!ial_start) return NULL;

    /* Find closing brace */
    const char *ial_end = strchr(ial_start, '}');
    if (!ial_end) return NULL;

    /* Parse IAL content */
    size_t content_len = (size_t)(ial_end - ial_start);
    if (content_len == 0) return NULL;

    /* Use parse_ial_content from ial.c - it already handles the content format */
    /* We need to access the static function, so we'll duplicate the logic here */
    /* Actually, we can't access static functions, so we'll create a wrapper */
    apex_attributes *attrs = malloc(sizeof(apex_attributes));
    if (!attrs) return NULL;
    memset(attrs, 0, sizeof(apex_attributes));

    char buffer[2048];
    if (content_len >= sizeof(buffer)) content_len = sizeof(buffer) - 1;
    memcpy(buffer, ial_start, content_len);
    buffer[content_len] = '\0';

    /* Parse attributes manually (similar to parse_ial_content) */
    char *buf_p = buffer;
    while (*buf_p) {
        /* Skip whitespace */
        while (isspace((unsigned char)*buf_p)) buf_p++;
        if (!*buf_p) break;

        /* Check for ID (#id) */
        if (*buf_p == '#') {
            buf_p++;
            char *id_start = buf_p;
            while (*buf_p && !isspace((unsigned char)*buf_p) && *buf_p != '.' && *buf_p != '}') buf_p++;
            if (buf_p > id_start) {
                char saved = *buf_p;
                *buf_p = '\0';
                attrs->id = strdup(id_start);
                *buf_p = saved;
            }
            continue;
        }

        /* Check for class (.class) */
        if (*buf_p == '.') {
            buf_p++;
            char *class_start = buf_p;
            while (*buf_p && !isspace((unsigned char)*buf_p) && *buf_p != '.' && *buf_p != '#' && *buf_p != '}') buf_p++;
            if (buf_p > class_start) {
                char saved = *buf_p;
                *buf_p = '\0';
                attrs->classes = realloc(attrs->classes, sizeof(char*) * (attrs->class_count + 1));
                attrs->classes[attrs->class_count++] = strdup(class_start);
                *buf_p = saved;
            }
            continue;
        }

        /* Check for key="value" */
        char *key_start = buf_p;
        while (*buf_p && *buf_p != '=' && *buf_p != ' ' && *buf_p != '\t' && *buf_p != '}') buf_p++;

        if (*buf_p == '=') {
            char saved = *buf_p;
            *buf_p = '\0';
            char *key = strdup(key_start);
            *buf_p = saved;
            buf_p++; /* Skip = */

            /* Parse value */
            char *value = NULL;
            if (*buf_p == '"' || *buf_p == '\'') {
                char quote = *buf_p++;
                char *value_start = buf_p;
                while (*buf_p && *buf_p != quote) {
                    if (*buf_p == '\\' && *(buf_p+1)) buf_p++;
                    buf_p++;
                }
                if (*buf_p == quote) {
                    *buf_p = '\0';
                    value = strdup(value_start);
                    *buf_p = quote;
                    buf_p++;
                }
            } else {
                char *value_start = buf_p;
                while (*buf_p && !isspace((unsigned char)*buf_p) && *buf_p != '}') buf_p++;
                char saved_val = *buf_p;
                *buf_p = '\0';
                value = strdup(value_start);
                *buf_p = saved_val;
            }

            attrs->keys = realloc(attrs->keys, sizeof(char*) * (attrs->attr_count + 1));
            attrs->values = realloc(attrs->values, sizeof(char*) * (attrs->attr_count + 1));
            attrs->keys[attrs->attr_count] = key;
            attrs->values[attrs->attr_count] = value ? value : strdup("");
            attrs->attr_count++;
            continue;
        }

        /* Unknown token, skip */
        buf_p++;
    }

    return attrs;
}

/**
 * Convert attributes to HTML attribute string
 */
static char *attributes_to_html_string(apex_attributes *attrs) {
    if (!attrs) return NULL;

    char buffer[4096];
    char *p = buffer;
    bool first_attr = true;

    #define APPEND(s) do { \
        size_t len = strlen(s); \
        if ((size_t)(p - buffer) + len < sizeof(buffer)) { \
            memcpy(p, s, len); \
            p += len; \
        } \
    } while (0)

    if (attrs->id) {
        APPEND(" id=\"");
        APPEND(attrs->id);
        APPEND("\"");
        first_attr = false;
    }

    if (attrs->class_count > 0) {
        if (first_attr) {
            APPEND(" class=\"");
        } else {
            APPEND(" class=\"");
        }
        for (int i = 0; i < attrs->class_count; i++) {
            if (i > 0) APPEND(" ");
            APPEND(attrs->classes[i]);
        }
        APPEND("\"");
        first_attr = false;
    }

    for (int i = 0; i < attrs->attr_count; i++) {
        char attr_str[1024];
        const char *val = attrs->values[i];
        if (first_attr) {
            snprintf(attr_str, sizeof(attr_str), "%s=\"%s\"", attrs->keys[i], val);
            first_attr = false;
        } else {
            snprintf(attr_str, sizeof(attr_str), " %s=\"%s\"", attrs->keys[i], val);
        }
        APPEND(attr_str);
    }

    #undef APPEND

    *p = '\0';
    return strdup(buffer);
}

/**
 * Check if a paragraph is a table caption format (without adjacency check)
 * Used when we're already looking backwards from a table and know it's adjacent
 */
static bool is_table_caption_format(cmark_node *para, char **caption_text, const char **original_text_ptr) {
    if (!para || cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) {
        return false;
    }

    /* Collect all text from all text nodes in the paragraph */
    cmark_node *text_node = cmark_node_first_child(para);
    if (!text_node) {
        return false;
    }

    /* Build full text by concatenating all text nodes */
    size_t text_len = 0;
    cmark_node *node = text_node;
    while (node) {
        if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
            const char *node_text = cmark_node_get_literal(node);
            if (node_text) {
                text_len += strlen(node_text);
            }
        }
        node = cmark_node_next(node);
    }

    if (text_len == 0) return false;

    /* Allocate buffer for full text */
    char *full_text = malloc(text_len + 1);
    if (!full_text) return false;
    full_text[0] = '\0';

    /* Concatenate all text nodes */
    node = text_node;
    while (node) {
        if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
            const char *node_text = cmark_node_get_literal(node);
            if (node_text) {
                strcat(full_text, node_text);
            }
        }
        node = cmark_node_next(node);
    }

    const char *text = full_text;

    /* Check for [Caption] format */
    if (text[0] == '[') {
        const char *end = strchr(text + 1, ']');
        if (end) {
            const char *after = end + 1;
            while (*after && isspace((unsigned char)*after)) after++;

            bool has_ial = false;
            if (*after == '{') {
                if ((after[1] == ':' || after[1] == '#' || after[1] == '.') &&
                    strchr(after, '}')) {
                    has_ial = true;
                }
            }

            if (!has_ial && *after == '\0') {
                size_t len = end - (text + 1);
                *caption_text = malloc(len + 1);
                if (*caption_text) {
                    memcpy(*caption_text, text + 1, len);
                    (*caption_text)[len] = '\0';
                }
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            } else if (has_ial) {
                size_t len = end - (text + 1);
                *caption_text = malloc(len + 1);
                if (*caption_text) {
                    memcpy(*caption_text, text + 1, len);
                    (*caption_text)[len] = '\0';
                }
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            }
        }
    }

    /* Check for : caption format (without adjacency check since we're already looking backwards) */
    const char *p = text;
    size_t text_length = strlen(p);
    int spaces = 0;
    /* Allow up to 4 spaces (definition list allows 3, so 4+ prevents definition list matching) */
    while (spaces < 4 && spaces < (int)text_length && p[spaces] == ' ') {
        spaces++;
    }

    if (spaces < (int)text_length && p[spaces] == ':' && (spaces + 1) < (int)text_length) {
        if (p[spaces + 1] == ' ' || p[spaces + 1] == '\t') {
            const char *caption_start = p + spaces + 2;
            const char *caption_end = p + strlen(p);
            const char *ial_start = NULL;

            /* Look for IAL at the end */
            const char *text_end = p + text_length;
            const char *search = caption_end - 1;
            while (search >= caption_start) {
                if (*search == '}') {
                    const char *open = search;
                    while (open >= caption_start && *open != '{') {
                        open--;
                    }
                    if (open >= caption_start && *open == '{' && (open + 1) < text_end) {
                        if ((open[1] == ':' || open[1] == '#' || open[1] == '.') &&
                            search > open) {
                            ial_start = open;
                            caption_end = open;
                            break;
                        }
                    }
                }
                search--;
            }

            /* Extract caption text (trim whitespace) */
            while (caption_start < caption_end && isspace((unsigned char)*caption_start)) {
                caption_start++;
            }
            while (caption_end > caption_start && isspace((unsigned char)*(caption_end - 1))) {
                caption_end--;
            }

            if (caption_end > caption_start) {
                size_t len = caption_end - caption_start;
                *caption_text = malloc(len + 1);
                if (*caption_text) {
                    memcpy(*caption_text, caption_start, len);
                    (*caption_text)[len] = '\0';
                }
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            } else if (ial_start) {
                *caption_text = strdup("");
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            }
        }
    }

    /* Not a caption format - free allocated memory */
    /* Note: We don't modify user_data here because it might contain important state
     * from previous checks. The caller should handle cleanup if needed. */
    free(full_text);
    return false;
}

static bool is_table_caption(cmark_node *para, char **caption_text, const char **original_text_ptr) {
    if (!para || cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) {
        return false;
    }

    /* Collect all text from all text nodes in the paragraph */
    /* This is necessary because IAL might be in a separate text node after whitespace */
    cmark_node *text_node = cmark_node_first_child(para);
    if (!text_node) return false;

    /* Build full text by concatenating all text nodes */
    size_t text_len = 0;
    cmark_node *node = text_node;
    while (node) {
        if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
            const char *node_text = cmark_node_get_literal(node);
            if (node_text) {
                text_len += strlen(node_text);
            }
        }
        node = cmark_node_next(node);
    }

    if (text_len == 0) return false;

    /* Allocate buffer for full text */
    char *full_text = malloc(text_len + 1);
    if (!full_text) return false;
    full_text[0] = '\0';

    /* Concatenate all text nodes */
    node = text_node;
    while (node) {
        if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
            const char *node_text = cmark_node_get_literal(node);
            if (node_text) {
                strcat(full_text, node_text);
            }
        }
        node = cmark_node_next(node);
    }

    const char *text = full_text;

    /* Check for [Caption] format */
    if (text[0] == '[') {
        const char *end = strchr(text + 1, ']');
        if (end) {
            /* Check if there's IAL after the closing bracket */
            const char *after = end + 1;
            while (*after && isspace((unsigned char)*after)) after++;

            /* Look for IAL pattern after bracket */
            bool has_ial = false;
            if (*after == '{') {
                if ((after[1] == ':' || after[1] == '#' || after[1] == '.') &&
                    strchr(after, '}')) {
                    has_ial = true;
                }
            }

            if (!has_ial && *after == '\0') {
                /* No IAL, just [Caption] */
                size_t len = end - (text + 1);
                *caption_text = malloc(len + 1);
                if (*caption_text) {
                    memcpy(*caption_text, text + 1, len);
                    (*caption_text)[len] = '\0';
                }
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            } else if (has_ial) {
                /* Has IAL after [Caption] */
                size_t len = end - (text + 1);
                *caption_text = malloc(len + 1);
                if (*caption_text) {
                    memcpy(*caption_text, text + 1, len);
                    (*caption_text)[len] = '\0';
                }
                /* Store full_text pointer in paragraph user_data so we can free it later */
                char *existing_data = (char *)cmark_node_get_user_data(para);
                if (existing_data) free(existing_data);
                cmark_node_set_user_data(para, full_text);
                if (original_text_ptr) {
                    *original_text_ptr = text;
                }
                return true;
            }
        }
    }

    /* Check for : caption format (Pandoc-style) */
    /* Only recognize this if adjacent to a table to avoid conflict with definition lists */
    if (is_adjacent_to_table(para)) {
        const char *p = text;

        /* Skip leading whitespace (up to 3 spaces, matching definition list rules) */
        int spaces = 0;
        while (spaces < 3 && p[spaces] == ' ') {
            spaces++;
        }

        /* Must start with : */
        if (p[spaces] == ':') {
            /* Must be followed by space or tab */
            if (p[spaces + 1] == ' ' || p[spaces + 1] == '\t') {
                const char *caption_start = p + spaces + 2; /* Skip : and space */

                /* Find IAL at the end (if any) */
                const char *ial_start = NULL;
                const char *caption_end = p + strlen(p);

                /* Look for IAL pattern from the end */
                const char *search = caption_end - 1;
                const char *text_end_here = text + strlen(text);
                while (search >= caption_start) {
                    if (*search == '}') {
                        /* Found closing brace, look backwards for opening brace */
                        const char *open = search;
                        while (open >= caption_start && *open != '{') {
                            open--;
                        }
                        if (open >= caption_start && *open == '{' && (open + 1) < text_end_here) {
                            /* Check if it's a valid IAL pattern */
                            if ((open[1] == ':' || open[1] == '#' || open[1] == '.') &&
                                search > open) {
                                ial_start = open;
                                caption_end = open; /* Caption ends before IAL */
                                break;
                            }
                        }
                    }
                    search--;
                }

                /* Extract caption text (trim whitespace) */
                while (caption_start < caption_end && isspace((unsigned char)*caption_start)) {
                    caption_start++;
                }
                while (caption_end > caption_start && isspace((unsigned char)*(caption_end - 1))) {
                    caption_end--;
                }

                if (caption_end > caption_start) {
                    size_t len = caption_end - caption_start;
                    *caption_text = malloc(len + 1);
                    if (*caption_text) {
                        memcpy(*caption_text, caption_start, len);
                        (*caption_text)[len] = '\0';
                    }
                    /* Store full_text pointer in paragraph user_data so we can free it later */
                    char *existing_data = (char *)cmark_node_get_user_data(para);
                    if (existing_data) free(existing_data);
                    cmark_node_set_user_data(para, full_text);
                    if (original_text_ptr) {
                        *original_text_ptr = text;
                    }
                    return true;
                } else if (ial_start) {
                    /* Caption is empty but has IAL - still valid */
                    *caption_text = strdup("");
                    /* Store full_text pointer in paragraph user_data so we can free it later */
                    char *existing_data = (char *)cmark_node_get_user_data(para);
                    if (existing_data) free(existing_data);
                    cmark_node_set_user_data(para, full_text);
                    if (original_text_ptr) {
                        *original_text_ptr = text;
                    }
                    return true;
                }
            }
        }
    }

    /* If we got here, no caption format was found - free allocated memory */
    free(full_text);
    return false;
}

/**
 * Add caption to table and extract IAL attributes from caption text
 */
static void add_table_caption(cmark_node *table, const char *caption, const char *original_text) {
    if (!table || !caption) return;

    /* Extract IAL attributes from original text if present */
    apex_attributes *ial_attrs = NULL;
    if (original_text) {
        ial_attrs = parse_ial_from_text(original_text);
    }

    /* Store caption in user_data */
    char *existing_user_data = (char *)cmark_node_get_user_data(table);

    /* Check if caption is already in existing data */
    if (existing_user_data && strstr(existing_user_data, "data-caption=")) {
        if (ial_attrs) {
            apex_free_attributes(ial_attrs);
        }
        return; /* Caption already present */
    }

    char *attrs = malloc(strlen(caption) + 50);
    if (attrs) {
        snprintf(attrs, strlen(caption) + 50, " data-caption=\"%s\"", caption);

        /* Append IAL attributes if found */
        if (ial_attrs) {
            char *ial_str = attributes_to_html_string(ial_attrs);
            if (ial_str) {
                size_t new_len = strlen(attrs) + strlen(ial_str) + 1;
                char *new_attrs = realloc(attrs, new_len);
                if (new_attrs) {
                    attrs = new_attrs;
                    strcat(attrs, ial_str);
                    free(ial_str);
                } else {
                    free(ial_str);
                }
            }
            apex_free_attributes(ial_attrs);
        }

        /* Append to existing user_data if present */
        if (existing_user_data) {
            char *combined = malloc(strlen(existing_user_data) + strlen(attrs) + 1);
            if (combined) {
                strcpy(combined, existing_user_data);
                strcat(combined, attrs);
                free(existing_user_data); /* Free old user_data before replacing */
                cmark_node_set_user_data(table, combined);
                free(attrs);
            } else {
                free(attrs);
            }
        } else {
            cmark_node_set_user_data(table, attrs);
        }
    } else if (ial_attrs) {
        /* No caption text but IAL attributes found - apply them */
        char *ial_str = attributes_to_html_string(ial_attrs);
        if (ial_str) {
            char *existing = (char *)cmark_node_get_user_data(table);
            if (existing) {
                char *combined = malloc(strlen(existing) + strlen(ial_str) + 1);
                if (combined) {
                    strcpy(combined, existing);
                    strcat(combined, ial_str);
                    free(existing);
                    cmark_node_set_user_data(table, combined);
                    free(ial_str);
                } else {
                    free(ial_str);
                }
            } else {
                cmark_node_set_user_data(table, ial_str);
            }
        }
        apex_free_attributes(ial_attrs);
    }
}

/**
 * Process tables in document
 */
cmark_node *apex_process_advanced_tables(cmark_node *root) {
    if (!root) return root;

    cmark_iter *iter = cmark_iter_new(root);
    cmark_event_type ev_type;
    cmark_node *cur;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cur = cmark_iter_get_node(iter);

        if (ev_type == CMARK_EVENT_ENTER) {
            cmark_node_type type = cmark_node_get_type(cur);

            /* Process table */
            if (type == CMARK_NODE_TABLE) {
                /* Check for caption before table */
                /* Skip blank paragraphs to find the actual caption */
                cmark_node *prev = cmark_node_previous(cur);
                while (prev) {
                    /* Validate node before accessing - check if it's a valid node pointer */
                    /* Try to get type - if this segfaults, the node is invalid */
                    cmark_node_type prev_type;
                    /* Use a safe access pattern - get type first to validate node */
                    prev_type = cmark_node_get_type(prev);
                    /* Allow all valid node types - custom node types can be > 100 */
                    /* Only reject if type is 0 (which shouldn't happen) */
                    if (prev_type == 0) {
                        /* Invalid node type - likely corrupted pointer */
                        break;
                    }
                    /* Skip blank paragraphs and other non-paragraph nodes */
                    if (prev_type == CMARK_NODE_PARAGRAPH) {
                        /* Check if paragraph is blank (empty or only whitespace) */
                        cmark_node *child = cmark_node_first_child(prev);
                        bool is_blank = true;
                        if (child) {
                            cmark_node_type child_type = cmark_node_get_type(child);
                            if (child_type == CMARK_NODE_TEXT) {
                                const char *text = cmark_node_get_literal(child);
                                if (text) {
                                    const char *p = text;
                                    while (*p != '\0') {
                                        if (!isspace((unsigned char)*p)) {
                                            is_blank = false;
                                            break;
                                        }
                                        p++;
                                    }
                                } else {
                                    /* No text literal - treat as blank */
                                }
                            } else {
                                is_blank = false; /* Has non-text content */
                            }
                        }

                        if (!is_blank) {
                            /* Found a non-blank paragraph - check if it's a caption */
                            char *prev_data = (char *)cmark_node_get_user_data(prev);
                            /* Skip paragraphs that have already been used as captions */
                            if (!prev_data || !strstr(prev_data, "data-remove")) {
                                /* Quick check: does this paragraph start with [ or : (caption indicators)? */
                                /* This avoids calling is_table_caption_format on paragraphs that clearly aren't captions */
                                cmark_node *first_text = cmark_node_first_child(prev);
                                bool might_be_caption = false;
                                if (first_text) {
                                    cmark_node_type first_text_type = cmark_node_get_type(first_text);
                                    if (first_text_type == CMARK_NODE_TEXT) {
                                        const char *first_char = cmark_node_get_literal(first_text);
                                        if (first_char && strlen(first_char) > 0) {
                                            size_t first_char_len = strlen(first_char);
                                            /* Skip leading whitespace (up to 3 spaces) */
                                            int check_spaces = 0;
                                            while (check_spaces < 3 && check_spaces < (int)first_char_len && first_char[check_spaces] == ' ') {
                                                check_spaces++;
                                            }
                                            if (check_spaces < (int)first_char_len &&
                                                (first_char[check_spaces] == '[' || first_char[check_spaces] == ':')) {
                                                might_be_caption = true;
                                            }
                                        }
                                    }
                                }

                                if (might_be_caption) {
                                    char *caption = NULL;
                                    const char *original_text = NULL;
                                    /* We're already looking backwards from a table, so we know this paragraph
                                     * is before the table. We can check if it's a caption format without
                                     * needing to verify adjacency (which might fail if there are headers
                                     * or other nodes between the caption and table). */
                                    if (is_table_caption_format(prev, &caption, &original_text)) {
                                        add_table_caption(cur, caption, original_text);
                                        /* Free the allocated full_text that was stored in user_data */
                                        char *stored_text = (char *)cmark_node_get_user_data(prev);
                                        if (stored_text && stored_text == original_text) {
                                            free(stored_text);
                                        }
                                        /* Mark caption paragraph for removal so it is not reused */
                                        cmark_node_set_user_data(prev, strdup(" data-remove=\"true\""));
                                        free(caption);
                                        break; /* Found caption, stop looking */
                                    }
                                }
                            }
                            /* Found the nearest non-blank paragraph and it is not a caption.
                             * Stop searching so a distant caption cannot leak forward to this table. */
                            break;
                        }
                        /* This paragraph is blank, continue to previous sibling */
                        prev = cmark_node_previous(prev);
                        /* Continue loop to check the next node (prev is checked at start of while) */
                        continue;
                    } else {
                        /* Not a paragraph - could be a header or other block element */
                        /* Headers can appear between caption and table, so continue looking */
                        /* But stop for other block types that shouldn't have captions after them */
                        if (prev_type == CMARK_NODE_HEADING) {
                            /* The closest previous non-blank node is a heading, so stop.
                             * Do not skip backwards across headings when resolving captions. */
                            break;
                        }
                        /* Other block type - stop looking */
                        break;
                    }
                }

                /* Check for caption after table */
                cmark_node *next = cmark_node_next(cur);
                if (next) {
                    cmark_node_type next_type = cmark_node_get_type(next);
                    char *next_data = (char *)cmark_node_get_user_data(next);
                    /* Skip paragraphs that have already been used as captions */
                    if (!next_data || !strstr(next_data, "data-remove")) {
                        char *caption = NULL;
                        const char *original_text = NULL;
                        if (is_table_caption(next, &caption, &original_text)) {
                            add_table_caption(cur, caption, original_text);
                            /* Free the allocated full_text that was stored in user_data */
                            char *stored_text = (char *)cmark_node_get_user_data(next);
                            if (stored_text && stored_text == original_text) {
                                free(stored_text);
                            }
                            /* Mark caption paragraph for removal so it is not reused */
                            cmark_node_set_user_data(next, strdup(" data-remove=\"true\""));
                            free(caption);
                        } else {
                            /* Also check if it's a : Caption format (which is_table_caption might not handle) */
                            if (next_type == CMARK_NODE_PARAGRAPH) {
                                if (is_table_caption_format(next, &caption, &original_text)) {
                                    add_table_caption(cur, caption, original_text);
                                    char *stored_text = (char *)cmark_node_get_user_data(next);
                                    if (stored_text && stored_text == original_text) {
                                        /* stored_text is the full_text allocated by is_table_caption_format */
                                        free(stored_text);
                                    }
                                    /* Mark caption paragraph for removal so it is not reused */
                                    cmark_node_set_user_data(next, strdup(" data-remove=\"true\""));
                                    free(caption);
                                }
                            }
                        }
                    }
                }

                /* Check for caption rows BEFORE processing spans */
                /* Caption rows should be detected and removed before colspan processing */
                cmark_node *row_check = cmark_node_first_child(cur);
                cmark_node *caption_row = NULL;
                while (row_check) {
                    if (cmark_node_get_type(row_check) == CMARK_NODE_TABLE_ROW && is_caption_row(row_check)) {
                        caption_row = row_check;
                        /* Continue to find the last caption row if there are multiple */
                    }
                    row_check = cmark_node_next(row_check);
                }

                /* If we found a caption row, extract the caption and mark row for removal */
                if (caption_row) {
                    cmark_node *cell = cmark_node_first_child(caption_row);
                    if (cell) {
                        /* Find the cell with caption text (might be first cell, might span) */
                        while (cell) {
                            if (cmark_node_get_type(cell) == CMARK_NODE_TABLE_CELL) {
                                cmark_node *text_node = cmark_node_first_child(cell);
                                if (text_node && cmark_node_get_type(text_node) == CMARK_NODE_TEXT) {
                                    const char *text = cmark_node_get_literal(text_node);
                                    if (text && text[0] == '[') {
                                        const char *end = strchr(text + 1, ']');
                                        if (end) {
                                            size_t caption_len = end - text - 1;
                                            char *caption = malloc(caption_len + 1);
                                            if (caption) {
                                                memcpy(caption, text + 1, caption_len);
                                                caption[caption_len] = '\0';
                                                add_table_caption(cur, caption, text);
                                                free(caption);
                                                /* Mark the entire row for removal */
                                                char *existing = (char *)cmark_node_get_user_data(caption_row);
                                                if (existing) free(existing);
                                                cmark_node_set_user_data(caption_row, strdup(" data-remove=\"true\""));
                                                break; /* Found caption, done */
                                            }
                                        }
                                    }
                                }
                            }
                            cell = cmark_node_next(cell);
                        }
                    }
                }

                /* Process spans - this also detects tfoot rows */
                process_table_spans(cur);
            }
        }
    }

    cmark_iter_free(iter);
    return root;
}

/**
 * Custom HTML renderer for tables with spans and captions
 */
__attribute__((unused))
static void html_render_table(cmark_syntax_extension *ext,
                              struct cmark_html_renderer *renderer,
                              cmark_node *node,
                              cmark_event_type ev_type,
                              int options) {
    (void)ext;
    (void)options;
    cmark_strbuf *html = renderer->html;
    cmark_node_type type = cmark_node_get_type(node);

    if (ev_type == CMARK_EVENT_ENTER && type == CMARK_NODE_TABLE) {
        /* Check for caption */
        char *user_data = (char *)cmark_node_get_user_data(node);
        if (user_data && strstr(user_data, "data-caption=")) {
            char caption[512];
            if (sscanf(user_data, " data-caption=\"%[^\"]\"", caption) == 1) {
                cmark_strbuf_puts(html, "<figure class=\"table-figure\">\n");
                cmark_strbuf_puts(html, "<figcaption>");
                cmark_strbuf_puts(html, caption);
                cmark_strbuf_puts(html, "</figcaption>\n");
            }
        }
        /* Let default renderer handle the table tag */
        return;
    } else if (ev_type == CMARK_EVENT_EXIT && type == CMARK_NODE_TABLE) {
        char *user_data = (char *)cmark_node_get_user_data(node);
        if (user_data && strstr(user_data, "data-caption=")) {
            cmark_strbuf_puts(html, "</figure>\n");
        }
        return;
    }

    /* Handle ALL table cells to properly handle removal and spans */
    if (type == CMARK_NODE_TABLE_CELL) {
        char *attrs = (char *)cmark_node_get_user_data(node);

        /* Skip cells marked for removal entirely (don't render enter or exit) */
        if (attrs && strstr(attrs, "data-remove")) {
            return; /* Don't render this cell at all */
        }

        /* If this cell has rowspan/colspan, we need custom rendering */
        if (ev_type == CMARK_EVENT_ENTER && attrs &&
            (strstr(attrs, "colspan=") || strstr(attrs, "rowspan="))) {
            /* Determine if header or data cell by checking parent row */
            bool is_header = false;
            cmark_node *row = cmark_node_parent(node);
            if (row) {
                cmark_node *parent = cmark_node_parent(row);
                if (parent && cmark_node_get_type(parent) == CMARK_NODE_TABLE) {
                    /* First row is header in cmark-gfm tables */
                    cmark_node *first_row = cmark_node_first_child(parent);
                    is_header = (first_row == row);
                }
            }

            /* Output opening tag */
            if (is_header) {
                cmark_strbuf_puts(html, "<th");
            } else {
                cmark_strbuf_puts(html, "<td");
            }

            /* Add rowspan/colspan attributes */
            cmark_strbuf_puts(html, attrs);

            cmark_strbuf_putc(html, '>');
            return; /* We've handled the opening tag */
        } else if (ev_type == CMARK_EVENT_EXIT && attrs &&
                   (strstr(attrs, "colspan=") || strstr(attrs, "rowspan="))) {
            /* Closing tag */
            bool is_header = false;
            cmark_node *row = cmark_node_parent(node);
            if (row) {
                cmark_node *parent = cmark_node_parent(row);
                if (parent && cmark_node_get_type(parent) == CMARK_NODE_TABLE) {
                    cmark_node *first_row = cmark_node_first_child(parent);
                    is_header = (first_row == row);
                }
            }

            if (is_header) {
                cmark_strbuf_puts(html, "</th>\n");
            } else {
                cmark_strbuf_puts(html, "</td>\n");
            }
            return; /* We've handled the closing tag */
        }
        /* For normal cells without spans, let default renderer handle them */
    }
}

/**
 * Postprocess function
 */
static cmark_node *postprocess(cmark_syntax_extension *ext,
                               cmark_parser *parser,
                               cmark_node *root) {
    (void)ext;
    (void)parser;
    return apex_process_advanced_tables(root);
}

/**
 * Create advanced tables extension
 */
cmark_syntax_extension *create_advanced_tables_extension(bool per_cell_alignment) {
    cmark_syntax_extension *ext = cmark_syntax_extension_new("advanced_tables");
    if (!ext) return NULL;

    /* Store per_cell_alignment flag in static variable */
    g_per_cell_alignment = per_cell_alignment;

    /* Set postprocess callback to add span/caption attributes to AST */
    cmark_syntax_extension_set_postprocess_func(ext, postprocess);

    /* NOTE: We don't use html_render_func here because it conflicts with GFM table renderer.
     * Instead, we do HTML postprocessing in apex.c after rendering.
     * The post-processing should remove cells with data-remove and inject colspan/rowspan attributes. */
    /* cmark_syntax_extension_set_html_render_func(ext, html_render_table); */

    /* Register to handle table and table cell rendering */
    cmark_syntax_extension_set_can_contain_func(ext, NULL);

    return ext;
}

