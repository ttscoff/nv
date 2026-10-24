#include "plugins.h"
#include "extensions/metadata.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <sys/time.h>
#include <stdio.h>

#ifdef APEX_HAVE_LIBYAML
#include <yaml.h>
#endif

/* External command runner for plugins (from plugins_env.c) */
char *apex_run_external_plugin_command(const char *cmd,
                                       const char *phase,
                                       const char *plugin_id,
                                       const char *text,
                                       int timeout_ms);

/* ------------------------------------------------------------------------- */
/* Profiling helpers                                                         */
/*                                                                           */
/* Plugin profiling is controlled by environment variables:                  */
/*   - APEX_PROFILE_PLUGINS: if set to 1/yes/true, enables plugin profiling  */
/*   - otherwise, falls back to APEX_PROFILE (same flag used in apex.c)      */
/*                                                                           */
/* When enabled, we emit timing for each plugin invocation and for the       */
/* overall phase run (pre_parse/post_render). Output goes to stderr.         */
/* ------------------------------------------------------------------------- */

static int apex_plugins_profiling_enabled(void) {
    const char *env = getenv("APEX_PROFILE_PLUGINS");
    if (!env || !*env) {
        env = getenv("APEX_PROFILE");
    }
    if (!env) return 0;
    return (strcmp(env, "1") == 0 ||
            strcmp(env, "yes") == 0 ||
            strcmp(env, "true") == 0);
}

static double apex_plugins_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* ------------------------------------------------------------------------- */
/* Git helpers                                                               */
/*                                                                           */
/* When running inside a Git repository, we optionally consult the repo's    */
/* top-level directory for a project-scoped `.apex/plugins` directory.       */
/*                                                                           */
/* This is deliberately best-effort: if Git is unavailable or the current    */
/* directory is not inside a work tree, we simply skip this step.           */
/* ------------------------------------------------------------------------- */

static char *apex_git_toplevel(void) {
    /* Suppress stderr from git so we don't spam users when not in a repo. */
    FILE *fp = popen("git rev-parse --show-toplevel 2>/dev/null", "r");
    if (!fp) {
        return NULL;
    }

    char buf[1024];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return NULL;
    }

    int rc = pclose(fp);
    if (rc != 0) {
        return NULL;
    }

    /* Strip trailing newline(s). */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    if (len == 0) {
        return NULL;
    }

    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len + 1);
    return out;
}


typedef struct apex_plugin {
    char *id;
    char *title;
    char *author;
    char *description;
    char *homepage;
    char *repo;
    apex_plugin_phase_mask phases;
    int priority;

    char *handler_command; /* External command to be executed */
    int timeout_ms;
    /* Declarative regex support */
    char *pattern;
    char *replacement;
    regex_t regex;
    int has_regex;
    /* Owning directory for this plugin (used for APEX_PLUGIN_DIR) */
    char *dir_path;
    /* Per-plugin support directory (used for APEX_SUPPORT_DIR) */
    char *support_dir;
    /* A custom callback to transform the passed text. Executed only if handler_command and has_regex are not set. */
    char *(*callback)(const char *text, const char *id_plugin, apex_plugin_phase_mask phase, const struct apex_options *options);
    struct apex_plugin *next;
} apex_plugin;

struct apex_plugin_manager {
    struct apex_plugin *pre_parse;
    struct apex_plugin *post_render;
};

static apex_plugin *init_plugin(void) {
    return calloc(1, sizeof(struct apex_plugin));
}

static void free_plugin(struct apex_plugin *p) {
    while (p) {
        struct apex_plugin *next = p->next;
        free(p->id);
        free(p->title);
        free(p->author);
        free(p->description);
        free(p->homepage);
        free(p->repo);
        free(p->handler_command);
        free(p->pattern);
        free(p->replacement);
        free(p->dir_path);
        free(p->support_dir);
        if (p->has_regex) {
            regfree(&p->regex);
        }
        p->callback = NULL;
        free(p);
        p = next;
    }
}

void apex_plugins_free(apex_plugin_manager *manager) {
    if (!manager) return;
    free_plugin(manager->pre_parse);
    free_plugin(manager->post_render);
    free(manager);
}

static int plugin_phase_mask_from_string(const char *phase) {
    if (!phase) return 0;
    if (strcmp(phase, "pre_parse") == 0) return APEX_PLUGIN_PHASE_PRE_PARSE;
    if (strcmp(phase, "block") == 0) return APEX_PLUGIN_PHASE_BLOCK;
    if (strcmp(phase, "inline") == 0) return APEX_PLUGIN_PHASE_INLINE;
    if (strcmp(phase, "post_render") == 0) return APEX_PLUGIN_PHASE_POST_RENDER;
    return 0;
}

static int read_file_into_buffer(const char *path, char **out) {
    *out = NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return -1; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return -1; }
    size_t n = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[n] = '\0';
    *out = buf;
    return 0;
}

static bool file_has_yaml_front_matter(const char *path) {
    char *buf = NULL;
    if (read_file_into_buffer(path, &buf) != 0 || !buf) return false;
    bool result = (strncmp(buf, "---", 3) == 0);
    free(buf);
    return result;
}

static void append_plugin_sorted(struct apex_plugin **head, struct apex_plugin *p) {
    if (!p) return;
    if (!*head || (*head)->priority > p->priority ||
        ((*head)->priority == p->priority &&
         strcmp((*head)->id ? (*head)->id : "", p->id ? p->id : "") > 0)) {
        p->next = *head;
        *head = p;
        return;
    }
    struct apex_plugin *cur = *head;
    while (cur->next &&
           (cur->next->priority < p->priority ||
            (cur->next->priority == p->priority &&
             strcmp(cur->next->id ? cur->next->id : "", p->id ? p->id : "") <= 0))) {
        cur = cur->next;
    }
    p->next = cur->next;
    cur->next = p;
}

static bool plugin_id_exists(struct apex_plugin *head, const char *id) {
    if (!id) return false;
    for (struct apex_plugin *p = head; p; p = p->next) {
        if (p->id && strcmp(p->id, id) == 0) return true;
    }
    return false;
}

bool apex_plugin_register(apex_plugin_manager *manager, const char *id, apex_plugin_phase_mask phase, char *(*callback)(const char *text,  const char *id_plugin, apex_plugin_phase_mask phase, const struct apex_options *options)) {
    apex_plugin *plugin;
    plugin = init_plugin();
    if (!plugin || !manager) {
        return false;
    }

    plugin->id = id != NULL ? strdup(id) : NULL;
    plugin->callback = callback;
    plugin->phases = phase;

    if (plugin->phases & APEX_PLUGIN_PHASE_PRE_PARSE) {
        if (!plugin_id_exists(manager->pre_parse, plugin->id)) {
            append_plugin_sorted(&manager->pre_parse, plugin);
            return true;
        }
    } else if (plugin->phases & APEX_PLUGIN_PHASE_POST_RENDER) {
        if (!plugin_id_exists(manager->post_render, plugin->id)) {
            append_plugin_sorted(&manager->post_render, plugin);
            return true;
        }
    }
    return false;
}

/* Determine base support directory for plugins, creating it if needed.
 * This follows XDG conventions: $XDG_CONFIG_HOME/apex/support or
 * $HOME/.config/apex/support.
 */
static char *apex_get_support_base_dir(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char buf[1024];

    if (xdg && *xdg) {
        snprintf(buf, sizeof(buf), "%s/apex/support", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home || !*home) {
            return NULL;
        }
        snprintf(buf, sizeof(buf), "%s/.config/apex/support", home);
    }

    /* Ensure directory exists (mkdir -p style for two levels) */
    char *path = strdup(buf);
    if (!path) return NULL;

    /* Create parent ~/.config/apex if needed when using HOME path */
    if (!xdg || !*xdg) {
        char parent[1024];
        snprintf(parent, sizeof(parent), "%s/.config/apex", getenv("HOME"));
        mkdir(parent, 0700);
    }

    mkdir(path, 0700);
    return path;
}

static void load_plugins_from_dir(apex_plugin_manager *manager,
                                  const char *dirpath) {
    if (!manager || !dirpath) return;
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    char *support_base = apex_get_support_base_dir();

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char plugin_dir[1024];
        snprintf(plugin_dir, sizeof(plugin_dir), "%s/%s", dirpath, ent->d_name);

        struct stat st;
        if (stat(plugin_dir, &st) != 0) {
            continue;
        }

        char manifest_path[1024];
        if (S_ISDIR(st.st_mode)) {
            /* New style: each subdirectory is a plugin, with plugin.yml/yaml */
            snprintf(manifest_path, sizeof(manifest_path), "%s/plugin.yml", plugin_dir);
            if (!file_has_yaml_front_matter(manifest_path)) {
                snprintf(manifest_path, sizeof(manifest_path), "%s/plugin.yaml", plugin_dir);
                if (!file_has_yaml_front_matter(manifest_path)) {
                    continue;
                }
            }
        } else {
            /* Backwards compatibility: flat *.yml / *.yaml manifests */
            size_t len = strlen(ent->d_name);
            if (!( (len > 4 && strcmp(ent->d_name + len - 4, ".yml") == 0) ||
                   (len > 5 && strcmp(ent->d_name + len - 5, ".yaml") == 0))) {
                continue;
            }
            if (!file_has_yaml_front_matter(plugin_dir)) {
                continue;
            }
            snprintf(manifest_path, sizeof(manifest_path), "%s", plugin_dir);
        }

        apex_metadata_item *meta = apex_load_metadata_from_file(manifest_path);
        if (!meta) continue;

#ifdef APEX_HAVE_LIBYAML
        /* Check for bundle array - if found, process each bundle entry as a separate plugin */
        size_t bundle_count = 0;
        apex_metadata_item **bundles = apex_extract_plugin_bundle(manifest_path, &bundle_count);
        if (bundles && bundle_count > 0) {
            /* Process each bundle entry as a plugin */
            for (size_t b_idx = 0; b_idx < bundle_count; b_idx++) {
                apex_metadata_item *bundle_meta = bundles[b_idx];
                if (!bundle_meta) continue;

                /* Merge bundle-level metadata (from top-level) with bundle entry metadata */
                /* Bundle entry metadata takes precedence */
                apex_metadata_item *merged = apex_merge_metadata(meta, bundle_meta, NULL);

                /* Extract plugin fields from merged metadata */
                const char *id = NULL;
                const char *title = NULL;
                const char *author = NULL;
                const char *description = NULL;
                const char *phase = NULL;
                const char *handler_command = NULL;
                const char *priority_str = NULL;
                const char *timeout_str = NULL;
                const char *pattern_str = NULL;
                const char *replacement_str = NULL;
                const char *flags_str = NULL;
                const char *homepage = NULL;
                const char *repo = NULL;

                /* First, get bundle-level metadata (from top-level meta) */
                for (apex_metadata_item *m = meta; m; m = m->next) {
                    if (strcmp(m->key, "author") == 0) author = m->value;
                    else if (strcmp(m->key, "homepage") == 0) homepage = m->value;
                    else if (strcmp(m->key, "repo") == 0) repo = m->value;
                }

                /* Then, get bundle entry-specific metadata (overrides bundle-level) */
                for (apex_metadata_item *m = merged; m; m = m->next) {
                    if (strcmp(m->key, "id") == 0) id = m->value;
                    else if (strcmp(m->key, "title") == 0) title = m->value;
                    else if (strcmp(m->key, "author") == 0 && m->value) author = m->value;
                    else if (strcmp(m->key, "description") == 0) description = m->value;
                    else if (strcmp(m->key, "homepage") == 0 && m->value) homepage = m->value;
                    else if (strcmp(m->key, "repo") == 0 && m->value) repo = m->value;
                    else if (strcmp(m->key, "phase") == 0) phase = m->value;
                    else if (strcmp(m->key, "handler.command") == 0) handler_command = m->value;
                    else if (strcmp(m->key, "handler_command") == 0) handler_command = m->value;
                    else if (strcmp(m->key, "priority") == 0) priority_str = m->value;
                    else if (strcmp(m->key, "timeout_ms") == 0) timeout_str = m->value;
                    else if (strcmp(m->key, "pattern") == 0) pattern_str = m->value;
                    else if (strcmp(m->key, "replacement") == 0) replacement_str = m->value;
                    else if (strcmp(m->key, "flags") == 0) flags_str = m->value;
                }

                if (!id || !phase) {
                    apex_free_metadata(merged);
                    continue;
                }

                int phase_mask = plugin_phase_mask_from_string(phase);
                if (!(phase_mask & (APEX_PLUGIN_PHASE_PRE_PARSE | APEX_PLUGIN_PHASE_POST_RENDER))) {
                    apex_free_metadata(merged);
                    continue;
                }

                struct apex_plugin *p = init_plugin();
                if (!p) {
                    apex_free_metadata(merged);
                    continue;
                }
                p->id = strdup(id);
                p->title = title ? strdup(title) : NULL;
                p->author = author ? strdup(author) : NULL;
                p->description = description ? strdup(description) : NULL;
                p->homepage = homepage ? strdup(homepage) : NULL;
                p->repo = repo ? strdup(repo) : NULL;
                p->phases = phase_mask;
                p->handler_command = handler_command ? strdup(handler_command) : NULL;
                p->priority = priority_str ? atoi(priority_str) : 100;
                p->timeout_ms = timeout_str ? atoi(timeout_str) : 0;
                p->has_regex = 0;
                p->dir_path = strdup(plugin_dir);

                /* Compute per-plugin support directory */
                char *support_base = apex_get_support_base_dir();
                if (support_base && id) {
                    char supp[1024];
                    snprintf(supp, sizeof(supp), "%s/%s", support_base, id);
                    mkdir(supp, 0700);
                    p->support_dir = strdup(supp);
                }
                if (support_base) free(support_base);

                /* Compile declarative regex if provided */
                if (!p->handler_command && pattern_str && replacement_str) {
                    int cflags = REG_EXTENDED;
                    if (flags_str && strchr(flags_str, 'i')) {
                        cflags |= REG_ICASE;
                    }
                    if (regcomp(&p->regex, pattern_str, cflags) != 0) {
                        free(p->id);
                        free(p->title);
                        free(p->author);
                        free(p->description);
                        free(p->homepage);
                        free(p->repo);
                        free(p);
                        apex_free_metadata(merged);
                        continue;
                    }
                    p->pattern = strdup(pattern_str);
                    p->replacement = strdup(replacement_str);
                    p->has_regex = 1;
                }

                /* Attach to appropriate phase lists */
                if (phase_mask & APEX_PLUGIN_PHASE_PRE_PARSE) {
                    if (!plugin_id_exists(manager->pre_parse, id)) {
                        append_plugin_sorted(&manager->pre_parse, p);
                    }
                }
                if (phase_mask & APEX_PLUGIN_PHASE_POST_RENDER) {
                    if (!plugin_id_exists(manager->post_render, id)) {
                        append_plugin_sorted(&manager->post_render, p);
                    }
                }

                apex_free_metadata(merged);
            }

            /* Free bundle arrays */
            for (size_t b_idx = 0; b_idx < bundle_count; b_idx++) {
                if (bundles[b_idx]) {
                    apex_free_metadata(bundles[b_idx]);
                }
            }
            free(bundles);
            apex_free_metadata(meta);
            continue;  /* Skip single-plugin processing for bundle manifests */
        }
        if (bundles) {
            free(bundles);
        }
#endif

        const char *id = NULL;
        const char *title = NULL;
        const char *author = NULL;
        const char *description = NULL;
        const char *phase = NULL;
        const char *handler_command = NULL;
        const char *priority_str = NULL;
        const char *timeout_str = NULL;
        const char *pattern_str = NULL;
        const char *replacement_str = NULL;
        const char *flags_str = NULL;
        const char *homepage = NULL;
        const char *repo = NULL;

        for (apex_metadata_item *m = meta; m; m = m->next) {
            if (strcmp(m->key, "id") == 0) id = m->value;
            else if (strcmp(m->key, "title") == 0) title = m->value;
            else if (strcmp(m->key, "author") == 0) author = m->value;
            else if (strcmp(m->key, "description") == 0) description = m->value;
            else if (strcmp(m->key, "homepage") == 0) homepage = m->value;
            else if (strcmp(m->key, "repo") == 0) repo = m->value;
            else if (strcmp(m->key, "phase") == 0) phase = m->value;
            else if (strcmp(m->key, "handler.command") == 0) handler_command = m->value;
            else if (strcmp(m->key, "handler_command") == 0) handler_command = m->value;
            else if (strcmp(m->key, "priority") == 0) priority_str = m->value;
            else if (strcmp(m->key, "timeout_ms") == 0) timeout_str = m->value;
            else if (strcmp(m->key, "pattern") == 0) pattern_str = m->value;
            else if (strcmp(m->key, "replacement") == 0) replacement_str = m->value;
            else if (strcmp(m->key, "flags") == 0) flags_str = m->value;
        }

        if (!phase) {
            apex_free_metadata(meta);
            continue;
        }

        int phase_mask = plugin_phase_mask_from_string(phase);
        if (!(phase_mask & (APEX_PLUGIN_PHASE_PRE_PARSE | APEX_PLUGIN_PHASE_POST_RENDER))) {
            apex_free_metadata(meta);
            continue;
        }

        const char *final_id = id ? id : ent->d_name;
        struct apex_plugin *p = init_plugin();
        if (!p) {
            apex_free_metadata(meta);
            continue;
        }
        p->id = final_id ? strdup(final_id) : NULL;
        p->title = title ? strdup(title) : NULL;
        p->author = author ? strdup(author) : NULL;
        p->description = description ? strdup(description) : NULL;
        p->homepage = homepage ? strdup(homepage) : NULL;
        p->repo = repo ? strdup(repo) : NULL;
        p->phases = phase_mask;
        p->handler_command = handler_command ? strdup(handler_command) : NULL;
        p->priority = priority_str ? atoi(priority_str) : 100;
        p->timeout_ms = timeout_str ? atoi(timeout_str) : 0;
        p->has_regex = 0;
        p->dir_path = strdup(S_ISDIR(st.st_mode) ? plugin_dir : dirpath);

        /* Compute per-plugin support directory, if we have a base and id */
        if (support_base && final_id) {
            char supp[1024];
            snprintf(supp, sizeof(supp), "%s/%s", support_base, final_id);
            mkdir(supp, 0700);
            p->support_dir = strdup(supp);
        }

        /* Compile declarative regex if provided and no external handler */
        if (!p->handler_command && pattern_str && replacement_str) {
            int cflags = REG_EXTENDED;
            if (flags_str && strchr(flags_str, 'i')) {
                cflags |= REG_ICASE;
            }
            if (regcomp(&p->regex, pattern_str, cflags) != 0) {
                /* Invalid regex, skip this plugin */
                free(p->id);
                free(p->title);
                free(p->author);
                free(p->description);
                free(p->homepage);
                free(p->repo);
                free(p);
                apex_free_metadata(meta);
                continue;
            }
            p->pattern = strdup(pattern_str);
            p->replacement = strdup(replacement_str);
            p->has_regex = 1;
        }

        /* Attach to appropriate phase lists, enforcing per-list id uniqueness */
        if (phase_mask & APEX_PLUGIN_PHASE_PRE_PARSE) {
            if (!plugin_id_exists(manager->pre_parse, final_id)) {
                append_plugin_sorted(&manager->pre_parse, p);
            }
        }
        if (phase_mask & APEX_PLUGIN_PHASE_POST_RENDER) {
            if (!plugin_id_exists(manager->post_render, final_id)) {
                append_plugin_sorted(&manager->post_render, p);
            }
        }

        apex_free_metadata(meta);
    }

    if (support_base) free(support_base);
    closedir(dir);
}

static char *dup_join(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    char *res = malloc(la + lb + 2);
    if (!res) return NULL;
    if (la) memcpy(res, a, la);
    if (lb) {
        if (la) res[la] = '/';
        memcpy(res + la + (la ? 1 : 0), b, lb);
        res[la + (la ? 1 : 0) + lb] = '\0';
    } else {
        res[la] = '\0';
    }
    return res;
}

apex_plugin_manager *apex_plugins_load(const apex_options *options) {
    if (!options || !options->enable_plugins) return NULL;

    apex_plugin_manager *manager = calloc(1, sizeof(apex_plugin_manager));
    if (!manager) return NULL;

    if (options->plugin_register) {
        options->plugin_register(manager, options);
    }

    if (options->allow_external_plugin_detection) {
        /* Project-scoped (current working directory): CWD/.apex/plugins */
        char cwd[1024];
        cwd[0] = '\0';
        if (getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0') {
            char *cwd_proj_dir = dup_join(cwd, ".apex/plugins");
            if (cwd_proj_dir) {
                load_plugins_from_dir(manager, cwd_proj_dir);
                free(cwd_proj_dir);
            }
        }

        /* Project-scoped (explicit base_directory): base_directory/.apex/plugins */
        if (options->base_directory && options->base_directory[0] != '\0') {
            char *proj_dir = dup_join(options->base_directory, ".apex/plugins");
            if (proj_dir) {
                load_plugins_from_dir(manager, proj_dir);
                free(proj_dir);
            }
        }

        /* Project-scoped (Git repository root): <git top>/\.apex/plugins
         * Only used when the current directory is inside the work tree.
         */
        char *git_root = apex_git_toplevel();
        if (git_root && git_root[0] != '\0' && cwd[0] != '\0') {
            size_t root_len = strlen(git_root);
            /* Ensure the Git root is a parent of (or equal to) the current directory. */
            if (strncmp(cwd, git_root, root_len) == 0 &&
                (cwd[root_len] == '/' || cwd[root_len] == '\0')) {
                /* Avoid re-loading if git_root is the same as base_directory. */
                if (!options->base_directory ||
                    strcmp(git_root, options->base_directory) != 0) {
                    char *git_proj_dir = dup_join(git_root, ".apex/plugins");
                    if (git_proj_dir) {
                        load_plugins_from_dir(manager, git_proj_dir);
                        free(git_proj_dir);
                    }
                    }
                }
            free(git_root);
        }

        /* User-global: $XDG_CONFIG_HOME/apex/plugins or $HOME/.config/apex/plugins */
        const char *xdg = getenv("XDG_CONFIG_HOME");
        char *global_dir = NULL;
        if (xdg && *xdg) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s/apex/plugins", xdg);
            global_dir = strdup(buf);
        } else {
            const char *home = getenv("HOME");
            if (home && *home) {
                char buf[1024];
                snprintf(buf, sizeof(buf), "%s/.config/apex/plugins", home);
                global_dir = strdup(buf);
            }
        }
        if (global_dir) {
            load_plugins_from_dir(manager, global_dir);
            free(global_dir);
        }
    }

    if (!manager->pre_parse && !manager->post_render) {
        apex_plugins_free(manager);
        return NULL;
    }
    return manager;
}

/* Apply declarative regex replacement: pattern compiled in p->regex,
 * replacement template may reference $0..$9 for capture groups.
 */
static char *apply_regex_replacement(struct apex_plugin *p, const char *input) {
    if (!p || !p->has_regex || !p->replacement || !input) return NULL;

    size_t in_len = strlen(input);
    size_t cap = in_len + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t out_len = 0;

    const char *cursor = input;
    regmatch_t matches[10];
    int rc;
    int any_match = 0;

    while ((rc = regexec(&p->regex, cursor, 10, matches, 0)) == 0) {
        if (matches[0].rm_so < 0) {
            break;
        }
        any_match = 1;

        size_t pre_len = (size_t)matches[0].rm_so;
        if (out_len + pre_len + 1 > cap) {
            cap = (out_len + pre_len + 1) * 2;
            char *nb = realloc(out, cap);
            if (!nb) {
                free(out);
                return NULL;
            }
            out = nb;
        }
        memcpy(out + out_len, cursor, pre_len);
        out_len += pre_len;

        /* Build replacement text */
        const char *tpl = p->replacement;
        while (*tpl) {
            if (*tpl == '$' && tpl[1] >= '0' && tpl[1] <= '9') {
                int g = tpl[1] - '0';
                tpl += 2;
                if (g < 10 && matches[g].rm_so != -1 && matches[g].rm_eo >= matches[g].rm_so) {
                    size_t start = (size_t)matches[g].rm_so;
                    size_t end = (size_t)matches[g].rm_eo;
                    size_t glen = end - start;
                    if (out_len + glen + 1 > cap) {
                        cap = (out_len + glen + 1) * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) {
                            free(out);
                            return NULL;
                        }
                        out = nb;
                    }
                    memcpy(out + out_len, cursor + start, glen);
                    out_len += glen;
                }
            } else {
                if (out_len + 2 > cap) {
                    cap = (out_len + 2) * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        return NULL;
                    }
                    out = nb;
                }
                out[out_len++] = *tpl++;
            }
        }

        size_t adv = (size_t)matches[0].rm_eo;
        if (adv == 0) {
            /* Avoid infinite loop on zero-length matches */
            if (*cursor) {
                if (out_len + 2 > cap) {
                    cap = (out_len + 2) * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        return NULL;
                    }
                    out = nb;
                }
                out[out_len++] = *cursor;
                cursor++;
            } else {
                break;
            }
        } else {
            cursor += adv;
        }
    }

    if (!any_match) {
        free(out);
        return NULL;
    }

    /* Copy remainder */
    size_t tail_len = strlen(cursor);
    if (out_len + tail_len + 1 > cap) {
        cap = out_len + tail_len + 1;
        char *nb = realloc(out, cap);
        if (!nb) {
            free(out);
            return NULL;
        }
        out = nb;
    }
    memcpy(out + out_len, cursor, tail_len);
    out_len += tail_len;
    out[out_len] = '\0';
    return out;
}

char *apex_plugins_run_text_phase(apex_plugin_manager *manager,
                                  apex_plugin_phase_mask phase,
                                  const char *text,
                                  const apex_options *options) {
    if (!manager || !text || !options) return NULL;

    int do_profile = apex_plugins_profiling_enabled();
    double phase_start = 0.0;
    if (do_profile) {
        phase_start = apex_plugins_time_ms();
    }

    const char *phase_name = "unknown";
    struct apex_plugin *plist = NULL;
    if (phase == APEX_PLUGIN_PHASE_PRE_PARSE) {
        plist = manager->pre_parse;
        phase_name = "pre_parse";
    } else if (phase == APEX_PLUGIN_PHASE_POST_RENDER) {
        plist = manager->post_render;
        phase_name = "post_render";
    }

    char *current = strdup(text);
    if (!current) return NULL;

    for (struct apex_plugin *p = plist; p; p = p->next) {
        if (!(p->phases & phase)) continue;

        char *next = NULL;
        const char *plugin_id = p->id ? p->id : "plugin";

        double plugin_start = 0.0;
        if (do_profile) {
            plugin_start = apex_plugins_time_ms();
        }

        if (p->handler_command) {
            /* Temporarily set APEX_PLUGIN_DIR, APEX_SUPPORT_DIR, and APEX_FILE_PATH for this plugin */
            char *old_dir = NULL;
            char *old_support = NULL;
            char *old_file_path = NULL;

            const char *prev_dir = getenv("APEX_PLUGIN_DIR");
            if (prev_dir) {
                old_dir = strdup(prev_dir);
            }
            if (p->dir_path) {
                setenv("APEX_PLUGIN_DIR", p->dir_path, 1);
            }

            const char *prev_support = getenv("APEX_SUPPORT_DIR");
            if (prev_support) {
                old_support = strdup(prev_support);
            }
            if (p->support_dir) {
                setenv("APEX_SUPPORT_DIR", p->support_dir, 1);
            }

            /* APEX_FILE_PATH: full path to input file, or base dir / empty for stdin */
            const char *prev_file = getenv("APEX_FILE_PATH");
            if (prev_file) {
                old_file_path = strdup(prev_file);
            }
            const char *file_path = (options && options->input_file_path)
                                      ? options->input_file_path
                                      : "";
            setenv("APEX_FILE_PATH", file_path, 1);

            next = apex_run_external_plugin_command(p->handler_command,
                                                    phase_name,
                                                    plugin_id,
                                                    current,
                                                    p->timeout_ms);

            /* Restore previous APEX_PLUGIN_DIR */
            if (p->dir_path) {
                if (old_dir) {
                    setenv("APEX_PLUGIN_DIR", old_dir, 1);
                    free(old_dir);
                } else {
                    unsetenv("APEX_PLUGIN_DIR");
                }
            } else if (old_dir) {
                free(old_dir);
            }

            /* Restore previous APEX_SUPPORT_DIR */
            if (p->support_dir) {
                if (old_support) {
                    setenv("APEX_SUPPORT_DIR", old_support, 1);
                    free(old_support);
                } else {
                    unsetenv("APEX_SUPPORT_DIR");
                }
            } else if (old_support) {
                free(old_support);
            }

            /* Restore previous APEX_FILE_PATH */
            if (old_file_path) {
                setenv("APEX_FILE_PATH", old_file_path, 1);
                free(old_file_path);
            } else {
                unsetenv("APEX_FILE_PATH");
            }
        } else if (p->has_regex) {
            next = apply_regex_replacement(p, current);
        } else if (p->callback) {
            next = p->callback(current, p->id, phase, options);
        }

        if (do_profile) {
            double plugin_elapsed = apex_plugins_time_ms() - plugin_start;
            fprintf(stderr,
                    "[PROFILE] plugin %-24s (%s): %8.2f ms\n",
                    plugin_id,
                    phase_name,
                    plugin_elapsed);
        }

        if (next) {
            free(current);
            current = next;
        }
    }

    if (do_profile) {
        double phase_elapsed = apex_plugins_time_ms() - phase_start;
        fprintf(stderr,
                "[PROFILE] plugins_phase (%s):       %8.2f ms\n",
                phase_name,
                phase_elapsed);
    }

    if (strcmp(current, text) == 0) {
        free(current);
        return NULL;
    }
    return current;
}

