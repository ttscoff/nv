/**
 * Inline Tables Extension for Apex
 * Implementation
 */

#include "inline_tables.h"
#include "includes.h"  /* for apex_csv_to_table */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/* Detect delimiter for a block of lines: TSV if any tab, else CSV if any comma, else none */
static int detect_delimiter(const char *start, const char *end) {
    bool has_tab = false;
    bool has_comma = false;

    for (const char *p = start; p < end; p++) {
        if (*p == '\t') has_tab = true;
        else if (*p == ',') has_comma = true;
        if (has_tab) break;
    }

    if (has_tab) return '\t';
    if (has_comma) return ',';
    return 0;
}

/* Process ```table fenced blocks and <!--TABLE--> markers */
char *apex_process_inline_tables(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    size_t capacity = len * 3 + 1; /* allow for table expansion */
    char *out = malloc(capacity);
    if (!out) return strdup(text);

    const char *read = text;
    char *write = out;

    while (*read) {
        /* ```table fenced block */
        if (read[0] == '`' && read[1] == '`' && read[2] == '`') {
            const char *line_start = read;
            const char *p = read + 3;

            /* Read info string until end of line */
            while (*p && *p != '\n') p++;
            const char *info_start = read + 3;
            const char *info_end = p;
            while (info_start < info_end && isspace((unsigned char)*info_start)) info_start++;
            while (info_end > info_start && isspace((unsigned char)*(info_end - 1))) info_end--;

            bool is_table_fence = false;
            if (info_end > info_start) {
                size_t info_len = (size_t)(info_end - info_start);
                if (info_len == 5 && strncasecmp(info_start, "table", 5) == 0) {
                    is_table_fence = true;
                }
            }

            /* Move past newline after fence line */
            if (*p == '\n') p++;
            const char *content_start = p;

            /* Find closing fence */
            const char *closing = NULL;
            while (*p) {
                if (p[0] == '`' && p[1] == '`' && p[2] == '`') {
                    closing = p;
                    break;
                }
                p++;
            }

            const char *content_end = closing ? closing : text + len;

            if (is_table_fence) {
                int delim = detect_delimiter(content_start, content_end);
                if (delim == 0) {
                    /* No delimiter detected: emit original fence block unchanged */
                    size_t block_len = (closing ? (p + 3 - line_start) : (text + len - line_start));
                    if (write + block_len >= out + capacity) {
                        size_t used = (size_t)(write - out);
                        capacity = (used + block_len + 1) * 2;
                        char *tmp = realloc(out, capacity);
                        if (!tmp) {
                            free(out);
                            return strdup(text);
                        }
                        out = tmp;
                        write = out + used;
                    }
                    memcpy(write, line_start, block_len);
                    write += block_len;
                    read = closing ? (p + 3) : (text + len);
                    continue;
                }

                /* Convert to table */
                bool is_tsv = (delim == '\t');
                /* content_start..content_end may not be null-terminated */
                size_t data_len = (size_t)(content_end - content_start);
                char *data = malloc(data_len + 1);
                if (!data) {
                    free(out);
                    return strdup(text);
                }
                memcpy(data, content_start, data_len);
                data[data_len] = '\0';

                char *table = apex_csv_to_table(data, is_tsv);
                free(data);

                if (!table) {
                    /* Fallback: emit original block */
                    size_t block_len = (closing ? (p + 3 - line_start) : (text + len - line_start));
                    if (write + block_len >= out + capacity) {
                        size_t used = (size_t)(write - out);
                        capacity = (used + block_len + 1) * 2;
                        char *tmp = realloc(out, capacity);
                        if (!tmp) {
                            free(out);
                            return strdup(text);
                        }
                        out = tmp;
                        write = out + used;
                    }
                    memcpy(write, line_start, block_len);
                    write += block_len;
                    read = closing ? (p + 3) : (text + len);
                    continue;
                }

                size_t table_len = strlen(table);
                if (write + table_len >= out + capacity) {
                    size_t used = (size_t)(write - out);
                    capacity = (used + table_len + 1) * 2;
                    char *tmp = realloc(out, capacity);
                    if (!tmp) {
                        free(out);
                        free(table);
                        return strdup(text);
                    }
                    out = tmp;
                    write = out + used;
                }
                memcpy(write, table, table_len);
                write += table_len;
                free(table);

                /* Skip closing fence line */
                if (closing) {
                    p += 3;
                    while (*p && *p != '\n') p++;
                    if (*p == '\n') {
                        *write++ = '\n';
                        p++;
                    }
                    read = p;
                } else {
                    read = text + len;
                }
                continue;
            } else {
                /* Not a table fence: copy as-is up to first char after closing fence */
                const char *block_end = closing ? (p + 3) : (text + len);
                size_t block_len = (size_t)(block_end - line_start);
                if (write + block_len >= out + capacity) {
                    size_t used = (size_t)(write - out);
                    capacity = (used + block_len + 1) * 2;
                    char *tmp = realloc(out, capacity);
                    if (!tmp) {
                        free(out);
                        return strdup(text);
                    }
                    out = tmp;
                    write = out + used;
                }
                memcpy(write, line_start, block_len);
                write += block_len;
                read = block_end;
                continue;
            }
        }

        /* <!--TABLE--> marker */
        if (strncmp(read, "<!--TABLE-->", 12) == 0) {
            const char *marker_start = read;
            const char *p = read + 12;
            /* Skip optional trailing spaces and one newline */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;

            const char *data_start = p;
            const char *data_end = data_start;
            bool seen_nonblank = false;

            while (*data_end) {
                const char *line = data_end;
                const char *line_end = line;
                while (*line_end && *line_end != '\n') line_end++;

                /* Check if line is blank */
                const char *q = line;
                while (q < line_end && (*q == ' ' || *q == '\t' || *q == '\r')) q++;
                bool blank = (q == line_end);

                if (blank) {
                    break;
                }

                seen_nonblank = true;
                data_end = (*line_end == '\n') ? (line_end + 1) : line_end;
            }

            if (!seen_nonblank) {
                /* No data: just copy marker through */
                size_t marker_len = (size_t)(p - marker_start);
                if (write + marker_len >= out + capacity) {
                    size_t used = (size_t)(write - out);
                    capacity = (used + marker_len + 1) * 2;
                    char *tmp = realloc(out, capacity);
                    if (!tmp) {
                        free(out);
                        return strdup(text);
                    }
                    out = tmp;
                    write = out + used;
                }
                memcpy(write, marker_start, marker_len);
                write += marker_len;
                read = p;
                continue;
            }

            int delim = detect_delimiter(data_start, data_end);
            if (delim == 0) {
                /* No delimiter: emit marker and data unchanged */
                size_t block_len = (size_t)(data_end - marker_start);
                if (write + block_len >= out + capacity) {
                    size_t used = (size_t)(write - out);
                    capacity = (used + block_len + 1) * 2;
                    char *tmp = realloc(out, capacity);
                    if (!tmp) {
                        free(out);
                        return strdup(text);
                    }
                    out = tmp;
                    write = out + used;
                }
                memcpy(write, marker_start, block_len);
                write += block_len;
                read = data_end;
                continue;
            }

            bool is_tsv = (delim == '\t');
            size_t data_len = (size_t)(data_end - data_start);
            char *data = malloc(data_len + 1);
            if (!data) {
                free(out);
                return strdup(text);
            }
            memcpy(data, data_start, data_len);
            data[data_len] = '\0';

            char *table = apex_csv_to_table(data, is_tsv);
            free(data);

            if (!table) {
                /* Fallback: copy original */
                size_t block_len = (size_t)(data_end - marker_start);
                if (write + block_len >= out + capacity) {
                    size_t used = (size_t)(write - out);
                    capacity = (used + block_len + 1) * 2;
                    char *tmp = realloc(out, capacity);
                    if (!tmp) {
                        free(out);
                        return strdup(text);
                    }
                    out = tmp;
                    write = out + used;
                }
                memcpy(write, marker_start, block_len);
                write += block_len;
                read = data_end;
                continue;
            }

            size_t table_len = strlen(table);
            if (write + table_len >= out + capacity) {
                size_t used = (size_t)(write - out);
                capacity = (used + table_len + 1) * 2;
                char *tmp = realloc(out, capacity);
                if (!tmp) {
                    free(out);
                    free(table);
                    return strdup(text);
                }
                out = tmp;
                write = out + used;
            }
            memcpy(write, table, table_len);
            write += table_len;
            free(table);

            read = data_end;
            continue;
        }

        /* Default: copy one char */
        *write++ = *read++;
        if ((size_t)(write - out) + 1 >= capacity) {
            size_t used = (size_t)(write - out);
            capacity = (used * 2) + 1;
            char *tmp = realloc(out, capacity);
            if (!tmp) {
                free(out);
                return strdup(text);
            }
            out = tmp;
            write = out + used;
        }
    }

    *write = '\0';
    return out;
}


