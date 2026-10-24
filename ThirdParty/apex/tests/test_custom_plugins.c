//
// Created by Sbarex on 10/02/26.
//

#include "test_helpers.h"
#include "apex/apex.h"
#include <stdlib.h>


/* cmark-gfm headers */
#include "cmark-gfm.h"
#include <string.h>

static char * my_plugin_callback(const char *text, __attribute__((unused)) const char *id_plugin, apex_plugin_phase_mask phase_mask, __attribute__((unused)) const apex_options *options) {
    if (phase_mask & APEX_PLUGIN_PHASE_PRE_PARSE) {
        const char *suffix = "\n_Hello Sbarex_\n";
        size_t len1 = strlen(text);
        size_t len2 = strlen(suffix);

        char *result = malloc(len1 + len2 + 1);
        if (!result) return NULL;

        memcpy(result, text, len1);
        memcpy(result + len1, suffix, len2 + 1); // include '\0'
        return result;
    } else if (phase_mask & APEX_PLUGIN_PHASE_POST_RENDER) {
        const char *suffix = "\n<p>Everything is fine</p>";
        size_t len1 = strlen(text);
        size_t len2 = strlen(suffix);

        char *result = malloc(len1 + len2 + 1);
        if (!result) return NULL;

        memcpy(result, text, len1);
        memcpy(result + len1, suffix, len2 + 1);
        return result;
    }
    return NULL;
}

static void my_plugin_register(apex_plugin_manager *manager, __attribute__((unused)) const apex_options *options) {
    printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin register callback called\n");

    /* Attach to appropriate phase lists, enforcing per-list id uniqueness */
    if (apex_plugin_register(manager, "my_plugin", APEX_PLUGIN_PHASE_PRE_PARSE, my_plugin_callback)) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin has been registered for pre parse\n");
    } else {
        printf(COLOR_RED "✗" COLOR_RESET " Custom plugin has not been registered for pre parse\n");
    }

    /* Attach to appropriate phase lists, enforcing per-list id uniqueness */
    if (apex_plugin_register(manager, "my_plugin", APEX_PLUGIN_PHASE_POST_RENDER, my_plugin_callback)) {
        printf(COLOR_GREEN "✓" COLOR_RESET " Custom plugin has been registered for post render\n");
    } else {
        printf(COLOR_RED "✗" COLOR_RESET " Custom plugin has not been registered for post render\n");
    }
}

void test_custom_plugins(void) {
    int suite_failures = suite_start();
    print_suite_title("Custom plugins Tests", false, true);

    apex_options opts = apex_options_default();
    opts.enable_plugins = true;
    opts.allow_external_plugin_detection = false;
    opts.plugin_register = my_plugin_register;

    const char *s = "# Custom plugin callback test\n\n";

    char *html;
    html = apex_markdown_to_html(s, strlen(s), &opts);
    assert_contains(html, "<em>Hello Sbarex</em>", "Custom pre parse plugin executed");
    assert_contains(html, "<p>Everything is fine</p>", "Custom post render plugin executed");
    apex_free_string(html);

    bool had_failures = suite_end(suite_failures);
    print_suite_title("Cmark Callbacks Tests", had_failures, false);
}
