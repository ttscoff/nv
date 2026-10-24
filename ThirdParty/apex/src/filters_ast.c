#include "filters_ast.h"
#include "ast_json.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

/* Helper: run a single external filter command as a JSON AST transformer.
 *
 * Protocol:
 *   - Apex sends the Pandoc JSON AST on stdin (no wrapper object).
 *   - Filter writes a Pandoc JSON AST on stdout.
 *   - Target format is exposed via the APEX_TARGET_FORMAT env var.
 *
 * On success, returns a newly allocated JSON string; caller must free().
 * On failure, returns NULL.
 */
static char *run_single_ast_filter(const char *cmd,
                                   const char *target_format,
                                   const char *json_input) {
    if (!cmd || !*cmd || !json_input) return NULL;

    /* Ensure target format env var is visible to the child process. */
    if (target_format && *target_format) {
        setenv("APEX_TARGET_FORMAT", target_format, 1);
    }

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
        /* Child */
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(in_pipe[0]);
    close(out_pipe[1]);

    /* Write JSON to child stdin */
    size_t json_len = strlen(json_input);
    const char *p = json_input;
    ssize_t remaining = (ssize_t)json_len;
    while (remaining > 0) {
        ssize_t written = write(in_pipe[1], p, (size_t)remaining);
        if (written <= 0) break;
        p += written;
        remaining -= written;
    }
    close(in_pipe[1]);

    /* Read child's stdout into a growable buffer */
    size_t cap = 8192;
    size_t size = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        close(out_pipe[0]);
        int status;
        waitpid(pid, &status, 0);
        return NULL;
    }

    for (;;) {
        if (size + 4096 > cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
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

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        free(buf);
        return NULL;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}

cmark_node *apex_run_ast_filters(cmark_node *document,
                                 const apex_options *options,
                                 const char *target_format) {
    if (!document || !options) return document;
    if (!options->ast_filter_commands || options->ast_filter_count == 0) {
        return document;
    }

    /* Serialize once per filter step. We keep 'current_doc' as the latest
     * successfully transformed tree reference.
     */
    cmark_node *current_doc = document;

    for (size_t i = 0; i < options->ast_filter_count; i++) {
        const char *cmd = options->ast_filter_commands[i];
        if (!cmd || !*cmd) {
            continue;
        }

        /* Serialize current_doc -> JSON */
        char *json_in = apex_cmark_to_pandoc_json(current_doc, options);
        if (!json_in) {
            if (options->ast_filter_strict) {
                return NULL;
            }
            continue;
        }

        /* Run external filter */
        char *json_out = run_single_ast_filter(cmd, target_format, json_in);
        free(json_in);

        if (!json_out) {
            if (options->ast_filter_strict) {
                return NULL;
            }
            continue;
        }

        /* Parse JSON back into cmark */
        cmark_node *new_doc = apex_pandoc_json_to_cmark(json_out, options);
        free(json_out);

        if (!new_doc) {
            if (options->ast_filter_strict) {
                return NULL;
            }
            continue;
        }

        /* If the filter returned a document with no blocks, keep the original
         * to avoid blank output (e.g. parser dropped blocks it didn't recognise).
         */
        if (!cmark_node_first_child(new_doc) && cmark_node_first_child(current_doc)) {
            cmark_node_free(new_doc);
            continue;
        }

        /* Replace current document with transformed one. We leave freeing
         * of the original 'document' to the caller; we only free intermediate
         * transformed trees that we owned.
         */
        if (current_doc != document) {
            cmark_node_free(current_doc);
        }
        current_doc = new_doc;
    }

    return current_doc;
}

