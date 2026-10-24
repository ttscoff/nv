/**
 * Special Markers Extension for Apex
 * Implementation
 */

#include "special_markers.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static int looks_like_list_marker(const char *p) {
    if (!*p) return 0;
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return 0;
}

/** True if we're at the start of a line that is an indented code block (4+ spaces or tab). */
static int line_is_indented_code_block(const char *read) {
    if (!*read) return 0;
    if (*read == '\t')
        return !looks_like_list_marker(read + 1);
    if (read[0] != ' ' || read[1] != ' ' || read[2] != ' ' || read[3] != ' ')
        return 0;
    const char *content = read + 4;
    while (*content == ' ') content++;
    return *content && !looks_like_list_marker(content);
}

/**
 * Process special markers in text
 */
char *apex_process_special_markers(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Page break divs are ~64 bytes each, so need generous capacity */
    size_t capacity = len * 4;  /* Room for expansion */
    char *output = malloc(capacity);
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_indented_code_block = false;

    while (*read) {
        /* At line start: indented code block only if 4+ spaces/tab and not a list line */
        if (read == text || read[-1] == '\n') {
            in_indented_code_block = line_is_indented_code_block(read);
        }

        /* Track fenced code blocks (```) and inline code (`) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Skip special marker processing inside any code context */
        if (in_code_block || in_inline_code || in_indented_code_block) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        /* Check for End of Block marker (Kramdown) */
        /* Pattern: ^ on a line by itself (with optional leading whitespace) */
        if (*read == '^') {
            /* Check if it's on its own line */
            const char *before = read - 1;
            bool line_start = (read == text);

            /* Skip back over whitespace to check for line start */
            while (!line_start && before >= text && (*before == ' ' || *before == '\t')) {
                before--;
            }
            if (!line_start && before >= text && *before == '\n') {
                line_start = true;
            }

            /* Check what comes after */
            const char *after = read + 1;
            bool line_end = (*after == '\n' || *after == '\0');
            while (!line_end && (*after == ' ' || *after == '\t')) {
                after++;
            }
            if (!line_end && (*after == '\n' || *after == '\0')) {
                line_end = true;
            }

            if (line_start && line_end) {
                /* This is an end-of-block marker */
                /* Replace with a paragraph containing zero-width space (U+200B) to force block separation */
                /* This ensures lists are not merged by the parser, and the paragraph won't render visibly */
                const char *replacement = "\n\n\u200B\n\n";
                size_t repl_len = strlen(replacement);
                if (repl_len < remaining) {
                    memcpy(write, replacement, repl_len);
                    write += repl_len;
                    remaining -= repl_len;
                }
                /* Skip to after the ^ and any trailing whitespace/newline */
                read = after;
                if (*read == '\n') read++;
                continue;
            }
        }

        /* Check for <!--BREAK--> */
        if (strncmp(read, "<!--BREAK-->", 12) == 0) {
            const char *replacement =
                "\n\n<div class=\"mkpagebreak manualbreak\" "
                "title=\"Page break created by marker\" "
                "data-description=\"PAGE (Marker)\" "
                "style=\"page-break-after:always\">"
                "<span style=\"display:none\">&nbsp;</span></div>\n\n";
            size_t repl_len = strlen(replacement);
            if (repl_len >= remaining) {
                /* Expand buffer */
                size_t written = (size_t)(write - output);
                capacity = (written + repl_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return strdup(text);
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, replacement, repl_len);
            write += repl_len;
            remaining -= repl_len;
            read += 12;
            continue;
        }

        /* Check for <!--PAUSE:X--> */
        if (strncmp(read, "<!--PAUSE:", 10) == 0) {
            const char *num_start = read + 10;
            const char *num_end = num_start;
            while (isdigit((unsigned char)*num_end)) num_end++;

            if (*num_end == '-' && num_end[1] == '-' && num_end[2] == '>') {
                /* Valid PAUSE marker */
                int seconds = atoi(num_start);
                char replacement[256];
                snprintf(replacement, sizeof(replacement),
                        "<div class=\"autoscroll-pause\" data-pause=\"%d\"></div>",
                        seconds);

                size_t repl_len = strlen(replacement);
                if (repl_len < remaining) {
                    memcpy(write, replacement, repl_len);
                    write += repl_len;
                    remaining -= repl_len;
                }
                read = num_end + 3;
                continue;
            }
        }

        /* Check for {::pagebreak /} (Leanpub style) */
        if (strncmp(read, "{::pagebreak /}", 15) == 0) {
            const char *replacement =
                "\n\n<div class=\"mkpagebreak manualbreak\" "
                "title=\"Page break created by marker\" "
                "data-description=\"PAGE (Marker)\" "
                "style=\"page-break-after:always\">"
                "<span style=\"display:none\">&nbsp;</span></div>\n\n";
            size_t repl_len = strlen(replacement);
            if (repl_len >= remaining) {
                /* Expand buffer */
                size_t written = (size_t)(write - output);
                capacity = (written + repl_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return strdup(text);
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, replacement, repl_len);
            write += repl_len;
            remaining -= repl_len;
            read += 15;
            continue;
        }

        /* Check for {index} (Leanpub index placement marker).
         * Replaced with <!--INDEX--> so the index extension inserts the index there.
         * The marker is always removed from the document (either replaced by the
         * index block or left as an invisible HTML comment when index is suppressed). */
        if (strncmp(read, "{index}", 8) == 0) {
            const char *replacement = "<!--INDEX-->";
            size_t repl_len = strlen(replacement);
            if (repl_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + repl_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return strdup(text);
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, replacement, repl_len);
            write += repl_len;
            remaining -= repl_len;
            read += 8;
            continue;
        }

        /* Not a special marker, copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}

