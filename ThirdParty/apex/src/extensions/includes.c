/**
 * File Includes Extension for Apex
 * Implementation
 */

#include "includes.h"
#include "metadata.h"
#include "../plugins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>
#include <regex.h>
#include <glob.h>

/**
 * Read file contents
 */
static char *read_file_contents(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0 || size > 10 * 1024 * 1024) {  /* Limit to 10MB */
        fclose(fp);
        return NULL;
    }

    /* Read content */
    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);

    return content;
}

/**
 * Decode percent-encoded path in place (RFC 3986: %XX -> byte).
 * Used so include paths like with%20space.txt resolve to files with spaces.
 */
static void percent_decode_inplace(char *path) {
    if (!path) return;
    char *r = path;
    char *w = path;
    while (*r) {
        if (r[0] == '%' && r[1] && r[2] &&
            isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            int hi = (r[1] <= '9') ? (r[1] - '0') : ((r[1] & 0x0f) + 9);
            int lo = (r[2] <= '9') ? (r[2] - '0') : ((r[2] & 0x0f) + 9);
            *w++ = (char)((hi << 4) | lo);
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/**
 * Resolve relative path from base directory
 */
static char *resolve_path(const char *filepath, const char *base_dir) {
    if (!filepath) return NULL;

    /* If absolute path, return as-is */
    if (filepath[0] == '/') {
        return strdup(filepath);
    }

    /* Relative path - combine with base_dir */
    if (!base_dir || !*base_dir) {
        return strdup(filepath);
    }

    size_t len = strlen(base_dir) + strlen(filepath) + 2;
    char *resolved = malloc(len);
    if (!resolved) return NULL;

    snprintf(resolved, len, "%s/%s", base_dir, filepath);
    return resolved;
}

/**
 * Get directory of a file path
 */
static char *get_directory(const char *filepath) {
    if (!filepath) return strdup(".");

    char *path_copy = strdup(filepath);
    if (!path_copy) return strdup(".");

    char *dir = dirname(path_copy);
    char *result = strdup(dir);
    free(path_copy);

    return result ? result : strdup(".");
}

/**
 * Check if a file exists
 */
bool apex_file_exists(const char *filepath) {
    if (!filepath) return false;
    struct stat st;
    return (stat(filepath, &st) == 0);
}

/**
 * File type enum
 */
typedef enum {
    FILE_TYPE_MARKDOWN,
    FILE_TYPE_IMAGE,
    FILE_TYPE_CODE,
    FILE_TYPE_HTML,
    FILE_TYPE_CSV,
    FILE_TYPE_TSV,
    FILE_TYPE_TEXT
} apex_file_type_t;

/**
 * Address specification structure
 */
typedef struct {
    bool is_line_range;
    bool is_regex_range;
    int start_line;      // 1-based
    int end_line;        // 1-based, -1 means to end
    char *regex_start;   // NULL if not regex
    char *regex_end;     // NULL if not regex
    char *prefix;        // NULL if no prefix
} address_spec_t;

/**
 * Detect file type from extension
 */
static apex_file_type_t apex_detect_file_type(const char *filepath) {
    if (!filepath) return FILE_TYPE_TEXT;

    const char *ext = strrchr(filepath, '.');
    if (!ext) return FILE_TYPE_TEXT;
    ext++;

    /* Images */
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "webp") == 0 || strcasecmp(ext, "svg") == 0) {
        return FILE_TYPE_IMAGE;
    }

    /* CSV/TSV */
    if (strcasecmp(ext, "csv") == 0) return FILE_TYPE_CSV;
    if (strcasecmp(ext, "tsv") == 0) return FILE_TYPE_TSV;

    /* HTML */
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        return FILE_TYPE_HTML;
    }

    /* Markdown */
    if (strcasecmp(ext, "md") == 0 || strcasecmp(ext, "markdown") == 0 ||
        strcasecmp(ext, "mmd") == 0) {
        return FILE_TYPE_MARKDOWN;
    }

    /* Code files */
    if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "h") == 0 ||
        strcasecmp(ext, "cpp") == 0 || strcasecmp(ext, "py") == 0 ||
        strcasecmp(ext, "js") == 0 || strcasecmp(ext, "java") == 0 ||
        strcasecmp(ext, "swift") == 0 || strcasecmp(ext, "go") == 0 ||
        strcasecmp(ext, "rs") == 0 || strcasecmp(ext, "sh") == 0) {
        return FILE_TYPE_CODE;
    }

    return FILE_TYPE_TEXT;
}

/**
 * Convert CSV/TSV to Markdown table
 *
 * Alignment handling:
 * - First row is always treated as header.
 * - If the second row cells are all one of: left, right, center, auto (case-insensitive),
 *   it is treated as an alignment row and converted to :---, ---:, :---:, or ---.
 * - Alternatively, if the second row cells contain only colons and dashes (e.g. :--, --:,
 *   :--:), they are parsed as Markdown-style alignment specs:
 *   :-- or :--- (colon at start) = left; --: or ---: (colon at end) = right;
 *   :--: or :---: (colon both ends) = center; --- (no colon) = auto.
 *   The alignment row itself is NOT emitted as a data row.
 * - Otherwise, a default '---' separator row is generated after the header.
 *   The second row is emitted as normal data.
 */
char *apex_csv_to_table_with_delimiter(const char *csv_content, bool is_tsv, char delimiter_override) {
    if (!csv_content) return NULL;

    char delim = delimiter_override ? delimiter_override : (is_tsv ? '\t' : ',');
    size_t len = strlen(csv_content);
    if (len == 0) return NULL;

    /* First pass: parse into rows and cells */
    typedef struct {
        char **cells;
        int cell_count;
    } csv_row_t;

    int row_cap = 8;
    int row_count = 0;
    csv_row_t *rows = calloc(row_cap, sizeof(csv_row_t));
    if (!rows) return NULL;

    const char *line_start = csv_content;
    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        if (!line_end) line_end = csv_content + strlen(csv_content);

        /* Allocate new row */
        if (row_count >= row_cap) {
            row_cap *= 2;
            csv_row_t *tmp = realloc(rows, (size_t)row_cap * sizeof(csv_row_t));
            if (!tmp) {
                /* Cleanup already allocated cells */
                for (int r = 0; r < row_count; r++) {
                    for (int c = 0; c < rows[r].cell_count; c++) {
                        free(rows[r].cells[c]);
                    }
                    free(rows[r].cells);
                }
                free(rows);
                return NULL;
            }
            rows = tmp;
        }

        csv_row_t *row = &rows[row_count];
        row->cells = NULL;
        row->cell_count = 0;

        int cell_cap = 8;
        row->cells = malloc((size_t)cell_cap * sizeof(char *));
        if (!row->cells) {
            for (int r = 0; r < row_count; r++) {
                for (int c = 0; c < rows[r].cell_count; c++) {
                    free(rows[r].cells[c]);
                }
                free(rows[r].cells);
            }
            free(rows);
            return NULL;
        }

        const char *cell_start = line_start;
        while (cell_start <= line_end) {
            const char *cell_end = cell_start;
            while (cell_end < line_end && *cell_end != delim) cell_end++;

            if (row->cell_count >= cell_cap) {
                cell_cap *= 2;
                char **tmp_cells = realloc(row->cells, (size_t)cell_cap * sizeof(char *));
                if (!tmp_cells) {
                    /* Cleanup */
                    for (int r = 0; r <= row_count; r++) {
                        int max_c = (r == row_count) ? row->cell_count : rows[r].cell_count;
                        char **cell_arr = (r == row_count) ? row->cells : rows[r].cells;
                        if (cell_arr) {
                            for (int c = 0; c < max_c; c++) {
                                free(cell_arr[c]);
                            }
                            free(cell_arr);
                        }
                    }
                    free(rows);
                    return NULL;
                }
                row->cells = tmp_cells;
            }

            size_t cell_len = (size_t)(cell_end - cell_start);
            char *cell = malloc(cell_len + 1);
            if (!cell) {
                for (int r = 0; r <= row_count; r++) {
                    int max_c = (r == row_count) ? row->cell_count : rows[r].cell_count;
                    char **cell_arr = (r == row_count) ? row->cells : rows[r].cells;
                    if (cell_arr) {
                        for (int c = 0; c < max_c; c++) {
                            free(cell_arr[c]);
                        }
                        free(cell_arr);
                    }
                }
                free(rows);
                return NULL;
            }
            memcpy(cell, cell_start, cell_len);
            cell[cell_len] = '\0';
            row->cells[row->cell_count++] = cell;

            if (cell_end < line_end) cell_start = cell_end + 1;
            else break;
        }

        row_count++;

        line_start = line_end;
        if (*line_start == '\n') line_start++;
    }

    if (row_count == 0) {
        free(rows);
        return NULL;
    }

    /* Determine column count from first row */
    int col_count = rows[0].cell_count;
    if (col_count <= 0) {
        for (int r = 0; r < row_count; r++) {
            for (int c = 0; c < rows[r].cell_count; c++) free(rows[r].cells[c]);
            free(rows[r].cells);
        }
        free(rows);
        return NULL;
    }

    /* Check for alignment row (second row with keywords) */
    bool has_alignment_row = false;
    enum { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER, ALIGN_AUTO } *align = NULL;

    if (row_count > 1) {
        csv_row_t *arow = &rows[1];
        bool all_keywords = (arow->cell_count == col_count);

        if (all_keywords) {
            align = malloc((size_t)col_count * sizeof(*align));
            if (!align) {
                for (int r = 0; r < row_count; r++) {
                    for (int c = 0; c < rows[r].cell_count; c++) free(rows[r].cells[c]);
                    free(rows[r].cells);
                }
                free(rows);
                return NULL;
            }

            for (int i = 0; i < col_count; i++) {
                char *cell = arow->cells[i];
                /* Trim whitespace */
                char *start = cell;
                while (*start && isspace((unsigned char)*start)) start++;
                char *end = start + strlen(start);
                while (end > start && isspace((unsigned char)end[-1])) end--;
                size_t tlen = (size_t)(end - start);

                if (tlen == 0) { all_keywords = false; break; }

                /* Lowercase copy for comparison */
                char buf[16];
                if (tlen >= sizeof(buf)) { all_keywords = false; break; }
                for (size_t j = 0; j < tlen; j++) {
                    buf[j] = (char)tolower((unsigned char)start[j]);
                }
                buf[tlen] = '\0';

                if (strcmp(buf, "left") == 0) {
                    align[i] = ALIGN_LEFT;
                } else if (strcmp(buf, "right") == 0) {
                    align[i] = ALIGN_RIGHT;
                } else if (strcmp(buf, "center") == 0) {
                    align[i] = ALIGN_CENTER;
                } else if (strcmp(buf, "auto") == 0) {
                    align[i] = ALIGN_AUTO;
                } else {
                    all_keywords = false;
                    break;
                }
            }

            if (!all_keywords) {
                free(align);
                align = NULL;
            } else {
                has_alignment_row = true;
            }
        }

        /* If keywords failed, try Markdown-style alignment (:--, --:, :--:) */
        if (!has_alignment_row && arow->cell_count == col_count) {
            align = malloc((size_t)col_count * sizeof(*align));
            if (align) {
                bool all_colon_dash = true;
                for (int i = 0; i < col_count && all_colon_dash; i++) {
                    char *cell = arow->cells[i];
                    char *start = cell;
                    while (*start && isspace((unsigned char)*start)) start++;
                    char *end = start + strlen(start);
                    while (end > start && isspace((unsigned char)end[-1])) end--;
                    size_t tlen = (size_t)(end - start);

                    if (tlen == 0) {
                        all_colon_dash = false;
                        break;
                    }

                    bool has_dash = false;
                    for (size_t j = 0; j < tlen; j++) {
                        char ch = start[j];
                        if (ch == '-') has_dash = true;
                        else if (ch != ':') {
                            all_colon_dash = false;
                            break;
                        }
                    }
                    if (!all_colon_dash || !has_dash) {
                        all_colon_dash = false;
                        break;
                    }

                    bool colon_start = (start[0] == ':');
                    bool colon_end = (end > start && end[-1] == ':');
                    if (colon_start && colon_end) {
                        align[i] = ALIGN_CENTER;
                    } else if (colon_start) {
                        align[i] = ALIGN_LEFT;
                    } else if (colon_end) {
                        align[i] = ALIGN_RIGHT;
                    } else {
                        align[i] = ALIGN_AUTO;
                    }
                }
                if (all_colon_dash) {
                    has_alignment_row = true;
                } else {
                    free(align);
                    align = NULL;
                }
            }
        }
    }

    /* Allocate output buffer: original size * 4 should be enough with extra alignment row */
    char *output = malloc(len * 4 + 64);
    if (!output) {
        if (align) free(align);
        for (int r = 0; r < row_count; r++) {
            for (int c = 0; c < rows[r].cell_count; c++) free(rows[r].cells[c]);
            free(rows[r].cells);
        }
        free(rows);
        return NULL;
    }

    char *write = output;

    /* Emit header row (first row) */
    {
        csv_row_t *row = &rows[0];
        *write++ = '|';
        for (int c = 0; c < col_count; c++) {
            *write++ = ' ';
            if (c < row->cell_count && row->cells[c]) {
                const char *val = row->cells[c];
                size_t vlen = strlen(val);
                memcpy(write, val, vlen);
                write += vlen;
            }
            *write++ = ' ';
            *write++ = '|';
        }
        *write++ = '\n';
    }

    /* Emit separator/alignment row */
    *write++ = '|';
    for (int c = 0; c < col_count; c++) {
        const char *spec = " --- ";
        if (has_alignment_row && align) {
            switch (align[c]) {
                case ALIGN_LEFT:   spec = " :--- "; break;
                case ALIGN_RIGHT:  spec = " ---: "; break;
                case ALIGN_CENTER: spec = " :---: "; break;
                case ALIGN_AUTO:   spec = " --- "; break;
            }
        }
        size_t slen = strlen(spec);
        memcpy(write, spec, slen);
        write += slen;
        *write++ = '|';
    }
    *write++ = '\n';

    /* Emit data rows (skip alignment row if present) */
    int start_row = has_alignment_row ? 2 : 1;
    for (int r = start_row; r < row_count; r++) {
        csv_row_t *row = &rows[r];
        *write++ = '|';
        for (int c = 0; c < col_count; c++) {
            *write++ = ' ';
            if (c < row->cell_count && row->cells[c]) {
                const char *val = row->cells[c];
                size_t vlen = strlen(val);
                memcpy(write, val, vlen);
                write += vlen;
            }
            *write++ = ' ';
            *write++ = '|';
        }
        *write++ = '\n';
    }

    *write = '\0';

    if (align) free(align);
    for (int r = 0; r < row_count; r++) {
        for (int c = 0; c < rows[r].cell_count; c++) free(rows[r].cells[c]);
        free(rows[r].cells);
    }
    free(rows);

    return output;
}

char *apex_csv_to_table(const char *csv_content, bool is_tsv) {
    return apex_csv_to_table_with_delimiter(csv_content, is_tsv, '\0');
}

/**
 * Free address specification
 */
static void free_address_spec(address_spec_t *spec) {
    if (!spec) return;
    if (spec->regex_start) free(spec->regex_start);
    if (spec->regex_end) free(spec->regex_end);
    if (spec->prefix) free(spec->prefix);
    free(spec);
}

/**
 * Parse address specification
 * Supports:
 * - Line numbers: N,M or N,
 * - Regex: /pattern1/,/pattern2/
 * - Prefix: prefix="..."
 */
static address_spec_t *parse_address_spec(const char *address_str) {
    if (!address_str || *address_str == '\0') {
        return NULL;
    }

    address_spec_t *spec = calloc(1, sizeof(address_spec_t));
    if (!spec) return NULL;

    const char *pos = address_str;

    /* Skip leading whitespace */
    while (*pos && isspace(*pos)) pos++;

    /* Check if it's just a prefix parameter (starts with prefix=) */
    if (strncmp(pos, "prefix=\"", 8) == 0) {
        /* Just a prefix, no line range or regex */
        pos += 8;
        const char *prefix_start = pos;
        const char *prefix_end = strchr(prefix_start, '"');

        if (prefix_end) {
            size_t prefix_len = prefix_end - prefix_start;
            spec->prefix = malloc(prefix_len + 1);
            if (spec->prefix) {
                memcpy(spec->prefix, prefix_start, prefix_len);
                spec->prefix[prefix_len] = '\0';
            }
        }
        return spec;
    }

    /* Check if it's a regex pattern (starts with /) */
    if (*pos == '/') {
        spec->is_regex_range = true;
        const char *regex_start_begin = pos + 1;
        const char *regex_start_end = strchr(regex_start_begin, '/');

        if (!regex_start_end) {
            free_address_spec(spec);
            return NULL;
        }

        /* Extract first regex pattern */
        size_t regex_start_len = regex_start_end - regex_start_begin;
        spec->regex_start = malloc(regex_start_len + 1);
        if (!spec->regex_start) {
            free_address_spec(spec);
            return NULL;
        }
        memcpy(spec->regex_start, regex_start_begin, regex_start_len);
        spec->regex_start[regex_start_len] = '\0';

        pos = regex_start_end + 1;

        /* Skip comma and whitespace */
        while (*pos && (isspace(*pos) || *pos == ',')) pos++;

        /* Check for second regex pattern */
        if (*pos == '/') {
            const char *regex_end_begin = pos + 1;
            const char *regex_end_end = strchr(regex_end_begin, '/');

            if (regex_end_end) {
                size_t regex_end_len = regex_end_end - regex_end_begin;
                spec->regex_end = malloc(regex_end_len + 1);
                if (spec->regex_end) {
                    memcpy(spec->regex_end, regex_end_begin, regex_end_len);
                    spec->regex_end[regex_end_len] = '\0';
                }
                pos = regex_end_end + 1;
            }
        }
    } else {
        /* Line number format */
        spec->is_line_range = true;
        char *endptr;
        spec->start_line = (int)strtol(pos, &endptr, 10);

        if (endptr == pos || spec->start_line < 1) {
            /* Invalid start line, check if it's just a prefix */
            spec->is_line_range = false;
            spec->start_line = 0;
            spec->end_line = -1;
        } else {
            pos = endptr;

            /* Skip whitespace */
            while (*pos && isspace(*pos)) pos++;

            if (*pos == ',') {
                pos++;
                /* Skip whitespace after comma */
                while (*pos && isspace(*pos)) pos++;

                if (*pos == '\0' || *pos == ';') {
                    /* N, format - from line N to end */
                    spec->end_line = -1;
                } else {
                    /* N,M format */
                    spec->end_line = (int)strtol(pos, &endptr, 10);
                    if (endptr == pos || spec->end_line < spec->start_line) {
                        /* Invalid end line */
                        spec->end_line = -1;
                    }
                    pos = endptr;
                }
            } else {
                /* Just N - single line */
                spec->end_line = spec->start_line;
            }
        }
    }

    /* Look for prefix parameter */
    while (*pos && isspace(*pos)) pos++;
    if (*pos == ';') {
        pos++;
        while (*pos && isspace(*pos)) pos++;

        /* Check for prefix="..." */
        if (strncmp(pos, "prefix=\"", 8) == 0) {
            pos += 8;
            const char *prefix_start = pos;
            const char *prefix_end = strchr(prefix_start, '"');

            if (prefix_end) {
                size_t prefix_len = prefix_end - prefix_start;
                spec->prefix = malloc(prefix_len + 1);
                if (spec->prefix) {
                    memcpy(spec->prefix, prefix_start, prefix_len);
                    spec->prefix[prefix_len] = '\0';
                }
            }
        }
    }

    return spec;
}

/**
 * Extract lines from content based on address specification
 */
static char *extract_lines(const char *content, address_spec_t *spec) {
    if (!content || !spec) return NULL;

    /* If no specification, return full content */
    if (!spec->is_line_range && !spec->is_regex_range) {
        if (spec->prefix) {
            /* Apply prefix to all lines */
            size_t content_len = strlen(content);
            size_t prefix_len = strlen(spec->prefix);
            size_t lines = 0;
            const char *p = content;
            while (*p) {
                if (*p == '\n') lines++;
                p++;
            }
            if (*p == '\0' && p > content && p[-1] != '\n') lines++;

            size_t output_size = content_len + (lines * prefix_len) + 1;
            char *output = malloc(output_size);
            if (!output) return NULL;

            char *write = output;
            const char *read = content;
            bool at_line_start = true;

            while (*read) {
                if (at_line_start && *read != '\n') {
                    strcpy(write, spec->prefix);
                    write += prefix_len;
                    at_line_start = false;
                }
                *write++ = *read++;
                if (read[-1] == '\n') {
                    at_line_start = true;
                }
            }
            *write = '\0';
            return output;
        }
        return strdup(content);
    }

    /* Count lines in content */
    int total_lines = 1;
    const char *p = content;
    while (*p) {
        if (*p == '\n') total_lines++;
        p++;
    }

    if (spec->is_regex_range) {
        /* Regex-based extraction */
        regex_t regex_start, regex_end;
        int ret_start = 0, ret_end = 0;
        bool compiled_start = false, compiled_end = false;

        /* Compile start regex */
        if (spec->regex_start) {
            ret_start = regcomp(&regex_start, spec->regex_start, REG_EXTENDED);
            if (ret_start == 0) {
                compiled_start = true;
            }
        }

        /* Compile end regex */
        if (spec->regex_end) {
            ret_end = regcomp(&regex_end, spec->regex_end, REG_EXTENDED);
            if (ret_end == 0) {
                compiled_end = true;
            }
        }

        if (!compiled_start && !compiled_end) {
            /* No valid regex, return full content */
            if (compiled_start) regfree(&regex_start);
            if (compiled_end) regfree(&regex_end);
            return spec->prefix ? extract_lines(content, spec) : strdup(content);
        }

        /* Find line numbers matching regex patterns */
        int start_line = 1;
        int end_line = total_lines;
        regmatch_t match;
        int line_num = 1;
        const char *line_start = content;
        bool found_start = false;

        while (*line_start && line_num <= total_lines) {
            const char *line_end = strchr(line_start, '\n');
            if (!line_end) line_end = line_start + strlen(line_start);

            size_t line_len = line_end - line_start;
            char *line = malloc(line_len + 1);
            if (!line) {
                if (compiled_start) regfree(&regex_start);
                if (compiled_end) regfree(&regex_end);
                return NULL;
            }
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';

            /* Check start pattern */
            if (compiled_start && !found_start) {
                if (regexec(&regex_start, line, 1, &match, 0) == 0) {
                    start_line = line_num;
                    found_start = true;
                }
            }

            /* Check end pattern (only if start found or no start pattern) */
            if (compiled_end && (found_start || !compiled_start)) {
                if (regexec(&regex_end, line, 1, &match, 0) == 0) {
                    end_line = line_num;
                    break;
                }
            }

            free(line);
            line_start = line_end;
            if (*line_start == '\n') line_start++;
            line_num++;
        }

        if (compiled_start) regfree(&regex_start);
        if (compiled_end) regfree(&regex_end);

        /* If start not found and we have a start pattern, return empty */
        if (compiled_start && !found_start) {
            return strdup("");
        }

        /* Extract lines from start_line to end_line */
        line_num = 1;
        line_start = content;
        size_t output_size = strlen(content) + 1024;
        char *output = malloc(output_size);
        if (!output) return NULL;
        char *write = output;
        size_t remaining = output_size;

        while (*line_start && line_num <= total_lines) {
            const char *line_end = strchr(line_start, '\n');
            if (!line_end) line_end = line_start + strlen(line_start);

            if (line_num >= start_line && line_num < end_line) {
                size_t line_len = line_end - line_start;
                if (line_len + 100 < remaining) {
                    if (spec->prefix) {
                        size_t prefix_len = strlen(spec->prefix);
                        if (prefix_len + line_len + 10 < remaining) {
                            strcpy(write, spec->prefix);
                            write += prefix_len;
                            remaining -= prefix_len;
                        }
                    }
                    memcpy(write, line_start, line_len);
                    write += line_len;
                    remaining -= line_len;
                    *write++ = '\n';
                    remaining--;
                }
            }

            line_start = line_end;
            if (*line_start == '\n') line_start++;
            line_num++;
        }

        *write = '\0';
        return output;
    } else {
        /* Line number-based extraction */
        if (spec->start_line < 1 || spec->start_line > total_lines) {
            return strdup("");
        }

        int end = spec->end_line;
        if (end == -1) {
            end = total_lines;
        } else if (end > total_lines) {
            end = total_lines;
        }
        if (end < spec->start_line) {
            return strdup("");
        }

        /* Extract lines */
        int line_num = 1;
        const char *line_start = content;
        size_t output_size = strlen(content) + 1024;
        char *output = malloc(output_size);
        if (!output) return NULL;
        char *write = output;
        size_t remaining = output_size;

        while (*line_start && line_num <= total_lines) {
            const char *line_end = strchr(line_start, '\n');
            if (!line_end) line_end = line_start + strlen(line_start);

            if (line_num >= spec->start_line && line_num < end) {
                size_t line_len = line_end - line_start;
                if (line_len + 100 < remaining) {
                    if (spec->prefix) {
                        size_t prefix_len = strlen(spec->prefix);
                        if (prefix_len + line_len + 10 < remaining) {
                            strcpy(write, spec->prefix);
                            write += prefix_len;
                            remaining -= prefix_len;
                        }
                    }
                    memcpy(write, line_start, line_len);
                    write += line_len;
                    remaining -= line_len;
                    *write++ = '\n';
                    remaining--;
                }
            }

            line_start = line_end;
            if (*line_start == '\n') line_start++;
            line_num++;
        }

        *write = '\0';
        return output;
    }
}

/**
 * Resolve wildcard path.
 *
 * Supported patterns:
 * - Legacy "file.*" patterns:
 *   - Preferentially resolve to file.html, file.md, file.txt, file.tex (in that order)
 * - General globbing using standard shell-style patterns:
 *   - '*' and '?' wildcards
 *   - Character classes '[]'
 *   - Brace expansion '{a,b}.md', '*.{html,md}' where supported by the platform
 *
 * The path is resolved relative to base_dir (or current directory) before globbing.
 * Returns a newly-allocated path string or NULL if no match is found.
 */
char *apex_resolve_wildcard(const char *filepath, const char *base_dir) {
    if (!filepath) return NULL;

    /* Fast path: legacy "file.*" handling with explicit extension preference.
     * Only trigger this when the pattern ends with ".*" and contains no
     * other glob characters. This preserves the documented MMD-style
     * wildcard behavior while allowing more general globbing for
     * patterns like "*.c" or "{intro,part1}.md".
     */
    const char *wildcard = strstr(filepath, ".*");
    bool has_other_glob = (strpbrk(filepath, "*?[{") != NULL &&
                           !(wildcard && wildcard[1] == '\0'));

    if (wildcard && !has_other_glob) {
        /* Extract base filename (before .*) */
        size_t base_len = (size_t)(wildcard - filepath);
        char base_filename[1024];
        if (base_len >= sizeof(base_filename)) return NULL;

        memcpy(base_filename, filepath, base_len);
        base_filename[base_len] = '\0';

        /* Try common extensions in preferred order */
        const char *extensions[] = {".html", ".md", ".txt", ".tex", NULL};

        for (int i = 0; extensions[i]; i++) {
            char test_path[1024];
            snprintf(test_path, sizeof(test_path), "%s%s", base_filename, extensions[i]);

            char *resolved = resolve_path(test_path, base_dir);
            if (resolved && apex_file_exists(resolved)) {
                return resolved;
            }
            free(resolved);
        }

        /* No match found for legacy pattern, fall through to globbing
         * so that users can still benefit from more general patterns
         * if they mixed syntax.
         */
    }

    /* General case: use glob() to resolve shell-style patterns
     * (supports *, ?, [], and optionally brace expansion).
     */
    if (strpbrk(filepath, "*?[{") != NULL) {
        char *pattern_path = resolve_path(filepath, base_dir);
        if (!pattern_path) return NULL;

        glob_t results;
        int flags = 0;
#ifdef GLOB_BRACE
        /* Enable brace expansion where available (BSD/macOS, some libcs) */
        if (strchr(pattern_path, '{') || strchr(pattern_path, '}')) {
            flags |= GLOB_BRACE;
        }
#endif

        int rc = glob(pattern_path, flags, NULL, &results);
        free(pattern_path);

        if (rc == 0 && results.gl_pathc > 0 && results.gl_pathv[0]) {
            char *match = strdup(results.gl_pathv[0]);
            globfree(&results);
            return match;
        }

        globfree(&results);
        return NULL;
    }

    /* No wildcard characters - behave like resolve_path */
    return resolve_path(filepath, base_dir);
}

/**
 * Get transclude base from metadata, or return default base_dir
 * Returns newly allocated string (caller must free) or NULL
 */
static char *get_transclude_base(const char *base_dir, apex_metadata_item *metadata) {
    if (!metadata) {
        return base_dir ? strdup(base_dir) : NULL;
    }

    /* Note: apex_metadata_get now handles case-insensitive matching and spaces being removed
     * So "transclude base", "Transclude Base", "transcludebase" all work
     */
    const char *transclude_base = apex_metadata_get(metadata, "transclude base");

    if (transclude_base) {
        /* If absolute path, return as-is */
        if (transclude_base[0] == '/') {
            return strdup(transclude_base);
        }

        /* If relative path starting with ".", use current file's directory */
        if (transclude_base[0] == '.' && (transclude_base[1] == '/' || transclude_base[1] == '\0')) {
            if (base_dir) {
                return strdup(base_dir);
            }
            return strdup(".");
        }

        /* Relative path - combine with base_dir */
        if (base_dir) {
            size_t len = strlen(base_dir) + strlen(transclude_base) + 2;
            char *result = malloc(len);
            if (result) {
                snprintf(result, len, "%s/%s", base_dir, transclude_base);
            }
            return result;
        }

        return strdup(transclude_base);
    }

    /* No transclude base metadata, use default */
    return base_dir ? strdup(base_dir) : NULL;
}

/**
 * Normalize fenced code block delimiters to use odd numbers of backticks.
 * This ensures our simple toggle-based code span detection works correctly.
 * Only modifies fence delimiters at the start of lines, not backticks
 * inside code blocks.
 */
static char *normalize_fence_delimiters(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    char *output = malloc(len * 2 + 1);  /* Allocate generously */
    if (!output) return NULL;

    char *write = output;
    const char *read = text;
    bool in_fenced_block = false;
    int original_fence_length = 0;  /* Original count from opening fence */
    int normalized_fence_length = 0; /* Normalized count (always odd) */

    while (*read) {
        /* Check if we're at the start of a line */
        bool at_line_start = (read == text || read[-1] == '\n');

        if (at_line_start && *read == '`') {
            /* Count consecutive backticks */
            int backtick_count = 0;
            const char *backtick_start = read;
            while (*read == '`') {
                backtick_count++;
                read++;
            }

            /* Check if this looks like a fence delimiter:
             * - At least 3 backticks
             * - Followed by optional language info and newline or end of text
             */
            bool is_fence = (backtick_count >= 3);
            if (is_fence) {
                /* Check what follows - should be whitespace, language info, or newline */
                const char *after_backticks = read;
                while (*after_backticks && *after_backticks != '\n' &&
                       *after_backticks != '\r') {
                    after_backticks++;
                }

                /* If we're closing a fence, check if it matches the original length */
                if (in_fenced_block && backtick_count == original_fence_length) {
                    /* Closing fence that matches opening - normalize to match the normalized opening fence */
                    for (int i = 0; i < normalized_fence_length; i++) {
                        *write++ = '`';
                    }
                    in_fenced_block = false;
                    original_fence_length = 0;
                    normalized_fence_length = 0;
                } else if (!in_fenced_block) {
                    /* Opening fence - normalize to odd number */
                    if (backtick_count % 2 == 0) {
                        /* Even number - add one backtick */
                        normalized_fence_length = backtick_count + 1;
                        for (int i = 0; i < normalized_fence_length; i++) {
                            *write++ = '`';
                        }
                    } else {
                        /* Already odd - copy as-is */
                        normalized_fence_length = backtick_count;
                        memcpy(write, backtick_start, backtick_count);
                        write += backtick_count;
                    }
                    original_fence_length = backtick_count;
                    in_fenced_block = true;
                } else {
                    /* Inside a code block but this doesn't match our fence - copy as-is */
                    memcpy(write, backtick_start, backtick_count);
                    write += backtick_count;
                }

                /* Copy the rest of the line */
                while (read < after_backticks) {
                    *write++ = *read++;
                }
            } else {
                /* Not a fence delimiter - copy backticks as-is */
                memcpy(write, backtick_start, backtick_count);
                write += backtick_count;
            }
        } else {
            /* Not at start of line or not a backtick - copy character */
            *write++ = *read++;
        }
    }

    *write = '\0';
    return output;
}

/* Parse optional delimiter override tokens used after CSV/TSV includes.
 * Supported forms:
 *   {;}
 *   {delimiter=;}
 *   {delimiter=\t}
 */
static bool parse_include_delimiter_override(const char *start, const char **after_out, char *delimiter_out) {
    if (!start || !after_out || !delimiter_out) return false;

    const char *p = start;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return false;

    const char *close = strchr(p + 1, '}');
    if (!close || close <= p + 1) return false;

    char inner[64];
    size_t inner_len = (size_t)(close - (p + 1));
    if (inner_len >= sizeof(inner)) return false;
    memcpy(inner, p + 1, inner_len);
    inner[inner_len] = '\0';

    char *s = inner;
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';

    if (s[0] == '\0') return false;

    /* Shorthand: {;} */
    if (s[0] && s[1] == '\0') {
        *delimiter_out = s[0];
        *after_out = close + 1;
        return true;
    }

    /* Verbose: {delimiter=;} */
    const char *key = "delimiter";
    size_t key_len = strlen(key);
    if (strncasecmp(s, key, key_len) != 0) return false;
    s += key_len;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s != '=') return false;
    s++;
    while (*s && isspace((unsigned char)*s)) s++;

    if (s[0] == '\\' && s[1] == 't' && s[2] == '\0') {
        *delimiter_out = '\t';
        *after_out = close + 1;
        return true;
    }
    if (s[0] && s[1] == '\0') {
        *delimiter_out = s[0];
        *after_out = close + 1;
        return true;
    }

    return false;
}

/* Parse delimiter override embedded at end of filepath, e.g.:
 *   data.csv{;}
 *   data.csv{delimiter=;}
 * If found, trims filepath in-place and sets delimiter_out.
 */
static bool parse_embedded_delimiter_override(char *filepath, char *delimiter_out) {
    if (!filepath || !delimiter_out) return false;

    char *brace = strrchr(filepath, '{');
    if (!brace) return false;
    size_t tail_len = strlen(brace);
    if (tail_len < 3) return false; /* at least "{x}" */
    if (brace[tail_len - 1] != '}') return false;

    const char *after = NULL;
    char parsed = '\0';
    if (!parse_include_delimiter_override(brace, &after, &parsed)) return false;
    if (after == NULL || *after != '\0') return false;

    *brace = '\0';
    *delimiter_out = parsed;
    return true;
}

static char *trim_copy(const char *s, size_t len) {
    if (!s) return NULL;
    while (len > 0 && isspace((unsigned char)*s)) {
        s++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static void split_section_fragment(char *filepath, char **section_out) {
    if (section_out) *section_out = NULL;
    if (!filepath) return;

    char *hash = strchr(filepath, '#');
    if (!hash) return;

    *hash = '\0';
    hash++;
    while (*hash && isspace((unsigned char)*hash)) hash++;
    if (!*hash) return;

    if (section_out) {
        *section_out = strdup(hash);
    }
}

static bool has_extension(const char *path) {
    if (!path || !*path) return false;
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    return dot && dot[1] != '\0';
}

static char *with_default_extension(const char *path, const char *ext_without_dot) {
    if (!path || !*path) return NULL;
    if (has_extension(path)) return strdup(path);
    if (!ext_without_dot || !*ext_without_dot) return strdup(path);

    size_t len = strlen(path) + strlen(ext_without_dot) + 2;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s.%s", path, ext_without_dot);
    return out;
}

static const char *next_line_start(const char *line_end) {
    if (!line_end) return NULL;
    if (*line_end == '\n') return line_end + 1;
    if (*line_end == '\0') return line_end;
    return line_end + 1;
}

static bool parse_setext_underline(const char *line, size_t len, int *level_out) {
    if (!line || len == 0) return false;
    size_t i = 0;
    while (i < len && line[i] == ' ') i++;
    if (i > 3 || i >= len) return false;

    char marker = 0;
    int marker_count = 0;
    for (; i < len; i++) {
        char c = line[i];
        if (c == '\r') continue;
        if (c == '=' || c == '-') {
            if (marker == 0) marker = c;
            if (c != marker) return false;
            marker_count++;
            continue;
        }
        if (!isspace((unsigned char)c)) return false;
    }
    if (marker_count == 0 || marker == 0) return false;
    if (level_out) *level_out = (marker == '=') ? 1 : 2;
    return true;
}

static char *normalize_heading_text(const char *start, size_t len, bool is_atx) {
    if (!start) return NULL;

    while (len > 0 && isspace((unsigned char)*start)) {
        start++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }

    if (is_atx && len > 0) {
        size_t t = len;
        while (t > 0 && start[t - 1] == '#') t--;
        if (t < len) {
            size_t u = t;
            while (u > 0 && isspace((unsigned char)start[u - 1])) u--;
            if (u < t) len = u;
        }
    }

    /* Collapse spaces and lowercase for matching. */
    char *out = malloc(len + 1);
    if (!out) return NULL;

    size_t w = 0;
    bool in_space = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)start[i];
        if (isspace(c)) {
            if (!in_space) {
                out[w++] = ' ';
                in_space = true;
            }
            continue;
        }
        out[w++] = (char)tolower(c);
        in_space = false;
    }
    while (w > 0 && out[w - 1] == ' ') w--;
    out[w] = '\0';
    return out;
}

static bool parse_atx_heading(const char *line_start, const char *line_end,
                              int *level_out, char **normalized_out) {
    if (normalized_out) *normalized_out = NULL;
    if (!line_start || !line_end || line_end < line_start) return false;

    const char *p = line_start;
    int leading_spaces = 0;
    while (p < line_end && *p == ' ' && leading_spaces < 4) {
        p++;
        leading_spaces++;
    }
    if (leading_spaces > 3 || p >= line_end || *p != '#') return false;

    int level = 0;
    while (p < line_end && *p == '#' && level < 6) {
        p++;
        level++;
    }
    if (level == 0) return false;
    if (p < line_end && !isspace((unsigned char)*p)) return false;

    while (p < line_end && isspace((unsigned char)*p)) p++;
    size_t text_len = (size_t)(line_end - p);

    if (level_out) *level_out = level;
    if (normalized_out) {
        *normalized_out = normalize_heading_text(p, text_len, true);
    }
    return true;
}

static char *extract_markdown_section(const char *content, const char *section_name) {
    if (!content || !section_name || !*section_name) {
        return content ? strdup(content) : NULL;
    }

    char *target = normalize_heading_text(section_name, strlen(section_name), false);
    if (!target || !*target) {
        if (target) free(target);
        return NULL;
    }

    const char *cursor = content;
    const char *section_start = NULL;
    const char *section_end = NULL;
    int target_level = 0;
    bool found = false;

    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        if (!line_end) line_end = cursor + strlen(cursor);
        const char *next = next_line_start(line_end);
        const char *next_end = (*next) ? (strchr(next, '\n') ? strchr(next, '\n') : next + strlen(next)) : next;

        int level = 0;
        char *heading_norm = NULL;
        bool heading_found = parse_atx_heading(cursor, line_end, &level, &heading_norm);
        bool is_setext = false;

        if (!heading_found && *next && next > cursor) {
            int setext_level = 0;
            size_t cur_len = (size_t)(line_end - cursor);
            if (cur_len > 0 && parse_setext_underline(next, (size_t)(next_end - next), &setext_level)) {
                heading_found = true;
                is_setext = true;
                level = setext_level;
                heading_norm = normalize_heading_text(cursor, cur_len, false);
            }
        }

        if (heading_found) {
            if (!found) {
                if (heading_norm && strcmp(heading_norm, target) == 0) {
                    found = true;
                    target_level = level;
                    section_start = cursor;
                    if (is_setext) {
                        cursor = (*next_end == '\n') ? (next_end + 1) : next_end;
                        free(heading_norm);
                        continue;
                    }
                }
            } else {
                if (level <= target_level) {
                    section_end = cursor;
                    free(heading_norm);
                    break;
                }
            }
        }

        if (heading_norm) free(heading_norm);
        cursor = (*line_end == '\n') ? (line_end + 1) : line_end;
    }

    free(target);

    if (!found || !section_start) return NULL;
    if (!section_end) section_end = content + strlen(content);

    size_t out_len = (size_t)(section_end - section_start);
    char *out = malloc(out_len + 1);
    if (!out) return NULL;
    memcpy(out, section_start, out_len);
    out[out_len] = '\0';
    return out;
}

/* Find closing "}}" for MMD transclusion, allowing embedded single-brace
 * segments inside filepath (e.g. {{data.csv{;}}}). */
static const char *find_mmd_transclusion_end(const char *filepath_start) {
    if (!filepath_start) return NULL;

    int brace_depth = 0;
    const char *p = filepath_start;
    while (*p) {
        if (p[0] == '{') {
            brace_depth++;
            p++;
            continue;
        }
        if (p[0] == '}' && brace_depth > 0) {
            brace_depth--;
            p++;
            continue;
        }
        if (p[0] == '}' && p[1] == '}' && brace_depth == 0) {
            return p;
        }
        p++;
    }

    return NULL;
}

static char *run_preparse_plugins_on_text(const char *text,
                                          apex_plugin_manager *plugin_manager,
                                          const apex_options *options) {
    if (!text) {
        return NULL;
    }

    if (!plugin_manager || !options) {
        return NULL;
    }

    char *plugin_text = apex_plugins_run_text_phase(plugin_manager,
                                                    APEX_PLUGIN_PHASE_PRE_PARSE,
                                                    text,
                                                    options);
    return plugin_text;
}

/**
 * Process file includes in text
 */
char *apex_process_includes(const char *text,
                            const char *base_dir,
                            apex_metadata_item *metadata,
                            int depth,
                            const char *default_extension,
                            apex_plugin_manager *plugin_manager,
                            const apex_options *options) {
    if (!text) return NULL;
    if (depth > MAX_INCLUDE_DEPTH) {
        return strdup(text);  /* Silently return original text */
    }

    /* Normalize fenced code block delimiters to odd numbers of backticks
     * so our toggle-based code span detection works correctly */
    char *normalized_text = normalize_fence_delimiters(text);
    if (!normalized_text) return NULL;

    const char *text_to_process = normalized_text;

    size_t text_len = strlen(text_to_process);
    size_t output_capacity = text_len * 10;  /* Generous for includes */
    if (output_capacity < 1024 * 1024) output_capacity = 1024 * 1024;  /* At least 1MB */
    char *output = malloc(output_capacity);
    if (!output) {
        free(normalized_text);
        return NULL;
    }

    /* Get effective base directory from transclude base metadata */
    char *effective_base_dir = get_transclude_base(base_dir, metadata);
    if (!effective_base_dir && base_dir) {
        effective_base_dir = strdup(base_dir);
    }

    const char *read_pos = text_to_process;
    char *write_pos = output;
    size_t remaining = output_capacity;
    bool in_code_span = false; /* Tracks inline/fenced code spans delimited by backticks */

    while (*read_pos) {
        bool processed_include = false;

        /* Toggle code-span state on backticks so we can avoid
         * processing include syntax that appears inside inline
         * or fenced code (e.g. `<<[path/file]`). This is a simple
         * state machine that treats all backticks as paired; it
         * correctly covers common cases like single backtick code
         * spans and triple-backtick fences. */
        if (*read_pos == '`') {
            in_code_span = !in_code_span;
        }

        /* Obsidian embed syntax: ![[file]] or ![[file#Section]] */
        if (!in_code_span && read_pos[0] == '!' && read_pos[1] == '[' && read_pos[2] == '[') {
            const char *content_start = read_pos + 3;
            const char *close = strstr(content_start, "]]");
            if (close && (close - content_start) > 0 && (close - content_start) < 1024) {
                char target[1024];
                size_t target_len = (size_t)(close - content_start);
                memcpy(target, content_start, target_len);
                target[target_len] = '\0';

                char *pipe = strchr(target, '|');
                if (pipe) *pipe = '\0'; /* Ignore alias text for includes */

                percent_decode_inplace(target);

                char *section_name = NULL;
                split_section_fragment(target, &section_name);

                const char *effective_default_ext = default_extension;
                while (effective_default_ext && *effective_default_ext == '.') {
                    effective_default_ext++;
                }
                if (effective_default_ext && *effective_default_ext == '\0') {
                    effective_default_ext = NULL;
                }

                bool had_explicit_ext = has_extension(target);
                char *target_with_ext = with_default_extension(target, effective_default_ext ? effective_default_ext : "md");
                char *resolved_path = apex_resolve_wildcard(target_with_ext ? target_with_ext : target, effective_base_dir);
                if (!resolved_path) {
                    resolved_path = resolve_path(target_with_ext ? target_with_ext : target, effective_base_dir);
                }

                /* If a non-md default extension was configured but didn't resolve,
                 * fall back to .md to keep Obsidian-style embeds practical. */
                if (!had_explicit_ext &&
                    (!resolved_path || !apex_file_exists(resolved_path)) &&
                    effective_default_ext &&
                    strcasecmp(effective_default_ext, "md") != 0) {
                    char *md_target = with_default_extension(target, "md");
                    char *md_resolved = apex_resolve_wildcard(md_target ? md_target : target, effective_base_dir);
                    if (!md_resolved) {
                        md_resolved = resolve_path(md_target ? md_target : target, effective_base_dir);
                    }
                    if (md_target) free(md_target);
                    if (md_resolved && apex_file_exists(md_resolved)) {
                        if (resolved_path) free(resolved_path);
                        resolved_path = md_resolved;
                    } else if (md_resolved) {
                        free(md_resolved);
                    }
                }

                if (resolved_path && apex_file_exists(resolved_path)) {
                    apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                    char *content = read_file_contents(resolved_path);
                    if (content) {
                        char *to_insert = NULL;
                        bool free_to_insert = false;

                        if (file_type == FILE_TYPE_IMAGE) {
                            size_t buf_size = strlen(target_with_ext ? target_with_ext : target) + 10;
                            to_insert = malloc(buf_size);
                            if (to_insert) snprintf(to_insert, buf_size, "![](%s)\n", target_with_ext ? target_with_ext : target);
                            free_to_insert = true;
                        } else if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                            char *plugin_csv_content = run_preparse_plugins_on_text(content, plugin_manager, options);
                            const char *csv_content = plugin_csv_content ? plugin_csv_content : content;
                            to_insert = apex_csv_to_table_with_delimiter(csv_content, file_type == FILE_TYPE_TSV, '\0');
                            if (plugin_csv_content) free(plugin_csv_content);
                            free_to_insert = true;
                        } else if (file_type == FILE_TYPE_CODE) {
                            char *plugin_code_content = run_preparse_plugins_on_text(content, plugin_manager, options);
                            const char *code_content = plugin_code_content ? plugin_code_content : content;
                            const char *ext = strrchr(target_with_ext ? target_with_ext : target, '.');
                            const char *lang = ext ? ext + 1 : "";
                            size_t buf_size = strlen(code_content) + strlen(lang) + 20;
                            to_insert = malloc(buf_size);
                            if (to_insert) snprintf(to_insert, buf_size, "\n```%s\n%s\n```\n", lang, code_content);
                            if (plugin_code_content) free(plugin_code_content);
                            free_to_insert = true;
                        } else {
                            char *section_content = section_name ? extract_markdown_section(content, section_name) : NULL;
                            char *base_content = section_content ? section_content : strdup(content);
                            char *plugin_base_content = run_preparse_plugins_on_text(base_content ? base_content : content,
                                                                                     plugin_manager,
                                                                                     options);
                            const char *content_for_processing = plugin_base_content ? plugin_base_content : (base_content ? base_content : content);

                            char *file_content_for_metadata = strdup(content_for_processing);
                            apex_metadata_item *file_metadata = NULL;
                            char *file_text_after_metadata = file_content_for_metadata;
                            if (file_content_for_metadata) {
                                file_metadata = apex_extract_metadata(&file_text_after_metadata);
                            }

                            char *transclude_base = NULL;
                            if (file_metadata) {
                                transclude_base = get_transclude_base(get_directory(resolved_path), file_metadata);
                            }
                            if (!transclude_base) {
                                transclude_base = get_directory(resolved_path);
                            }

                            to_insert = apex_process_includes(content_for_processing,
                                                              transclude_base,
                                                              file_metadata,
                                                              depth + 1,
                                                              default_extension,
                                                              plugin_manager,
                                                              options);
                            free_to_insert = true;

                            if (transclude_base) free(transclude_base);
                            if (file_metadata) apex_free_metadata(file_metadata);
                            if (file_content_for_metadata) free(file_content_for_metadata);
                            if (plugin_base_content) free(plugin_base_content);
                            if (base_content) free(base_content);
                        }

                        if (to_insert) {
                            size_t insert_len = strlen(to_insert);
                            if (insert_len < remaining) {
                                memcpy(write_pos, to_insert, insert_len);
                                write_pos += insert_len;
                                remaining -= insert_len;
                            }
                            if (free_to_insert) free(to_insert);
                        }

                        free(content);
                        read_pos = close + 2;
                        processed_include = true;
                    }
                }

                if (target_with_ext) free(target_with_ext);
                if (resolved_path) free(resolved_path);
                if (section_name) free(section_name);
            }
        }

        /* Look for iA Writer transclusion /filename (at start of line only) */
        if (!in_code_span && read_pos[0] == '/' && (read_pos == text_to_process || read_pos[-1] == '\n')) {
            const char *filepath_start = read_pos + 1;
            const char *line_end = read_pos;
            while (*line_end && *line_end != '\n' && *line_end != '\r') {
                line_end++;
            }

            if (line_end > filepath_start && (line_end - filepath_start) < 1024) {
                char filepath[1024];
                size_t filepath_len = (size_t)(line_end - filepath_start);
                char ia_delimiter_override = '\0';
                memcpy(filepath, filepath_start, filepath_len);
                filepath[filepath_len] = '\0';

                /* Trim leading/trailing whitespace for line-based iA include. */
                char *trimmed = trim_copy(filepath, strlen(filepath));
                if (!trimmed) {
                    read_pos = line_end;
                    processed_include = true;
                    continue;
                }
                strncpy(filepath, trimmed, sizeof(filepath) - 1);
                filepath[sizeof(filepath) - 1] = '\0';
                free(trimmed);

                /* Allow trailing delimiter override on iA syntax, e.g. /data.csv {delimiter=;} */
                char *last_brace = strrchr(filepath, '{');
                if (last_brace) {
                    const char *after_override = NULL;
                    char parsed = '\0';
                    if (parse_include_delimiter_override(last_brace, &after_override, &parsed) &&
                        after_override && *after_override == '\0') {
                        ia_delimiter_override = parsed;
                        *last_brace = '\0';
                        size_t trim_len = strlen(filepath);
                        while (trim_len > 0 && isspace((unsigned char)filepath[trim_len - 1])) {
                            filepath[trim_len - 1] = '\0';
                            trim_len--;
                        }
                    }
                }

                percent_decode_inplace(filepath);
                parse_embedded_delimiter_override(filepath, &ia_delimiter_override);

                char *section_name = NULL;
                split_section_fragment(filepath, &section_name);

                /* Resolve and check file exists */
                char *resolved_path = resolve_path(filepath, effective_base_dir);
                if (resolved_path && apex_file_exists(resolved_path)) {
                    apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                    char *content = read_file_contents(resolved_path);

                    if (content) {
                        char *to_insert = NULL;

                        if (file_type == FILE_TYPE_IMAGE) {
                            /* Image: create ![](path) */
                            size_t buf_size = strlen(filepath) + 10;
                            to_insert = malloc(buf_size);
                            if (to_insert) snprintf(to_insert, buf_size, "![](%s)\n", filepath);
                        } else if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                            /* CSV/TSV: convert to table */
                            char delimiter_override = ia_delimiter_override;
                            char *plugin_csv_content = run_preparse_plugins_on_text(content, plugin_manager, options);
                            const char *csv_content = plugin_csv_content ? plugin_csv_content : content;
                            to_insert = apex_csv_to_table_with_delimiter(csv_content, file_type == FILE_TYPE_TSV, delimiter_override);
                            if (plugin_csv_content) free(plugin_csv_content);
                        } else if (file_type == FILE_TYPE_CODE) {
                            /* Code: wrap in fenced code block */
                            char *plugin_code_content = run_preparse_plugins_on_text(content, plugin_manager, options);
                            const char *code_content = plugin_code_content ? plugin_code_content : content;
                            const char *ext = strrchr(filepath, '.');
                            const char *lang = ext ? ext + 1 : "";
                            size_t buf_size = strlen(code_content) + strlen(lang) + 20;
                            to_insert = malloc(buf_size);
                            if (to_insert) snprintf(to_insert, buf_size, "\n```%s\n%s\n```\n", lang, code_content);
                            if (plugin_code_content) free(plugin_code_content);
                        } else {
                            /* Text/Markdown: process and include */
                            char *section_content = section_name ? extract_markdown_section(content, section_name) : NULL;
                            char *base_text = section_content ? section_content : strdup(content);
                            char *plugin_base_text = run_preparse_plugins_on_text(base_text ? base_text : content,
                                                                                  plugin_manager,
                                                                                  options);
                            const char *content_for_processing = plugin_base_text ? plugin_base_text : (base_text ? base_text : content);

                            /* Extract metadata from transcluded file */
                            char *file_content_for_metadata = strdup(content_for_processing);
                            apex_metadata_item *file_metadata = NULL;
                            char *file_text_after_metadata = file_content_for_metadata;
                            if (file_content_for_metadata) {
                                file_metadata = apex_extract_metadata(&file_text_after_metadata);
                            }

                            /* Get transclude base from file's metadata, or use file's directory */
                            char *transclude_base = NULL;
                            if (file_metadata) {
                                transclude_base = get_transclude_base(get_directory(resolved_path), file_metadata);
                            }
                            if (!transclude_base) {
                                transclude_base = get_directory(resolved_path);
                            }

                            to_insert = apex_process_includes(content_for_processing,
                                                              transclude_base,
                                                              file_metadata,
                                                              depth + 1,
                                                              default_extension,
                                                              plugin_manager,
                                                              options);

                            /* Cleanup */
                            if (transclude_base) free(transclude_base);
                            if (file_metadata) apex_free_metadata(file_metadata);
                            if (file_content_for_metadata) free(file_content_for_metadata);
                            if (plugin_base_text) free(plugin_base_text);
                            if (base_text) free(base_text);
                        }

                        if (to_insert) {
                            size_t insert_len = strlen(to_insert);
                            if (insert_len < remaining) {
                                memcpy(write_pos, to_insert, insert_len);
                                write_pos += insert_len;
                                remaining -= insert_len;
                            }
                            if (to_insert != content) free(to_insert);
                        }

                        free(content);
                        free(resolved_path);
                        read_pos = line_end;
                        processed_include = true;
                        if (section_name) free(section_name);
                    } else {
                        free(resolved_path);
                        if (section_name) free(section_name);
                    }
                } else if (resolved_path) {
                    free(resolved_path);
                    if (section_name) free(section_name);
                }
            }
        }

        /* Look for MMD transclusion {{file}} */
        if (!processed_include && !in_code_span && read_pos[0] == '{' && read_pos[1] == '{') {
            const char *filepath_start = read_pos + 2;
            const char *filepath_end = find_mmd_transclusion_end(filepath_start);

            if (filepath_end && (filepath_end - filepath_start) > 0 && (filepath_end - filepath_start) < 1024) {
                /* Extract filepath */
                size_t filepath_len = (size_t)(filepath_end - filepath_start);
                char filepath[1024];
                char mmd_delimiter_override = '\0';
                memcpy(filepath, filepath_start, filepath_len);
                filepath[filepath_len] = '\0';
                percent_decode_inplace(filepath);
                parse_embedded_delimiter_override(filepath, &mmd_delimiter_override);

                char *section_name = NULL;
                split_section_fragment(filepath, &section_name);

                /* Check for address specification [address] */
                const char *address_start = filepath_end + 2;
                const char *address_end = NULL;
                address_spec_t *address_spec = NULL;

                if (*address_start == '[') {
                    address_start++;
                    address_end = strchr(address_start, ']');
                    if (address_end) {
                        size_t address_len = (size_t)(address_end - address_start);
                        char address_str[1024];
                        if (address_len > 0 && address_len < sizeof(address_str)) {
                            memcpy(address_str, address_start, address_len);
                            address_str[address_len] = '\0';
                            address_spec = parse_address_spec(address_str);
                        }
                    }
                }

                /* Resolve path (handle wildcards) */
                char *resolved_path = apex_resolve_wildcard(filepath, effective_base_dir);
                if (!resolved_path) {
                    /* Try without wildcard resolution */
                    resolved_path = resolve_path(filepath, effective_base_dir);
                }

                if (resolved_path) {
                    apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                    char *content = read_file_contents(resolved_path);
                    if (content) {
                        char *section_content = section_name ? extract_markdown_section(content, section_name) : NULL;
                        char *section_base = section_content ? section_content : strdup(content);

                        /* Extract metadata from original file content FIRST (before any processing) */
                        char *file_content_for_metadata = section_base ? strdup(section_base) : NULL;
                        apex_metadata_item *file_metadata = NULL;
                        char *file_text_after_metadata = file_content_for_metadata;
                        if (file_content_for_metadata) {
                            file_metadata = apex_extract_metadata(&file_text_after_metadata);
                        }

                        /* Apply address specification if present */
                        char *extracted_content = section_base ? section_base : content;
                        bool free_extracted = false;

                        if (address_spec) {
                            extracted_content = extract_lines(section_base ? section_base : content, address_spec);
                            if (extracted_content && extracted_content != (section_base ? section_base : content)) {
                                free_extracted = true;
                            }
                        }

                        char *to_process = extracted_content;
                        bool free_to_process = false;

                        /* Convert CSV/TSV to table */
                        if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                            char *table = apex_csv_to_table_with_delimiter(
                                extracted_content,
                                file_type == FILE_TYPE_TSV,
                                mmd_delimiter_override
                            );
                            if (table) {
                                to_process = table;
                                free_to_process = true;
                            }
                        }

                        if (file_type == FILE_TYPE_MARKDOWN || file_type == FILE_TYPE_TEXT) {
                            char *plugin_text = run_preparse_plugins_on_text(to_process, plugin_manager, options);
                            if (plugin_text) {
                                if (free_to_process) free(to_process);
                                to_process = plugin_text;
                                free_to_process = true;
                            }
                        }

                        /* Get transclude base from file's metadata, or use file's directory */
                        char *transclude_base = NULL;
                        if (file_metadata) {
                            transclude_base = get_transclude_base(get_directory(resolved_path), file_metadata);
                        }
                        if (!transclude_base) {
                            transclude_base = get_directory(resolved_path);
                        }

                        /* Recursively process with file's metadata and transclude base */
                        char *processed = apex_process_includes(to_process,
                                                                transclude_base,
                                                                file_metadata,
                                                                depth + 1,
                                                                default_extension,
                                                                plugin_manager,
                                                                options);

                        /* Cleanup */
                        if (transclude_base) free(transclude_base);
                        if (file_metadata) apex_free_metadata(file_metadata);
                        if (file_content_for_metadata) free(file_content_for_metadata);

                        if (processed) {
                            size_t proc_len = strlen(processed);
                            if (proc_len < remaining) {
                                memcpy(write_pos, processed, proc_len);
                                write_pos += proc_len;
                                remaining -= proc_len;
                            }
                            free(processed);
                        }

                        if (free_to_process) free(to_process);
                        if (free_extracted) free(extracted_content);
                        free(content);
                        if (section_base) free(section_base);

                        if (address_end) {
                            read_pos = address_end + 1;
                        } else {
                            read_pos = filepath_end + 2;
                        }
                        free(resolved_path);
                        if (address_spec) free_address_spec(address_spec);
                        processed_include = true;
                        if (section_name) free(section_name);
                    } else {
                        free(resolved_path);
                        if (address_spec) free_address_spec(address_spec);
                        if (section_name) free(section_name);
                    }
                } else {
                    if (address_spec) free_address_spec(address_spec);
                    if (section_name) free(section_name);
                }
            }
        }

        /* Look for << (Marked syntax) at the very start of a line */
        if (!processed_include && !in_code_span &&
            (read_pos == text_to_process || read_pos[-1] == '\n') &&
            read_pos[0] == '<' && read_pos[1] == '<') {
            char bracket_type = 0;
            const char *filepath_start = NULL;
            const char *filepath_end = NULL;

            /* Determine include type */
            if (read_pos[2] == '[') {
                /* <<[file.md] - Markdown include */
                bracket_type = '[';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, ']');
            } else if (read_pos[2] == '(') {
                /* <<(file.ext) - Code block include */
                bracket_type = '(';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, ')');
            } else if (read_pos[2] == '{') {
                /* <<{file.html} - Raw HTML include */
                bracket_type = '{';
                filepath_start = read_pos + 3;
                filepath_end = strchr(filepath_start, '}');
            }

            if (bracket_type && filepath_start && filepath_end) {
                /* Extract filepath */
                size_t filepath_len = (size_t)(filepath_end - filepath_start);
                char filepath[1024];
                char marked_delimiter_override = '\0';
                if (filepath_len > 0 && filepath_len < sizeof(filepath)) {
                    memcpy(filepath, filepath_start, filepath_len);
                    filepath[filepath_len] = '\0';
                    percent_decode_inplace(filepath);
                    if (bracket_type == '[') {
                        parse_embedded_delimiter_override(filepath, &marked_delimiter_override);
                    }

                    char *section_name = NULL;
                    split_section_fragment(filepath, &section_name);

                    /* Check for address specification [address] on the SAME line only.
                     * Do not skip across newlines, otherwise following reference
                     * definitions like "[^1]: ..." are misparsed as include addresses. */
                    const char *address_start = filepath_end + 1;
                    while (*address_start == ' ' || *address_start == '\t') address_start++;
                    const char *address_end = NULL;
                    address_spec_t *address_spec = NULL;

                    if (*address_start == '[') {
                        address_start++;
                        address_end = strchr(address_start, ']');
                        if (address_end) {
                            size_t address_len = (size_t)(address_end - address_start);
                            char address_str[1024];
                            if (address_len > 0 && address_len < sizeof(address_str)) {
                                memcpy(address_str, address_start, address_len);
                                address_str[address_len] = '\0';
                                address_spec = parse_address_spec(address_str);
                            }
                        }
                    }

                    /* Resolve path */
                    char *resolved_path = resolve_path(filepath, effective_base_dir);
                    if (resolved_path) {
                        apex_file_type_t file_type = apex_detect_file_type(resolved_path);
                        char *content = read_file_contents(resolved_path);
                        if (content) {
                            char *section_content = section_name ? extract_markdown_section(content, section_name) : NULL;
                            char *section_base = section_content ? section_content : strdup(content);

                            /* Extract metadata from original file content FIRST (before any processing) */
                            char *file_content_for_metadata = section_base ? strdup(section_base) : NULL;
                            apex_metadata_item *file_metadata = NULL;
                            char *file_text_after_metadata = file_content_for_metadata;
                            if (file_content_for_metadata) {
                                file_metadata = apex_extract_metadata(&file_text_after_metadata);
                            }

                            /* Apply address specification if present */
                            char *extracted_content = section_base ? section_base : content;
                            bool free_extracted = false;

                            if (address_spec) {
                                extracted_content = extract_lines(section_base ? section_base : content, address_spec);
                                if (extracted_content && extracted_content != (section_base ? section_base : content)) {
                                    free_extracted = true;
                                }
                            }

                            /* Process based on include type */
                            if (bracket_type == '[') {
                                char *to_process = extracted_content;
                                bool free_to_process = false;

                                /* Convert CSV/TSV to table */
                                if (file_type == FILE_TYPE_CSV || file_type == FILE_TYPE_TSV) {
                                    char *table = apex_csv_to_table_with_delimiter(
                                        extracted_content,
                                        file_type == FILE_TYPE_TSV,
                                        marked_delimiter_override
                                    );
                                    if (table) {
                                        to_process = table;
                                        free_to_process = true;
                                    }
                                }
                                if (file_type == FILE_TYPE_MARKDOWN || file_type == FILE_TYPE_TEXT) {
                                    char *plugin_text = run_preparse_plugins_on_text(to_process, plugin_manager, options);
                                    if (plugin_text) {
                                        if (free_to_process) free(to_process);
                                        to_process = plugin_text;
                                        free_to_process = true;
                                    }
                                }

                                /* Markdown include - recursively process */

                                /* Get transclude base from file's metadata, or use file's directory */
                                char *transclude_base = NULL;
                                if (file_metadata) {
                                    transclude_base = get_transclude_base(get_directory(resolved_path), file_metadata);
                                }
                                if (!transclude_base) {
                                    transclude_base = get_directory(resolved_path);
                                }

                                char *processed = apex_process_includes(to_process,
                                                                        transclude_base,
                                                                        file_metadata,
                                                                        depth + 1,
                                                                        default_extension,
                                                                        plugin_manager,
                                                                        options);

                                /* Cleanup */
                                if (transclude_base) free(transclude_base);
                                if (file_metadata) apex_free_metadata(file_metadata);
                                if (file_content_for_metadata) free(file_content_for_metadata);

                                if (processed) {
                                    size_t proc_len = strlen(processed);
                                    if (proc_len < remaining) {
                                        memcpy(write_pos, processed, proc_len);
                                        write_pos += proc_len;
                                        remaining -= proc_len;
                                    }
                                    free(processed);
                                }

                                if (free_to_process) free(to_process);
                            } else if (bracket_type == '(') {
                                /* Code block include - wrap in code fence */
                                /* Try to detect language from extension */
                                const char *ext = strrchr(filepath, '.');
                                const char *lang = "";
                                if (ext) {
                                    ext++;
                                    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) lang = "c";
                                    else if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0) lang = "cpp";
                                    else if (strcmp(ext, "py") == 0) lang = "python";
                                    else if (strcmp(ext, "js") == 0) lang = "javascript";
                                    else if (strcmp(ext, "rb") == 0) lang = "ruby";
                                    else if (strcmp(ext, "sh") == 0) lang = "bash";
                                    else lang = ext;
                                }

                                char code_header[128];
                                snprintf(code_header, sizeof(code_header), "\n```%s\n", lang);
                                const char *code_footer = "\n```\n";

                                size_t total_len = strlen(code_header) + strlen(extracted_content) + strlen(code_footer);
                                if (total_len < remaining) {
                                    strcpy(write_pos, code_header);
                                    write_pos += strlen(code_header);
                                    strcpy(write_pos, extracted_content);
                                    write_pos += strlen(extracted_content);
                                    strcpy(write_pos, code_footer);
                                    write_pos += strlen(code_footer);
                                    remaining -= total_len;
                                }
                            } else if (bracket_type == '{') {
                                /* Raw HTML - will be inserted after processing */
                                /* For now, insert a placeholder marker */
                                char marker[1024];
                                snprintf(marker, sizeof(marker), "<!--APEX_RAW_INCLUDE:%s-->", resolved_path);
                                size_t marker_len = strlen(marker);
                                if (marker_len < remaining) {
                                    memcpy(write_pos, marker, marker_len);
                                    write_pos += marker_len;
                                    remaining -= marker_len;
                                }
                            }

                            if (free_extracted) free(extracted_content);
                            free(content);
                            if (section_base) free(section_base);
                        }
                        free(resolved_path);
                    }

                    /* Skip past the include syntax */
                    if (address_end) {
                        const char *override_end = address_end + 1;
                        char delimiter_unused = '\0';
                        if (parse_include_delimiter_override(address_end + 1, &override_end, &delimiter_unused)) {
                            read_pos = override_end;
                        } else {
                            read_pos = address_end + 1;
                        }
                    } else {
                        const char *override_end = filepath_end + 1;
                        char delimiter_unused = '\0';
                        if (parse_include_delimiter_override(filepath_end + 1, &override_end, &delimiter_unused)) {
                            read_pos = override_end;
                        } else {
                            read_pos = filepath_end + 1;
                        }
                    }
                    if (address_spec) free_address_spec(address_spec);
                    if (section_name) free(section_name);
                    processed_include = true;
                }
            }
        }

        /* Not an include, copy character */
        if (!processed_include) {
            if (remaining > 0) {
                *write_pos++ = *read_pos;
                remaining--;
            }
            read_pos++;
        }
    }

    *write_pos = '\0';

    /* Cleanup */
    if (effective_base_dir) free(effective_base_dir);
    if (normalized_text) free(normalized_text);

    return output;
}

