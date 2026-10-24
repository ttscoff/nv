/**
 * @file syntax_highlight.c
 * @brief External syntax highlighting support for code blocks
 *
 * Implements integration with external syntax highlighting tools
 * (Pygments, Skylighting) to produce colorized HTML output.
 */

#include "syntax_highlight.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/**
 * Get the binary name for a syntax highlighting tool.
 */
static const char *get_tool_binary(const char *tool) {
    if (strcmp(tool, "pygments") == 0) {
        return "pygmentize";
    } else if (strcmp(tool, "skylighting") == 0) {
        return "skylighting";
    } else if (strcmp(tool, "shiki") == 0) {
        return "shiki";
    }
    return NULL;
}

/**
 * Check if a syntax highlighting tool is available in PATH.
 */
bool apex_syntax_highlighter_available(const char *tool) {
    const char *binary = get_tool_binary(tool);
    if (!binary) return false;

    /* Use 'which' to check if the binary exists */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", binary);
    return system(cmd) == 0;
}

/**
 * Unescape HTML entities in code content.
 * Converts &lt; &gt; &amp; &quot; back to their original characters.
 */
static char *unescape_html(const char *html, size_t len) {
    char *result = malloc(len + 1);
    if (!result) return NULL;

    const char *read = html;
    const char *end = html + len;
    char *write = result;

    while (read < end) {
        if (*read == '&') {
            if (strncmp(read, "&lt;", 4) == 0) {
                *write++ = '<';
                read += 4;
            } else if (strncmp(read, "&gt;", 4) == 0) {
                *write++ = '>';
                read += 4;
            } else if (strncmp(read, "&amp;", 5) == 0) {
                *write++ = '&';
                read += 5;
            } else if (strncmp(read, "&quot;", 6) == 0) {
                *write++ = '"';
                read += 6;
            } else if (strncmp(read, "&#39;", 5) == 0 || strncmp(read, "&apos;", 6) == 0) {
                *write++ = '\'';
                read += (read[2] == '3') ? 5 : 6;
            } else {
                *write++ = *read++;
            }
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
    return result;
}

/**
 * Run an external command with input on stdin and capture stdout.
 * Returns newly allocated string with output, or NULL on failure.
 */
static char *run_command(const char *cmd, const char *input) {
    if (!cmd || !input) return NULL;

    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) == -1 || pipe(out_pipe) == -1) {
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child: stdin from in_pipe[0], stdout to out_pipe[1] */
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        /* Redirect stderr to /dev/null to suppress tool warnings */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(in_pipe[0]);
    close(out_pipe[1]);

    /* Write input to child stdin */
    size_t input_len = strlen(input);
    ssize_t to_write = (ssize_t)input_len;
    const char *p = input;
    while (to_write > 0) {
        ssize_t written = write(in_pipe[1], p, (size_t)to_write);
        if (written <= 0) break;
        p += written;
        to_write -= written;
    }
    close(in_pipe[1]);

    /* Read all of child's stdout */
    size_t cap = 8192;
    size_t size = 0;
    char *buf = malloc(cap);
    if (!buf) {
        close(out_pipe[0]);
        int status;
        waitpid(pid, &status, 0);
        return NULL;
    }

    for (;;) {
        if (size + 4096 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                close(out_pipe[0]);
                int status;
                waitpid(pid, &status, 0);
                return NULL;
            }
            buf = nb;
        }
        ssize_t n = read(out_pipe[0], buf + size, 4096);
        if (n < 0) {
            if (errno == EINTR) continue;
            free(buf);
            close(out_pipe[0]);
            int status;
            waitpid(pid, &status, 0);
            return NULL;
        }
        if (n == 0) break;
        size += (size_t)n;
    }
    close(out_pipe[0]);

    /* Reap child */
    int status;
    waitpid(pid, &status, 0);

    /* Check if command succeeded */
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

/**
 * Highlight a single code block using the specified tool.
 * Returns newly allocated HTML (or ANSI when ansi_output is true), or NULL on failure.
 */
static char *highlight_code_block(const char *code, const char *language,
                                  const char *tool, bool line_numbers, bool ansi_output,
                                  const char *theme) {
    char cmd[512];
    const char *binary = get_tool_binary(tool);
    if (!binary) return NULL;

    if (strcmp(tool, "pygments") == 0) {
        /* Pygments: pygmentize -l LANG -f html [-O linenos=1] */
        if (language && *language) {
            if (line_numbers && theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s -l %s -f html -O linenos=1,style=%s", binary, language, theme);
            } else if (line_numbers) {
                snprintf(cmd, sizeof(cmd), "%s -l %s -f html -O linenos=1", binary, language);
            } else if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s -l %s -f html -O style=%s", binary, language, theme);
            } else {
                snprintf(cmd, sizeof(cmd), "%s -l %s -f html", binary, language);
            }
        } else {
            /* Use -g for auto-detection when no language specified */
            if (line_numbers && theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s -g -f html -O linenos=1,style=%s", binary, theme);
            } else if (line_numbers) {
                snprintf(cmd, sizeof(cmd), "%s -g -f html -O linenos=1", binary);
            } else if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s -g -f html -O style=%s", binary, theme);
            } else {
                snprintf(cmd, sizeof(cmd), "%s -g -f html", binary);
            }
        }
    } else if (strcmp(tool, "skylighting") == 0) {
        /* Skylighting: skylighting --syntax LANG -f html -r [-n]
         * -r = fragment mode (no full HTML document wrapper) */
        if (language && *language) {
            if (line_numbers && theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --syntax %s --style %s -f html -r -n",
                         binary, language, theme);
            } else if (line_numbers) {
                snprintf(cmd, sizeof(cmd), "%s --syntax %s -f html -r -n",
                         binary, language);
            } else if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --syntax %s --style %s -f html -r",
                         binary, language, theme);
            } else {
                snprintf(cmd, sizeof(cmd), "%s --syntax %s -f html -r",
                         binary, language);
            }
        } else {
            /* Skylighting without syntax tries to auto-detect, but may fail */
            if (line_numbers && theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --style %s -f html -r -n",
                         binary, theme);
            } else if (line_numbers) {
                snprintf(cmd, sizeof(cmd), "%s -f html -r -n", binary);
            } else if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --style %s -f html -r",
                         binary, theme);
            } else {
                snprintf(cmd, sizeof(cmd), "%s -f html -r", binary);
            }
        }
    } else if (strcmp(tool, "shiki") == 0) {
        /* Shiki CLI: shiki [--lang LANG] --format html|ansi
         * Reads code from stdin. Exits non-zero if lang is missing and cannot be auto-detected;
         * we capture that and fall back to plain text. */
        const char *fmt = ansi_output ? "ansi" : "html";
        if (language && *language) {
            if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --lang %s --theme %s --format %s", binary, language, theme, fmt);
            } else {
                snprintf(cmd, sizeof(cmd), "%s --lang %s --format %s", binary, language, fmt);
            }
        } else {
            /* No language: Shiki may fail (non-zero exit); run_command returns NULL → we use original block */
            if (theme && *theme) {
                snprintf(cmd, sizeof(cmd), "%s --theme %s --format %s", binary, theme, fmt);
            } else {
                snprintf(cmd, sizeof(cmd), "%s --format %s", binary, fmt);
            }
        }
    } else {
        return NULL;
    }

    return run_command(cmd, code);
}

/**
 * Apply syntax highlighting to code blocks in HTML.
 */
char *apex_apply_syntax_highlighting(const char *html, const char *tool, bool line_numbers,
                                     bool language_only, bool ansi_output, const char *theme) {
    if (!html || !tool) return html ? strdup(html) : NULL;

    /* Check if tool is available */
    if (!apex_syntax_highlighter_available(tool)) {
        const char *binary = get_tool_binary(tool);
        /* Suppress warning if APEX_SUPPRESS_HIGHLIGHT_WARNINGS is set (e.g., during tests) */
        if (!getenv("APEX_SUPPRESS_HIGHLIGHT_WARNINGS")) {
            fprintf(stderr, "Warning: Syntax highlighting tool '%s' not found in PATH. "
                    "Code blocks will not be highlighted.\n", binary ? binary : tool);
        }
        return strdup(html);
    }

    size_t html_len = strlen(html);
    /* Allocate generous buffer for output (highlighted code can be larger) */
    size_t cap = html_len * 3 + 1024;
    char *output = malloc(cap);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = cap;

    while (*read) {
        /* Look for <pre pattern (handles both <pre><code and <pre lang="XXX"><code) */
        if (strncmp(read, "<pre", 4) == 0 && (read[4] == '>' || read[4] == ' ')) {
            const char *pre_start = read;

            /* Find end of <pre ...> tag */
            const char *pre_tag_end = strchr(read, '>');
            if (!pre_tag_end) {
                /* Malformed, copy as-is */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Check if <code follows */
            const char *after_pre = pre_tag_end + 1;
            /* Skip whitespace/newlines between <pre> and <code> */
            while (*after_pre && (*after_pre == ' ' || *after_pre == '\t' || *after_pre == '\n' || *after_pre == '\r')) {
                after_pre++;
            }
            if (strncmp(after_pre, "<code", 5) != 0) {
                /* Not a code block, copy <pre and continue */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            const char *code_tag = after_pre;
            const char *code_tag_end = strchr(code_tag, '>');
            if (!code_tag_end) {
                /* Malformed, copy as-is */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Extract language - check both formats:
             * 1. <pre lang="XXX"><code> (cmark-gfm format)
             * 2. <pre><code class="language-XXX"> (standard format)
             */
            char language[64] = {0};

            /* First check for lang= attribute on <pre> tag */
            const char *lang_attr = strstr(pre_start, "lang=\"");
            if (lang_attr && lang_attr < pre_tag_end) {
                const char *lang_start = lang_attr + 6;
                const char *lang_end = strchr(lang_start, '"');
                if (lang_end && lang_end < pre_tag_end) {
                    size_t lang_len = lang_end - lang_start;
                    if (lang_len < sizeof(language)) {
                        memcpy(language, lang_start, lang_len);
                        language[lang_len] = '\0';
                    }
                }
            }

            /* If no lang= on pre, check for class="language-XXX" on code tag */
            if (!language[0]) {
                const char *class_attr = strstr(code_tag, "class=\"");
                if (class_attr && class_attr < code_tag_end) {
                    const char *class_start = class_attr + 7;
                    const char *lang_prefix = strstr(class_start, "language-");
                    if (lang_prefix && lang_prefix < code_tag_end) {
                        const char *lang_start = lang_prefix + 9;
                        const char *lang_end = lang_start;
                        while (lang_end < code_tag_end && *lang_end != '"' && *lang_end != ' ') {
                            lang_end++;
                        }
                        size_t lang_len = lang_end - lang_start;
                        if (lang_len < sizeof(language)) {
                            memcpy(language, lang_start, lang_len);
                            language[lang_len] = '\0';
                        }
                    }
                }
            }

            /* If language_only is set and no language was found, skip this block */
            if (language_only && !language[0]) {
                /* Copy the original block as-is */
                const char *block_end = strstr(code_tag_end + 1, "</code></pre>");
                if (block_end) {
                    size_t block_len = (block_end + 13) - pre_start;
                    if (block_len <= remaining) {
                        memcpy(write, pre_start, block_len);
                        write += block_len;
                        remaining -= block_len;
                    }
                    read = block_end + 13;
                    continue;
                }
            }

            /* Find </code></pre> */
            const char *code_content_start = code_tag_end + 1;
            const char *code_end = strstr(code_content_start, "</code></pre>");
            if (!code_end) {
                /* Malformed, copy as-is */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Extract and unescape code content */
            size_t code_len = code_end - code_content_start;
            char *code = unescape_html(code_content_start, code_len);
            if (!code) {
                /* Failed to unescape, copy as-is */
                size_t block_len = (code_end + 13) - pre_start;
                if (block_len <= remaining) {
                    memcpy(write, pre_start, block_len);
                    write += block_len;
                    remaining -= block_len;
                }
                read = code_end + 13;
                continue;
            }

            /* Per-block line numbers from Quarto/Pandoc fence attr markers */
            bool block_line_numbers = line_numbers;
            if (!block_line_numbers && pre_start > html) {
                size_t lookback = (size_t)(pre_start - html);
                if (lookback > 512) {
                    lookback = 512;
                }
                const char *region_start = pre_start - lookback;
                char region_buf[513];
                memcpy(region_buf, region_start, lookback);
                region_buf[lookback] = '\0';
                if (strstr(region_buf, "data-linenos=\"true\"")) {
                    block_line_numbers = true;
                }
            }
            if (!block_line_numbers) {
                const char *linenos_attr = strstr(pre_start, "data-linenos=\"true\"");
                if (linenos_attr && linenos_attr < pre_tag_end) {
                    block_line_numbers = true;
                }
            }

            /* Run syntax highlighter */
            char *highlighted = highlight_code_block(code, language, tool, block_line_numbers, ansi_output, theme);
            free(code);

            if (highlighted && *highlighted) {
                /* Use highlighted output */
                size_t hl_len = strlen(highlighted);

                /* Ensure we have enough space */
                if (hl_len >= remaining) {
                    size_t written = write - output;
                    size_t new_cap = (written + hl_len + 1) * 2;
                    char *new_output = realloc(output, new_cap);
                    if (!new_output) {
                        free(highlighted);
                        free(output);
                        return strdup(html);
                    }
                    output = new_output;
                    write = output + written;
                    remaining = new_cap - written;
                    cap = new_cap;
                }

                memcpy(write, highlighted, hl_len);
                write += hl_len;
                remaining -= hl_len;
                free(highlighted);
            } else {
                /* Highlighting failed, copy original block */
                size_t block_len = (code_end + 13) - pre_start;
                if (block_len <= remaining) {
                    memcpy(write, pre_start, block_len);
                    write += block_len;
                    remaining -= block_len;
                }
                if (highlighted) free(highlighted);
            }

            read = code_end + 13; /* Skip past </code></pre> */
            continue;
        }

        /* Copy character */
        if (remaining > 1) {
            *write++ = *read++;
            remaining--;
        } else {
            /* Need more space */
            size_t written = write - output;
            size_t new_cap = cap * 2;
            char *new_output = realloc(output, new_cap);
            if (!new_output) {
                free(output);
                return strdup(html);
            }
            output = new_output;
            write = output + written;
            remaining = new_cap - written;
            cap = new_cap;
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';
    return output;
}
