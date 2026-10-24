/**
 * Grid Tables Extension for Apex
 * Implementation
 *
 * Preprocessing extension that converts Pandoc grid table syntax to
 * pipe table format before the regular cmark parser runs.
 */

#include "grid_tables.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_COLUMNS 64

/**
 * Check if a line starts a grid table block (separator row: +---+ or +===+).
 * Bare lines starting with '+' alone are not grid tables.
 */
static bool is_grid_table_start(const char *line) {
    if (!line) return false;

    while (*line == ' ' || *line == '\t') {
        line++;
    }

    if (*line != '+') return false;
    line++;

    while (*line == ' ' || *line == '\t') {
        line++;
    }

    return *line == '-' || *line == '=';
}

/**
 * Check if a line is a grid table separator (starts with + followed by - or =)
 * A separator row contains only +, -, =, :, spaces
 * It should NOT contain | characters (pipes indicate cell boundaries, not separators)
 *
 * IMPORTANT: This function checks if a line is a TABLE-LEVEL separator (separating rows).
 * Nested separators within cells (like +-------+ within | Cell +-------+ |) are NOT
 * table separators - they're just part of the cell content.
 */
static bool is_grid_table_separator(const char *line) {
    if (!line) return false;

    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p != '+') return false;
    p++;

    /* Check for pattern: +---+ or +===+ */
    /* A separator should only contain +, -, =, :, and spaces - NO pipes */
    /* Note: === may have been converted to <mark> tags by earlier preprocessing,
     * so we need to handle that case too */
    bool has_dash_or_equal = false;
    bool in_mark_tag = false;
    int mark_tag_depth = 0;

    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '|') {
            /* Pipes indicate this is a content row, not a separator */
            /* However, nested separators within cells might have pipes around them,
             * so we need to check if this is a full-line separator or nested */
            /* If we see a pipe, this is NOT a table-level separator */
            return false;
        } else if (*p == '<' && strncmp(p, "<mark>", 6) == 0) {
            /* Handle <mark> tag (converted from ===) */
            in_mark_tag = true;
            mark_tag_depth++;
            p += 6;
            continue;
        } else if (*p == '<' && strncmp(p, "</mark>", 7) == 0) {
            /* Handle </mark> tag */
            in_mark_tag = false;
            mark_tag_depth--;
            if (mark_tag_depth == 0) {
                has_dash_or_equal = true; /* Treat <mark> as equivalent to = */
            }
            p += 7;
            continue;
        } else if (in_mark_tag) {
            /* Inside <mark> tag, skip content */
            p++;
            continue;
        } else if (*p == '-' || *p == '=') {
            has_dash_or_equal = true;
        } else if (*p != ':' && *p != '+' && *p != ' ' && *p != '\t') {
            /* Contains other characters - not a separator */
            return false;
        }
        p++;
    }

    return has_dash_or_equal;
}

static bool grid_block_has_separator(char **lines, size_t line_count) {
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && is_grid_table_separator(lines[i])) {
            return true;
        }
    }
    return false;
}

static bool grid_block_has_content_rows(char **lines, size_t line_count) {
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && strchr(lines[i], '|') && !is_grid_table_separator(lines[i])) {
            return true;
        }
    }
    return false;
}

/**
 * Check if a line is a header separator (contains = characters)
 */
static bool is_header_separator(const char *line) {
    if (!line) return false;

    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p != '+') return false;

    /* Check if line contains '=' characters (header separator) */
    /* Note: === may have been converted to <mark> tags by earlier preprocessing */
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '<' && strncmp(p, "<mark>", 6) == 0) {
            /* Handle <mark> tag (converted from ===) - treat as = */
            return true;
        } else if (*p == '=') {
            return true;
        }
        p++;
    }

    return false;
}

/**
 * Parse alignment from a separator line
 * Returns alignment array (0=left, 1=center, 2=right)
 * Column count is returned via column_count parameter
 * Format: +:===:+ (center), +===:+ (right), +:===+ (left with colon), +---+ (left)
 */
static void parse_alignment(const char *line, int *alignments, size_t *column_count) {
    if (!line || !alignments || !column_count) return;

    *column_count = 0;

    /* Skip whitespace */
    const char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p != '+') return;
    p++;

    size_t col = 0;
    bool left_colon = false;
    bool right_colon = false;

    while (*p && col < MAX_COLUMNS) {
        if (*p == '+') {
            /* Column boundary - determine alignment */
            if (left_colon && right_colon) {
                alignments[col] = 1; /* Center: :---: or :===: */
            } else if (right_colon) {
                alignments[col] = 2; /* Right: ---: or ===: */
            } else {
                alignments[col] = 0; /* Left: --- or === or :--- */
            }
            col++;
            left_colon = false;
            right_colon = false;
        } else if (*p == ':') {
            if (p == line + 1 || (p > line && (p[-1] == '+' || p[-1] == ' ' || p[-1] == '\t'))) {
                left_colon = true;
            } else {
                right_colon = true;
            }
        } else if (*p == '-' || *p == '=') {
            /* Part of separator, continue */
        }
        p++;
    }

    *column_count = col;
}

/**
 * Extract cell count from a single line (for column count inference)
 */
static size_t count_cells_in_line(const char *line) {
    if (!line) return 0;

    const char *p = line;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    /* Skip leading + if present */
    if (*p == '+') {
        p++;
    }

    bool starts_with_pipe = (*p == '|');
    size_t pipe_count = 0;

    /* Count | characters */
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == '|') {
            pipe_count++;
        }
        p++;
    }

    /* Number of cells: if starts with |, cells = pipes - 1, else pipes + 1 */
    if (starts_with_pipe) {
        return pipe_count > 0 ? pipe_count - 1 : 0;
    } else {
        return pipe_count + 1;
    }
}

/**
 * Check if cell content contains block-level elements
 * (code blocks, lists, multiple paragraphs, etc.)
 */
static bool cell_has_block_elements(const char *content) {
    if (!content) return false;

    const char *p = content;
    int list_marker_count = 0;
    int paragraph_count = 0;
    bool prev_was_blank = false;

    while (*p) {
        /* Check for code blocks */
        if (strncmp(p, "```", 3) == 0 || strncmp(p, "~~~", 3) == 0) {
            return true; /* Code block found */
        }

        /* Check for list markers */
        if ((*p == '-' || *p == '*' || *p == '+') &&
            (p[1] == ' ' || p[1] == '\t')) {
            list_marker_count++;
        }
        if (isdigit((unsigned char)*p)) {
            const char *num_start = p;
            while (isdigit((unsigned char)*p)) p++;
            if (*p == '.' && (p[1] == ' ' || p[1] == '\t')) {
                list_marker_count++;
            }
            p = num_start;
        }

        /* Check for multiple paragraphs (blank line between content) */
        if (*p == '\n') {
            if (prev_was_blank && paragraph_count == 0) {
                /* Found blank line - check if there's content before and after */
                const char *before = p - 1;
                while (before > content && (*before == ' ' || *before == '\t' || *before == '\r')) {
                    before--;
                }
                if (before > content) {
                    const char *after = p + 1;
                    while (*after == '\n' || *after == '\r' || *after == ' ' || *after == '\t') {
                        after++;
                    }
                    if (*after && *after != '\0') {
                        paragraph_count++;
                    }
                }
            }
            prev_was_blank = true;
        } else if (*p != ' ' && *p != '\t' && *p != '\r') {
            prev_was_blank = false;
        }

        p++;
    }

    /* If we found lists or multiple paragraphs, it has block elements */
    return (list_marker_count > 0 || paragraph_count > 0);
}

/**
 * Convert block-level markdown in cell to HTML format
 * This allows block elements to be preserved in pipe table cells
 */
static char *convert_cell_block_elements_to_html(const char *content) __attribute__((unused));
static char *convert_cell_block_elements_to_html(const char *content) {
    if (!content) return NULL;

    /* For now, preserve content as-is but wrap in a div for block elements */
    /* This is a simplified approach - full implementation would parse and convert each block type */
    size_t len = strlen(content);
    size_t html_len = len + 100; /* Extra space for HTML tags */
    char *html = malloc(html_len);
    if (!html) return strdup(content); /* Fallback to original */

    /* Check if it has block elements */
    if (!cell_has_block_elements(content)) {
        /* No block elements - return as-is */
        strcpy(html, content);
        return html;
    }

    /* Has block elements - wrap in div and preserve newlines as <br> or keep structure */
    /* Simple approach: wrap in <div> and preserve line breaks */
    char *p = html;
    const char *src = content;

    strcpy(p, "<div>");
    p += 5;

    /* Convert newlines to <br> for simple cases, or preserve structure for code blocks */
    bool in_code = false;
    while (*src) {
        if (strncmp(src, "```", 3) == 0 || strncmp(src, "~~~", 3) == 0) {
            in_code = !in_code;
            /* Copy code block marker */
            *p++ = *src++;
            *p++ = *src++;
            *p++ = *src++;
            continue;
        }

        if (*src == '\n') {
            if (in_code) {
                *p++ = '\n'; /* Preserve newlines in code */
            } else {
                /* Convert to <br> for paragraphs, but preserve double newlines for block separation */
                const char *next = src + 1;
                while (*next == ' ' || *next == '\t' || *next == '\r') next++;
                if (*next == '\n' || *next == '\0') {
                    /* Double newline or end - preserve as paragraph break */
                    strcpy(p, "</p><p>");
                    p += 7;
                } else {
                    strcpy(p, "<br>");
                    p += 4;
                }
            }
            src++;
        } else {
            *p++ = *src++;
        }

        /* Check buffer size */
        if ((size_t)(p - html) > html_len - 20) {
            size_t current_len = p - html;
            html_len *= 2;
            char *new_html = realloc(html, html_len);
            if (!new_html) {
                free(html);
                return strdup(content);
            }
            html = new_html;
            p = html + current_len;
        }
    }

    strcpy(p, "</div>");
    p += 6;
    *p = '\0';

    return html;
}

/**
 * Check if two lines have the same column structure (same number of pipes in same positions)
 * This helps determine if lines should be combined into multi-line cells
 */
static bool has_same_column_structure(const char *line1, const char *line2) {
    if (!line1 || !line2) return false;

    const char *p1 = line1;
    const char *p2 = line2;

    /* Skip leading whitespace */
    while (*p1 == ' ' || *p1 == '\t') p1++;
    while (*p2 == ' ' || *p2 == '\t') p2++;

    /* Skip leading + if present */
    if (*p1 == '+') p1++;
    if (*p2 == '+') p2++;

    /* Count pipes in both lines - they should have the same number */
    size_t pipe_count1 = 0, pipe_count2 = 0;
    const char *q1 = p1, *q2 = p2;
    while (*q1 && *q1 != '\n' && *q1 != '\r') {
        if (*q1 == '|') pipe_count1++;
        q1++;
    }
    while (*q2 && *q2 != '\n' && *q2 != '\r') {
        if (*q2 == '|') pipe_count2++;
        q2++;
    }

    /* Lines must have the same number of pipes to have the same column structure */
    return pipe_count1 > 0 && pipe_count1 == pipe_count2;
}

static char *str_append(char *dest, const char *add);

/**
 * Append a line to a string with newline separator
 */
static char *str_append_line(char *dest, const char *line) {
    if (!line) return dest;
    if (!dest) return str_append(NULL, line);
    dest = str_append(dest, "\n");
    return str_append(dest, line);
}

static char *str_append(char *dest, const char *add) {
    if (!add) return dest;
    size_t old_len = dest ? strlen(dest) : 0;
    size_t add_len = strlen(add);
    char *new_str = realloc(dest, old_len + add_len + 1);
    if (!new_str) return dest;
    memcpy(new_str + old_len, add, add_len + 1);
    return new_str;
}

/**
 * Extract cell content from grid table row lines
 * Grid table rows can span multiple lines until the next separator
 * Returns array of cell strings (caller must free)
 */
static char **extract_cells_from_row_lines(char **row_lines, size_t row_line_count, size_t *cell_count) {
    if (!row_lines || row_line_count == 0 || !cell_count) return NULL;

    char **cells = malloc(MAX_COLUMNS * sizeof(char*));
    if (!cells) return NULL;

    *cell_count = 0;

    /* Collect all cell content from all row lines */
    for (size_t line_idx = 0; line_idx < row_line_count; line_idx++) {
        const char *line = row_lines[line_idx];
        if (!line) continue;

        /* Skip separator lines - they shouldn't be in row_lines, but double-check */
        if (is_grid_table_separator(line)) {
            /* Nested grid separator inside colspan cell: preserve as literal content */
            if (*cell_count > 0) {
                size_t idx = (*cell_count > 0) ? 0 : 0;
                if (cells[idx]) {
                    cells[idx] = str_append_line(cells[idx], line);
                }
            }
            continue;
        }

        /* Skip whitespace */
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        /* Skip leading + if present (shouldn't be in content rows, but handle it) */
        if (*p == '+') {
            p++;
        }

        /* Extract cells from this line */
        const char *cell_start = NULL;
        size_t current_col = 0;
        bool first_pipe = true;

        while (*p && current_col < MAX_COLUMNS) {
            if (*p == '|') {
                /* Cell boundary */
                if (cell_start) {
                    /* Trim trailing whitespace */
                    const char *cell_end = p;
                    while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) {
                        cell_end--;
                    }

                    size_t cell_len = cell_end - cell_start;

                    if (current_col < *cell_count && cells[current_col]) {
                        /* Append to existing cell (multi-line cell) */
                        /* Join with newline to preserve block element structure */
                        size_t old_len = strlen(cells[current_col]);
                        size_t new_len = old_len + cell_len + 1; /* +1 for newline */
                        char *new_cell = realloc(cells[current_col], new_len + 1);
                        if (new_cell) {
                            cells[current_col] = new_cell;
                            if (old_len > 0) {
                                cells[current_col][old_len] = '\n'; /* Join with newline for block elements */
                                old_len++;
                            }
                            memcpy(cells[current_col] + old_len, cell_start, cell_len);
                            cells[current_col][old_len + cell_len] = '\0';
                        }
                    } else {
                        /* New cell */
                        if (current_col >= *cell_count) {
                            *cell_count = current_col + 1;
                        }
                        cells[current_col] = malloc(cell_len + 1);
                        if (cells[current_col]) {
                            memcpy(cells[current_col], cell_start, cell_len);
                            cells[current_col][cell_len] = '\0';
                        }
                    }
                    current_col++;
                } else if (!first_pipe) {
                    /* Empty cell (not at start of line) */
                    if (current_col >= *cell_count) {
                        *cell_count = current_col + 1;
                    }
                    cells[current_col] = malloc(1);
                    if (cells[current_col]) {
                        cells[current_col][0] = '\0';
                    }
                    current_col++;
                }
                /* Skip the first pipe (leading pipe) - don't create a cell for it */
                first_pipe = false;
                cell_start = p + 1;
            } else if (*p == '\n' || *p == '\r') {
                break;
            }
            p++;
        }

        /* Handle final cell if line doesn't end with | */
        if (cell_start && *cell_start && current_col < MAX_COLUMNS) {
            const char *cell_end = p;
            while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) {
                cell_end--;
            }

            size_t cell_len = cell_end - cell_start;
            if (current_col >= *cell_count) {
                *cell_count = current_col + 1;
            }

            if (cells[current_col]) {
                /* Append to existing */
                size_t old_len = strlen(cells[current_col]);
                size_t new_len = old_len + cell_len + 1;
                char *new_cell = realloc(cells[current_col], new_len + 1);
                if (new_cell) {
                    cells[current_col] = new_cell;
                    if (old_len > 0) {
                        cells[current_col][old_len] = '\n'; /* Join with newline */
                        old_len++;
                    }
                    memcpy(cells[current_col] + old_len, cell_start, cell_len);
                    cells[current_col][old_len + cell_len] = '\0';
                }
            } else {
                cells[current_col] = malloc(cell_len + 1);
                if (cells[current_col]) {
                    memcpy(cells[current_col], cell_start, cell_len);
                    cells[current_col][cell_len] = '\0';
                }
            }
        }
    }

    return cells;
}

/**
 * Create pipe table separator row from alignment array
 */
static char *create_pipe_separator(int *alignments, size_t column_count) {
    if (!alignments || column_count == 0) return NULL;

    /* Calculate size needed */
    size_t len = column_count * 8 + 2; /* | :---: | ... | */
    char *sep = malloc(len);
    if (!sep) return NULL;

    char *p = sep;

    for (size_t i = 0; i < column_count; i++) {
        *p++ = '|';

        int align = alignments[i];
        if (align == 1) {
            /* Center: :---: */
            *p++ = ' ';
            *p++ = ':';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ':';
            *p++ = ' ';
        } else if (align == 2) {
            /* Right: ---: */
            *p++ = ' ';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ':';
            *p++ = ' ';
        } else {
            /* Left: --- */
            *p++ = ' ';
            *p++ = '-';
            *p++ = '-';
            *p++ = '-';
            *p++ = ' ';
        }
    }

    *p++ = '|';

    *p = '\0';
    return sep;
}

static bool is_nested_grid_separator(const char *line, size_t table_cols) {
    if (!line || !is_grid_table_separator(line)) return false;
    if (is_header_separator(line)) return false;
    size_t sep_cols = 0;
    int tmp[MAX_COLUMNS] = {0};
    parse_alignment(line, tmp, &sep_cols);
    return sep_cols > table_cols;
}

/**
 * Check if table contains partial separators (|...+...+...) that require HTML output.
 */
static bool table_has_nested_grid(char **lines, size_t line_count, size_t table_cols) {
    if (table_cols == 0) return false;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && is_nested_grid_separator(lines[i], table_cols)) return true;
    }
    return false;
}

static bool table_has_partial_separators(char **lines, size_t line_count) {
    for (size_t i = 0; i < line_count; i++) {
        if (!lines[i]) continue;
        const char *p = lines[i];
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '+' && *p != '|') continue;
        if (is_grid_table_separator(lines[i])) continue;
        /* Mixed row: starts with | and contains internal +---+ pattern */
        if (*p == '|') {
            bool seen_pipe = false;
            for (const char *q = p; *q && *q != '\n'; q++) {
                if (*q == '|') seen_pipe = true;
                if (seen_pipe && *q == '+' && q[1] == '-') return true;
            }
        }
    }
    return false;
}

static bool table_has_multiline_cells(char **lines, size_t line_count) {
    for (size_t i = 0; i + 1 < line_count; i++) {
        if (!lines[i] || !lines[i + 1]) continue;
        if (is_grid_table_separator(lines[i]) || is_grid_table_separator(lines[i + 1])) continue;
        if (strchr(lines[i], '|') && strchr(lines[i + 1], '|') &&
            has_same_column_structure(lines[i], lines[i + 1])) {
            return true;
        }
    }
    return false;
}

static bool is_full_row_colspan_line(const char *line) {
    return line && count_cells_in_line(line) == 1;
}

/**
 * Parse | c0 | c1 | c2 | ... into cell strings (trimmed). Returns cell count.
 */
static size_t parse_grid_row_cells(const char *line, char **cells, size_t max_cells) {
    if (!line || !cells || max_cells == 0) return 0;

    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '+') return 0;

    size_t count = 0;
    const char *cell_start = NULL;
    bool first_pipe = true;

    while (*p && count < max_cells) {
        if (*p == '|') {
            if (cell_start) {
                const char *cell_end = p;
                while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) cell_end--;
                size_t len = (size_t)(cell_end - cell_start);
                cells[count] = malloc(len + 1);
                if (cells[count]) {
                    memcpy(cells[count], cell_start, len);
                    cells[count][len] = '\0';
                }
                count++;
            } else if (!first_pipe) {
                cells[count] = strdup("");
                if (cells[count]) count++;
            }
            first_pipe = false;
            cell_start = p + 1;
        }
        p++;
    }

    if (cell_start && count < max_cells) {
        const char *cell_end = p;
        while (cell_end > cell_start && isspace((unsigned char)cell_end[-1])) cell_end--;
        if (cell_end > cell_start) {
            size_t len = (size_t)(cell_end - cell_start);
            cells[count] = malloc(len + 1);
            if (cells[count]) {
                memcpy(cells[count], cell_start, len);
                cells[count][len] = '\0';
                count++;
            }
        }
    }

    return count;
}

static bool line_is_partial_col0_separator(const char *line) {
    if (!line) return false;
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '+' && p[1] == '-') return true;
    return false;
}

static char *extract_partial_col0_text(const char *line) {
    if (!line) return NULL;
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    const char *start = p;
    const char *plus = NULL;
    for (const char *q = p; *q && *q != '\n'; q++) {
        if (*q == '+' && q[1] == '-') {
            plus = q;
            break;
        }
    }
    if (!plus) return NULL;

    const char *end = plus;
    while (end > start && isspace((unsigned char)end[-1])) end--;
    if (end <= start) return NULL;

    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static char *append_col0_label(char *label, const char *add) {
    if (!add || !*add) return label;
    if (!label) return strdup(add);
    size_t old = strlen(label);
    size_t add_len = strlen(add);
    char *out = realloc(label, old + add_len + 2);
    if (!out) return label;
    if (old > 0) {
        out[old] = ' ';
        out[old + 1] = '\0';
        old++;
    }
    strcat(out, add);
    return out;
}

static void write_html_cell(char **p, size_t *remaining, char **output, size_t *total_size,
                            const char *tag, const char *content, size_t colspan, size_t rowspan,
                            bool markdown) {
    char open[256];
    if (colspan > 1 && rowspan > 1) {
        snprintf(open, sizeof(open), "<%s%s colspan=\"%zu\" rowspan=\"%zu\">\n\n",
                 tag, markdown ? " markdown=\"1\"" : "", colspan, rowspan);
    } else if (colspan > 1) {
        snprintf(open, sizeof(open), "<%s%s colspan=\"%zu\">\n\n",
                 tag, markdown ? " markdown=\"1\"" : "", colspan);
    } else if (rowspan > 1) {
        snprintf(open, sizeof(open), "<%s%s rowspan=\"%zu\">\n\n",
                 tag, markdown ? " markdown=\"1\"" : "", rowspan);
    } else {
        snprintf(open, sizeof(open), "<%s%s>\n\n", tag, markdown ? " markdown=\"1\"" : "");
    }

    size_t need = strlen(open) + (content ? strlen(content) : 0) + 32;
    char *base = *output;
    size_t used = (size_t)(*p - base);
    if (*remaining < need) {
        *total_size = (used + need + 1) * 2;
        char *grown = realloc(base, *total_size);
        if (!grown) return;
        *output = grown;
        *p = grown + used;
        *remaining = *total_size - used;
    }

    size_t ol = strlen(open);
    memcpy(*p, open, ol);
    *p += ol;
    *remaining -= ol;

    if (content && *content) {
        size_t cl = strlen(content);
        memcpy(*p, content, cl);
        *p += cl;
        *remaining -= cl;
    }

    char close[16];
    snprintf(close, sizeof(close), "\n\n</%s>\n", tag);
    size_t cll = strlen(close);
    memcpy(*p, close, cll);
    *p += cll;
    *remaining -= cll;
}

/**
 * Convert grid tables with in-row partial separators (Pandoc-style).
 */
static char *convert_partial_grid_table_to_html(char **lines, size_t line_count,
                                                size_t column_count) {
    if (!lines || line_count == 0 || column_count == 0) return NULL;

    size_t header_sep_idx = line_count;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && is_header_separator(lines[i])) {
            header_sep_idx = i;
            break;
        }
    }

    size_t total_size = 4096;
    char *output = malloc(total_size);
    if (!output) return NULL;
    char *p = output;
    size_t remaining = total_size;

    memcpy(p, "\n\n<table>\n", 10);
    p += 10;
    remaining -= 10;

    /* Header rows before +===+ separator */
    for (size_t i = 0; i < header_sep_idx; i++) {
        if (!lines[i] || is_grid_table_separator(lines[i]) || !strchr(lines[i], '|')) continue;

        char *cells[MAX_COLUMNS] = {0};
        size_t ncells = parse_grid_row_cells(lines[i], cells, MAX_COLUMNS);
        if (ncells == 0) continue;

        const char *thead = "<thead>\n<tr>\n";
        size_t tl = strlen(thead);
        memcpy(p, thead, tl);
        p += tl;
        remaining -= tl;

        size_t first_span = (ncells < column_count) ? (column_count - ncells + 1) : 1;
        for (size_t c = 0; c < ncells; c++) {
            size_t span = (ncells < column_count && c == 0) ? first_span : 1;
            write_html_cell(&p, &remaining, &output, &total_size, "th", cells[c], span, 1, true);
            free(cells[c]);
        }

        const char *end = "</tr>\n</thead>\n<tbody>\n";
        size_t el = strlen(end);
        memcpy(p, end, el);
        p += el;
        remaining -= el;
        break;
    }

    if (header_sep_idx >= line_count) {
        const char *tbody = "<tbody>\n";
        size_t bl = strlen(tbody);
        memcpy(p, tbody, bl);
        p += bl;
        remaining -= bl;
    }

    /* Body rows after header separator */
    char *col0_label = NULL;
    char **row_c1 = NULL;
    char **row_c2 = NULL;
    size_t row_count = 0;
    size_t row_cap = 8;

    row_c1 = malloc(row_cap * sizeof(char*));
    row_c2 = malloc(row_cap * sizeof(char*));
    if (!row_c1 || !row_c2) {
        free(col0_label);
        free(row_c1);
        free(row_c2);
        free(output);
        return NULL;
    }

    for (size_t i = header_sep_idx + 1; i < line_count; i++) {
        if (!lines[i] || is_grid_table_separator(lines[i])) continue;

        if (line_is_partial_col0_separator(lines[i])) continue;

        char *partial = extract_partial_col0_text(lines[i]);
        if (partial) {
            col0_label = append_col0_label(col0_label, partial);
            free(partial);
            continue;
        }

        char *cells[MAX_COLUMNS] = {0};
        size_t ncells = parse_grid_row_cells(lines[i], cells, column_count);
        if (ncells == 0) {
            continue;
        }

        if (ncells >= 1 && cells[0] && *cells[0]) {
            col0_label = append_col0_label(col0_label, cells[0]);
        }

        if (ncells >= 3) {
            if (row_count >= row_cap) {
                row_cap *= 2;
                row_c1 = realloc(row_c1, row_cap * sizeof(char*));
                row_c2 = realloc(row_c2, row_cap * sizeof(char*));
            }
            row_c1[row_count] = cells[1] ? strdup(cells[1]) : strdup("");
            row_c2[row_count] = cells[2] ? strdup(cells[2]) : strdup("");
            free(cells[0]);
            free(cells[1]);
            free(cells[2]);
            row_count++;
        } else {
            for (size_t c = 0; c < ncells; c++) free(cells[c]);
        }
    }

    for (size_t r = 0; r < row_count; r++) {
        const char *tr = "<tr>\n";
        size_t trl = strlen(tr);
        if (trl < remaining) {
            memcpy(p, tr, trl);
            p += trl;
            remaining -= trl;
        }

        if (r == 0 && col0_label && *col0_label) {
            write_html_cell(&p, &remaining, &output, &total_size, "td", col0_label,
                              1, row_count, true);
        } else if (r > 0) {
            /* rowspan covers later rows */
        }

        write_html_cell(&p, &remaining, &output, &total_size, "td", row_c1[r], 1, 1, true);
        write_html_cell(&p, &remaining, &output, &total_size, "td", row_c2[r], 1, 1, true);

        const char *endtr = "</tr>\n";
        size_t etl = strlen(endtr);
        if (etl < remaining) {
            memcpy(p, endtr, etl);
            p += etl;
            remaining -= etl;
        }
    }

    free(col0_label);
    for (size_t r = 0; r < row_count; r++) {
        free(row_c1[r]);
        free(row_c2[r]);
    }
    free(row_c1);
    free(row_c2);

    const char *close = "</tbody>\n</table>\n\n";
    size_t cl = strlen(close);
    if (cl < remaining) {
        memcpy(p, close, cl);
        p += cl;
        remaining -= cl;
    }
    *p = '\0';
    return output;
}

char *apex_preprocess_grid_tables(const char *text);

static char *preprocess_grid_text(const char *text, bool embed_as_html);
static char *convert_collected_grid_table(char **table_lines, size_t table_line_count,
                                          bool force_html);

static char *preprocess_cell_grid_content(const char *content) {
    if (!content || !strchr(content, '+')) return NULL;
    /* Pipe tables are not parsed inside markdown="1" cells; emit HTML for embedded grids. */
    return preprocess_grid_text(content, true);
}

static char *escape_pipes_in_multiline_cell(const char *content) {
    if (!content || !strchr(content, '\n')) return NULL;
    if (!strchr(content, '|') && !strchr(content, '-')) return NULL;

    size_t len = strlen(content);
    char *out = malloc(len * 6 + 1);
    if (!out) return NULL;

    char *p = out;
    for (const char *s = content; *s; s++) {
        if (*s == '|') {
            memcpy(p, "&#124;", 6);
            p += 6;
        } else if (*s == '\n') {
            memcpy(p, "<br>", 4);
            p += 4;
        } else {
            *p++ = *s;
        }
    }
    *p = '\0';
    return out;
}

static char *trim_grid_cell_border_pipes(char *content) {
    if (!content) return content;
    char *start = content;
    while (*start == ' ' || *start == '\t') start++;
    char *end = content + strlen(content);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
    if (*start == '|') start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    if (end > start && end[-1] == '|') end--;
    while (end > start && end[-1] == ' ') end--;
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return content;
    memcpy(out, start, len);
    out[len] = '\0';
    free(content);
    return out;
}

/**
 * Trim border pipes from each grid row line before colspan cell escaping.
 */
static char *normalize_colspan_merged_content(const char *merged) {
    if (!merged) return NULL;

    char *result = NULL;
    const char *s = merged;
    while (*s) {
        const char *line_end = strchr(s, '\n');
        if (!line_end) line_end = s + strlen(s);

        size_t line_len = (size_t)(line_end - s);
        char *line = malloc(line_len + 1);
        if (!line) break;

        memcpy(line, s, line_len);
        line[line_len] = '\0';

        if (strchr(line, '|') && !is_grid_table_separator(line) &&
            count_cells_in_line(line) <= 1) {
            line = trim_grid_cell_border_pipes(line);
        }

        result = str_append_line(result, line);
        free(line);

        s = *line_end ? line_end + 1 : line_end;
    }

    return result;
}

static char *convert_grid_table(char **lines, size_t line_count,
                                int *alignments, size_t column_count,
                                bool as_html);
static char *convert_grid_table_to_pipe(char **lines, size_t line_count,
                                        int *alignments, size_t column_count) {
    return convert_grid_table(lines, line_count, alignments, column_count, false);
}
static char *convert_grid_table_to_html(char **lines, size_t line_count,
                                        int *alignments, size_t column_count) {
    return convert_grid_table(lines, line_count, alignments, column_count, true);
}

/**
 * Convert grid table rows to pipe or HTML format
 */
static char *convert_grid_table(char **lines, size_t line_count,
                                int *alignments, size_t column_count,
                                bool as_html) {
    if (!lines || line_count == 0 || !alignments || column_count == 0) {
        return NULL;
    }

    /* Calculate approximate size needed */
    size_t total_size = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i]) {
            total_size += strlen(lines[i]) + column_count * 4 + 10;
        }
    }
    total_size += as_html ? 4096 : 200;

    char *output = malloc(total_size);
    if (!output) return NULL;

    char *p = output;
    size_t remaining = total_size;

    if (as_html) {
        memcpy(p, "\n\n<table>\n", 10);
        p += 10;
        remaining -= 10;
    } else {
        /* Add blank lines at start of table for proper recognition */
        *p++ = '\n';
        *p++ = '\n';
        remaining -= 2;
    }

    bool separator_processed = false;
    bool header_written = false;
    bool rows_written = false;
    bool in_thead = false;
    bool pipe_header_promoted = false;

    /* First, find the header separator to identify header vs body */
    size_t header_sep_idx = 0;
    bool has_header_sep = false;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] && is_header_separator(lines[i])) {
            header_sep_idx = i;
            has_header_sep = true;
            break;
        }
    }

    /* Group lines into rows (lines between separators) */
    size_t row_start = 0;

    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i];
        if (!line) continue;

        /* Check if this is a separator line (table boundary, not nested grid in cell) */
        if (is_grid_table_separator(line) && !is_nested_grid_separator(line, column_count)) {
            bool is_header_sep = is_header_separator(line);
            bool is_footer_sep = false;

            /* Check if this is a footer separator (last separator before blank line) */
            if (is_header_sep && i > 0) {
                /* Check if there are more non-separator lines after this */
                bool has_more_content = false;
                for (size_t j = i + 1; j < line_count; j++) {
                    if (lines[j] && !is_grid_table_separator(lines[j]) &&
                        strchr(lines[j], '|')) {
                        has_more_content = true;
                        break;
                    }
                }
                if (!has_more_content) {
                    is_footer_sep = true;
                }
            }

            /* Process rows between previous separator and this one */
            if (i > row_start) {
                /* Check if this is header section (before header separator) */
                bool is_header_section = (has_header_sep && i <= header_sep_idx);

                /* Group lines into logical rows (lines with same column structure are multi-line cells) */
                size_t line_idx = row_start;
                while (line_idx < i) {
                    /* Find all consecutive lines with same column structure */
                    size_t row_line_start = line_idx;
                    size_t row_line_count = 0;

                    /* Collect lines that form a single logical row */
                    const char *first_line = NULL;
                    bool colspan_block = false;
                    for (size_t j = line_idx; j < i; j++) {
                        if (!lines[j]) continue;

                        if (is_grid_table_separator(lines[j])) {
                            if (is_nested_grid_separator(lines[j], column_count)) {
                                if (first_line) row_line_count++;
                                continue;
                            }
                            if (first_line) break;
                            continue;
                        }

                        if (strchr(lines[j], '|')) {
                            if (!first_line) {
                                first_line = lines[j];
                                row_line_start = j;
                                row_line_count = 1;
                                colspan_block = is_full_row_colspan_line(first_line);
                            } else if (colspan_block) {
                                row_line_count++;
                            } else if (has_same_column_structure(first_line, lines[j])) {
                                row_line_count++;
                            } else {
                                break;
                            }
                        } else if (first_line) {
                            row_line_count++;
                        } else {
                            line_idx++;
                        }
                    }

                    if (row_line_count > 0) {
                        /* Extract cells from all lines in this logical row */
                        char **row_lines_array = malloc(row_line_count * sizeof(char*));
                        if (row_lines_array) {
                            for (size_t j = 0; j < row_line_count; j++) {
                                row_lines_array[j] = lines[row_line_start + j];
                            }

                            size_t cell_count = 0;
                            char **cells = NULL;
                            bool colspan_group = is_full_row_colspan_line(row_lines_array[0]);

                            if (colspan_group) {
                                cells = malloc(MAX_COLUMNS * sizeof(char*));
                                if (cells) {
                                    char *merged = NULL;
                                    for (size_t j = 0; j < row_line_count; j++) {
                                        merged = str_append_line(merged, row_lines_array[j]);
                                    }
                                    cells[0] = merged;
                                    cell_count = 1;
                                }
                                if (cells && cells[0] && cell_count == 1 && column_count > 1) {
                                    cells[0] = trim_grid_cell_border_pipes(cells[0]);
                                }
                            } else {
                                cells = extract_cells_from_row_lines(row_lines_array, row_line_count, &cell_count);
                            }

                                if (cells && cell_count > 0) {
                                bool force_colspan = (row_line_count > 0 && row_lines_array[0] &&
                                                      is_full_row_colspan_line(row_lines_array[0]));
                                /* Write header separator after first header row, before first body row */
                                if (is_header_section && !header_written && !separator_processed && !is_footer_sep) {
                                    header_written = true;
                                } else if (!is_header_section && !separator_processed && !is_footer_sep) {
                                    if (!as_html && has_header_sep) {
                                        char *sep = create_pipe_separator(alignments, column_count);
                                        if (sep) {
                                            size_t sep_len = strlen(sep);
                                            if (sep_len < remaining) {
                                                memcpy(p, sep, sep_len);
                                                p += sep_len;
                                                remaining -= sep_len;
                                                *p++ = '\n';
                                                remaining--;
                                            }
                                            free(sep);
                                        }
                                        separator_processed = true;
                                    }
                                }

                                /* Convert to pipe or HTML table row */
                                if (remaining > column_count * 50) {
                                    bool use_colspan = force_colspan;
                                    if (use_colspan && force_colspan && cells && cell_count > 0) {
                                        /* Collapse to single cell for colspan row */
                                        char *merged_one = NULL;
                                        for (size_t ci = 0; ci < cell_count; ci++) {
                                            if (cells[ci]) {
                                                const char *t = cells[ci];
                                                while (*t && isspace((unsigned char)*t)) t++;
                                                if (*t) {
                                                    merged_one = strdup(cells[ci]);
                                                    break;
                                                }
                                            }
                                        }
                                        if (!merged_one && cells[0]) merged_one = strdup(cells[0]);
                                        if (merged_one) {
                                            for (size_t ci = 0; ci < cell_count; ci++) free(cells[ci]);
                                            cells[0] = trim_grid_cell_border_pipes(merged_one);
                                            cell_count = 1;
                                        }
                                    }

                                    char *colspan_processed = NULL;
                                    if (as_html && use_colspan && cells[0]) {
                                        colspan_processed = preprocess_cell_grid_content(cells[0]);
                                    }

                                    if (as_html) {
                                        if (is_header_section && !separator_processed && !in_thead) {
                                            const char *thead = "<thead>\n";
                                            size_t tl = strlen(thead);
                                            if (tl < remaining) {
                                                memcpy(p, thead, tl);
                                                p += tl;
                                                remaining -= tl;
                                                in_thead = true;
                                            }
                                        } else if (!is_header_section && in_thead) {
                                            const char *end = "</thead>\n<tbody>\n";
                                            size_t el = strlen(end);
                                            if (el < remaining) {
                                                memcpy(p, end, el);
                                                p += el;
                                                remaining -= el;
                                                in_thead = false;
                                            }
                                        } else if (!is_header_section && !in_thead && !rows_written) {
                                            const char *tbody = "<tbody>\n";
                                            size_t bl = strlen(tbody);
                                            if (bl < remaining) {
                                                memcpy(p, tbody, bl);
                                                p += bl;
                                                remaining -= bl;
                                            }
                                        }

                                        const char *tr = "<tr>\n";
                                        size_t trl = strlen(tr);
                                        if (trl < remaining) {
                                            memcpy(p, tr, trl);
                                            p += trl;
                                            remaining -= trl;
                                        }

                                        size_t cells_to_write = use_colspan ? 1 : cell_count;
                                        for (size_t k = 0; k < cells_to_write; k++) {
                                            const char *cell_content = (k < cell_count && cells[k]) ? cells[k] : "";
                                            if (use_colspan) {
                                                cell_content = colspan_processed ? colspan_processed : cells[0];
                                            }

                                            const char *tag = (is_header_section && !separator_processed) ? "th" : "td";
                                            size_t span = 1;
                                            if (use_colspan && column_count > 1) {
                                                span = column_count;
                                            } else if (!use_colspan && cell_count < column_count && k == 0) {
                                                span = column_count - cell_count + 1;
                                            }

                                            char open[256];
                                            if (span > 1) {
                                                snprintf(open, sizeof(open),
                                                         "<%s markdown=\"1\" colspan=\"%zu\">\n\n",
                                                         tag, span);
                                            } else {
                                                snprintf(open, sizeof(open),
                                                         "<%s markdown=\"1\">\n\n", tag);
                                            }
                                            size_t ol = strlen(open);
                                            if (ol < remaining) {
                                                memcpy(p, open, ol);
                                                p += ol;
                                                remaining -= ol;
                                            }
                                            size_t cl = strlen(cell_content);
                                            if (cl < remaining) {
                                                memcpy(p, cell_content, cl);
                                                p += cl;
                                                remaining -= cl;
                                            }
                                            char close[16];
                                            snprintf(close, sizeof(close), "\n\n</%s>\n", tag);
                                            size_t cll = strlen(close);
                                            if (cll < remaining) {
                                                memcpy(p, close, cll);
                                                p += cll;
                                                remaining -= cll;
                                            }
                                        }

                                        const char *endtr = "</tr>\n";
                                        size_t etl = strlen(endtr);
                                        if (etl < remaining) {
                                            memcpy(p, endtr, etl);
                                            p += etl;
                                            remaining -= etl;
                                        }

                                        if (colspan_processed) free(colspan_processed);
                                        rows_written = true;
                                    } else {
                                    if (use_colspan && cells[0]) {
                                        char *tmp_trim = strdup(cells[0]);
                                        if (tmp_trim) tmp_trim = trim_grid_cell_border_pipes(tmp_trim);
                                        const char *content = tmp_trim ? tmp_trim : cells[0];

                                        *p++ = '|';
                                        remaining--;
                                        *p++ = ' ';
                                        remaining--;
                                        size_t cl = strlen(content);
                                        if (cl < remaining - 10) {
                                            memcpy(p, content, cl);
                                            p += cl;
                                            remaining -= cl;
                                        }
                                        *p++ = ' ';
                                        remaining--;
                                        *p++ = '|';
                                        remaining--;
                                        const char *marker = " << |";
                                        size_t ml = strlen(marker);
                                        if (ml < remaining) {
                                            memcpy(p, marker, ml);
                                            p += ml;
                                            remaining -= ml;
                                        }
                                        *p++ = '\n';
                                        remaining--;
                                        if (tmp_trim) free(tmp_trim);
                                        rows_written = true;
                                    } else {
                                    *p++ = '|';
                                    remaining--;

                                    for (size_t k = 0; k < column_count; k++) {
                                        const char *cell_content = "";
                                        char *escaped = NULL;
                                        char *tmp_trim = NULL;
                                        if (use_colspan) {
                                            cell_content = (k == 0) ? cells[0] : "<<";
                                        } else if (k < cell_count && cells[k]) {
                                            cell_content = cells[k];
                                        }

                                        if (use_colspan && k == 0 && cell_content && *cell_content) {
                                            tmp_trim = strdup(cell_content);
                                            if (tmp_trim) {
                                                tmp_trim = trim_grid_cell_border_pipes(tmp_trim);
                                                cell_content = tmp_trim;
                                            }
                                        }

                                        if (cell_content && !use_colspan && strchr(cell_content, '\n')) {
                                            escaped = escape_pipes_in_multiline_cell(cell_content);
                                            if (escaped) cell_content = escaped;
                                        }

                                        const char *content_start = cell_content;
                                        const char *content_end = cell_content + strlen(cell_content);
                                        while (content_start < content_end && isspace((unsigned char)*content_start)) {
                                            content_start++;
                                        }
                                        while (content_end > content_start && isspace((unsigned char)content_end[-1])) {
                                            content_end--;
                                        }
                                        size_t content_len = content_end - content_start;

                                        *p++ = ' ';
                                        remaining--;
                                        if (content_len > 0 && content_len < remaining - 2) {
                                            memcpy(p, content_start, content_len);
                                            p += content_len;
                                            remaining -= content_len;
                                        }
                                        *p++ = ' ';
                                        remaining--;
                                        *p++ = '|';
                                        remaining--;
                                        if (escaped) free(escaped);
                                        if (tmp_trim) free(tmp_trim);
                                    }

                                    *p++ = '\n';
                                    remaining--;
                                    rows_written = true;

                                    if (!has_header_sep && !pipe_header_promoted &&
                                        !is_header_section && !use_colspan) {
                                        char *sep = create_pipe_separator(alignments, column_count);
                                        if (sep) {
                                            size_t sep_len = strlen(sep);
                                            if (sep_len < remaining) {
                                                memcpy(p, sep, sep_len);
                                                p += sep_len;
                                                remaining -= sep_len;
                                                *p++ = '\n';
                                                remaining--;
                                            }
                                            free(sep);
                                        }
                                        pipe_header_promoted = true;
                                        separator_processed = true;
                                    }
                                    }
                                    }
                                }

                                /* Free cells */
                                for (size_t k = 0; k < cell_count; k++) {
                                    if (cells[k]) free(cells[k]);
                                }
                                free(cells);
                            }

                            free(row_lines_array);
                        }

                        line_idx = row_line_start + row_line_count;
                    } else {
                        line_idx++;
                    }
                }
            }

            /* Write separator after header rows if this is the header separator */
            if (is_header_sep && !is_footer_sep && !separator_processed && header_written) {
                if (!as_html) {
                    char *sep = create_pipe_separator(alignments, column_count);
                    if (sep) {
                        size_t sep_len = strlen(sep);
                        if (sep_len < remaining) {
                            memcpy(p, sep, sep_len);
                            p += sep_len;
                            remaining -= sep_len;
                            *p++ = '\n';
                            remaining--;
                        }
                        free(sep);
                    }
                }
                separator_processed = true;
            } else if (!is_header_sep && !is_footer_sep && !separator_processed && rows_written &&
                       has_header_sep) {
                if (i > header_sep_idx) {
                    if (!as_html) {
                        char *sep = create_pipe_separator(alignments, column_count);
                        if (sep) {
                            size_t sep_len = strlen(sep);
                            if (sep_len < remaining) {
                                memcpy(p, sep, sep_len);
                                p += sep_len;
                                remaining -= sep_len;
                                *p++ = '\n';
                                remaining--;
                            }
                            free(sep);
                        }
                    }
                    separator_processed = true;
                }
            }

            row_start = i + 1;
            continue;
        }
    }

    /* Process final rows after last separator */
    if (row_start < line_count) {
        size_t first_content = row_start;
        for (; first_content < line_count; first_content++) {
            if (lines[first_content] && strchr(lines[first_content], '|')) break;
        }

        bool final_colspan = (first_content < line_count &&
                              is_full_row_colspan_line(lines[first_content]));

        if (final_colspan) {
            char *merged = NULL;
            for (size_t j = row_start; j < line_count; j++) {
                if (lines[j]) merged = str_append_line(merged, lines[j]);
            }

            if (merged && remaining > column_count * 50) {
                char *normalized = normalize_colspan_merged_content(merged);
                if (normalized) {
                    free(merged);
                    merged = normalized;
                }

                char *escaped = NULL;
                const char *content = merged;
                char *processed = NULL;
                if (as_html) {
                    processed = preprocess_cell_grid_content(merged);
                    if (processed) content = processed;
                } else {
                    escaped = escape_pipes_in_multiline_cell(merged);
                    if (escaped) content = escaped;
                }

                if (as_html) {
                    if (in_thead) {
                        const char *end = "</thead>\n<tbody>\n";
                        size_t el = strlen(end);
                        if (el < remaining) {
                            memcpy(p, end, el);
                            p += el;
                            remaining -= el;
                            in_thead = false;
                        }
                    } else if (!rows_written) {
                        const char *tbody = "<tbody>\n";
                        size_t bl = strlen(tbody);
                        if (bl < remaining) {
                            memcpy(p, tbody, bl);
                            p += bl;
                            remaining -= bl;
                        }
                    }

                    char open[64];
                    snprintf(open, sizeof(open), "<tr>\n<td markdown=\"1\" colspan=\"%zu\">\n\n", column_count);
                    size_t ol = strlen(open);
                    if (ol < remaining) { memcpy(p, open, ol); p += ol; remaining -= ol; }
                    size_t cl = strlen(content);
                    if (cl < remaining) { memcpy(p, content, cl); p += cl; remaining -= cl; }
                    const char *close = "\n\n</td>\n</tr>\n";
                    size_t cll = strlen(close);
                    if (cll < remaining) { memcpy(p, close, cll); p += cll; remaining -= cll; }
                    rows_written = true;
                } else {
                    *p++ = '|';
                    remaining--;
                    *p++ = ' ';
                    remaining--;
                    size_t cl = strlen(content);
                    if (cl < remaining - 10) {
                        memcpy(p, content, cl);
                        p += cl;
                        remaining -= cl;
                    }
                    *p++ = ' ';
                    remaining--;
                    *p++ = '|';
                    remaining--;
                    const char *marker = " << |";
                    size_t ml = strlen(marker);
                    if (ml < remaining) {
                        memcpy(p, marker, ml);
                        p += ml;
                        remaining -= ml;
                    }
                    *p++ = '\n';
                    remaining--;
                }
                if (escaped) free(escaped);
                if (processed) free(processed);
                free(merged);
            }
        } else {
        char **row_lines = malloc((line_count - row_start) * sizeof(char*));
        if (row_lines) {
            size_t row_line_count = 0;
            for (size_t j = row_start; j < line_count; j++) {
                /* Skip separator rows - only include content rows */
                if (lines[j] && !is_grid_table_separator(lines[j]) && strchr(lines[j], '|')) {
                    row_lines[row_line_count++] = lines[j];
                }
            }

            if (row_line_count > 0) {
                size_t cell_count = 0;
                char **cells = extract_cells_from_row_lines(row_lines, row_line_count, &cell_count);

                if (cells && cell_count > 0) {
                    if (remaining > column_count * 50) {
                        *p++ = '|';
                        remaining--;

                        for (size_t j = 0; j < column_count; j++) {
                            const char *cell_content = (j < cell_count && cells[j]) ? cells[j] : "";

                            /* Trim leading/trailing whitespace from cell content */
                            const char *content_start = cell_content;
                            const char *content_end = cell_content + strlen(cell_content);
                            while (content_start < content_end && isspace((unsigned char)*content_start)) {
                                content_start++;
                            }
                            while (content_end > content_start && isspace((unsigned char)content_end[-1])) {
                                content_end--;
                            }
                            size_t content_len = content_end - content_start;

                            *p++ = '|';
                            remaining--;

                            if (content_len > 0 && content_len < remaining - 5) {
                                *p++ = ' ';
                                remaining--;
                                memcpy(p, content_start, content_len);
                                p += content_len;
                                remaining -= content_len;
                                *p++ = ' ';
                                remaining--;
                            } else {
                                *p++ = ' ';
                                remaining--;
                            }
                        }

                        *p++ = '\n';
                        remaining--;
                    }

                    for (size_t j = 0; j < cell_count; j++) {
                        if (cells[j]) free(cells[j]);
                    }
                    free(cells);
                }
            }
            free(row_lines);
        }
        }
    }

    if (as_html) {
        if (in_thead) {
            const char *end = "</thead>\n<tbody>\n";
            size_t el = strlen(end);
            if (el < remaining) {
                memcpy(p, end, el);
                p += el;
                remaining -= el;
            }
        }
        const char *close = "</tbody>\n</table>\n\n";
        size_t cl = strlen(close);
        if (cl < remaining) {
            memcpy(p, close, cl);
            p += cl;
            remaining -= cl;
        }
    } else {
        /* Add blank line at end of table */
        if (p > output && p[-1] != '\n') {
            *p++ = '\n';
            remaining--;
        }
        *p++ = '\n';
        remaining--;
    }

    *p = '\0';
    return output;
}

static char *convert_collected_grid_table(char **table_lines, size_t table_line_count,
                                          bool force_html) {
    if (!table_lines || table_line_count == 0) return NULL;

    int alignments[MAX_COLUMNS] = {0};
    size_t column_count = 0;
    size_t first_sep_cols = 0;

    for (size_t i = 0; i < table_line_count; i++) {
        if (is_grid_table_separator(table_lines[i]) && first_sep_cols == 0) {
            parse_alignment(table_lines[i], alignments, &first_sep_cols);
            column_count = first_sep_cols;
        }
    }

    for (size_t i = 0; i < table_line_count; i++) {
        if (is_header_separator(table_lines[i])) {
            size_t sep_cols = 0;
            int sep_alignments[MAX_COLUMNS] = {0};
            parse_alignment(table_lines[i], sep_alignments, &sep_cols);
            if (sep_cols > column_count) {
                column_count = sep_cols;
                memcpy(alignments, sep_alignments, sizeof(sep_alignments));
            }
            break;
        }
    }

    if (column_count == 0) {
        for (size_t i = 0; i < table_line_count; i++) {
            if (is_grid_table_separator(table_lines[i])) {
                parse_alignment(table_lines[i], alignments, &column_count);
                if (column_count > 0) break;
            }
        }
    }

    if (column_count == 0) {
        for (size_t i = 0; i < table_line_count; i++) {
            if (strchr(table_lines[i], '|') && !is_grid_table_separator(table_lines[i])) {
                size_t row_cols = count_cells_in_line(table_lines[i]);
                if (row_cols > column_count) {
                    column_count = row_cols;
                }
            }
        }
        for (size_t j = 0; j < column_count && j < MAX_COLUMNS; j++) {
            alignments[j] = 0;
        }
    }

    if (column_count == 0) return NULL;

    if (table_has_partial_separators(table_lines, table_line_count)) {
        return convert_partial_grid_table_to_html(table_lines, table_line_count, column_count);
    }

    if (force_html ||
        table_has_nested_grid(table_lines, table_line_count, column_count) ||
        table_has_multiline_cells(table_lines, table_line_count)) {
        return convert_grid_table_to_html(table_lines, table_line_count,
                                          alignments, column_count);
    }

    return convert_grid_table_to_pipe(table_lines, table_line_count,
                                      alignments, column_count);
}

static char *preprocess_grid_text(const char *text, bool embed_as_html) {
    if (!text) return NULL;

    size_t len = strlen(text);
    if (len == 0) return strdup("");

    /* Allocate output buffer (may grow) */
    size_t cap = len * 2 + 1;
    char *output = malloc(cap);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = cap;
    bool in_code_block = false;

    while (*read) {
        const char *line_start = read;

        /* Find line ending */
        const char *line_end = strchr(read, '\n');
        if (!line_end) {
            line_end = read + strlen(read);
        }

        size_t line_len = line_end - line_start;
        bool has_newline = (*line_end == '\n');

        /* Track code blocks */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) {
            p++;
        }

        if (!in_code_block &&
            (line_end - p) >= 3 &&
            ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
             (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = true;
        } else if (in_code_block &&
                   (line_end - p) >= 3 &&
                   ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
                    (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = false;
        }

        /* Check if this is a grid table start */
        if (!in_code_block && is_grid_table_start(line_start)) {
            /* Collect all consecutive non-blank lines until blank line */
            char **table_lines = malloc(100 * sizeof(char*));
            if (!table_lines) {
                /* Out of memory - copy line as-is */
                if (line_len < remaining) {
                    memcpy(write, line_start, line_len);
                    write += line_len;
                    remaining -= line_len;
                    if (has_newline) {
                        *write++ = '\n';
                        remaining--;
                    }
                }
                read = has_newline ? line_end + 1 : line_end;
                continue;
            }

            size_t table_line_count = 0;

            /* Collect table lines */
            while (*read && table_line_count < 100) {
                const char *current_line_start = read;
                const char *current_line_end = strchr(read, '\n');
                if (!current_line_end) {
                    current_line_end = read + strlen(read);
                }

                /* Check if blank line */
                bool is_blank = true;
                for (const char *q = current_line_start; q < current_line_end; q++) {
                    if (!isspace((unsigned char)*q)) {
                        is_blank = false;
                        break;
                    }
                }

                if (is_blank) {
                    break;
                }

                size_t current_line_len = current_line_end - current_line_start;
                table_lines[table_line_count] = malloc(current_line_len + 1);
                if (table_lines[table_line_count]) {
                    memcpy(table_lines[table_line_count], current_line_start, current_line_len);
                    table_lines[table_line_count][current_line_len] = '\0';
                    table_line_count++;
                }

                read = (*current_line_end == '\n') ? current_line_end + 1 : current_line_end;
            }

            if (table_line_count > 0) {
                char *table_output = NULL;
                if (grid_block_has_separator(table_lines, table_line_count) &&
                    grid_block_has_content_rows(table_lines, table_line_count)) {
                    table_output = convert_collected_grid_table(table_lines, table_line_count,
                                                                embed_as_html);
                }

                if (table_output) {
                    size_t pipe_len = strlen(table_output);

                    if (remaining < pipe_len) {
                        size_t written = write - output;
                        cap = (written + pipe_len + 100) * 2;
                        char *new_output = realloc(output, cap);
                        if (new_output) {
                            output = new_output;
                            write = output + written;
                            remaining = cap - written;
                        }
                    }

                    if (pipe_len <= remaining) {
                        memcpy(write, table_output, pipe_len);
                        write += pipe_len;
                        remaining -= pipe_len;
                    }

                    free(table_output);
                } else {
                    /* Not a valid grid or conversion failed: preserve source lines */
                    for (size_t i = 0; i < table_line_count; i++) {
                        if (!table_lines[i]) continue;
                        size_t tl = strlen(table_lines[i]);
                        if (remaining < tl + 2) {
                            size_t written = write - output;
                            cap = (written + tl + 100) * 2;
                            char *new_output = realloc(output, cap);
                            if (new_output) {
                                output = new_output;
                                write = output + written;
                                remaining = cap - written;
                            }
                        }
                        if (tl <= remaining) {
                            memcpy(write, table_lines[i], tl);
                            write += tl;
                            remaining -= tl;
                        }
                        if (remaining > 0) {
                            *write++ = '\n';
                            remaining--;
                        }
                    }
                }

                for (size_t i = 0; i < table_line_count; i++) {
                    if (table_lines[i]) free(table_lines[i]);
                }
            }

            free(table_lines);

            if (*read == '\n') {
                read++;
            }
            continue;
        }

        /* Not a grid table - copy line as-is */
        if (line_len < remaining) {
            memcpy(write, line_start, line_len);
            write += line_len;
            remaining -= line_len;
            if (has_newline) {
                *write++ = '\n';
                remaining--;
            }
        } else {
            size_t written = write - output;
            cap = (written + line_len + 1) * 2;
            char *new_output = realloc(output, cap);
            if (new_output) {
                output = new_output;
                write = output + written;
                remaining = cap - written;
                memcpy(write, line_start, line_len);
                write += line_len;
                remaining -= line_len;
                if (has_newline) {
                    *write++ = '\n';
                    remaining--;
                }
            }
        }

        read = has_newline ? line_end + 1 : line_end;
    }

    *write = '\0';
    return output;
}

char *apex_preprocess_grid_tables(const char *text) {
    return preprocess_grid_text(text, false);
}
