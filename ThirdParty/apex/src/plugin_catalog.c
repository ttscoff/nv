/**
 * Plugin catalog discovery, installation, and uninstallation.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/apex/plugins.h"
#include "extensions/metadata.h"
#include "plugins_remote.h"

static void plugin_set_error(char *buf, size_t buflen, const char *msg) {
    if (!buf || buflen == 0 || !msg) return;
    snprintf(buf, buflen, "%s", msg);
}

static int plugin_global_directory(char *buf, size_t buflen) {
    if (!buf || buflen == 0) return -1;

    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(buf, buflen, "%s/apex/plugins", xdg);
        return 0;
    }

    const char *home = getenv("HOME");
    if (!home || !*home) return -1;

    snprintf(buf, buflen, "%s/.config/apex/plugins", home);
    return 0;
}

int apex_plugin_global_directory(char *buf, size_t buflen) {
    return plugin_global_directory(buf, buflen);
}

static char *apex_plugin_git_toplevel(void) {
    FILE *fp = popen("git rev-parse --show-toplevel 2>/dev/null", "r");
    if (!fp) return NULL;

    char line[1024];
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        return NULL;
    }

    int rc = pclose(fp);
    if (rc != 0) return NULL;

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    if (len == 0) return NULL;

    return strdup(line);
}

static int plugin_catalog_has_id(const apex_plugin_catalog *catalog, const char *id) {
    if (!catalog || !id) return 0;
    for (size_t i = 0; i < catalog->count; i++) {
        if (catalog->items[i].id && strcmp(catalog->items[i].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static void plugin_catalog_append_info(apex_plugin_catalog *catalog,
                                       const apex_plugin_info *info) {
    if (!catalog || !info || !info->id) return;

    apex_plugin_info *items = realloc(catalog->items,
                                      (catalog->count + 1) * sizeof(apex_plugin_info));
    if (!items) return;

    catalog->items = items;
    apex_plugin_info *dest = &catalog->items[catalog->count];
    memset(dest, 0, sizeof(*dest));
    dest->id = info->id ? strdup(info->id) : NULL;
    dest->title = info->title ? strdup(info->title) : NULL;
    dest->description = info->description ? strdup(info->description) : NULL;
    dest->author = info->author ? strdup(info->author) : NULL;
    dest->homepage = info->homepage ? strdup(info->homepage) : NULL;
    dest->repo = info->repo ? strdup(info->repo) : NULL;
    catalog->count++;
}

static void plugin_collect_installed_from_root(const char *root,
                                               apex_plugin_catalog *catalog) {
    if (!root || !*root || !catalog) return;

    DIR *d = opendir(root);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char plugin_dir[1200];
        snprintf(plugin_dir, sizeof(plugin_dir), "%s/%s", root, ent->d_name);

        struct stat st;
        if (stat(plugin_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        char manifest[1300];
        snprintf(manifest, sizeof(manifest), "%s/plugin.yml", plugin_dir);
        FILE *test = fopen(manifest, "r");
        if (!test) {
            snprintf(manifest, sizeof(manifest), "%s/plugin.yaml", plugin_dir);
            test = fopen(manifest, "r");
        }
        if (!test) continue;
        fclose(test);

        apex_metadata_item *meta = apex_load_metadata_from_file(manifest);
        if (!meta) continue;

        apex_plugin_info info;
        memset(&info, 0, sizeof(info));

        for (apex_metadata_item *m = meta; m; m = m->next) {
            if (strcmp(m->key, "id") == 0) info.id = (char *)m->value;
            else if (strcmp(m->key, "title") == 0) info.title = (char *)m->value;
            else if (strcmp(m->key, "author") == 0) info.author = (char *)m->value;
            else if (strcmp(m->key, "description") == 0) info.description = (char *)m->value;
            else if (strcmp(m->key, "homepage") == 0) info.homepage = (char *)m->value;
        }

        if (!info.id) info.id = ent->d_name;
        if (!plugin_catalog_has_id(catalog, info.id)) {
            plugin_catalog_append_info(catalog, &info);
        }

        apex_free_metadata(meta);
    }

    closedir(d);
}

apex_plugin_catalog *apex_plugins_list_installed(
    const apex_plugin_discovery_options *options) {
    bool include_project = true;
    bool include_user_global = true;
    const char *base_directory = NULL;

    if (options) {
        include_project = options->include_project;
        include_user_global = options->include_user_global;
        base_directory = options->base_directory;
    }

    apex_plugin_catalog *catalog = calloc(1, sizeof(apex_plugin_catalog));
    if (!catalog) return NULL;

    if (include_project) {
        char cwd[1024];
        cwd[0] = '\0';
        if (getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0') {
            char path[1200];
            snprintf(path, sizeof(path), "%s/.apex/plugins", cwd);
            plugin_collect_installed_from_root(path, catalog);
        }

        if (base_directory && base_directory[0] != '\0') {
            char path[1200];
            snprintf(path, sizeof(path), "%s/.apex/plugins", base_directory);
            plugin_collect_installed_from_root(path, catalog);
        }

        char *git_root = apex_plugin_git_toplevel();
        if (git_root && git_root[0] != '\0' && cwd[0] != '\0') {
            size_t root_len = strlen(git_root);
            if (strncmp(cwd, git_root, root_len) == 0 &&
                (cwd[root_len] == '/' || cwd[root_len] == '\0')) {
                if (!base_directory || strcmp(git_root, base_directory) != 0) {
                    char path[1200];
                    snprintf(path, sizeof(path), "%s/.apex/plugins", git_root);
                    plugin_collect_installed_from_root(path, catalog);
                }
            }
        }
        free(git_root);
    }

    if (include_user_global) {
        char root[1024];
        if (plugin_global_directory(root, sizeof(root)) == 0) {
            plugin_collect_installed_from_root(root, catalog);
        }
    }

    return catalog;
}

static char *normalize_plugin_repo_url(const char *arg) {
    if (!arg || !*arg) return NULL;

    if (strstr(arg, "://") != NULL || strstr(arg, "@") != NULL) {
        if (strncmp(arg, "https://github.com/", 19) == 0 ||
            strncmp(arg, "http://github.com/", 18) == 0 ||
            strncmp(arg, "git@github.com:", 15) == 0) {
            size_t len = strlen(arg);
            if (len < 4 || strcmp(arg + len - 4, ".git") != 0) {
                char *url = malloc(len + 5);
                if (!url) return NULL;
                snprintf(url, len + 5, "%s.git", arg);
                return url;
            }
        }
        return strdup(arg);
    }

    const char *slash = strchr(arg, '/');
    if (slash && slash != arg && slash[1] != '\0') {
        size_t len = strlen(arg);
        char *url = malloc(19 + len + 5);
        if (!url) return NULL;
        snprintf(url, 19 + len + 5, "https://github.com/%s.git", arg);
        return url;
    }

    return NULL;
}

static char *extract_plugin_id_from_repo(const char *repo_path) {
    char manifest[1300];
    snprintf(manifest, sizeof(manifest), "%s/plugin.yml", repo_path);
    FILE *mt = fopen(manifest, "r");
    if (!mt) {
        snprintf(manifest, sizeof(manifest), "%s/plugin.yaml", repo_path);
        mt = fopen(manifest, "r");
    }

    if (mt) {
        fclose(mt);
        apex_metadata_item *meta = apex_load_metadata_from_file(manifest);
        if (meta) {
            const char *id = NULL;
            for (apex_metadata_item *m = meta; m; m = m->next) {
                if (strcmp(m->key, "id") == 0) {
                    id = m->value;
                    break;
                }
            }
            if (id && *id) {
                char *result = strdup(id);
                apex_free_metadata(meta);
                return result;
            }
            apex_free_metadata(meta);
        }
    }

    const char *last_slash = strrchr(repo_path, '/');
    if (last_slash && last_slash[1] != '\0') {
        const char *name = last_slash + 1;
        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".git") == 0) {
            len -= 4;
        }
        char *result = malloc(len + 1);
        if (result) {
            memcpy(result, name, len);
            result[len] = '\0';
            return result;
        }
    }

    return NULL;
}

static int plugin_run_post_install(const char *plugin_dir,
                                   const char *plugin_id,
                                   char *error_buf,
                                   size_t error_buf_len) {
    char manifest[1300];
    snprintf(manifest, sizeof(manifest), "%s/plugin.yml", plugin_dir);
    FILE *mt = fopen(manifest, "r");
    if (!mt) {
        snprintf(manifest, sizeof(manifest), "%s/plugin.yaml", plugin_dir);
        mt = fopen(manifest, "r");
    }
    if (!mt) return 0;
    fclose(mt);

    apex_metadata_item *meta = apex_load_metadata_from_file(manifest);
    if (!meta) return 0;

    const char *post_install = NULL;
    for (apex_metadata_item *m = meta; m; m = m->next) {
        if (strcmp(m->key, "post_install") == 0) {
            post_install = m->value;
            break;
        }
    }

    int rc = 0;
    if (post_install && *post_install) {
        char hook_cmd[2048];
        snprintf(hook_cmd, sizeof(hook_cmd), "cd \"%s\" && %s", plugin_dir, post_install);
        int hook_rc = system(hook_cmd);
        if (hook_rc != 0) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "post-install hook for '%s' exited with status %d",
                     plugin_id ? plugin_id : "", hook_rc);
            plugin_set_error(error_buf, error_buf_len, msg);
            rc = -1;
        }
    }

    apex_free_metadata(meta);
    return rc;
}

int apex_plugin_install(const char *id_or_repo,
                        const apex_plugin_install_options *options,
                        char *error_buf,
                        size_t error_buf_len) {
    if (!id_or_repo || !*id_or_repo) {
        plugin_set_error(error_buf, error_buf_len, "plugin id or repository URL is required");
        return -1;
    }

    bool allow_untrusted_repo = false;
    bool run_post_install = true;
    if (options) {
        allow_untrusted_repo = options->allow_untrusted_repo;
        run_post_install = options->run_post_install;
    }

    char root[1024];
    if (plugin_global_directory(root, sizeof(root)) != 0) {
        plugin_set_error(error_buf, error_buf_len, "HOME not set; cannot determine plugin directory");
        return -1;
    }

    char *normalized_repo = normalize_plugin_repo_url(id_or_repo);
    const char *repo = NULL;
    char *final_plugin_id = NULL;
    apex_remote_plugin_list *plist = NULL;

    if (normalized_repo) {
        if (!allow_untrusted_repo) {
            plugin_set_error(error_buf, error_buf_len,
                             "untrusted repository install requires allow_untrusted_repo");
            free(normalized_repo);
            return -1;
        }
        repo = normalized_repo;
    } else {
        plist = apex_remote_fetch_directory(APEX_PLUGIN_DIRECTORY_URL);
        if (!plist) {
            plugin_set_error(error_buf, error_buf_len,
                             "failed to fetch plugin directory");
            return -1;
        }
        apex_remote_plugin *rp = apex_remote_find_plugin(plist, id_or_repo);
        repo = apex_remote_plugin_repo(rp);
        if (!rp || !repo) {
            plugin_set_error(error_buf, error_buf_len, "plugin not found in directory");
            apex_remote_free_plugins(plist);
            return -1;
        }
        final_plugin_id = strdup(id_or_repo);
        if (!final_plugin_id) {
            plugin_set_error(error_buf, error_buf_len, "out of memory");
            apex_remote_free_plugins(plist);
            return -1;
        }
    }

    char mkdir_cmd[1200];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", root);
    if (system(mkdir_cmd) != 0) {
        plugin_set_error(error_buf, error_buf_len, "failed to create plugin directory");
        free(normalized_repo);
        free(final_plugin_id);
        apex_remote_free_plugins(plist);
        return -1;
    }

    char temp_target[1200];
    if (!final_plugin_id) {
        const char *last_slash = strrchr(repo, '/');
        const char *name_start = last_slash ? (last_slash + 1) : repo;
        const char *name_end = strstr(name_start, ".git");
        if (!name_end) name_end = name_start + strlen(name_start);
        size_t name_len = (size_t)(name_end - name_start);
        if (name_len > 0 && name_len < 200) {
            char temp_name[256];
            memcpy(temp_name, name_start, name_len);
            temp_name[name_len] = '\0';
            snprintf(temp_target, sizeof(temp_target), "%s/.apex_install_%s", root, temp_name);
        } else {
            snprintf(temp_target, sizeof(temp_target), "%s/.apex_install_temp", root);
        }
    } else {
        snprintf(temp_target, sizeof(temp_target), "%s/%s", root, final_plugin_id);
    }

    char test_cmd[1300];
    snprintf(test_cmd, sizeof(test_cmd), "[ -d \"%s\" ]", temp_target);
    if (system(test_cmd) == 0 && final_plugin_id) {
        plugin_set_error(error_buf, error_buf_len,
                         "plugin already installed; uninstall before reinstalling");
        free(normalized_repo);
        free(final_plugin_id);
        apex_remote_free_plugins(plist);
        return -1;
    }

    char clone_cmd[2048];
    snprintf(clone_cmd, sizeof(clone_cmd), "git clone \"%s\" \"%s\"", repo, temp_target);
    if (system(clone_cmd) != 0) {
        plugin_set_error(error_buf, error_buf_len, "git clone failed");
        free(normalized_repo);
        free(final_plugin_id);
        apex_remote_free_plugins(plist);
        return -1;
    }

    if (!final_plugin_id) {
        final_plugin_id = extract_plugin_id_from_repo(temp_target);
        if (!final_plugin_id) {
            char rm_cmd[1300];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
            system(rm_cmd);
            plugin_set_error(error_buf, error_buf_len,
                             "could not determine plugin id from repository");
            free(normalized_repo);
            apex_remote_free_plugins(plist);
            return -1;
        }

        char final_target[1200];
        snprintf(final_target, sizeof(final_target), "%s/%s", root, final_plugin_id);

        char final_test_cmd[1300];
        snprintf(final_test_cmd, sizeof(final_test_cmd), "[ -d \"%s\" ]", final_target);
        if (system(final_test_cmd) == 0) {
            char rm_cmd[1300];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
            system(rm_cmd);
            plugin_set_error(error_buf, error_buf_len,
                             "plugin already installed; uninstall before reinstalling");
            free(final_plugin_id);
            free(normalized_repo);
            apex_remote_free_plugins(plist);
            return -1;
        }

        char mv_cmd[2500];
        snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", temp_target, final_target);
        if (system(mv_cmd) != 0) {
            char rm_cmd[1300];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
            system(rm_cmd);
            plugin_set_error(error_buf, error_buf_len, "failed to move plugin into place");
            free(final_plugin_id);
            free(normalized_repo);
            apex_remote_free_plugins(plist);
            return -1;
        }
        strncpy(temp_target, final_target, sizeof(temp_target) - 1);
        temp_target[sizeof(temp_target) - 1] = '\0';
    }

    if (run_post_install) {
        plugin_run_post_install(temp_target, final_plugin_id, error_buf, error_buf_len);
    }

    free(normalized_repo);
    free(final_plugin_id);
    apex_remote_free_plugins(plist);
    return 0;
}

int apex_plugin_uninstall(const char *id,
                          char *error_buf,
                          size_t error_buf_len) {
    if (!id || !*id) {
        plugin_set_error(error_buf, error_buf_len, "plugin id is required");
        return -1;
    }

    char root[1024];
    if (plugin_global_directory(root, sizeof(root)) != 0) {
        plugin_set_error(error_buf, error_buf_len, "HOME not set; cannot determine plugin directory");
        return -1;
    }

    char target[1200];
    snprintf(target, sizeof(target), "%s/%s", root, id);

    struct stat st;
    if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode)) {
        plugin_set_error(error_buf, error_buf_len, "plugin is not installed");
        return -1;
    }

    char rm_cmd[1400];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", target);
    if (system(rm_cmd) != 0) {
        plugin_set_error(error_buf, error_buf_len, "failed to remove plugin directory");
        return -1;
    }

    return 0;
}
