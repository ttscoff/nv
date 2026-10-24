/**
 * Plugin System Integration Tests
 *
 * Purely test-side coverage improvements:
 * - Creates temporary plugin manifests/scripts under a temporary XDG_CONFIG_HOME
 * - Enables plugins via apex_options and exercises pre_parse and post_render phases
 * - Exercises both declarative regex plugins and external handler plugins
 * - Exercises APEX_PRE_PARSE_PLUGIN env helper (plugins_env.c)
 */

#include "test_helpers.h"
#include "apex/apex.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Not in a public header, but we want to cover it. */
char *apex_run_preparse_plugin_env(const char *text, const apex_options *options);

static int mkdir_p(const char *path) {
    if (!path || !*path) return -1;
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

static int write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t n = fwrite(content, 1, strlen(content), fp);
    fclose(fp);
    return (n == strlen(content)) ? 0 : -1;
}

static void env_set_restore(const char *key, const char *value, void (*fn)(void *), void *ctx) {
    const char *old = getenv(key);
    char *old_dup = old ? strdup(old) : NULL;

    if (value) setenv(key, value, 1);
    else unsetenv(key);

    fn(ctx);

    if (old_dup) {
        setenv(key, old_dup, 1);
        free(old_dup);
    } else {
        unsetenv(key);
    }
}

typedef struct {
    const char *xdg_home;
} plugin_test_ctx;

static void plugin_test_cb(void *vctx) {
    plugin_test_ctx *ctx = (plugin_test_ctx *)vctx;

    /* Create:
     *  $XDG_CONFIG_HOME/apex/plugins/a-regex/plugin.yml
     *  $XDG_CONFIG_HOME/apex/plugins/b-handler/plugin.yml + handler.py
     *  $XDG_CONFIG_HOME/apex/plugins/c-post/plugin.yml + handler.py
     */
    char plugins_root[1024];
    snprintf(plugins_root, sizeof(plugins_root), "%s/apex/plugins", ctx->xdg_home);
    mkdir_p(plugins_root);

    /* a-regex: pre_parse regex replacement, high priority (runs first) */
    {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/a-regex", plugins_root);
        mkdir_p(dir);

        char manifest[1024];
        snprintf(manifest, sizeof(manifest), "%s/plugin.yml", dir);
        const char *yml =
            "---\n"
            "id: a-regex\n"
            "phase: pre_parse\n"
            "priority: 10\n"
            "pattern: \"FOO\"\n"
            "replacement: \"BAR\"\n"
            "---\n";
        write_file(manifest, yml);
    }

    /* b-handler: pre_parse external handler, runs after regex */
    {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/b-handler", plugins_root);
        mkdir_p(dir);

        char script[1024];
        snprintf(script, sizeof(script), "%s/handler.py", dir);
        const char *py =
            "import json, os, sys\n"
            "req = json.loads(sys.stdin.read() or '{}')\n"
            "text = req.get('text','')\n"
            "# Confirm env vars are set by embedding them into output.\n"
            "plugdir = os.environ.get('APEX_PLUGIN_DIR','')\n"
            "suppdir = os.environ.get('APEX_SUPPORT_DIR','')\n"
            "text = text.replace('BAR', 'BAZ')\n"
            "sys.stdout.write(text + '\\n<!--PLUGIN_DIR=' + plugdir + '-->' + '\\n<!--SUPPORT_DIR=' + suppdir + '-->\\n')\n";
        write_file(script, py);

        char manifest[1024];
        snprintf(manifest, sizeof(manifest), "%s/plugin.yml", dir);
        const char *yml =
            "---\n"
            "id: b-handler\n"
            "phase: pre_parse\n"
            "priority: 20\n"
            "handler.command: \"/usr/bin/env python3 ${APEX_PLUGIN_DIR}/handler.py\"\n"
            "timeout_ms: 500\n"
            "---\n";
        write_file(manifest, yml);
    }

    /* c-post: post_render external handler that appends marker with APEX_FILE_PATH */
    {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/c-post", plugins_root);
        mkdir_p(dir);

        char script[1024];
        snprintf(script, sizeof(script), "%s/handler.py", dir);
        const char *py =
            "import json, os, sys\n"
            "req = json.loads(sys.stdin.read() or '{}')\n"
            "text = req.get('text','')\n"
            "fp = os.environ.get('APEX_FILE_PATH','')\n"
            "sys.stdout.write(text + '\\n<!--POST_RENDER_FILE=' + fp + '-->')\n";
        write_file(script, py);

        char manifest[1024];
        snprintf(manifest, sizeof(manifest), "%s/plugin.yml", dir);
        const char *yml =
            "---\n"
            "id: c-post\n"
            "phase: post_render\n"
            "priority: 10\n"
            "handler.command: \"/usr/bin/env python3 ${APEX_PLUGIN_DIR}/handler.py\"\n"
            "timeout_ms: 500\n"
            "---\n";
        write_file(manifest, yml);
    }

    /* Run a conversion that should exercise:
     * - plugin discovery (XDG_CONFIG_HOME path)
     * - regex plugin application (FOO->BAR)
     * - handler plugin application (BAR->BAZ plus markers)
     * - post_render handler append marker
     */
    apex_options opts = apex_options_for_mode(APEX_MODE_UNIFIED);
    opts.enable_plugins = true;
    opts.input_file_path = "/tmp/apex-test-input.md";

    const char *md = "FOO\n";
    char *html = apex_markdown_to_html(md, strlen(md), &opts);

    assert_contains(html, "BAZ", "plugins: regex + handler transformed text (FOO->BAR->BAZ)");
    assert_contains(html, "PLUGIN_DIR=", "plugins: handler saw APEX_PLUGIN_DIR");
    assert_contains(html, "SUPPORT_DIR=", "plugins: handler saw APEX_SUPPORT_DIR");
    assert_contains(html, "POST_RENDER_FILE=/tmp/apex-test-input.md", "plugins: post_render saw APEX_FILE_PATH");

    apex_free_string(html);
}

void test_plugins_integration(void) {
    int suite_failures = suite_start();
    print_suite_title("Plugin System Integration Tests", false, true);

    /* Exercise plugins_env.c helper using APEX_PRE_PARSE_PLUGIN */
    {
        /* A command that reads JSON and prints just the text value. */
        const char *cmd =
            "/usr/bin/env python3 -c 'import json,sys; print(json.loads(sys.stdin.read()).get(\"text\",\"\"))'";
        setenv("APEX_PRE_PARSE_PLUGIN", cmd, 1);

        apex_options opts = apex_options_default();
        char *out = apex_run_preparse_plugin_env("ENV_PLUGIN_OK", &opts);
        test_result(out && strstr(out, "ENV_PLUGIN_OK") != NULL, "APEX_PRE_PARSE_PLUGIN env helper runs");
        if (out) free(out);

        unsetenv("APEX_PRE_PARSE_PLUGIN");
    }

    /* Create temp XDG_CONFIG_HOME and run full plugin integration. */
    char tmp_template[] = "/tmp/apex-xdg-plugins-XXXXXX";
    char *tmp = mkdtemp(tmp_template);
    if (!tmp) {
        tests_run++;
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " Failed to create temp directory for plugin tests\n");
    } else {
        plugin_test_ctx ctx = { .xdg_home = tmp };
        env_set_restore("XDG_CONFIG_HOME", tmp, plugin_test_cb, &ctx);

        /* Best-effort cleanup (not fatal on failure). */
        char rm_cmd[1200];
        snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf '%s'", tmp);
        system(rm_cmd);
    }

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Plugin System Integration Tests", had_failures, false);
}

