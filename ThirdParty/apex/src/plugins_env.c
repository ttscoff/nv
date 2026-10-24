#include "../include/apex/apex.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

/**
 * Very small helper to JSON-escape a string for inclusion as a value.
 * We only need to support the characters that can reasonably appear
 * in markdown input: backslash, quote, and control newlines.
 */
char *apex_json_escape(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    /* Worst case every char becomes \uXXXX or escape; be generous */
    size_t cap = len * 6 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    char *w = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
            case '\\': *w++ = '\\'; *w++ = '\\'; break;
            case '"':  *w++ = '\\'; *w++ = '"';  break;
            case '\n': *w++ = '\\'; *w++ = 'n';  break;
            case '\r': *w++ = '\\'; *w++ = 'r';  break;
            case '\t': *w++ = '\\'; *w++ = 't';  break;
            default:
                if (c < 0x20) {
                    /* Control character – encode as \u00XX */
                    int written = snprintf(w, cap - (size_t)(w - out), "\\u%04X", c);
                    if (written <= 0 || (size_t)written >= cap - (size_t)(w - out)) {
                        free(out);
                        return NULL;
                    }
                    w += written;
                } else {
                    *w++ = (char)c;
                }
        }
    }
    *w = '\0';
    return out;
}

/**
 * Run a single external plugin command for a text-based phase.
 * Protocol:
 *  - Host sends JSON on stdin with fields: version, plugin_id, phase, text.
 *  - Plugin writes transformed text to stdout (no JSON response parsing).
 */
char *apex_run_external_plugin_command(const char *cmd,
                                       const char *phase,
                                       const char *plugin_id,
                                       const char *text,
                                       int timeout_ms) {
    (void)timeout_ms; /* Reserved for future timeout handling */
    if (!cmd || !*cmd || !text || !phase || !plugin_id) return NULL;

    /* Build JSON request */
    char *escaped = apex_json_escape(text);
    if (!escaped) return NULL;

    const char *prefix = "{ \"version\": 1, \"plugin_id\": \"";
    const char *mid1   = "\", \"phase\": \"";
    const char *mid2   = "\", \"text\": \"";
    const char *suffix = "\" }\n";
    size_t json_len = strlen(prefix) + strlen(plugin_id) +
                      strlen(mid1) + strlen(phase) +
                      strlen(mid2) + strlen(escaped) + strlen(suffix);
    char *json = malloc(json_len + 1);
    if (!json) {
        free(escaped);
        return NULL;
    }
    snprintf(json, json_len + 1, "%s%s%s%s%s%s%s",
             prefix, plugin_id, mid1, phase, mid2, escaped, suffix);
    free(escaped);

    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) == -1 || pipe(out_pipe) == -1) {
        free(json);
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        free(json);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child: stdin from in_pipe[0], stdout to out_pipe[1] */
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        /* If exec fails */
        _exit(127);
    }

    /* Parent */
    close(in_pipe[0]);
    close(out_pipe[1]);

    /* Write JSON to child stdin */
    ssize_t to_write = (ssize_t)json_len;
    const char *p = json;
    while (to_write > 0) {
        ssize_t written = write(in_pipe[1], p, (size_t)to_write);
        if (written <= 0) break;
        p += written;
        to_write -= written;
    }
    close(in_pipe[1]);
    free(json);

    /* Read all of child's stdout */
    size_t cap = 8192;
    size_t size = 0;
    char *buf = malloc(cap);
    if (!buf) {
        close(out_pipe[0]);
        /* Reap child */
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

    /* Reap child; ignore status for now but ensure no zombies */
    int status;
    waitpid(pid, &status, 0);

    buf[size] = '\0';
    return buf;
}

/**
 * Backwards-compatible helper: use APEX_PRE_PARSE_PLUGIN env var as a single
 * pre-parse plugin. This is effectively a thin wrapper around the generic
 * external command runner.
 */
char *apex_run_preparse_plugin_env(const char *text, const apex_options *options) {
    (void)options; /* reserved for future routing decisions */
    const char *cmd = getenv("APEX_PRE_PARSE_PLUGIN");
    if (!cmd || !*cmd || !text) {
        return NULL;
    }
    return apex_run_external_plugin_command(cmd, "pre_parse", "env-pre-parse", text, 0);
}

