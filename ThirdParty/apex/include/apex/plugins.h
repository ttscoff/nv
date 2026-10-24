/**
 * Apex plugin catalog and installation API.
 *
 * Fetch available plugins from the central directory, list installed plugins,
 * and install or uninstall plugins into the user-global plugins directory.
 */

#ifndef APEX_PLUGINS_PUBLIC_H
#define APEX_PLUGINS_PUBLIC_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default URL for apex-plugins.json (ApexMarkdown/apex-plugins). */
#define APEX_PLUGIN_DIRECTORY_URL \
    "https://raw.githubusercontent.com/ApexMarkdown/apex-plugins/refs/heads/main/apex-plugins.json"

/** One plugin entry (catalog or installed). String fields are owned by the catalog. */
typedef struct apex_plugin_info {
    char *id;
    char *title;
    char *description;
    char *author;
    char *homepage;
    char *repo;
} apex_plugin_info;

/** Array of plugin entries. */
typedef struct apex_plugin_catalog {
    apex_plugin_info *items;
    size_t count;
} apex_plugin_catalog;

/** Options for discovering installed plugins (same precedence as runtime loading). */
typedef struct apex_plugin_discovery_options {
    /** Optional document/project base directory for .apex/plugins lookup. */
    const char *base_directory;
    /** Include project-scoped dirs (cwd, base_directory, git root). Default true when NULL opts. */
    bool include_project;
    /** Include user-global ~/.config/apex/plugins. Default true when NULL opts. */
    bool include_user_global;
} apex_plugin_discovery_options;

/** Options for apex_plugin_install(). */
typedef struct apex_plugin_install_options {
    /** Allow install by Git URL or user/repo shorthand (not in the directory). */
    bool allow_untrusted_repo;
    /** Run post_install from plugin.yml after clone. Default true when NULL opts. */
    bool run_post_install;
} apex_plugin_install_options;

/**
 * Fetch the plugin catalog from a JSON directory URL.
 * @return New catalog (caller must free with apex_plugin_catalog_free), or NULL on failure.
 */
apex_plugin_catalog *apex_plugin_catalog_fetch_url(const char *url);

/** Fetch the catalog from APEX_PLUGIN_DIRECTORY_URL. */
apex_plugin_catalog *apex_plugin_catalog_fetch_default(void);

/** Free a catalog returned by fetch or list functions. */
void apex_plugin_catalog_free(apex_plugin_catalog *catalog);

/**
 * List installed plugins using the same discovery order as apex_plugins_load().
 * @return New catalog, or NULL on failure.
 */
apex_plugin_catalog *apex_plugins_list_installed(
    const apex_plugin_discovery_options *options);

/**
 * Resolve the user-global plugins directory (~/.config/apex/plugins or XDG).
 * @return 0 on success, -1 if buffer too small or HOME unset.
 */
int apex_plugin_global_directory(char *buf, size_t buflen);

/**
 * Install a plugin by directory id or Git URL / GitHub shorthand.
 * Installs into the user-global plugins directory.
 * @param error_buf Optional buffer for an error message (may be NULL).
 * @return 0 on success, -1 on failure.
 */
int apex_plugin_install(const char *id_or_repo,
                        const apex_plugin_install_options *options,
                        char *error_buf,
                        size_t error_buf_len);

/**
 * Remove an installed plugin from the user-global plugins directory.
 * Does not remove support data under apex/support/<id>/.
 * @return 0 on success, -1 on failure.
 */
int apex_plugin_uninstall(const char *id,
                          char *error_buf,
                          size_t error_buf_len);

#ifdef __cplusplus
}
#endif

#endif /* APEX_PLUGINS_PUBLIC_H */
