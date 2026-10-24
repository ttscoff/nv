/**
 * Apex CLI - Command-line interface for the Apex Markdown processor
 */

#include "../include/apex/apex.h"
#include "../include/apex/ast_terminal.h"
#include "../src/extensions/metadata.h"
#include "../src/extensions/includes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>

static char *read_file(const char *filename, size_t *len);

/**
 * Tracks which apex_options fields were set explicitly from argv so that
 * merged config/document metadata cannot override them after apex_apply_metadata_to_options.
 */
typedef struct apex_cli_option_mask {
    bool mode;
    bool output_format;
    bool toc_min_max;
    bool xhtml;
    bool strict_xhtml;
    bool theme_name;
    bool enable_plugins;
    bool enable_tables;
    bool enable_footnotes;
    bool enable_smart_typography;
    bool enable_math;
    bool enable_file_includes;
    bool hardbreaks;
    bool standalone;
    bool embed_stylesheet;
    bool document_title;
    bool pretty;
    bool critic;
    bool id_format;
    bool generate_header_ids;
    bool header_anchors;
    bool relaxed_tables;
    bool per_cell_alignment;
    bool caption_position;
    bool code_highlighter;
    bool code_highlight_theme;
    bool code_line_numbers;
    bool highlight_language_only;
    bool allow_alpha_lists;
    bool allow_mixed_list_markers;
    bool unsafe;
    bool enable_sup_sub;
    bool enable_divs;
    bool enable_py_callouts;
    bool enable_quarto_callouts;
    bool enable_definition_lists;
    bool enable_spans;
    bool enable_grid_tables;
    bool enable_autolink;
    bool enable_strikethrough;
    bool obfuscate_emails;
    bool enable_aria;
    bool enable_wiki_links;
    bool enable_emoji;
    bool enable_emoji_autocorrect;
    bool enable_widont;
    bool code_is_poetry;
    bool enable_markdown_in_html;
    bool random_footnote_ids;
    bool enable_hashtags;
    bool style_hashtags;
    bool proofreader;
    bool hr_page_break;
    bool title_from_h1;
    bool page_break_before_footnotes;
    bool wikilink_space;
    bool wikilink_extension;
    bool wikilink_sanitize;
    bool enable_metadata_transforms;
    bool embed_images;
    bool enable_image_captions;
    bool title_captions_only;
    bool base_directory;
    bool bibliography;
    bool csl_file;
    bool suppress_bibliography;
    bool link_citations;
    bool show_tooltips;
    bool indices;
    bool suppress_index;
    bool concordance;
    bool stylesheet;
} apex_cli_option_mask;

static void apex_cli_restore_argv_options(apex_options *opts,
                                          const apex_options *snap,
                                          const apex_cli_option_mask *m) {
    if (m->mode) {
        opts->mode = snap->mode;
        /* Global/project config may reset quarto defaults when mode is overridden via metadata. */
        if (snap->mode == APEX_MODE_QUARTO) {
            opts->enable_quarto_extensions = snap->enable_quarto_extensions;
            opts->enable_quarto_raw = snap->enable_quarto_raw;
            opts->enable_quarto_example_lists = snap->enable_quarto_example_lists;
            opts->enable_quarto_line_blocks = snap->enable_quarto_line_blocks;
            opts->enable_quarto_roman_lists = snap->enable_quarto_roman_lists;
            opts->enable_quarto_code_attrs = snap->enable_quarto_code_attrs;
            opts->enable_quarto_diagrams = snap->enable_quarto_diagrams;
            opts->enable_quarto_shortcodes = snap->enable_quarto_shortcodes;
            opts->enable_quarto_strict_lists = snap->enable_quarto_strict_lists;
            opts->enable_quarto_xrefs = snap->enable_quarto_xrefs;
            if (!m->enable_quarto_callouts) {
                opts->enable_quarto_callouts = snap->enable_quarto_callouts;
            }
        }
    }
    if (m->output_format) opts->output_format = snap->output_format;
    if (m->toc_min_max) {
        opts->toc_min = snap->toc_min;
        opts->toc_max = snap->toc_max;
    }
    if (m->xhtml) opts->xhtml = snap->xhtml;
    if (m->strict_xhtml) opts->strict_xhtml = snap->strict_xhtml;
    if (m->theme_name) opts->theme_name = snap->theme_name;
    if (m->enable_plugins) opts->enable_plugins = snap->enable_plugins;
    if (m->enable_tables) opts->enable_tables = snap->enable_tables;
    if (m->enable_footnotes) opts->enable_footnotes = snap->enable_footnotes;
    if (m->enable_smart_typography) opts->enable_smart_typography = snap->enable_smart_typography;
    if (m->enable_math) opts->enable_math = snap->enable_math;
    if (m->enable_file_includes) opts->enable_file_includes = snap->enable_file_includes;
    if (m->hardbreaks) opts->hardbreaks = snap->hardbreaks;
    if (m->standalone) opts->standalone = snap->standalone;
    if (m->embed_stylesheet) opts->embed_stylesheet = snap->embed_stylesheet;
    if (m->document_title) opts->document_title = snap->document_title;
    if (m->pretty) opts->pretty = snap->pretty;
    if (m->critic) {
        opts->enable_critic_markup = snap->enable_critic_markup;
        opts->critic_mode = snap->critic_mode;
    }
    if (m->id_format) opts->id_format = snap->id_format;
    if (m->generate_header_ids) opts->generate_header_ids = snap->generate_header_ids;
    if (m->header_anchors) opts->header_anchors = snap->header_anchors;
    if (m->relaxed_tables) opts->relaxed_tables = snap->relaxed_tables;
    if (m->per_cell_alignment) opts->per_cell_alignment = snap->per_cell_alignment;
    if (m->caption_position) opts->caption_position = snap->caption_position;
    if (m->code_highlighter) opts->code_highlighter = snap->code_highlighter;
    if (m->code_highlight_theme) opts->code_highlight_theme = snap->code_highlight_theme;
    if (m->code_line_numbers) opts->code_line_numbers = snap->code_line_numbers;
    if (m->highlight_language_only) opts->highlight_language_only = snap->highlight_language_only;
    if (m->allow_alpha_lists) opts->allow_alpha_lists = snap->allow_alpha_lists;
    if (m->allow_mixed_list_markers) opts->allow_mixed_list_markers = snap->allow_mixed_list_markers;
    if (m->unsafe) opts->unsafe = snap->unsafe;
    if (m->enable_sup_sub) opts->enable_sup_sub = snap->enable_sup_sub;
    if (m->enable_divs) opts->enable_divs = snap->enable_divs;
    if (m->enable_py_callouts) opts->enable_py_callouts = snap->enable_py_callouts;
    if (m->enable_quarto_callouts) opts->enable_quarto_callouts = snap->enable_quarto_callouts;
    if (m->enable_definition_lists) opts->enable_definition_lists = snap->enable_definition_lists;
    if (m->enable_spans) opts->enable_spans = snap->enable_spans;
    if (m->enable_grid_tables) opts->enable_grid_tables = snap->enable_grid_tables;
    if (m->enable_autolink) opts->enable_autolink = snap->enable_autolink;
    if (m->enable_strikethrough) opts->enable_strikethrough = snap->enable_strikethrough;
    if (m->obfuscate_emails) opts->obfuscate_emails = snap->obfuscate_emails;
    if (m->enable_aria) opts->enable_aria = snap->enable_aria;
    if (m->enable_wiki_links) opts->enable_wiki_links = snap->enable_wiki_links;
    if (m->enable_emoji) opts->enable_emoji = snap->enable_emoji;
    if (m->enable_emoji_autocorrect) opts->enable_emoji_autocorrect = snap->enable_emoji_autocorrect;
    if (m->enable_widont) opts->enable_widont = snap->enable_widont;
    if (m->code_is_poetry) {
        opts->code_is_poetry = snap->code_is_poetry;
        opts->highlight_language_only = snap->highlight_language_only;
    }
    if (m->enable_markdown_in_html) opts->enable_markdown_in_html = snap->enable_markdown_in_html;
    if (m->random_footnote_ids) opts->random_footnote_ids = snap->random_footnote_ids;
    if (m->enable_hashtags) opts->enable_hashtags = snap->enable_hashtags;
    if (m->style_hashtags) opts->style_hashtags = snap->style_hashtags;
    if (m->proofreader) {
        opts->proofreader_mode = snap->proofreader_mode;
        opts->enable_critic_markup = snap->enable_critic_markup;
        opts->critic_mode = snap->critic_mode;
    }
    if (m->hr_page_break) opts->hr_page_break = snap->hr_page_break;
    if (m->title_from_h1) opts->title_from_h1 = snap->title_from_h1;
    if (m->page_break_before_footnotes) opts->page_break_before_footnotes = snap->page_break_before_footnotes;
    if (m->wikilink_space) opts->wikilink_space = snap->wikilink_space;
    if (m->wikilink_extension) opts->wikilink_extension = snap->wikilink_extension;
    if (m->wikilink_sanitize) opts->wikilink_sanitize = snap->wikilink_sanitize;
    if (m->enable_metadata_transforms) opts->enable_metadata_transforms = snap->enable_metadata_transforms;
    if (m->embed_images) opts->embed_images = snap->embed_images;
    if (m->enable_image_captions) opts->enable_image_captions = snap->enable_image_captions;
    if (m->title_captions_only) {
        opts->title_captions_only = snap->title_captions_only;
        opts->enable_image_captions = snap->enable_image_captions;
    }
    if (m->base_directory) opts->base_directory = snap->base_directory;
    if (m->bibliography) {
        opts->bibliography_files = snap->bibliography_files;
        opts->enable_citations = snap->enable_citations;
    }
    if (m->csl_file) {
        opts->csl_file = snap->csl_file;
        opts->enable_citations = snap->enable_citations;
    }
    if (m->suppress_bibliography) opts->suppress_bibliography = snap->suppress_bibliography;
    if (m->link_citations) opts->link_citations = snap->link_citations;
    if (m->show_tooltips) opts->show_tooltips = snap->show_tooltips;
    if (m->indices) {
        opts->enable_indices = snap->enable_indices;
        opts->enable_mmark_index_syntax = snap->enable_mmark_index_syntax;
        opts->enable_textindex_syntax = snap->enable_textindex_syntax;
        opts->enable_leanpub_index_syntax = snap->enable_leanpub_index_syntax;
    }
    if (m->suppress_index) opts->suppress_index = snap->suppress_index;
    if (m->concordance) {
        opts->concordance_files = snap->concordance_files;
        opts->enable_indices = snap->enable_indices;
        opts->enable_textindex_syntax = snap->enable_textindex_syntax;
    }
    if (m->stylesheet) {
        opts->stylesheet_paths = snap->stylesheet_paths;
        opts->stylesheet_count = snap->stylesheet_count;
    }
}

#include "plugins_remote.h"
#include <apex/plugins.h>

/* ------------------------------------------------------------------------- */
/* Syntax highlighting theme listing                                         */
/* ------------------------------------------------------------------------- */

static int apex_cli_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    const char *cols_env = getenv("COLUMNS");
    if (cols_env && *cols_env) {
        long v = strtol(cols_env, NULL, 10);
        if (v > 0 && v < INT_MAX) {
            return (int)v;
        }
    }
    return 80;
}

static void apex_cli_print_theme_section(const char *title,
                                         const char *const *items,
                                         size_t count) {
    fprintf(stdout, "%s\n", title);
    fprintf(stdout, "----------------------------------------\n");
    if (count == 0) {
        fprintf(stdout, "(no themes)\n\n");
        return;
    }

    size_t max_len = 0;
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(items[i]);
        if (len > max_len) max_len = len;
    }

    int term_width = apex_cli_terminal_width();
    int col_width = (int)max_len + 2;
    if (col_width <= 0) col_width = 16;
    int max_cols = term_width > 0 ? term_width / col_width : 1;
    if (max_cols < 1) max_cols = 1;
    if (max_cols > 4) max_cols = 4;
    int cols = max_cols;
    int rows = (int)((count + (size_t)cols - 1) / (size_t)cols);
    if (rows < 1) rows = 1;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            size_t idx = (size_t)r + (size_t)rows * (size_t)c;
            if (idx >= count) {
                continue;
            }
            const char *name = items[idx];
            int pad = col_width - (int)strlen(name);
            if (pad < 1) pad = 1;
            fprintf(stdout, "%s", name);
            for (int p = 0; p < pad; p++) fputc(' ', stdout);
        }
        fputc('\n', stdout);
    }
    fputc('\n', stdout);
}

static void apex_cli_print_highlight_themes(void) {
    static const char *const pygments_themes[] = {
        "abap",
        "algol",
        "algol_nu",
        "arduino",
        "autumn",
        "bw",
        "borland",
        "coffee",
        "colorful",
        "default",
        "dracula",
        "emacs",
        "friendly_grayscale",
        "friendly",
        "fruity",
        "github-dark",
        "gruvbox-dark",
        "gruvbox-light",
        "igor",
        "inkpot",
        "lightbulb",
        "lilypond",
        "lovelace",
        "manni",
        "material",
        "monokai",
        "murphy",
        "native",
        "nord-darker",
        "nord",
        "one-dark",
        "paraiso-dark",
        "paraiso-light",
        "pastie",
        "perldoc",
        "rainbow_dash",
        "rrt",
        "sas",
        "solarized-dark",
        "solarized-light",
        "staroffice",
        "stata-dark",
        "stata-light",
        "tango",
        "trac",
        "vim",
        "vs",
        "xcode",
        "zenburn"
    };
    static const char *const skylighting_themes[] = {
        "kate",
        "breezeDark",
        "pygments",
        "espresso",
        "tango",
        "haddock",
        "monochrome",
        "zenburn"
    };
    static const char *const shiki_themes[] = {
        "Bundled themes: see https://shiki.style/themes",
        "Special theme: none (disables highlighting)"
    };

    apex_cli_print_theme_section("Pygments themes", pygments_themes,
                                 sizeof(pygments_themes) / sizeof(pygments_themes[0]));
    apex_cli_print_theme_section("Skylighting themes", skylighting_themes,
                                 sizeof(skylighting_themes) / sizeof(skylighting_themes[0]));
    apex_cli_print_theme_section("Shiki themes", shiki_themes,
                                 sizeof(shiki_themes) / sizeof(shiki_themes[0]));
}

/* ------------------------------------------------------------------------- */
/* Git helpers (mirrored from src/plugins.c for CLI-only use)               */
/*                                                                           */
/* Best-effort detection of the Git repository root for the current         */
/* working directory. Used to locate project-scoped `.apex/plugins`.        */
/* ------------------------------------------------------------------------- */
static char *apex_cli_git_toplevel(void) {
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

/* Determine global config path: $XDG_CONFIG_HOME/apex/config.yml
 * or ~/.config/apex/config.yml. Returns malloc'd string or NULL.
 */
static char *apex_cli_find_global_config(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char path[1024];
    const char *candidate = NULL;

    if (xdg && *xdg) {
        snprintf(path, sizeof(path), "%s/apex/config.yml", xdg);
        candidate = path;
    } else {
        const char *home = getenv("HOME");
        if (home && *home) {
            snprintf(path, sizeof(path), "%s/.config/apex/config.yml", home);
            candidate = path;
        }
    }

    if (!candidate) {
        return NULL;
    }

    FILE *fp = fopen(candidate, "r");
    if (!fp) {
        return NULL;
    }
    fclose(fp);

    size_t len = strlen(candidate);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, candidate, len + 1);
    return out;
}

/* Determine project-scoped config path:
 *   - CWD/.apex/config.yml
 *   - base_directory/.apex/config.yml (if set)
 *   - <git repo root>/.apex/config.yml (if inside work tree, and different from base_directory)
 * The first existing file in this order wins. Returns malloc'd string or NULL.
 */
static char *apex_cli_find_project_config(const apex_options *options) {
    char cwd[1024];
    cwd[0] = '\0';

    /* 1. CWD/.apex/config.yml */
    if (getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0') {
        char path[1200];
        snprintf(path, sizeof(path), "%s/.apex/config.yml", cwd);
        FILE *fp = fopen(path, "r");
        if (fp) {
            fclose(fp);
            size_t len = strlen(path);
            char *out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, path, len + 1);
            return out;
        }
    }

    /* 2. base_directory/.apex/config.yml */
    if (options && options->base_directory && options->base_directory[0] != '\0') {
        char path[1200];
        snprintf(path, sizeof(path), "%s/.apex/config.yml", options->base_directory);
        FILE *fp = fopen(path, "r");
        if (fp) {
            fclose(fp);
            size_t len = strlen(path);
            char *out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, path, len + 1);
            return out;
        }
    }

    /* 3. <git repo root>/.apex/config.yml, if CWD is inside the work tree */
    char *git_root = apex_cli_git_toplevel();
    if (git_root && git_root[0] != '\0' && cwd[0] != '\0') {
        size_t root_len = strlen(git_root);
        if (strncmp(cwd, git_root, root_len) == 0 &&
            (cwd[root_len] == '/' || cwd[root_len] == '\0')) {
            /* Avoid duplicating base_directory if it matches git_root */
            if (!options || !options->base_directory ||
                strcmp(git_root, options->base_directory) != 0) {
                char path[1200];
                snprintf(path, sizeof(path), "%s/.apex/config.yml", git_root);
                FILE *fp = fopen(path, "r");
                if (fp) {
                    fclose(fp);
                    size_t len = strlen(path);
                    char *out = malloc(len + 1);
                    if (!out) {
                        free(git_root);
                        return NULL;
                    }
                    memcpy(out, path, len + 1);
                    free(git_root);
                    return out;
                }
            }
        }
    }
    if (git_root) {
        free(git_root);
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Local helpers for listing installed plugins                               */
/*                                                                           */
/* These mirror the discovery rules in src/plugins.c so that                 */
/* `--list-plugins` reports the same set of plugins that would actually run  */
/* when `--plugins` is enabled:                                              */
/*   - Project-local:                                                        */
/*       - CWD/.apex/plugins                                                 */
/*       - base_directory/.apex/plugins (if set)                             */
/*       - <git repo root>/.apex/plugins (if inside a Git work tree)        */
/*   - User-global:                                                          */
/*       - $XDG_CONFIG_HOME/apex/plugins OR ~/.config/apex/plugins           */
/*                                                                           */
/* When the same plugin id exists in multiple locations, the first location  */
/* in the search order wins. This matches runtime behavior, where later      */
/* directories are skipped if the id is already loaded.                      */
/* ------------------------------------------------------------------------- */

typedef struct cli_installed_plugin {
    char *id;
    char *title;
    char *author;
    char *description;
    char *homepage;
    struct cli_installed_plugin *next;
} cli_installed_plugin;

static int cli_installed_plugin_exists(cli_installed_plugin *head, const char *id) {
    if (!id) return 0;
    for (cli_installed_plugin *p = head; p; p = p->next) {
        if (p->id && strcmp(p->id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static void cli_free_installed_plugins(cli_installed_plugin *head) {
    while (head) {
        cli_installed_plugin *next = head->next;
        free(head->id);
        free(head->title);
        free(head->author);
        free(head->description);
        free(head->homepage);
        free(head);
        head = next;
    }
}

static void cli_collect_installed_from_root(const char *root,
                                            cli_installed_plugin **head,
                                            char ***installed_ids,
                                            size_t *installed_count,
                                            size_t *installed_cap) {
    if (!root || !*root || !head || !installed_ids || !installed_count || !installed_cap) {
        return;
    }

    DIR *d = opendir(root);
    if (!d) {
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char plugin_dir[1200];
        snprintf(plugin_dir, sizeof(plugin_dir), "%s/%s", root, ent->d_name);
        struct stat st2;
        if (stat(plugin_dir, &st2) != 0 || !S_ISDIR(st2.st_mode)) {
            continue;
        }

        /* Look for plugin.yml or plugin.yaml */
        char manifest[1300];
        snprintf(manifest, sizeof(manifest), "%s/plugin.yml", plugin_dir);
        FILE *test = fopen(manifest, "r");
        if (!test) {
            snprintf(manifest, sizeof(manifest), "%s/plugin.yaml", plugin_dir);
            test = fopen(manifest, "r");
        }
        if (!test) {
            continue;
        }
        fclose(test);

        apex_metadata_item *meta = apex_load_metadata_from_file(manifest);
        if (!meta) continue;

        const char *id = NULL;
        const char *title = NULL;
        const char *author = NULL;
        const char *description = NULL;
        const char *homepage = NULL;

        for (apex_metadata_item *m = meta; m; m = m->next) {
            if (strcmp(m->key, "id") == 0) id = m->value;
            else if (strcmp(m->key, "title") == 0) title = m->value;
            else if (strcmp(m->key, "author") == 0) author = m->value;
            else if (strcmp(m->key, "description") == 0) description = m->value;
            else if (strcmp(m->key, "homepage") == 0) homepage = m->value;
        }

        const char *final_id = id ? id : ent->d_name;
        if (!final_id || cli_installed_plugin_exists(*head, final_id)) {
            apex_free_metadata(meta);
            continue;
        }

        /* Grow installed_ids array as needed */
        if (*installed_count == *installed_cap) {
            size_t new_cap = *installed_cap ? (*installed_cap * 2) : 8;
            char **tmp = realloc(*installed_ids, new_cap * sizeof(char *));
            if (!tmp) {
                apex_free_metadata(meta);
                break;
            }
            *installed_ids = tmp;
            *installed_cap = new_cap;
        }

        (*installed_ids)[*installed_count] = strdup(final_id);
        if (!(*installed_ids)[*installed_count]) {
            apex_free_metadata(meta);
            break;
        }
        (*installed_count)++;

        cli_installed_plugin *node = calloc(1, sizeof(cli_installed_plugin));
        if (!node) {
            apex_free_metadata(meta);
            break;
        }
        node->id = strdup(final_id);
        node->title = title ? strdup(title) : NULL;
        node->author = author ? strdup(author) : NULL;
        node->description = description ? strdup(description) : NULL;
        node->homepage = homepage ? strdup(homepage) : NULL;
        node->next = NULL;

        /* Append to end to preserve discovery order */
        if (!*head) {
            *head = node;
        } else {
            cli_installed_plugin *tail = *head;
            while (tail->next) tail = tail->next;
            tail->next = node;
        }

        apex_free_metadata(meta);
    }

    closedir(d);
}

/**
 * Merge document metadata from one or more files (later files override).
 * Sets *out to merged metadata (may be NULL if no blocks found in any file).
 * Returns 0 on success, non-zero if a file could not be read.
 */
static int apex_cli_merge_doc_metadata_from_files(apex_mode_t mode,
                                                  char **paths,
                                                  size_t n,
                                                  apex_metadata_item **out) {
    apex_metadata_item *acc = NULL;
    *out = NULL;
    for (size_t i = 0; i < n; i++) {
        if (!paths || !paths[i]) continue;
        size_t len = 0;
        char *raw = read_file(paths[i], &len);
        if (!raw) {
            apex_free_metadata(acc);
            return 1;
        }
        char *copy = malloc(len + 1);
        if (!copy) {
            free(raw);
            apex_free_metadata(acc);
            return 1;
        }
        memcpy(copy, raw, len + 1);
        free(raw);
        char *ptr = copy;
        apex_metadata_item *chunk = apex_extract_metadata_for_mode(&ptr, mode);
        free(copy);
        if (!chunk) continue;
        apex_metadata_item *merged = apex_merge_metadata(acc, chunk, NULL);
        if (acc) apex_free_metadata(acc);
        apex_free_metadata(chunk);
        acc = merged;
    }
    *out = acc;
    return 0;
}

/**
 * Print version, merged config (global + project + --meta-file + --meta), and plugin resolution.
 */
static void apex_cli_print_info(FILE *out,
                                const apex_options *options,
                                bool plugins_cli_override,
                                bool plugins_cli_value,
                                const char *meta_file,
                                apex_metadata_item *cmdline_metadata) {
    fprintf(out, "version: %s\n\n", apex_version_string());

    char *global_config_path = apex_cli_find_global_config();
    char *project_config_path = apex_cli_find_project_config(options);

    apex_metadata_item *global_config_meta = NULL;
    apex_metadata_item *project_config_meta = NULL;
    apex_metadata_item *explicit_file_meta = NULL;

    if (global_config_path) {
        global_config_meta = apex_load_metadata_from_file(global_config_path);
    }
    if (project_config_path) {
        project_config_meta = apex_load_metadata_from_file(project_config_path);
    }
    if (meta_file) {
        explicit_file_meta = apex_load_metadata_from_file(meta_file);
    }

    apex_metadata_item *merged_config = NULL;
    if (global_config_meta || project_config_meta || explicit_file_meta || cmdline_metadata) {
        apex_metadata_item *lists[] = {
            global_config_meta, project_config_meta, explicit_file_meta, cmdline_metadata
        };
        merged_config = apex_merge_metadata_lists(lists, sizeof(lists) / sizeof(lists[0]));
    }

    if (global_config_meta) apex_free_metadata(global_config_meta);
    if (project_config_meta) apex_free_metadata(project_config_meta);
    if (explicit_file_meta) apex_free_metadata(explicit_file_meta);
    if (global_config_path) free(global_config_path);
    if (project_config_path) free(project_config_path);

    fprintf(out, "---\n");
    if (merged_config) {
        apex_metadata_fprint_yaml_mapping(out, merged_config);
    }
    fprintf(out, "---\n\n");

    apex_options eff = *options;
    if (merged_config) {
        apex_apply_metadata_to_options(merged_config, &eff);
    }
    if (plugins_cli_override) {
        eff.enable_plugins = plugins_cli_value;
    }
    apex_free_metadata(merged_config);

    if (!eff.enable_plugins) {
        fprintf(out, "plugins:\n  enabled: false\n");
        return;
    }

    fprintf(out, "plugins:\n  enabled: true\n  ids:\n");

    cli_installed_plugin *installed_head = NULL;
    char **installed_ids = NULL;
    size_t installed_count = 0;
    size_t installed_cap = 0;

    char cwd[1024];
    cwd[0] = '\0';
    if (getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0') {
        char cwd_plugins[1200];
        snprintf(cwd_plugins, sizeof(cwd_plugins), "%s/.apex/plugins", cwd);
        cli_collect_installed_from_root(cwd_plugins,
                                        &installed_head,
                                        &installed_ids,
                                        &installed_count,
                                        &installed_cap);
    }

    if (options->base_directory && options->base_directory[0] != '\0') {
        char base_plugins[1200];
        snprintf(base_plugins, sizeof(base_plugins), "%s/.apex/plugins", options->base_directory);
        cli_collect_installed_from_root(base_plugins,
                                        &installed_head,
                                        &installed_ids,
                                        &installed_count,
                                        &installed_cap);
    }

    char *git_root = apex_cli_git_toplevel();
    if (git_root && git_root[0] != '\0' && cwd[0] != '\0') {
        size_t root_len = strlen(git_root);
        if (strncmp(cwd, git_root, root_len) == 0 &&
            (cwd[root_len] == '/' || cwd[root_len] == '\0')) {
            char git_plugins[1200];
            snprintf(git_plugins, sizeof(git_plugins), "%s/.apex/plugins", git_root);
            cli_collect_installed_from_root(git_plugins,
                                            &installed_head,
                                            &installed_ids,
                                            &installed_count,
                                            &installed_cap);
        }
        free(git_root);
    }

    const char *xdg = getenv("XDG_CONFIG_HOME");
    char root[1024];
    root[0] = '\0';
    if (xdg && *xdg) {
        snprintf(root, sizeof(root), "%s/apex/plugins", xdg);
    } else {
        const char *home = getenv("HOME");
        if (home && *home) {
            snprintf(root, sizeof(root), "%s/.config/apex/plugins", home);
        }
    }
    if (root[0] != '\0') {
        cli_collect_installed_from_root(root,
                                        &installed_head,
                                        &installed_ids,
                                        &installed_count,
                                        &installed_cap);
    }

    if (installed_ids) {
        for (size_t i = 0; i < installed_count; i++) {
            free(installed_ids[i]);
        }
        free(installed_ids);
    }

    if (!installed_head) {
        fprintf(out, "    # (none installed)\n");
    } else {
        for (cli_installed_plugin *p = installed_head; p; p = p->next) {
            const char *pid = p->id ? p->id : "";
            fprintf(out, "    - %s\n", pid);
        }
    }
    cli_free_installed_plugins(installed_head);
}

/* Profiling helpers (same as in apex.c) */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static bool profiling_enabled(void) {
    const char *env = getenv("APEX_PROFILE");
    return env && (strcmp(env, "1") == 0 || strcmp(env, "yes") == 0 || strcmp(env, "true") == 0);
}

#define PROFILE_START(name) \
    double name##_start = 0; \
    if (profiling_enabled()) { name##_start = get_time_ms(); }

#define PROFILE_END(name) \
    if (profiling_enabled()) { \
        double name##_elapsed = get_time_ms() - name##_start; \
        fprintf(stderr, "[PROFILE] %-30s: %8.2f ms\n", #name, name##_elapsed); \
    }

#define BUFFER_SIZE 4096

/* Progress reporting state */
static bool progress_enabled = false;
static bool is_tty = false;
static double progress_start_time = 0.0;
static bool progress_shown = false;  /* Track if we've shown any progress yet */
static const char *last_stage = NULL;  /* Remember last stage in case we need to show it later */

/* Initialize progress reporting */
static void init_progress(void) {
    const char *env = getenv("APEX_PROGRESS");
    is_tty = isatty(STDERR_FILENO);

    if (env && (strcmp(env, "1") == 0 || strcmp(env, "yes") == 0 || strcmp(env, "true") == 0)) {
        progress_enabled = true;
    } else if (env && (strcmp(env, "0") == 0 || strcmp(env, "no") == 0 || strcmp(env, "false") == 0)) {
        progress_enabled = false;
    } else {
        /* Default: enable if stderr is a TTY */
        progress_enabled = is_tty;
    }

    /* Initialize start time for delay check */
    progress_start_time = get_time_ms();
    progress_shown = false;
    last_stage = NULL;
}

/* Progress callback function */
static void progress_callback(const char *stage, int percent, void *user_data) {
    (void)user_data;  /* Unused for now */

    if (!progress_enabled) return;

    /* Remember the last stage (unless stage is NULL, which means "refresh last stage") */
    if (stage) {
        last_stage = stage;
    }

    /* Check elapsed time */
    double elapsed = get_time_ms() - progress_start_time;

    /* If less than 1 second has elapsed and we haven't shown progress yet, just remember the stage */
    if (elapsed < 1000.0 && !progress_shown) {
        return;  /* Too soon, don't show yet - but remember the stage */
    }

    /* Once 1 second has passed, show progress (even if it's the same stage or NULL for refresh) */
    if (elapsed >= 1000.0) {
        progress_shown = true;
        const char *display_stage = stage ? stage : (last_stage ? last_stage : "Processing");
        if (percent >= 0) {
            fprintf(stderr, "\rProcessing: %s %3d%%", display_stage, percent);
        } else {
            fprintf(stderr, "\rProcessing: %s...", display_stage);
        }
        fflush(stderr);
    }
}

/* Force show progress if enough time has elapsed (called periodically) */
static void update_progress_if_needed(void) {
    if (!progress_enabled || !last_stage) return;

    double elapsed = get_time_ms() - progress_start_time;
    if (elapsed >= 1000.0) {
        /* 1 second has passed - show progress if we haven't yet, or refresh it */
        if (!progress_shown) {
            progress_shown = true;
        }
        fprintf(stderr, "\rProcessing: %s...", last_stage);
        fflush(stderr);
    }
}

/* Check if we should show delayed progress (called after processing completes) */
static void check_delayed_progress(void) {
    if (!progress_enabled || progress_shown || !last_stage) return;

    double elapsed = get_time_ms() - progress_start_time;
    if (elapsed >= 1000.0) {
        /* 1 second has passed, show the last stage we were processing */
        progress_shown = true;
        fprintf(stderr, "\rProcessing: %s...", last_stage);
        fflush(stderr);
    }
}

/* Clear progress line */
static void clear_progress(void) {
    if (progress_enabled && progress_shown) {
        /* Only clear if we actually showed progress */
        fprintf(stderr, "\r%*s\r", 80, "");  /* Clear line with spaces */
        fflush(stderr);
    }
}

static void print_usage(const char *program_name) {
    fprintf(stderr, "Apex Markdown Processor v%s\n", apex_version_string());
    fprintf(stderr, "One Markdown processor to rule them all\n\n");
    fprintf(stderr, "Project homepage: https://github.com/ApexMarkdown/apex\n\n");
    fprintf(stderr, "Usage: %s [options] [file]\n", program_name);
    fprintf(stderr, "       %s --combine [files...]\n", program_name);
    fprintf(stderr, "       %s --mmd-merge [index files...]\n\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --accept               Accept all Critic Markup changes (apply edits)\n");
    fprintf(stderr, "  --[no-]alpha-lists     Support alpha list markers (a., b., c. and A., B., C.)\n");
    fprintf(stderr, "  --[no-]autolink        Enable autolinking of URLs and email addresses\n");
    fprintf(stderr, "  --base-dir DIR         Base directory for resolving relative paths (for images, includes, wiki links)\n");
    fprintf(stderr, "  --bibliography FILE     Bibliography file (BibTeX, CSL JSON, or CSL YAML) - can be used multiple times\n");
    fprintf(stderr, "  --captions POSITION    Table caption position: above or below (default: below)\n");
    fprintf(stderr, "  --code-highlight TOOL  Use external tool for syntax highlighting (pygments, skylighting, shiki, or abbreviations p, s, sh)\n");
    fprintf(stderr, "  --code-highlight-theme THEME  Theme/style name for external syntax highlighters (tool-specific)\n");
    fprintf(stderr, "  --list-themes          List available syntax highlighting themes for pygments, skylighting, and Shiki\n");
    fprintf(stderr, "  --code-line-numbers    Include line numbers in syntax-highlighted code blocks (requires --code-highlight)\n");
    fprintf(stderr, "  --highlight-language-only  Only highlight code blocks that have a language specified (requires --code-highlight)\n");
    fprintf(stderr, "  --combine              Concatenate Markdown files (expanding includes) into a single Markdown stream\n");
    fprintf(stderr, "                         When a SUMMARY.md file is provided, treat it as a GitBook index and combine\n");
    fprintf(stderr, "                         the linked files in order. Output is raw Markdown suitable for piping back into Apex.\n");
    fprintf(stderr, "  --concordance FILE      TextIndex concordance TSV file (can be used multiple times; enables indices)\n");
    fprintf(stderr, "  --csl FILE              Citation style file (CSL format)\n");
    fprintf(stderr, "  --css FILE, --style FILE  Link to CSS file(s) in document head. With HTML: requires -s/--standalone.\n");
    fprintf(stderr, "                         With -t man-html -s: include custom CSS in the man page. Can be used multiple times or comma-separated (e.g., --css style.css)\n");
    fprintf(stderr, "  --embed-css            Embed CSS file contents into a <style> tag in the document head (used with --css)\n");
    fprintf(stderr, "  --embed-images         Embed local images as base64 data URLs in HTML output\n");
    fprintf(stderr, "  --[no-]image-captions  Wrap images with title or alt text in <figure>/<figcaption> (default: on in unified/mmd)\n");
    fprintf(stderr, "  --[no-]title-captions-only  Only add captions for images with title; alt-only images get no caption\n");
    fprintf(stderr, "  --hardbreaks           Treat newlines as hard breaks\n");
    fprintf(stderr, "  --header-anchors        Generate <a> anchor tags instead of header IDs\n");
    fprintf(stderr, "  -h, --help             Show this help message\n");
    fprintf(stderr, "  -i, --info             Show version, merged config (YAML), and plugin ids; to stdout without files, to stderr when processing files\n");
    fprintf(stderr, "  --extract-meta         Print merged document metadata from input file(s) as YAML and exit\n");
    fprintf(stderr, "  -e, --extract-meta-value KEY  Print one metadata value for KEY and exit (uses merged metadata from files, last wins)\n");
    fprintf(stderr, "  --id-format FORMAT      Header ID format: gfm (default), mmd, or kramdown\n");
    fprintf(stderr, "                          (modes auto-set format; use this to override in unified mode)\n");
    fprintf(stderr, "  --[no-]includes        Enable file inclusion (enabled by default in unified mode)\n");
    fprintf(stderr, "  --indices               Enable index processing (mmark, TextIndex, and Leanpub syntax)\n");
    fprintf(stderr, "  --install-plugin ID    Install plugin by id from directory, or by Git URL/GitHub shorthand (user/repo)\n");
    fprintf(stderr, "  --list-filters         List installed filters and available filters from the remote directory\n");
    fprintf(stderr, "  --install-filter ID    Install AST filter by id from the central filters directory or by Git URL/GitHub shorthand\n");
    fprintf(stderr, "  --uninstall-filter ID  Uninstall filter by id\n");
    fprintf(stderr, "  --filter NAME          Run a single AST filter from ~/.config/apex/filters/NAME (Pandoc-style JSON filter)\n");
    fprintf(stderr, "  --filters              Run all executable filters in ~/.config/apex/filters (sorted by name)\n");
    fprintf(stderr, "  --lua-filter FILE      Run a Lua script as an AST filter via 'lua FILE' (Pandoc-style JSON filter)\n");
    fprintf(stderr, "  --no-strict-filters    Do not abort on AST filter errors/invalid JSON; skip failing filters instead\n");
    fprintf(stderr, "  --link-citations       Link citations to bibliography entries\n");
    fprintf(stderr, "  --list-plugins         List installed plugins and available plugins from the remote directory\n");
    fprintf(stderr, "  --uninstall-plugin ID  Uninstall plugin by id\n");
    fprintf(stderr, "  --meta KEY=VALUE       Set metadata key-value pair (can be used multiple times, supports quotes and comma-separated pairs)\n");
    fprintf(stderr, "  --meta-file FILE       Load metadata from external file (YAML, MMD, or Pandoc format)\n");
    fprintf(stderr, "  --[no-]mixed-lists     Allow mixed list markers at same level (inherit type from first item)\n");
    fprintf(stderr, "  --mmd-merge            Merge files from one or more mmd_merge-style index files into a single Markdown stream\n");
    fprintf(stderr, "                         Index files list document parts line-by-line; indentation controls header level shifting.\n");
    fprintf(stderr, "  -m, --mode MODE        Processor mode: commonmark, gfm, mmd, kramdown, unified, quarto (default)\n");
    fprintf(stderr, "  -t, --to FORMAT        Output format: html (default), xhtml (alias for html + --xhtml), strict-xhtml (alias for html + --strict-xhtml), json (before filters), json-filtered/ast-json/ast (after filters), markdown/md, mmd, commonmark/cmark, kramdown, gfm, terminal/cli, terminal256, man, man-html, toc, rtf\n");
    fprintf(stderr, "  --no-bibliography       Suppress bibliography output\n");
    fprintf(stderr, "  --no-footnotes         Disable footnote support\n");
    fprintf(stderr, "  --no-ids                Disable automatic header ID generation\n");
    fprintf(stderr, "  --no-indices            Disable index processing\n");
    fprintf(stderr, "  --no-index              Suppress index generation (markers still created)\n");
    fprintf(stderr, "  --no-math              Disable math support\n");
    fprintf(stderr, "  --aria                  Add ARIA labels and accessibility attributes to HTML output\n");
    fprintf(stderr, "  --no-plugins            Disable external/plugin processing\n");
    fprintf(stderr, "  --no-relaxed-tables    Disable relaxed table parsing\n");
    fprintf(stderr, "  --no-smart             Disable smart typography\n");
    fprintf(stderr, "  --no-sup-sub           Disable superscript/subscript syntax\n");
    fprintf(stderr, "  --[no-]divs            Enable or disable Pandoc fenced divs (Unified mode only)\n");
    fprintf(stderr, "  --[no-]py-callouts     Enable or disable Python-Markdown !!! callout syntax (default: disabled)\n");
    fprintf(stderr, "  --[no-]quarto-callouts Enable or disable Quarto ::: {.callout-*} syntax (default: disabled)\n");
    fprintf(stderr, "  --[no-]one-line-definitions  Enable or disable one-line definition lists (Term :: Definition)\n");
    fprintf(stderr, "  --[no-]spans           Enable or disable bracketed spans [text]{IAL} (Pandoc-style, enabled by default in unified mode)\n");
    fprintf(stderr, "  --no-tables            Disable table support\n");
    fprintf(stderr, "  --no-transforms        Disable metadata variable transforms\n");
    fprintf(stderr, "  --no-unsafe            Disable raw HTML in output\n");
    fprintf(stderr, "  --no-wikilinks         Disable wiki link syntax\n");
    fprintf(stderr, "  --[no-]emoji            Enable/disable :name: emoji replacement (enabled by default in gfm/unified)\n");
    fprintf(stderr, "  --[no-]emoji-autocorrect  Enable/disable emoji name autocorrect (enabled by default in unified mode)\n");
    fprintf(stderr, "  --obfuscate-emails     Obfuscate email links/text using HTML entities\n");
    fprintf(stderr, "  -o, --output FILE      Write output to FILE instead of stdout\n");
    fprintf(stderr, "  --[no-]progress          Show progress indicator during processing (enabled by default for TTY)\n");
    fprintf(stderr, "  --plugins              Enable external/plugin processing\n");
    fprintf(stderr, "  --pretty               Pretty-print HTML with indentation and whitespace\n");
    fprintf(stderr, "  --xhtml                HTML5 output with self-closing void tags (<br />, <meta ... />). Same as -t xhtml.\n");
    fprintf(stderr, "  --strict-xhtml         Polyglot XHTML/XML for parsers (xmlns, application/xhtml+xml meta; implies --xhtml). Mutually exclusive with --xhtml. Same as -t strict-xhtml.\n");
    fprintf(stderr, "  --reject               Reject all Critic Markup changes (revert edits)\n");
    fprintf(stderr, "  --[no-]relaxed-tables  Enable or disable relaxed table parsing (no separator rows required)\n");
    fprintf(stderr, "  --[no-]grid-tables     Enable or disable Pandoc grid table syntax (+---+ borders; disabled by default)\n");
    fprintf(stderr, "  --[no-]per-cell-alignment  Enable or disable per-cell alignment markers (colons at start/end of cells, enabled by default in unified mode)\n");
    fprintf(stderr, "  --script VALUE         Inject <script> tags before </body> (standalone) or at end of HTML (snippet).\n");
    fprintf(stderr, "                          VALUE can be a path, URL, or shorthand (mermaid, mathjax, katex). Can be used multiple times or as a comma-separated list.\n");
    fprintf(stderr, "  --show-tooltips         Show tooltips on citations\n");
    fprintf(stderr, "  -s, --standalone       Generate complete HTML document (with <html>, <head>, <body>). For -t man-html, -s adds nav sidebar and full page; without -s, output is snippet only.\n");
    fprintf(stderr, "  --[no-]sup-sub         Enable or disable MultiMarkdown-style superscript (^text^) and subscript (~text~) syntax\n");
    fprintf(stderr, "  --[no-]strikethrough   Enable or disable GFM-style ~~strikethrough~~ processing\n");
    fprintf(stderr, "  --title TITLE          Document title (requires --standalone, default: \"Document\")\n");
    fprintf(stderr, "  --toc-min-max MIN,MAX  TOC heading depth (default 1,3) for -t toc and bare HTML TOC markers; marker ranges still override\n");
    fprintf(stderr, "  --[no-]transforms      Enable or disable metadata variable transforms [%%key:transform]\n");
    fprintf(stderr, "  --[no-]unsafe          Allow or disallow raw HTML in output\n");
    fprintf(stderr, "  --widont               Prevent short widows in headings by inserting non-breaking spaces between trailing words\n");
    fprintf(stderr, "  --code-is-poetry       Treat unlanguaged code blocks as poetry (adds 'poetry' class, implies --highlight-language-only)\n");
    fprintf(stderr, "  --[no-]markdown-in-html  Enable or disable markdown processing inside HTML blocks with markdown attributes\n");
    fprintf(stderr, "  --random-footnote-ids  Use hash-based footnote IDs to avoid collisions when combining documents\n");
    fprintf(stderr, "  --hashtags             Convert #tags into span-wrapped hashtags\n");
    fprintf(stderr, "  --style-hashtags       Use 'mkstyledtag' class instead of 'mkhashtag' for hashtags\n");
    fprintf(stderr, "  --proofreader          Treat ==highlight== and ~~delete~~ as CriticMarkup highlight/deletion\n");
    fprintf(stderr, "  --hr-page-break        Replace <hr> elements with Marked-style page break divs\n");
    fprintf(stderr, "  --title-from-h1        Use the first H1 as the document title when none is specified\n");
    fprintf(stderr, "  --page-break-before-footnotes  Insert a page break before the footnotes section\n");
    fprintf(stderr, "  -v, --version          Show version information\n");
    fprintf(stderr, "  --[no-]wikilinks       Enable or disable wiki link syntax [[PageName]]\n");
    fprintf(stderr, "  --wikilink-space MODE  Space replacement for wiki links: dash, none, underscore, space (default: dash)\n");
    fprintf(stderr, "  --wikilink-extension EXT  File extension to append to wiki links (e.g., html, md)\n");
    fprintf(stderr, "  --[no-]wikilink-sanitize  Sanitize wiki link URLs (lowercase, remove apostrophes, etc.)\n");
    fprintf(stderr, "  --theme NAME            Terminal theme name for -t terminal/terminal256 (from ~/.config/apex/terminal/themes/NAME.theme)\n");
    fprintf(stderr, "  --width N               Hard-wrap terminal/terminal256 output at N visible columns\n");
    fprintf(stderr, "  --no-terminal-images    Do not render local images via imgcat/chafa/viu/catimg on terminal output\n");
    fprintf(stderr, "  --terminal-image-width N  Max width/cells for terminal image tools (default: 50)\n");
    fprintf(stderr, "  -p, --paginate          Page terminal/cli/terminal256 output through a pager (APEX_PAGER, then PAGER, then less -R)\n");
    fprintf(stderr, "  --paginate-symbols      Page output and render images as chafa ANSI art (compatible with less -R)\n");
    fprintf(stderr, "  --no-paginate           Do not page terminal output (overrides -p and paginate: true in config/metadata)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If no file is specified, reads from stdin.\n");
}

/* ANSI art logo - 13 lines, 30 characters display width */
/* Uses 256-color ANSI escapes with transparent background (black bg stripped) */
static const char *logo_ansi[] = {
    "              \x1b[38;5;43;48;5;144m\xb1\x1b[38;5;80;48;5;246m\xb0\x1b[0m              ",
    "               \x1b[38;5;172;48;5;223m \x1b[38;5;208;48;5;216m \x1b[38;5;215;48;5;216m \x1b[38;5;166;48;5;222m \x1b[38;5;216;48;5;223m \x1b[38;5;86;48;5;144m\xb1\x1b[38;5;1;48;5;232m\xb0\x1b[0m        ",
    "               \x1b[38;5;208;48;5;223m \x1b[38;5;204;48;5;210m \x1b[38;5;204;48;5;210m \x1b[38;5;204;48;5;210m \x1b[38;5;211;48;5;211m \x1b[38;5;125;48;5;169m\xb0\x1b[38;5;200;48;5;169m\xb1\x1b[38;5;165;48;5;133m\xb1\x1b[38;5;171;48;5;134m\xb1\x1b[38;5;92;48;5;134m\xb1\x1b[38;5;54;48;5;66m\xb2\x1b[0m    ",
    "               \x1b[38;5;202;48;5;223m\xb0\x1b[0m   \x1b[38;5;77;48;5;53m\xb1\x1b[38;5;199;48;5;199m \x1b[38;5;206;48;5;200m \x1b[38;5;170;48;5;164m\xb0\x1b[38;5;129;48;5;92m\xb0\x1b[38;5;57;48;5;56m\xb0\x1b[38;5;244;48;5;244m\xb2\x1b[0m    ",
    "               \x1b[38;5;209;48;5;217m\xb0\x1b[0m              ",
    "             \x1b[38;5;130;48;5;239m\xb2\x1b[38;5;167;48;5;181m\xb1\x1b[38;5;204;48;5;217m\xb0\x1b[38;5;57;48;5;138m\xb0\x1b[0m             ",
    "            \x1b[38;5;144;48;5;238m\xb2\x1b[38;5;204;48;5;175m\xb1\x1b[38;5;204;48;5;211m\xb0\x1b[38;5;204;48;5;211m\xb0\x1b[38;5;197;48;5;175m\xb1\x1b[38;5;161;48;5;175m\xb1\x1b[38;5;94;48;5;240m\xb2\x1b[0m           ",
    "           \x1b[38;5;119;48;5;236m\xb1\x1b[38;5;204;48;5;211m\xb0\x1b[38;5;168;48;5;211m\xb0\x1b[38;5;161;48;5;175m\xb1\x1b[38;5;209;48;5;233m\xb0\x1b[38;5;162;48;5;175m\xb1\x1b[38;5;169;48;5;175m\xb1\x1b[38;5;206;48;5;133m\xb1\x1b[38;5;166;48;5;237m\xb1\x1b[0m          ",
    "      \x1b[38;5;119;48;5;131m\xb1\x1b[38;5;191;48;5;235m\xb1\x1b[0m \x1b[38;5;1;48;5;16m\xb0\x1b[38;5;84;48;5;95m\xb0\x1b[38;5;197;48;5;205m \x1b[38;5;161;48;5;205m \x1b[38;5;161;48;5;205m \x1b[38;5;84;48;5;95m\xb0\x1b[0m \x1b[38;5;70;48;5;95m\xb1\x1b[38;5;206;48;5;169m\xb1\x1b[38;5;200;48;5;169m\xb1\x1b[38;5;133;48;5;133m\xb1\x1b[38;5;94;48;5;237m\xb1\x1b[0m \x1b[38;5;209;48;5;235m\xb0\x1b[38;5;84;48;5;60m\xb1\x1b[0m      ",
    "     \x1b[38;5;84;48;5;125m\xb0\x1b[38;5;211;48;5;204m \x1b[38;5;168;48;5;168m\xb0\x1b[38;5;197;48;5;204m \x1b[38;5;197;48;5;204m\xb0\x1b[38;5;161;48;5;205m \x1b[38;5;205;48;5;205m \x1b[38;5;205;48;5;205m \x1b[38;5;35;48;5;168m\xb0\x1b[38;5;1;48;5;16m\xb0\x1b[38;5;185;48;5;89m\xb1\x1b[38;5;77;48;5;53m\xb1\x1b[38;5;206;48;5;89m\xb1\x1b[38;5;126;48;5;169m\xb0\x1b[38;5;200;48;5;169m\xb1\x1b[38;5;176;48;5;133m\xb1\x1b[38;5;50;48;5;97m\xb1\x1b[38;5;129;48;5;134m\xb1\x1b[38;5;92;48;5;91m\xb1\x1b[38;5;55;48;5;97m\xb1\x1b[38;5;209;48;5;235m\xb0\x1b[0m    ",
    "   \x1b[38;5;214;48;5;52m\xb0\x1b[38;5;99;48;5;161m\xb0\x1b[38;5;197;48;5;204m\xb0\x1b[38;5;216;48;5;233m\xb0\x1b[38;5;82;48;5;168m\xb0\x1b[38;5;205;48;5;205m \x1b[38;5;198;48;5;205m \x1b[38;5;125;48;5;205m \x1b[38;5;125;48;5;205m \x1b[38;5;198;48;5;198m\xb0\x1b[38;5;166;48;5;234m\xb0\x1b[0m \x1b[38;5;212;48;5;205m \x1b[38;5;212;48;5;205m \x1b[38;5;199;48;5;205m \x1b[38;5;206;48;5;205m\xb0\x1b[38;5;126;48;5;200m\xb0\x1b[38;5;200;48;5;164m\xb0\x1b[38;5;170;48;5;164m\xb1\x1b[38;5;211;48;5;5m\xb0\x1b[0m \x1b[38;5;172;48;5;235m\xb0\x1b[38;5;140;48;5;92m\xb0\x1b[38;5;202;48;5;234m\xb0\x1b[0m   ",
    "  \x1b[38;5;172;48;5;234m\xb0\x1b[38;5;161;48;5;198m \x1b[38;5;114;48;5;161m\xb1\x1b[0m \x1b[38;5;76;48;5;89m\xb0\x1b[38;5;125;48;5;198m \x1b[38;5;125;48;5;199m \x1b[38;5;125;48;5;199m \x1b[38;5;162;48;5;199m \x1b[38;5;125;48;5;199m \x1b[38;5;130;48;5;235m\xb0\x1b[0m      \x1b[38;5;119;48;5;5m\xb0\x1b[38;5;200;48;5;200m\xb0\x1b[38;5;201;48;5;164m\xb0\x1b[38;5;201;48;5;164m\xb0\x1b[38;5;165;48;5;128m\xb0\x1b[38;5;209;48;5;233m\xb0\x1b[38;5;209;48;5;233m\xb0\x1b[38;5;55;48;5;92m\xb0\x1b[38;5;119;48;5;54m\xb0\x1b[0m  ",
    " \x1b[38;5;119;48;5;125m\xb0\x1b[38;5;205;48;5;198m\xb0\x1b[38;5;205;48;5;198m\xb0\x1b[38;5;205;48;5;198m\xb0\x1b[38;5;198;48;5;198m\xb0\x1b[38;5;198;48;5;198m\xb0\x1b[38;5;125;48;5;198m\xb0\x1b[38;5;125;48;5;198m\xb0\x1b[38;5;125;48;5;198m\xb0\x1b[38;5;162;48;5;198m\xb0\x1b[38;5;136;48;5;235m\xb0\x1b[0m        \x1b[38;5;34;48;5;90m\xb0\x1b[38;5;200;48;5;163m\xb0\x1b[38;5;164;48;5;163m \x1b[38;5;164;48;5;128m \x1b[38;5;165;48;5;128m \x1b[38;5;91;48;5;92m \x1b[38;5;177;48;5;92m \x1b[38;5;55;48;5;92m \x1b[38;5;77;48;5;55m\xb1\x1b[38;5;1;48;5;232m \x1b[0m",
    NULL
};

/* Plain text logo - simple version for non-color terminals */
static const char *logo_plain[] = {
    "              ..              ",
    "               ..... ..      ",
    "               ..... ......  ",
    "               .   .. ....   ",
    "               .              ",
    "             ....             ",
    "            .......           ",
    "           .........          ",
    "      ..  ....  ..... ..      ",
    "     ......... ...........   ",
    "   .....    ..    .......... ",
    "  .. .     .      .........  ",
    " ...........        ........ ",
    NULL
};

#define LOGO_WIDTH 30
#define LOGO_HEIGHT 13
#define MIN_WIDTH_FOR_LOGO 70  /* logo (30) + spacing (3) + version text (~35) */

/* Get terminal width, returns 0 if cannot determine */
static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 0;
}

/* Check if terminal supports color output */
static bool supports_color(void) {
    /* Respect NO_COLOR convention */
    if (getenv("NO_COLOR") != NULL) {
        return false;
    }
    /* Check if stdout is a TTY */
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }
    /* Check TERM for known color-capable terminals */
    const char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
        return false;
    }
    return true;
}

static void print_version(void) {
    int term_width = get_terminal_width();
    bool use_color = supports_color();
    bool show_logo = (term_width >= MIN_WIDTH_FOR_LOGO) && isatty(STDOUT_FILENO);

    /* Version info lines */
    char version_line[64];
    snprintf(version_line, sizeof(version_line), "Apex %s", apex_version_string());
    const char *copyright_line = "Copyright (c) 2026 Brett Terpstra";
    const char *license_line = "Licensed under MIT License";

    if (show_logo) {
        const char **logo = use_color ? logo_ansi : logo_plain;
        /* Print logo with version info on the right side */
        /* Version info aligned to baseline (last 3 lines of logo) */
        for (int i = 0; i < LOGO_HEIGHT; i++) {
            printf("%s", logo[i]);
            if (use_color) {
                printf("\x1b[0m");  /* Reset after each line */
            }
            /* Add version info on specific lines (baseline aligned) */
            if (i == LOGO_HEIGHT - 3) {
                printf("   %s", version_line);
            } else if (i == LOGO_HEIGHT - 2) {
                printf("   %s", copyright_line);
            } else if (i == LOGO_HEIGHT - 1) {
                printf("   %s", license_line);
            }
            printf("\n");
        }
        if (use_color) {
            printf("\x1b[0m");  /* Final reset */
        }
    } else {
        /* Simple text-only output */
        printf("%s\n", version_line);
        printf("%s\n", copyright_line);
        printf("%s\n", license_line);
    }
}

/* Helper to append a script tag string to a dynamic NULL-terminated array.
 * On success, returns 0 and updates *tags, *count, and *capacity.
 * On failure, prints an error and returns non-zero.
 */
static int add_script_tag(char ***tags, size_t *count, size_t *capacity, const char *tag_str) {
    if (!tag_str || !*tag_str) return 0;

    if (!*tags) {
        *tags = malloc((*capacity) * sizeof(char *));
        if (!*tags) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return 1;
        }
    } else if (*count >= *capacity) {
        *capacity *= 2;
        char **new_tags = realloc(*tags, (*capacity) * sizeof(char *));
        if (!new_tags) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return 1;
        }
        *tags = new_tags;
    }

    (*tags)[*count] = strdup(tag_str);
    if (!(*tags)[*count]) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    (*count)++;
    return 0;
}

/* Detect inline terminal graphics that pagers (less -R, etc.) cannot render. */
static bool terminal_output_has_graphics(const char *buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }
    /* iTerm / WezTerm inline images (also chafa -f iterm). */
    if (memmem(buf, len, "]1337;", 6) != NULL) {
        return true;
    }
    /* Kitty graphics protocol. */
    if (memmem(buf, len, "\033_G", 3) != NULL) {
        return true;
    }
    return false;
}

/* Wrap ANSI-colored output to a fixed column width.
 * This operates on the final rendered string and counts only visible
 * characters toward the width, skipping over ANSI CSI sequences.
 */
static char *wrap_ansi_to_width(const char *input, size_t in_len, int width) {
    if (!input || width <= 0) {
        return NULL;
    }
    if (in_len == 0) {
        in_len = strlen(input);
    }

    /* Heuristic for output capacity: input length plus space for added newlines. */
    size_t cap = in_len + (in_len / (size_t)width + 2) * 2 + 1;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }

    size_t oi = 0;
    int col = 0;

    for (size_t i = 0; i < in_len; ) {
        char c = input[i];

        /* Newlines reset the column counter. */
        if (c == '\n') {
            if (oi + 1 >= cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[oi++] = c;
            col = 0;
            i++;
            continue;
        }

        /* Simple handling for carriage return: pass through. */
        if (c == '\r') {
            if (oi + 1 >= cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[oi++] = c;
            i++;
            continue;
        }

        /* Preserve OSC sequences (e.g. iTerm inline images: ESC ] ... BEL or ST). */
        if (c == '\x1b' && i + 1 < in_len && input[i + 1] == ']') {
            size_t start = i;
            i += 2;
            while (i < in_len) {
                if (input[i] == '\x07') {
                    i++;
                    break;
                }
                if (input[i] == '\x1b' && i + 1 < in_len && input[i + 1] == '\\') {
                    i += 2;
                    break;
                }
                i++;
            }
            size_t seq_len = i - start;
            if (oi + seq_len + 1 >= cap) {
                cap = cap + seq_len + 16;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            memcpy(out + oi, input + start, seq_len);
            oi += seq_len;
            continue;
        }

        /* Preserve DCS (sixel) and kitty graphics (ESC _ ... ST). */
        if (c == '\x1b' && i + 1 < in_len && (input[i + 1] == 'P' || input[i + 1] == '_')) {
            size_t start = i;
            i += 2;
            while (i < in_len) {
                if (input[i] == '\x1b' && i + 1 < in_len && input[i + 1] == '\\') {
                    i += 2;
                    break;
                }
                i++;
            }
            size_t seq_len = i - start;
            if (oi + seq_len + 1 >= cap) {
                cap = cap + seq_len + 16;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            memcpy(out + oi, input + start, seq_len);
            oi += seq_len;
            continue;
        }

        /* Preserve ANSI CSI sequences without counting them toward width. */
        if (c == '\x1b' && i + 1 < in_len && input[i + 1] == '[') {
            size_t start = i;
            i += 2;
            while (i < in_len && !((input[i] >= 'A' && input[i] <= 'Z') ||
                                   (input[i] >= 'a' && input[i] <= 'z'))) {
                i++;
            }
            if (i < in_len) {
                i++; /* consume final letter */
            }
            size_t seq_len = i - start;
            if (oi + seq_len + 1 >= cap) {
                cap = cap + seq_len + 16;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            memcpy(out + oi, input + start, seq_len);
            oi += seq_len;
            continue;
        }

        /* Insert a newline before adding another visible char if we've hit width. */
        if (col >= width) {
            if (oi + 1 >= cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[oi++] = '\n';
            col = 0;
        }

        if (oi + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(out, cap);
            if (!nb) {
                free(out);
                return NULL;
            }
            out = nb;
        }
        out[oi++] = c;
        col++;
        i++;
    }

    out[oi] = '\0';
    return out;
}

/**
 * Normalize a plugin identifier to a Git repository URL.
 * Returns a newly allocated string that must be freed by caller, or NULL on error.
 *
 * Handles:
 * - Full Git URLs (https://github.com/user/repo.git, git@github.com:user/repo.git, etc.)
 * - GitHub shorthand (user/repo or ttscoff/apex-plugin-kbd)
 * - Returns NULL if it doesn't look like a URL (treat as directory ID)
 */
static char *normalize_plugin_repo_url(const char *arg) {
    if (!arg || !*arg) return NULL;

    /* Check if it's already a full URL */
    if (strstr(arg, "://") != NULL || strstr(arg, "@") != NULL) {
        /* Looks like a URL - return as-is (but ensure .git suffix for GitHub URLs) */
        if (strncmp(arg, "https://github.com/", 19) == 0 ||
            strncmp(arg, "http://github.com/", 18) == 0 ||
            strncmp(arg, "git@github.com:", 15) == 0) {
            /* GitHub URL - ensure it ends with .git */
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

    /* Check if it's GitHub shorthand (user/repo format) */
    const char *slash = strchr(arg, '/');
    if (slash && slash != arg && slash[1] != '\0') {
        /* Looks like user/repo - convert to https://github.com/user/repo.git */
        size_t len = strlen(arg);
        char *url = malloc(19 + len + 5); /* "https://github.com/" + arg + ".git" */
        if (!url) return NULL;
        snprintf(url, 19 + len + 5, "https://github.com/%s.git", arg);
        return url;
    }

    /* Doesn't look like a URL - return NULL to indicate it should be treated as an ID */
    return NULL;
}

/**
 * Extract plugin ID from a cloned repository.
 * Reads plugin.yml or plugin.yaml and extracts the 'id' field.
 * Falls back to the directory name if no manifest is found.
 * Returns a newly allocated string that must be freed by caller.
 */
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

    /* Fallback: extract from repo path (last component) */
    const char *last_slash = strrchr(repo_path, '/');
    if (last_slash && last_slash[1] != '\0') {
        const char *name = last_slash + 1;
        /* Remove .git suffix if present */
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

static char *read_file(const char *filename, size_t *len) {
    PROFILE_START(file_read);
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        fprintf(stderr, "Error: Cannot determine file size\n");
        return NULL;
    }

    /* Allocate buffer */
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    /* Read file */
    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);
    PROFILE_END(file_read);

    if (len) *len = bytes_read;
    return buffer;
}

static char *read_stdin(size_t *len) {
    size_t capacity = BUFFER_SIZE;
    size_t size = 0;
    char *buffer = malloc(capacity);

    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    /* Use read() system call directly for better control with pipes */
    int fd = fileno(stdin);
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer + size, capacity - size)) > 0) {
        size += bytes_read;
        /* Ensure we have space for at least one more BUFFER_SIZE read */
        if (size + BUFFER_SIZE > capacity) {
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                fprintf(stderr, "Error: Memory allocation failed\n");
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    /* Check if we encountered an error (not EOF) */
    if (bytes_read < 0) {
        free(buffer);
        fprintf(stderr, "Error: Error reading from stdin\n");
        return NULL;
    }

    buffer[size] = '\0';
    if (len) *len = size;
    return buffer;
}

/**
 * Helper: get directory component of a path (malloc'd).
 */
static char *apex_cli_get_directory(const char *filepath) {
    if (!filepath || !*filepath) {
        char *dot = malloc(2);
        if (dot) {
            dot[0] = '.';
            dot[1] = '\0';
        }
        return dot;
    }

    char *copy = strdup(filepath);
    if (!copy) {
        return NULL;
    }
    char *dir = dirname(copy);
    char *result = dir ? strdup(dir) : NULL;
    free(copy);
    if (!result) {
        char *dot = malloc(2);
        if (dot) {
            dot[0] = '.';
            dot[1] = '\0';
        }
        return dot;
    }
    return result;
}

/**
 * Shift Markdown header levels in content by a given indent.
 *
 * For each indent level, this performs the equivalent of the Perl:
 *   $file =~ s/^#/##/gm;
 *
 * i.e., for each line that begins with '#', another '#' is inserted.
 * Lines that do not begin with '#' are left unchanged.
 */
static char *apex_cli_shift_headers(const char *content, int indent) {
    if (!content || indent <= 0) {
        return content ? strdup(content) : NULL;
    }

    size_t len = strlen(content);
    /* Worst case, every character is a header marker; be generous. */
    size_t capacity = len * (size_t)(indent + 1) + 1;
    char *buffer = malloc(capacity);
    if (!buffer) return NULL;

    char *out = buffer;
    const char *in = content;

    for (int level = 0; level < indent; level++) {
        out = buffer;
        in = (level == 0) ? content : buffer;

        bool at_line_start = true;
        while (*in) {
            char c = *in;
            if (at_line_start && c == '#') {
                /* Duplicate initial '#' */
                *out++ = '#';
                *out++ = '#';
                in++;
                at_line_start = false;
                continue;
            }

            *out++ = c;
            if (c == '\n') {
                at_line_start = true;
            } else {
                at_line_start = false;
            }
            in++;
        }
        *out = '\0';
    }

    return buffer;
}

/**
 * Process a MultiMarkdown mmd_merge-style index file:
 * - Each non-empty, non-comment line specifies a file to include
 * - Indentation (tabs or 4-space groups) controls header level shifting
 * - Lines whose first non-whitespace character is '#' are treated as comments
 */
static int apex_cli_mmd_merge_index(const char *index_path, FILE *out) {
    if (!index_path || !out) return 1;

    size_t len = 0;
    char *index_content = read_file(index_path, &len);
    if (!index_content) {
        fprintf(stderr, "Error: Cannot read mmd-merge index '%s'\n", index_path);
        return 1;
    }

    char *base_dir = apex_cli_get_directory(index_path);
    char *cursor = index_content;

    while (*cursor) {
        char *line_start = cursor;
        char *line_end = strchr(cursor, '\n');
        if (!line_end) {
            line_end = cursor + strlen(cursor);
        }

        /* Trim trailing whitespace (including CR) */
        char *trim_end = line_end;
        while (trim_end > line_start && (trim_end[-1] == ' ' || trim_end[-1] == '\t' ||
                                         trim_end[-1] == '\r')) {
            trim_end--;
        }

        size_t line_len = (size_t)(trim_end - line_start);

        if (line_len > 0) {
            /* Make a null-terminated copy for easier processing */
            char *line = malloc(line_len + 1);
            if (!line) {
                free(index_content);
                if (base_dir) free(base_dir);
                return 1;
            }
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';

            /* Skip leading whitespace to check for blank or comment lines */
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;

            if (*p != '\0' && *p != '#') {
                /* Count indentation: tabs and groups of 4 spaces at start of line */
                int indent = 0;
                char *q = line;
                while (*q == ' ' || *q == '\t') {
                    if (*q == '\t') {
                        indent++;
                        q++;
                    } else {
                        int spaces = 0;
                        while (*q == ' ' && spaces < 4) {
                            spaces++;
                            q++;
                        }
                        if (spaces == 4) {
                            indent++;
                        } else {
                            /* Fewer than 4 trailing spaces at start are ignored for indent */
                            break;
                        }
                    }
                }

                /* Extract filename from the remainder of the line */
                while (*q == ' ' || *q == '\t') q++;
                char *name_start = q;
                char *name_end = name_start + strlen(name_start);
                while (name_end > name_start &&
                       (name_end[-1] == ' ' || name_end[-1] == '\t')) {
                    name_end--;
                }
                *name_end = '\0';

                if (*name_start) {
                    char full_path[4096];
                    if (name_start[0] == '/') {
                        /* Absolute path */
                        snprintf(full_path, sizeof(full_path), "%s", name_start);
                    } else {
                        snprintf(full_path, sizeof(full_path), "%s/%s",
                                 base_dir ? base_dir : ".",
                                 name_start);
                    }

                    size_t file_len = 0;
                    char *file_content = read_file(full_path, &file_len);
                    if (!file_content) {
                        fprintf(stderr, "Warning: Skipping unreadable file '%s' from mmd-merge index '%s'\n",
                                full_path, index_path);
                    } else {
                        char *shifted = apex_cli_shift_headers(file_content, indent);
                        if (!shifted) {
                            shifted = file_content;
                            file_content = NULL;
                        }

                        fputs(shifted, out);
                        fputc('\n', out);
                        fputc('\n', out);

                        if (shifted != file_content && shifted) {
                            free(shifted);
                        }
                        if (file_content) {
                            free(file_content);
                        }
                    }
                }
            }

            free(line);
        }

        cursor = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    free(index_content);
    if (base_dir) free(base_dir);
    return 0;
}

/**
 * Process a single Markdown file:
 * - Read content
 * - Extract metadata (for transclude base)
 * - Run apex_process_includes
 * Returns newly allocated string or NULL on error.
 */
static char *apex_cli_combine_process_file(const char *filepath) {
    if (!filepath) return NULL;

    size_t len = 0;
    char *markdown = read_file(filepath, &len);
    if (!markdown) {
        return NULL;
    }

    /* Extract metadata in a copy so we don't modify the original text,
     * preserving verbatim Markdown while still honoring transclude base.
     */
    apex_metadata_item *doc_metadata = NULL;
    char *doc_copy = malloc(len + 1);
    if (doc_copy) {
        memcpy(doc_copy, markdown, len);
        doc_copy[len] = '\0';
        char *ptr = doc_copy;
        doc_metadata = apex_extract_metadata(&ptr);
    }

    char *base_dir = apex_cli_get_directory(filepath);
    char *processed = apex_process_includes(markdown, base_dir, doc_metadata, 0, NULL, NULL, NULL);

    if (doc_metadata) {
        apex_free_metadata(doc_metadata);
    }
    if (doc_copy) {
        free(doc_copy);
    }
    if (base_dir) {
        free(base_dir);
    }
    free(markdown);

    return processed ? processed : NULL;
}

/**
 * Append a chunk of Markdown to an output stream, ensuring separation
 * between documents.
 */
static void apex_cli_write_combined_chunk(FILE *out, const char *chunk, bool *needs_separator) {
    if (!out || !chunk) return;

    if (*needs_separator) {
        /* Ensure at least one blank line between documents */
        fputc('\n', out);
        fputc('\n', out);
    }

    fputs(chunk, out);
    *needs_separator = true;
}

/**
 * Parse a GitBook-style SUMMARY.md and write the combined Markdown
 * for all linked files in order.
 *
 * Returns 0 on success, non-zero on error.
 */
static int apex_cli_combine_from_summary(const char *summary_path, FILE *out) {
    size_t len = 0;
    char *summary = read_file(summary_path, &len);
    if (!summary) {
        fprintf(stderr, "Error: Cannot read SUMMARY file '%s'\n", summary_path);
        return 1;
    }

    char *base_dir = apex_cli_get_directory(summary_path);
    bool needs_separator = false;

    char *cursor = summary;
    while (*cursor) {
        char *line_start = cursor;
        char *line_end = strchr(cursor, '\n');
        if (!line_end) {
            line_end = cursor + strlen(cursor);
        }

        /* Look for [Title](path) pattern on this line */
        const char *lb = memchr(line_start, '[', (size_t)(line_end - line_start));
        const char *rb = NULL;
        const char *lp = NULL;
        const char *rp = NULL;

        if (lb) {
            rb = memchr(lb, ']', (size_t)(line_end - lb));
            if (rb && (rb + 1) < line_end && rb[1] == '(') {
                lp = rb + 2;
                rp = memchr(lp, ')', (size_t)(line_end - lp));
            }
        }

        if (lp && rp && rp > lp) {
            size_t path_len = (size_t)(rp - lp);
            char *rel_path = malloc(path_len + 1);
            if (rel_path) {
                memcpy(rel_path, lp, path_len);
                rel_path[path_len] = '\0';

                /* Trim whitespace */
                char *p = rel_path;
                while (*p == ' ' || *p == '\t') p++;
                char *start = p;
                char *end = start + strlen(start);
                while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
                    end--;
                }
                *end = '\0';

                if (*start) {
                    /* Strip anchor (#section) if present */
                    char *hash = strchr(start, '#');
                    if (hash) {
                        *hash = '\0';
                    }

                    /* Skip external links (with scheme) */
                    if (!strstr(start, "://")) {
                        size_t full_len = strlen(base_dir ? base_dir : ".") + strlen(start) + 2;
                        char *full_path = malloc(full_len);
                        if (full_path) {
                            snprintf(full_path, full_len, "%s/%s", base_dir ? base_dir : ".", start);
                            char *processed = apex_cli_combine_process_file(full_path);
                            if (processed) {
                                apex_cli_write_combined_chunk(out, processed, &needs_separator);
                                free(processed);
                            } else {
                                fprintf(stderr, "Warning: Skipping unreadable file '%s' from SUMMARY\n", full_path);
                            }
                            free(full_path);
                        }
                    }
                }

                free(rel_path);
            }
        }

        cursor = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    free(summary);
    if (base_dir) free(base_dir);
    return 0;
}

int main(int argc, char *argv[]) {
    /* Initialize progress reporting */
    init_progress();

    apex_options options = apex_options_default();
    apex_options cli_options_snapshot;
    apex_cli_option_mask cli_opt_mask = {0};
    bool plugins_cli_override = false;
    bool plugins_cli_value = false;
    bool list_plugins = false;
    bool list_themes = false;
    bool cli_info = false;
    bool cli_extract_meta = false;
    const char *extract_meta_value_key = NULL;
    const char *install_plugin_id = NULL;
    const char *uninstall_plugin_id = NULL;
    bool list_filters = false;
    const char *uninstall_filter_id = NULL;

    /* Filter install (AST filters) */
    const char *install_filter_id = NULL;
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *meta_file = NULL;
    apex_metadata_item *cmdline_metadata = NULL;
    char *allocated_base_dir = NULL;      /* Track if we allocated base_directory */
    char *allocated_input_file_path = NULL;  /* Track if we allocate input_file_path */

    /* Combine mode: concatenate Markdown files (with includes expanded) */
    bool combine_mode = false;
    char **combine_files = NULL;
    size_t combine_file_count = 0;
    size_t combine_file_capacity = 0;

    /* mmd-merge mode: emulate MultiMarkdown mmd_merge.pl behavior */
    bool mmd_merge_mode = false;
    char **mmd_merge_files = NULL;
    size_t mmd_merge_file_count = 0;
    size_t mmd_merge_file_capacity = 0;

    /* Bibliography files (NULL-terminated array) */
    char **bibliography_files = NULL;
    size_t bibliography_count = 0;
    size_t bibliography_capacity = 4;

    /* Concordance files (NULL-terminated array) */
    char **concordance_files = NULL;
    size_t concordance_count = 0;
    size_t concordance_capacity = 4;

    /* Stylesheet files (NULL-terminated array) */
    char **stylesheet_files = NULL;
    size_t stylesheet_count = 0;
    size_t stylesheet_capacity = 4;

    /* Script tags (NULL-terminated array of raw <script> HTML snippets) */
    char **script_tags = NULL;
    size_t script_tag_count = 0;
    size_t script_tag_capacity = 4;

    /* AST filters (Pandoc-style JSON filters) configured from CLI */
    char   **ast_filter_names = NULL;   /* Filter names from --filter (resolved in config dir) */
    size_t   ast_filter_name_count = 0;
    size_t   ast_filter_name_capacity = 4;
    bool     run_all_filters_dir = false;  /* --filters */
    bool     ast_filters_strict = true;    /* default strict mode; --no-strict-filters disables */

    /* Lua filters: explicit script paths run via 'lua <script>' */
    char   **lua_filter_paths = NULL;
    size_t   lua_filter_count = 0;
    size_t   lua_filter_capacity = 4;

    /* Optional fixed-width wrapping for terminal output */
    int width_override = 0;

    /* Pagination for terminal/terminal256 output */
    bool paginate_cli = false;
    bool paginate_symbols_cli = false;
    bool no_paginate_cli = false;

    /* Terminal inline images: --no-terminal-images / --terminal-image-width N */
    bool no_terminal_images_cli = false;
    int terminal_image_width_cli = -1; /* -1 = use options default */

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--info") == 0) {
            cli_info = true;
        } else if (strcmp(argv[i], "--extract-meta") == 0) {
            cli_extract_meta = true;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--extract-meta-value") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --extract-meta-value requires a KEY argument\n");
                return 1;
            }
            extract_meta_value_key = argv[i];
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --mode requires an argument\n");
                return 1;
            }
            cli_opt_mask.mode = true;
            if (strcmp(argv[i], "commonmark") == 0) {
                options = apex_options_for_mode(APEX_MODE_COMMONMARK);
            } else if (strcmp(argv[i], "gfm") == 0) {
                options = apex_options_for_mode(APEX_MODE_GFM);
            } else if (strcmp(argv[i], "mmd") == 0 || strcmp(argv[i], "multimarkdown") == 0) {
                options = apex_options_for_mode(APEX_MODE_MULTIMARKDOWN);
            } else if (strcmp(argv[i], "kramdown") == 0) {
                options = apex_options_for_mode(APEX_MODE_KRAMDOWN);
            } else if (strcmp(argv[i], "unified") == 0) {
                options = apex_options_for_mode(APEX_MODE_UNIFIED);
            } else if (strcmp(argv[i], "quarto") == 0) {
                options = apex_options_for_mode(APEX_MODE_QUARTO);
            } else {
                fprintf(stderr, "Error: Unknown mode '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--to") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --to requires an argument\n");
                return 1;
            }
            cli_opt_mask.output_format = true;
            if (strcmp(argv[i], "html") == 0) {
                options.output_format = APEX_OUTPUT_HTML;
            } else if (strcmp(argv[i], "xhtml") == 0) {
                /* Alias for -t html --xhtml */
                cli_opt_mask.xhtml = true;
                cli_opt_mask.strict_xhtml = true;
                options.output_format = APEX_OUTPUT_HTML;
                options.xhtml = true;
                options.strict_xhtml = false;
            } else if (strcmp(argv[i], "strict-xhtml") == 0) {
                /* Alias for -t html --strict-xhtml */
                cli_opt_mask.xhtml = true;
                cli_opt_mask.strict_xhtml = true;
                options.output_format = APEX_OUTPUT_HTML;
                options.strict_xhtml = true;
                options.xhtml = false;
            } else if (strcmp(argv[i], "json") == 0) {
                options.output_format = APEX_OUTPUT_JSON;
            } else if (strcmp(argv[i], "json-filtered") == 0 || strcmp(argv[i], "ast-json") == 0 || strcmp(argv[i], "ast") == 0) {
                options.output_format = APEX_OUTPUT_JSON_FILTERED;
            } else if (strcmp(argv[i], "markdown") == 0 || strcmp(argv[i], "md") == 0) {
                options.output_format = APEX_OUTPUT_MARKDOWN;
            } else if (strcmp(argv[i], "mmd") == 0) {
                options.output_format = APEX_OUTPUT_MMD;
            } else if (strcmp(argv[i], "commonmark") == 0 || strcmp(argv[i], "cmark") == 0) {
                options.output_format = APEX_OUTPUT_COMMONMARK;
            } else if (strcmp(argv[i], "kramdown") == 0) {
                options.output_format = APEX_OUTPUT_KRAMDOWN;
            } else if (strcmp(argv[i], "gfm") == 0) {
                options.output_format = APEX_OUTPUT_GFM;
            } else if (strcmp(argv[i], "terminal") == 0 || strcmp(argv[i], "cli") == 0) {
                options.output_format = APEX_OUTPUT_TERMINAL;
            } else if (strcmp(argv[i], "terminal256") == 0) {
                options.output_format = APEX_OUTPUT_TERMINAL256;
            } else if (strcmp(argv[i], "man") == 0) {
                options.output_format = APEX_OUTPUT_MAN;
            } else if (strcmp(argv[i], "man-html") == 0) {
                options.output_format = APEX_OUTPUT_MAN_HTML;
            } else if (strcmp(argv[i], "toc") == 0) {
                options.output_format = APEX_OUTPUT_TOC;
            } else if (strcmp(argv[i], "rtf") == 0) {
                options.output_format = APEX_OUTPUT_RTF;
            } else {
                fprintf(stderr, "Error: Unknown output format '%s'\n", argv[i]);
                fprintf(stderr, "Supported formats: html, xhtml, strict-xhtml, json, json-filtered/ast-json/ast, markdown/md, mmd, commonmark/cmark, kramdown, gfm, terminal/cli, terminal256, man, man-html, toc, rtf\n");
                return 1;
            }
        } else if (strncmp(argv[i], "--toc-min-max=", 14) == 0 ||
                   strcmp(argv[i], "--toc-min-max") == 0) {
            const char *arg_value = NULL;
            if (strncmp(argv[i], "--toc-min-max=", 14) == 0) {
                arg_value = argv[i] + 14;
            } else {
                if (++i >= argc) {
                    fprintf(stderr, "Error: --toc-min-max requires MIN,MAX\n");
                    return 1;
                }
                arg_value = argv[i];
            }

            int toc_min = 0;
            int toc_max = 0;
            int consumed = 0;
            if (sscanf(arg_value, "%d,%d%n", &toc_min, &toc_max, &consumed) != 2 ||
                arg_value[consumed] != '\0' ||
                toc_min < 1 || toc_min > toc_max || toc_max > 6) {
                fprintf(stderr, "Error: --toc-min-max must be MIN,MAX with 1 <= MIN <= MAX <= 6\n");
                return 1;
            }
            cli_opt_mask.toc_min_max = true;
            options.toc_min = toc_min;
            options.toc_max = toc_max;
        } else if (strcmp(argv[i], "--theme") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --theme requires a name argument\n");
                return 1;
            }
            cli_opt_mask.theme_name = true;
            options.theme_name = argv[i];
        } else if (strcmp(argv[i], "--width") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --width requires a column width argument\n");
                return 1;
            }
            width_override = atoi(argv[i]);
            if (width_override < 0) {
                width_override = 0;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--paginate") == 0) {
            paginate_cli = true;
        } else if (strcmp(argv[i], "--paginate-symbols") == 0) {
            paginate_symbols_cli = true;
        } else if (strcmp(argv[i], "--no-paginate") == 0) {
            no_paginate_cli = true;
        } else if (strcmp(argv[i], "--no-terminal-images") == 0) {
            no_terminal_images_cli = true;
        } else if (strcmp(argv[i], "--terminal-image-width") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --terminal-image-width requires a positive integer\n");
                return 1;
            }
            terminal_image_width_cli = atoi(argv[i]);
            if (terminal_image_width_cli < 1) {
                fprintf(stderr, "Error: --terminal-image-width must be at least 1\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --output requires an argument\n");
                return 1;
            }
            output_file = argv[i];
        } else if (strcmp(argv[i], "--plugins") == 0) {
            cli_opt_mask.enable_plugins = true;
            options.enable_plugins = true;
            plugins_cli_override = true;
            plugins_cli_value = true;
        } else if (strcmp(argv[i], "--no-plugins") == 0) {
            cli_opt_mask.enable_plugins = true;
            options.enable_plugins = false;
            plugins_cli_override = true;
            plugins_cli_value = false;
        } else if (strcmp(argv[i], "--list-plugins") == 0) {
            list_plugins = true;
        } else if (strcmp(argv[i], "--list-themes") == 0) {
            list_themes = true;
        } else if (strcmp(argv[i], "--install-plugin") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --install-plugin requires an id argument\n");
                return 1;
            }
            install_plugin_id = argv[i];
        } else if (strcmp(argv[i], "--list-filters") == 0) {
            list_filters = true;
        } else if (strcmp(argv[i], "--install-filter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --install-filter requires an id argument\n");
                return 1;
            }
            install_filter_id = argv[i];
        } else if (strcmp(argv[i], "--uninstall-filter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --uninstall-filter requires an id argument\n");
                return 1;
            }
            uninstall_filter_id = argv[i];
        } else if (strcmp(argv[i], "--filter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --filter requires a name argument\n");
                return 1;
            }
            /* Collect filter names; resolution to full paths happens later */
            if (!ast_filter_names) {
                ast_filter_names = malloc(ast_filter_name_capacity * sizeof(char *));
                if (!ast_filter_names) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else if (ast_filter_name_count >= ast_filter_name_capacity) {
                size_t new_cap = ast_filter_name_capacity ? ast_filter_name_capacity * 2 : 4;
                char **tmp = realloc(ast_filter_names, new_cap * sizeof(char *));
                if (!tmp) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_names = tmp;
                ast_filter_name_capacity = new_cap;
            }
            ast_filter_names[ast_filter_name_count++] = argv[i];
        } else if (strcmp(argv[i], "--filters") == 0) {
            run_all_filters_dir = true;
        } else if (strcmp(argv[i], "--no-strict-filters") == 0) {
            ast_filters_strict = false;
        } else if (strcmp(argv[i], "--lua-filter") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --lua-filter requires a script path argument\n");
                return 1;
            }
            if (!lua_filter_paths) {
                lua_filter_paths = malloc(lua_filter_capacity * sizeof(char *));
                if (!lua_filter_paths) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else if (lua_filter_count >= lua_filter_capacity) {
                size_t new_cap = lua_filter_capacity ? lua_filter_capacity * 2 : 4;
                char **tmp = realloc(lua_filter_paths, new_cap * sizeof(char *));
                if (!tmp) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                lua_filter_paths = tmp;
                lua_filter_capacity = new_cap;
            }
            lua_filter_paths[lua_filter_count++] = argv[i];
        } else if (strcmp(argv[i], "--uninstall-plugin") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --uninstall-plugin requires an id argument\n");
                return 1;
            }
            uninstall_plugin_id = argv[i];
        } else if (strcmp(argv[i], "--no-tables") == 0) {
            cli_opt_mask.enable_tables = true;
            options.enable_tables = false;
        } else if (strcmp(argv[i], "--grid-tables") == 0 || strcmp(argv[i], "--enable-grid-tables") == 0) {
            cli_opt_mask.enable_grid_tables = true;
            options.enable_grid_tables = true;
        } else if (strcmp(argv[i], "--no-grid-tables") == 0) {
            cli_opt_mask.enable_grid_tables = true;
            options.enable_grid_tables = false;
        } else if (strcmp(argv[i], "--no-footnotes") == 0) {
            cli_opt_mask.enable_footnotes = true;
            options.enable_footnotes = false;
        } else if (strcmp(argv[i], "--no-smart") == 0) {
            cli_opt_mask.enable_smart_typography = true;
            options.enable_smart_typography = false;
        } else if (strcmp(argv[i], "--no-math") == 0) {
            cli_opt_mask.enable_math = true;
            options.enable_math = false;
        } else if (strcmp(argv[i], "--includes") == 0) {
            cli_opt_mask.enable_file_includes = true;
            options.enable_file_includes = true;
        } else if (strcmp(argv[i], "--no-includes") == 0) {
            cli_opt_mask.enable_file_includes = true;
            options.enable_file_includes = false;
        } else if (strcmp(argv[i], "--hardbreaks") == 0) {
            cli_opt_mask.hardbreaks = true;
            options.hardbreaks = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--standalone") == 0) {
            cli_opt_mask.standalone = true;
            options.standalone = true;
        } else if (strcmp(argv[i], "--css") == 0 || strcmp(argv[i], "--style") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i-1]);
                return 1;
            }
            cli_opt_mask.standalone = true;
            cli_opt_mask.stylesheet = true;
            options.standalone = true;  /* Imply standalone if CSS is specified */

            /* Parse comma-separated stylesheet paths */
            const char *arg = argv[i];
            const char *start = arg;
            while (*arg) {
                /* Find comma or end of string */
                while (*arg && *arg != ',') {
                    arg++;
                }

                /* Extract this stylesheet path */
                size_t len = arg - start;
                if (len > 0) {
                    /* Skip leading whitespace */
                    while (len > 0 && (*start == ' ' || *start == '\t')) {
                        start++;
                        len--;
                    }
                    /* Skip trailing whitespace */
                    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
                        len--;
                    }

                    if (len > 0) {
                        /* Allocate or reallocate stylesheet files array */
                        if (!stylesheet_files) {
                            stylesheet_files = malloc(stylesheet_capacity * sizeof(char*));
                            if (!stylesheet_files) {
                                fprintf(stderr, "Error: Memory allocation failed\n");
                                return 1;
                            }
                        } else if (stylesheet_count >= stylesheet_capacity) {
                            stylesheet_capacity *= 2;
                            char **new_files = realloc(stylesheet_files, stylesheet_capacity * sizeof(char*));
                            if (!new_files) {
                                fprintf(stderr, "Error: Memory allocation failed\n");
                                return 1;
                            }
                            stylesheet_files = new_files;
                        }

                        /* Allocate and copy the stylesheet path */
                        stylesheet_files[stylesheet_count] = malloc(len + 1);
                        if (!stylesheet_files[stylesheet_count]) {
                            fprintf(stderr, "Error: Memory allocation failed\n");
                            return 1;
                        }
                        memcpy(stylesheet_files[stylesheet_count], start, len);
                        stylesheet_files[stylesheet_count][len] = '\0';
                        stylesheet_count++;
                    }
                }

                /* Skip comma and any following whitespace */
                if (*arg == ',') {
                    arg++;
                    while (*arg == ' ' || *arg == '\t') {
                        arg++;
                    }
                    start = arg;
                }
            }
        } else if (strcmp(argv[i], "--embed-css") == 0) {
            cli_opt_mask.embed_stylesheet = true;
            options.embed_stylesheet = true;
        } else if (strcmp(argv[i], "--script") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --script requires an argument\n");
                return 1;
            }
            /* Argument may be a single value or comma-separated list */
            const char *arg = argv[i];
            char *arg_copy = strdup(arg);
            if (!arg_copy) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                return 1;
            }

            char *token = arg_copy;
            while (token && *token) {
                /* Find next comma and split */
                char *comma = strchr(token, ',');
                if (comma) {
                    *comma = '\0';
                }

                /* Trim leading/trailing whitespace */
                char *start = token;
                while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
                    start++;
                }
                char *end = start + strlen(start);
                while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
                    end--;
                }
                *end = '\0';

                if (*start) {
                    /* Map common shorthands to CDN script tags, otherwise treat as src */
                    const char *lower = start;
                    /* Simple case-insensitive checks for known shorthands */
                    if (strcasecmp(lower, "mermaid") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script src=\"https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "mathjax") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "katex") == 0) {
                        /* KaTeX typically needs both the core script and auto-render helper */
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script defer src=\"https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script defer src=\"https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/contrib/auto-render.min.js\" onload=\"renderMathInElement(document.body, {delimiters: [{left: '\\\\[', right: '\\\\]', display: true}, {left: '\\\\\\(', right: '\\\\\\)', display: false}], ignoredClasses: ['math']}); document.querySelectorAll('span.math').forEach(function(el){var text=el.textContent.trim();if(text.indexOf('\\\\(')==0)text=text.slice(2,-2);else if(text.indexOf('\\\\\\[')==0)text=text.slice(2,-2);var isDisplay=el.classList.contains('display');try{katex.render(text,el,{displayMode:isDisplay,throwOnError:false});}catch(e){}});\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "highlightjs") == 0 || strcasecmp(lower, "highlight.js") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script src=\"https://cdn.jsdelivr.net/npm/highlight.js@11/lib/highlight.min.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "prism") == 0 || strcasecmp(lower, "prismjs") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script src=\"https://cdn.jsdelivr.net/npm/prismjs@1/components/prism-core.min.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "htmx") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script src=\"https://unpkg.com/htmx.org@1.9.10\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else if (strcasecmp(lower, "alpine") == 0 || strcasecmp(lower, "alpinejs") == 0) {
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity,
                                           "<script defer src=\"https://cdn.jsdelivr.net/npm/alpinejs@3.x.x/dist/cdn.min.js\"></script>") != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    } else {
                        /* Treat as a path or URL and create a simple script tag */
                        char buf[2048];
                        int n = snprintf(buf, sizeof(buf), "<script src=\"%s\"></script>", start);
                        if (n < 0 || (size_t)n >= sizeof(buf)) {
                            fprintf(stderr, "Error: --script value too long\n");
                            free(arg_copy);
                            return 1;
                        }
                        if (add_script_tag(&script_tags, &script_tag_count, &script_tag_capacity, buf) != 0) {
                            free(arg_copy);
                            return 1;
                        }
                    }
                }

                if (!comma) break;
                token = comma + 1;
            }

            free(arg_copy);
        } else if (strcmp(argv[i], "--title") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --title requires an argument\n");
                return 1;
            }
            cli_opt_mask.document_title = true;
            options.document_title = argv[i];
        } else if (strcmp(argv[i], "--pretty") == 0) {
            cli_opt_mask.pretty = true;
            options.pretty = true;
        } else if (strcmp(argv[i], "--xhtml") == 0) {
            cli_opt_mask.xhtml = true;
            options.xhtml = true;
        } else if (strcmp(argv[i], "--strict-xhtml") == 0) {
            cli_opt_mask.strict_xhtml = true;
            options.strict_xhtml = true;
        } else if (strcmp(argv[i], "--accept") == 0) {
            cli_opt_mask.critic = true;
            options.enable_critic_markup = true;
            options.critic_mode = 0;  /* CRITIC_ACCEPT */
        } else if (strcmp(argv[i], "--reject") == 0) {
            cli_opt_mask.critic = true;
            options.enable_critic_markup = true;
            options.critic_mode = 1;  /* CRITIC_REJECT */
        } else if (strcmp(argv[i], "--id-format") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --id-format requires an argument (gfm, mmd, or kramdown)\n");
                return 1;
            }
            cli_opt_mask.id_format = true;
            if (strcmp(argv[i], "gfm") == 0) {
                options.id_format = 0;  /* GFM format */
            } else if (strcmp(argv[i], "mmd") == 0) {
                options.id_format = 1;  /* MMD format */
            } else if (strcmp(argv[i], "kramdown") == 0) {
                options.id_format = 2;  /* Kramdown format */
            } else {
                fprintf(stderr, "Error: --id-format must be 'gfm', 'mmd', or 'kramdown'\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--no-ids") == 0) {
            cli_opt_mask.generate_header_ids = true;
            options.generate_header_ids = false;
        } else if (strcmp(argv[i], "--header-anchors") == 0) {
            cli_opt_mask.header_anchors = true;
            options.header_anchors = true;
        } else if (strcmp(argv[i], "--relaxed-tables") == 0) {
            cli_opt_mask.relaxed_tables = true;
            options.relaxed_tables = true;
        } else if (strcmp(argv[i], "--no-relaxed-tables") == 0) {
            cli_opt_mask.relaxed_tables = true;
            options.relaxed_tables = false;
        } else if (strcmp(argv[i], "--per-cell-alignment") == 0) {
            cli_opt_mask.per_cell_alignment = true;
            options.per_cell_alignment = true;
        } else if (strcmp(argv[i], "--no-per-cell-alignment") == 0) {
            cli_opt_mask.per_cell_alignment = true;
            options.per_cell_alignment = false;
        } else if (strcmp(argv[i], "--captions") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --captions requires an argument (above or below)\n");
                return 1;
            }
            cli_opt_mask.caption_position = true;
            if (strcmp(argv[i], "above") == 0) {
                options.caption_position = 0;
            } else if (strcmp(argv[i], "below") == 0) {
                options.caption_position = 1;
            } else {
                fprintf(stderr, "Error: --captions must be 'above' or 'below'\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--code-highlight") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --code-highlight requires a tool name (pygments, skylighting, shiki, or abbreviations p, s, sh)\n");
                return 1;
            }
            cli_opt_mask.code_highlighter = true;
            /* Accept full names and abbreviations */
            if (strcmp(argv[i], "pygments") == 0 || strcmp(argv[i], "p") == 0 || strcmp(argv[i], "pyg") == 0) {
                options.code_highlighter = "pygments";
            } else if (strcmp(argv[i], "skylighting") == 0 || strcmp(argv[i], "s") == 0 || strcmp(argv[i], "sky") == 0) {
                options.code_highlighter = "skylighting";
            } else if (strcmp(argv[i], "shiki") == 0 || strcmp(argv[i], "sh") == 0) {
                options.code_highlighter = "shiki";
            } else {
                fprintf(stderr, "Error: --code-highlight tool must be 'pygments' (p), 'skylighting' (s), or 'shiki' (sh)\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--code-highlight-theme") == 0 ||
                   strcmp(argv[i], "--code-hilight-theme") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --code-highlight-theme requires a theme name\n");
                return 1;
            }
            cli_opt_mask.code_highlight_theme = true;
            options.code_highlight_theme = argv[i];
        } else if (strcmp(argv[i], "--code-line-numbers") == 0) {
            cli_opt_mask.code_line_numbers = true;
            options.code_line_numbers = true;
        } else if (strcmp(argv[i], "--highlight-language-only") == 0) {
            cli_opt_mask.highlight_language_only = true;
            options.highlight_language_only = true;
        } else if (strcmp(argv[i], "--alpha-lists") == 0) {
            cli_opt_mask.allow_alpha_lists = true;
            options.allow_alpha_lists = true;
        } else if (strcmp(argv[i], "--no-alpha-lists") == 0) {
            cli_opt_mask.allow_alpha_lists = true;
            options.allow_alpha_lists = false;
        } else if (strcmp(argv[i], "--mixed-lists") == 0) {
            cli_opt_mask.allow_mixed_list_markers = true;
            options.allow_mixed_list_markers = true;
        } else if (strcmp(argv[i], "--no-mixed-lists") == 0) {
            cli_opt_mask.allow_mixed_list_markers = true;
            options.allow_mixed_list_markers = false;
        } else if (strcmp(argv[i], "--unsafe") == 0) {
            cli_opt_mask.unsafe = true;
            options.unsafe = true;
        } else if (strcmp(argv[i], "--no-unsafe") == 0) {
            cli_opt_mask.unsafe = true;
            options.unsafe = false;
        } else if (strcmp(argv[i], "--sup-sub") == 0) {
            cli_opt_mask.enable_sup_sub = true;
            options.enable_sup_sub = true;
        } else if (strcmp(argv[i], "--no-sup-sub") == 0) {
            cli_opt_mask.enable_sup_sub = true;
            options.enable_sup_sub = false;
        } else if (strcmp(argv[i], "--divs") == 0) {
            cli_opt_mask.enable_divs = true;
            options.enable_divs = true;
        } else if (strcmp(argv[i], "--no-divs") == 0) {
            cli_opt_mask.enable_divs = true;
            options.enable_divs = false;
        } else if (strcmp(argv[i], "--py-callouts") == 0) {
            cli_opt_mask.enable_py_callouts = true;
            options.enable_py_callouts = true;
        } else if (strcmp(argv[i], "--no-py-callouts") == 0) {
            cli_opt_mask.enable_py_callouts = true;
            options.enable_py_callouts = false;
        } else if (strcmp(argv[i], "--quarto-callouts") == 0) {
            cli_opt_mask.enable_quarto_callouts = true;
            options.enable_quarto_callouts = true;
        } else if (strcmp(argv[i], "--no-quarto-callouts") == 0) {
            cli_opt_mask.enable_quarto_callouts = true;
            options.enable_quarto_callouts = false;
        } else if (strcmp(argv[i], "--one-line-definitions") == 0) {
            cli_opt_mask.enable_definition_lists = true;
            options.enable_definition_lists = true;
        } else if (strcmp(argv[i], "--no-one-line-definitions") == 0) {
            cli_opt_mask.enable_definition_lists = true;
            options.enable_definition_lists = false;
        } else if (strcmp(argv[i], "--spans") == 0) {
            cli_opt_mask.enable_spans = true;
            options.enable_spans = true;
        } else if (strcmp(argv[i], "--no-spans") == 0) {
            cli_opt_mask.enable_spans = true;
            options.enable_spans = false;
        } else if (strcmp(argv[i], "--autolink") == 0) {
            cli_opt_mask.enable_autolink = true;
            options.enable_autolink = true;
        } else if (strcmp(argv[i], "--no-autolink") == 0) {
            cli_opt_mask.enable_autolink = true;
            options.enable_autolink = false;
        } else if (strcmp(argv[i], "--strikethrough") == 0) {
            cli_opt_mask.enable_strikethrough = true;
            options.enable_strikethrough = true;
        } else if (strcmp(argv[i], "--no-strikethrough") == 0) {
            cli_opt_mask.enable_strikethrough = true;
            options.enable_strikethrough = false;
        } else if (strcmp(argv[i], "--obfuscate-emails") == 0) {
            cli_opt_mask.obfuscate_emails = true;
            options.obfuscate_emails = true;
        } else if (strcmp(argv[i], "--progress") == 0) {
            progress_enabled = true;
        } else if (strcmp(argv[i], "--no-progress") == 0) {
            progress_enabled = false;
        } else if (strcmp(argv[i], "--aria") == 0) {
            cli_opt_mask.enable_aria = true;
            options.enable_aria = true;
        } else if (strcmp(argv[i], "--wikilinks") == 0) {
            cli_opt_mask.enable_wiki_links = true;
            options.enable_wiki_links = true;
        } else if (strcmp(argv[i], "--no-wikilinks") == 0) {
            cli_opt_mask.enable_wiki_links = true;
            options.enable_wiki_links = false;
        } else if (strcmp(argv[i], "--emoji") == 0) {
            cli_opt_mask.enable_emoji = true;
            options.enable_emoji = true;
        } else if (strcmp(argv[i], "--no-emoji") == 0) {
            cli_opt_mask.enable_emoji = true;
            options.enable_emoji = false;
        } else if (strcmp(argv[i], "--emoji-autocorrect") == 0) {
            cli_opt_mask.enable_emoji_autocorrect = true;
            options.enable_emoji_autocorrect = true;
        } else if (strcmp(argv[i], "--no-emoji-autocorrect") == 0) {
            cli_opt_mask.enable_emoji_autocorrect = true;
            options.enable_emoji_autocorrect = false;
        } else if (strcmp(argv[i], "--widont") == 0) {
            cli_opt_mask.enable_widont = true;
            options.enable_widont = true;
        } else if (strcmp(argv[i], "--code-is-poetry") == 0) {
            cli_opt_mask.code_is_poetry = true;
            options.code_is_poetry = true;
            options.highlight_language_only = true;
        } else if (strcmp(argv[i], "--markdown-in-html") == 0) {
            cli_opt_mask.enable_markdown_in_html = true;
            options.enable_markdown_in_html = true;
        } else if (strcmp(argv[i], "--no-markdown-in-html") == 0) {
            cli_opt_mask.enable_markdown_in_html = true;
            options.enable_markdown_in_html = false;
        } else if (strcmp(argv[i], "--random-footnote-ids") == 0) {
            cli_opt_mask.random_footnote_ids = true;
            options.random_footnote_ids = true;
        } else if (strcmp(argv[i], "--hashtags") == 0) {
            cli_opt_mask.enable_hashtags = true;
            options.enable_hashtags = true;
        } else if (strcmp(argv[i], "--style-hashtags") == 0) {
            cli_opt_mask.style_hashtags = true;
            options.style_hashtags = true;
        } else if (strcmp(argv[i], "--proofreader") == 0) {
            cli_opt_mask.proofreader = true;
            options.proofreader_mode = true;
            options.enable_critic_markup = true;
            options.critic_mode = 2;  /* Ensure markup mode */
        } else if (strcmp(argv[i], "--hr-page-break") == 0) {
            cli_opt_mask.hr_page_break = true;
            options.hr_page_break = true;
        } else if (strcmp(argv[i], "--title-from-h1") == 0) {
            cli_opt_mask.title_from_h1 = true;
            options.title_from_h1 = true;
        } else if (strcmp(argv[i], "--page-break-before-footnotes") == 0) {
            cli_opt_mask.page_break_before_footnotes = true;
            options.page_break_before_footnotes = true;
        } else if (strcmp(argv[i], "--wikilink-space") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --wikilink-space requires an argument (dash, none, underscore, or space)\n");
                return 1;
            }
            cli_opt_mask.wikilink_space = true;
            if (strcmp(argv[i], "dash") == 0) {
                options.wikilink_space = 0;
            } else if (strcmp(argv[i], "none") == 0) {
                options.wikilink_space = 1;
            } else if (strcmp(argv[i], "underscore") == 0) {
                options.wikilink_space = 2;
            } else if (strcmp(argv[i], "space") == 0) {
                options.wikilink_space = 3;
            } else {
                fprintf(stderr, "Error: --wikilink-space must be one of: dash, none, underscore, space\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--wikilink-extension") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --wikilink-extension requires an argument\n");
                return 1;
            }
            cli_opt_mask.wikilink_extension = true;
            options.wikilink_extension = argv[i];
        } else if (strcmp(argv[i], "--wikilink-sanitize") == 0) {
            cli_opt_mask.wikilink_sanitize = true;
            options.wikilink_sanitize = true;
        } else if (strcmp(argv[i], "--no-wikilink-sanitize") == 0) {
            cli_opt_mask.wikilink_sanitize = true;
            options.wikilink_sanitize = false;
        } else if (strcmp(argv[i], "--transforms") == 0) {
            cli_opt_mask.enable_metadata_transforms = true;
            options.enable_metadata_transforms = true;
        } else if (strcmp(argv[i], "--no-transforms") == 0) {
            cli_opt_mask.enable_metadata_transforms = true;
            options.enable_metadata_transforms = false;
        } else if (strcmp(argv[i], "--embed-images") == 0) {
            cli_opt_mask.embed_images = true;
            options.embed_images = true;
        } else if (strcmp(argv[i], "--image-captions") == 0) {
            cli_opt_mask.enable_image_captions = true;
            options.enable_image_captions = true;
        } else if (strcmp(argv[i], "--no-image-captions") == 0) {
            cli_opt_mask.enable_image_captions = true;
            options.enable_image_captions = false;
        } else if (strcmp(argv[i], "--title-captions-only") == 0) {
            cli_opt_mask.title_captions_only = true;
            options.title_captions_only = true;
            options.enable_image_captions = true;  /* implied when title-captions-only is set */
        } else if (strcmp(argv[i], "--no-title-captions-only") == 0) {
            cli_opt_mask.title_captions_only = true;
            options.title_captions_only = false;
        } else if (strcmp(argv[i], "--base-dir") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --base-dir requires an argument\n");
                return 1;
            }
            cli_opt_mask.base_directory = true;
            options.base_directory = argv[i];
        } else if (strcmp(argv[i], "--bibliography") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --bibliography requires an argument\n");
                return 1;
            }
            /* Allocate or reallocate bibliography files array */
            if (!bibliography_files) {
                bibliography_files = malloc(bibliography_capacity * sizeof(char*));
                if (!bibliography_files) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else if (bibliography_count >= bibliography_capacity) {
                bibliography_capacity *= 2;
                char **new_files = realloc(bibliography_files, bibliography_capacity * sizeof(char*));
                if (!new_files) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                bibliography_files = new_files;
            }
            cli_opt_mask.bibliography = true;
            bibliography_files[bibliography_count++] = argv[i];
            options.enable_citations = true;  /* Enable citations when bibliography is provided */
        } else if (strcmp(argv[i], "--concordance") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --concordance requires an argument\n");
                return 1;
            }
            if (!concordance_files) {
                concordance_files = malloc(concordance_capacity * sizeof(char*));
                if (!concordance_files) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else if (concordance_count >= concordance_capacity) {
                concordance_capacity *= 2;
                char **new_files = realloc(concordance_files, concordance_capacity * sizeof(char*));
                if (!new_files) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                concordance_files = new_files;
            }
            cli_opt_mask.concordance = true;
            concordance_files[concordance_count++] = argv[i];
            options.enable_indices = true;
            options.enable_textindex_syntax = true;
        } else if (strcmp(argv[i], "--csl") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --csl requires an argument\n");
                return 1;
            }
            cli_opt_mask.csl_file = true;
            options.csl_file = argv[i];
            options.enable_citations = true;  /* Enable citations when CSL is provided */
        } else if (strcmp(argv[i], "--no-bibliography") == 0) {
            cli_opt_mask.suppress_bibliography = true;
            options.suppress_bibliography = true;
        } else if (strcmp(argv[i], "--link-citations") == 0) {
            cli_opt_mask.link_citations = true;
            options.link_citations = true;
        } else if (strcmp(argv[i], "--show-tooltips") == 0) {
            cli_opt_mask.show_tooltips = true;
            options.show_tooltips = true;
        } else if (strcmp(argv[i], "--indices") == 0) {
            cli_opt_mask.indices = true;
            options.enable_indices = true;
            options.enable_mmark_index_syntax = true;
            options.enable_textindex_syntax = true;
            options.enable_leanpub_index_syntax = true;
        } else if (strcmp(argv[i], "--no-indices") == 0) {
            cli_opt_mask.indices = true;
            options.enable_indices = false;
        } else if (strcmp(argv[i], "--no-index") == 0) {
            cli_opt_mask.suppress_index = true;
            options.suppress_index = true;
        } else if (strcmp(argv[i], "--meta-file") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: --meta-file requires an argument\n");
                return 1;
            }
            meta_file = argv[i];
        } else if (strncmp(argv[i], "--meta", 6) == 0) {
            const char *arg_value;
            if (strlen(argv[i]) > 6 && argv[i][6] == '=') {
                /* --meta=KEY=VALUE format */
                arg_value = argv[i] + 7;
            } else {
                /* --meta KEY=VALUE format */
                if (++i >= argc) {
                    fprintf(stderr, "Error: --meta requires an argument\n");
                    return 1;
                }
                arg_value = argv[i];
            }
            apex_metadata_item *new_meta = apex_parse_command_metadata(arg_value);
            if (new_meta) {
                /* Merge with existing command-line metadata */
                if (cmdline_metadata) {
                    apex_metadata_item *merged = apex_merge_metadata(cmdline_metadata, new_meta, NULL);
                    apex_free_metadata(cmdline_metadata);
                    apex_free_metadata(new_meta);
                    cmdline_metadata = merged;
                } else {
                    cmdline_metadata = new_meta;
                }
            }
        } else if (strcmp(argv[i], "--combine") == 0) {
            combine_mode = true;
        } else if (strcmp(argv[i], "--mmd-merge") == 0) {
            mmd_merge_mode = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* Positional argument: input file(s) */
            if (combine_mode) {
                if (combine_file_count >= combine_file_capacity) {
                    size_t new_cap = combine_file_capacity ? combine_file_capacity * 2 : 8;
                    char **tmp = realloc(combine_files, new_cap * sizeof(char *));
                    if (!tmp) {
                        fprintf(stderr, "Error: Memory allocation failed\n");
                        return 1;
                    }
                    combine_files = tmp;
                    combine_file_capacity = new_cap;
                }
                combine_files[combine_file_count++] = argv[i];
            } else if (mmd_merge_mode) {
                if (mmd_merge_file_count >= mmd_merge_file_capacity) {
                    size_t new_cap = mmd_merge_file_capacity ? mmd_merge_file_capacity * 2 : 8;
                    char **tmp = realloc(mmd_merge_files, new_cap * sizeof(char *));
                    if (!tmp) {
                        fprintf(stderr, "Error: Memory allocation failed\n");
                        return 1;
                    }
                    mmd_merge_files = tmp;
                    mmd_merge_file_capacity = new_cap;
                }
                mmd_merge_files[mmd_merge_file_count++] = argv[i];
            } else {
                /* Single-file mode: last positional wins (for compatibility) */
                input_file = argv[i];
            }
        }
    }

    /* If --combine was provided but no files, error out early */
    if (combine_mode && combine_file_count == 0) {
        fprintf(stderr, "Error: --combine requires at least one input file\n");
        return 1;
    }

    if (options.xhtml && options.strict_xhtml) {
        fprintf(stderr, "Error: --xhtml and --strict-xhtml cannot be used together (use --strict-xhtml alone).\n");
        return 1;
    }

    /* --combine and --mmd-merge are mutually exclusive */
    if (combine_mode && mmd_merge_mode) {
        fprintf(stderr, "Error: --combine and --mmd-merge cannot be used together\n");
        return 1;
    }

    if (cli_info && (cli_extract_meta || extract_meta_value_key)) {
        fprintf(stderr, "Error: --info cannot be combined with --extract-meta or --extract-meta-value\n");
        return 1;
    }
    if (cli_extract_meta && extract_meta_value_key) {
        fprintf(stderr, "Error: --extract-meta cannot be combined with --extract-meta-value\n");
        return 1;
    }

    /* Handle theme listing before normal conversion */
    if (list_themes) {
        apex_cli_print_highlight_themes();
        return 0;
    }

    /* Extract document metadata and exit (before plugin/network subcommands) */
    if (cli_extract_meta || extract_meta_value_key) {
        if (mmd_merge_mode) {
            fprintf(stderr, "Error: --extract-meta/--extract-meta-value cannot be used with --mmd-merge\n");
            return 1;
        }
        bool has_files = (input_file != NULL) || (combine_mode && combine_file_count > 0);
        if (!has_files) {
            fprintf(stderr, "Error: --extract-meta requires at least one input file\n");
            return 1;
        }

        apex_metadata_item *docmeta = NULL;
        int merge_rc;
        if (combine_mode) {
            merge_rc = apex_cli_merge_doc_metadata_from_files(options.mode, combine_files, combine_file_count, &docmeta);
        } else {
            char *one_path[1];
            one_path[0] = (char *)input_file;
            merge_rc = apex_cli_merge_doc_metadata_from_files(options.mode, one_path, 1, &docmeta);
        }
        if (merge_rc != 0) {
            if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
            return 1;
        }

        if (cli_extract_meta) {
            apex_metadata_fprint_yaml_document(stdout, docmeta);
            if (docmeta) apex_free_metadata(docmeta);
            if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
            return 0;
        }

        const char *val = apex_metadata_get(docmeta, extract_meta_value_key);
        if (!val) {
            fprintf(stderr, "Error: metadata key '%s' not found\n", extract_meta_value_key);
            if (docmeta) apex_free_metadata(docmeta);
            if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
            return 1;
        }
        printf("%s\n", val);
        if (docmeta) apex_free_metadata(docmeta);
        if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
        return 0;
    }

    /* --info with no input files: print and exit */
    if (cli_info && !input_file && !combine_mode && !mmd_merge_mode) {
        apex_cli_print_info(stdout, &options, plugins_cli_override, plugins_cli_value, meta_file, cmdline_metadata);
        if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
        return 0;
    }

    /* Handle plugin listing/installation/uninstallation commands before normal conversion */
    if (list_plugins || install_plugin_id || uninstall_plugin_id) {
        if ((install_plugin_id && uninstall_plugin_id) || (install_plugin_id && list_plugins && uninstall_plugin_id)) {
            fprintf(stderr, "Error: --install-plugin and --uninstall-plugin cannot be combined.\n");
            return 1;
        }

        /* Determine plugins root: $XDG_CONFIG_HOME/apex/plugins or ~/.config/apex/plugins */
        const char *xdg = getenv("XDG_CONFIG_HOME");
        char root[1024];
        if (xdg && *xdg) {
            snprintf(root, sizeof(root), "%s/apex/plugins", xdg);
        } else {
            const char *home = getenv("HOME");
            if (!home || !*home) {
                fprintf(stderr, "Error: HOME not set; cannot determine plugin directory.\n");
                return 1;
            }
            snprintf(root, sizeof(root), "%s/.config/apex/plugins", home);
        }

        /* Uninstall plugin: local only, no remote directory needed */
        if (uninstall_plugin_id) {
            char target[1200];
            snprintf(target, sizeof(target), "%s/%s", root, uninstall_plugin_id);

            struct stat st;
            if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode)) {
                fprintf(stderr, "Error: plugin '%s' is not installed at %s\n", uninstall_plugin_id, target);
                return 1;
            }

            fprintf(stderr, "About to remove plugin directory:\n  %s\n", target);
            fprintf(stderr, "This will delete all files in that directory (but not any support data).\n");
            fprintf(stderr, "Proceed? [y/N]: ");
            fflush(stderr);

            char answer[16];
            if (!fgets(answer, sizeof(answer), stdin)) {
                fprintf(stderr, "Aborted.\n");
                return 1;
            }
            if (answer[0] != 'y' && answer[0] != 'Y') {
                fprintf(stderr, "Aborted.\n");
                return 1;
            }

            char rm_cmd[1400];
            snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", target);
            int rm_rc = system(rm_cmd);
            if (rm_rc != 0) {
                fprintf(stderr, "Error: failed to remove plugin directory '%s'.\n", target);
                return 1;
            }

            fprintf(stderr, "Uninstalled plugin '%s' from %s\n", uninstall_plugin_id, target);
            return 0;
        }

        /* List and install rely on the remote directory as well as local plugins */

        /* Collect installed plugin ids from all locations that apex_plugins_load()
         * would consult, in the same precedence order so that project plugins
         * override global ones with the same id.
         */
        cli_installed_plugin *installed_head = NULL;
        char **installed_ids = NULL;
        size_t installed_count = 0;
        size_t installed_cap = 0;

        /* Project-scoped (current working directory): CWD/.apex/plugins */
        char cwd[1024];
        cwd[0] = '\0';
        if (getcwd(cwd, sizeof(cwd)) != NULL && cwd[0] != '\0') {
            char cwd_plugins[1200];
            snprintf(cwd_plugins, sizeof(cwd_plugins), "%s/.apex/plugins", cwd);
            cli_collect_installed_from_root(cwd_plugins,
                                            &installed_head,
                                            &installed_ids,
                                            &installed_count,
                                            &installed_cap);
        }

        /* Project-scoped (explicit base_directory): base_directory/.apex/plugins */
        if (options.base_directory && options.base_directory[0] != '\0') {
            char base_plugins[1200];
            snprintf(base_plugins, sizeof(base_plugins), "%s/.apex/plugins", options.base_directory);
            cli_collect_installed_from_root(base_plugins,
                                            &installed_head,
                                            &installed_ids,
                                            &installed_count,
                                            &installed_cap);
        }

        /* Project-scoped (Git repository root): <git top>/.apex/plugins
         * Only used when the current directory is inside the work tree.
         */
        char *git_root = apex_cli_git_toplevel();
        if (git_root && git_root[0] != '\0' && cwd[0] != '\0') {
            size_t root_len = strlen(git_root);
            /* Ensure the Git root is a parent of (or equal to) the current directory. */
            if (strncmp(cwd, git_root, root_len) == 0 &&
                (cwd[root_len] == '/' || cwd[root_len] == '\0')) {
                char git_plugins[1200];
                snprintf(git_plugins, sizeof(git_plugins), "%s/.apex/plugins", git_root);
                cli_collect_installed_from_root(git_plugins,
                                                &installed_head,
                                                &installed_ids,
                                                &installed_count,
                                                &installed_cap);
            }
            free(git_root);
        }

        /* User-global: same root as used for install/uninstall */
        cli_collect_installed_from_root(root,
                                        &installed_head,
                                        &installed_ids,
                                        &installed_count,
                                        &installed_cap);

        if (list_plugins && installed_head) {
            printf("## Installed Plugins\n\n");
            for (cli_installed_plugin *p = installed_head; p; p = p->next) {
                const char *print_id = p->id ? p->id : "";
                const char *print_title = p->title ? p->title : print_id;
                const char *print_author = p->author ? p->author : "";
                printf("%-20s - %s", print_id, print_title);
                if (print_author && *print_author) {
                    printf("  (author: %s)", print_author);
                }
                printf("\n");
                if (p->description && *p->description) {
                    printf("    %s\n", p->description);
                }
                if (p->homepage && *p->homepage) {
                    printf("    homepage: %s\n", p->homepage);
                }
            }
        }

        if (list_plugins) {
            printf("\n---\n\n");
            printf("## Available Plugins\n\n");
        }

        /* Check if install_plugin_id is a direct URL/shorthand - if so, skip directory fetch */
        char *normalized_repo_check = NULL;
        bool is_direct_url = false;
        if (install_plugin_id) {
            normalized_repo_check = normalize_plugin_repo_url(install_plugin_id);
            is_direct_url = (normalized_repo_check != NULL);
            if (normalized_repo_check) {
                free(normalized_repo_check);
                normalized_repo_check = NULL;
            }
        }

        apex_remote_plugin_list *plist = NULL;
        if (!is_direct_url) {
            /* Only fetch directory if we need it (list_plugins or install by ID) */
            const char *dir_url = "https://raw.githubusercontent.com/ApexMarkdown/apex-plugins/refs/heads/main/apex-plugins.json";
            plist = apex_remote_fetch_directory(dir_url);
            if (!plist && (list_plugins || install_plugin_id)) {
                fprintf(stderr, "Error: failed to fetch plugin directory from %s\n", dir_url);
                if (installed_ids) {
                    for (size_t i = 0; i < installed_count; i++) free(installed_ids[i]);
                    free(installed_ids);
                }
                return 1;
            }
        }

        if (list_plugins) {
            if (!plist) {
                fprintf(stderr, "Error: cannot list plugins without directory access.\n");
                if (installed_ids) {
                    for (size_t i = 0; i < installed_count; i++) free(installed_ids[i]);
                    free(installed_ids);
                }
                cli_free_installed_plugins(installed_head);
                return 1;
            }
            apex_remote_print_plugins_filtered(plist, (const char **)installed_ids, installed_count, "plugins");
            apex_remote_free_plugins(plist);
            if (installed_ids) {
                for (size_t i = 0; i < installed_count; i++) free(installed_ids[i]);
                free(installed_ids);
            }
            cli_free_installed_plugins(installed_head);
            return 0;
        }
        if (install_plugin_id) {
            const char *repo = NULL;
            char *normalized_repo = NULL;
            char *final_plugin_id = NULL;

            /* Check if install_plugin_id is a URL or GitHub shorthand */
            normalized_repo = normalize_plugin_repo_url(install_plugin_id);

            if (normalized_repo) {
                /* Direct URL/shorthand - use it as the repo URL */
                repo = normalized_repo;

                /* Security confirmation for out-of-directory installs */
                fprintf(stderr,
                        "Apex plugins execute unverified code. Only install plugins from trusted sources.\n"
                        "Continue? (y/n) ");
                fflush(stderr);
                char answer[8] = {0};
                if (!fgets(answer, sizeof(answer), stdin) ||
                    (answer[0] != 'y' && answer[0] != 'Y')) {
                    fprintf(stderr, "Aborted plugin install.\n");
                    free(normalized_repo);
                    if (plist) {
                        apex_remote_free_plugins(plist);
                    }
                    return 1;
                }
                /* We'll extract the plugin ID after cloning */
            } else {
                /* Not a URL - treat as directory ID and look it up */
                apex_remote_plugin *rp = apex_remote_find_plugin(plist, install_plugin_id);
                repo = apex_remote_plugin_repo(rp);
                if (!rp || !repo) {
                    fprintf(stderr, "Error: plugin '%s' not found in directory.\n", install_plugin_id);
                    if (plist) {
                        apex_remote_free_plugins(plist);
                    }
                    return 1;
                }
                final_plugin_id = strdup(install_plugin_id);
            }

            /* Determine plugins root: $XDG_CONFIG_HOME/apex/plugins or ~/.config/apex/plugins */
            const char *xdg = getenv("XDG_CONFIG_HOME");
            char root[1024];
            if (xdg && *xdg) {
                snprintf(root, sizeof(root), "%s/apex/plugins", xdg);
            } else {
                const char *home = getenv("HOME");
                if (!home || !*home) {
                    fprintf(stderr, "Error: HOME not set; cannot determine plugin install directory.\n");
                    if (normalized_repo) free(normalized_repo);
                    if (final_plugin_id) free(final_plugin_id);
                    apex_remote_free_plugins(plist);
                    return 1;
                }
                snprintf(root, sizeof(root), "%s/.config/apex/plugins", home);
            }

            /* Ensure root directory exists */
            char mkdir_cmd[1200];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", root);
            int mkrc = system(mkdir_cmd);
            if (mkrc != 0) {
                fprintf(stderr, "Error: failed to create plugin directory '%s'.\n", root);
                if (normalized_repo) free(normalized_repo);
                if (final_plugin_id) free(final_plugin_id);
                apex_remote_free_plugins(plist);
                return 1;
            }

            /* For direct URLs, we need a temporary directory name for cloning */
            /* We'll rename it after extracting the plugin ID */
            char temp_target[1200];
            if (!final_plugin_id) {
                /* Extract a temporary name from the URL for cloning */
                const char *last_slash = strrchr(repo, '/');
                const char *name_start = last_slash ? (last_slash + 1) : repo;
                const char *name_end = strstr(name_start, ".git");
                if (!name_end) name_end = name_start + strlen(name_start);
                size_t name_len = name_end - name_start;
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

            /* Refuse to overwrite existing directory */
            char test_cmd[1300];
            if (final_plugin_id) {
                snprintf(test_cmd, sizeof(test_cmd), "[ -d \"%s\" ]", temp_target);
            } else {
                /* For temp dir, check if it exists and clean it up */
                snprintf(test_cmd, sizeof(test_cmd), "[ -d \"%s\" ]", temp_target);
            }
            int exists_rc = system(test_cmd);
            if (exists_rc == 0 && final_plugin_id) {
                fprintf(stderr, "Error: plugin directory '%s' already exists. Remove it first to reinstall.\n", temp_target);
                if (normalized_repo) free(normalized_repo);
                if (final_plugin_id) free(final_plugin_id);
                apex_remote_free_plugins(plist);
                return 1;
            }

            /* Clone repo using git */
            char clone_cmd[2048];
            snprintf(clone_cmd, sizeof(clone_cmd), "git clone \"%s\" \"%s\"", repo, temp_target);
            int git_rc = system(clone_cmd);
            if (git_rc != 0) {
                fprintf(stderr, "Error: git clone failed for '%s'. Is git installed and the URL correct?\n", repo);
                if (normalized_repo) free(normalized_repo);
                if (final_plugin_id) free(final_plugin_id);
                apex_remote_free_plugins(plist);
                return 1;
            }

            /* Extract plugin ID from cloned repo if we don't have it yet */
            if (!final_plugin_id) {
                final_plugin_id = extract_plugin_id_from_repo(temp_target);
                if (!final_plugin_id) {
                    fprintf(stderr, "Error: could not determine plugin ID from repository. Make sure plugin.yml exists with an 'id' field.\n");
                    /* Clean up temp directory */
                    char rm_cmd[1300];
                    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
                    system(rm_cmd);
                    if (normalized_repo) free(normalized_repo);
                    apex_remote_free_plugins(plist);
                    return 1;
                }

                /* Move temp directory to final location */
                char final_target[1200];
                snprintf(final_target, sizeof(final_target), "%s/%s", root, final_plugin_id);

                /* Check if final location already exists */
                char final_test_cmd[1300];
                snprintf(final_test_cmd, sizeof(final_test_cmd), "[ -d \"%s\" ]", final_target);
                int final_exists_rc = system(final_test_cmd);
                if (final_exists_rc == 0) {
                    fprintf(stderr, "Error: plugin directory '%s' already exists. Remove it first to reinstall.\n", final_target);
                    char rm_cmd[1300];
                    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
                    system(rm_cmd);
                    free(final_plugin_id);
                    if (normalized_repo) free(normalized_repo);
                    apex_remote_free_plugins(plist);
                    return 1;
                }

                /* Move temp to final */
                char mv_cmd[2500];
                snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", temp_target, final_target);
                int mv_rc = system(mv_cmd);
                if (mv_rc != 0) {
                    fprintf(stderr, "Error: failed to move plugin to final location '%s'.\n", final_target);
                    char rm_cmd[1300];
                    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
                    system(rm_cmd);
                    free(final_plugin_id);
                    if (normalized_repo) free(normalized_repo);
                    apex_remote_free_plugins(plist);
                    return 1;
                }
                strncpy(temp_target, final_target, sizeof(temp_target) - 1);
                temp_target[sizeof(temp_target) - 1] = '\0';
            }

            /* After successful clone, look for a post_install hook in plugin.yml/yaml */
            char manifest[1300];
            snprintf(manifest, sizeof(manifest), "%s/plugin.yml", temp_target);
            FILE *mt = fopen(manifest, "r");
            if (!mt) {
                snprintf(manifest, sizeof(manifest), "%s/plugin.yaml", temp_target);
                mt = fopen(manifest, "r");
            }
            if (mt) {
                fclose(mt);
                apex_metadata_item *meta = apex_load_metadata_from_file(manifest);
                if (meta) {
                    const char *post_install = NULL;
                    for (apex_metadata_item *m = meta; m; m = m->next) {
                        if (strcmp(m->key, "post_install") == 0) {
                            post_install = m->value;
                            break;
                        }
                    }
                    if (post_install && *post_install) {
                        fprintf(stderr, "Running post-install hook for '%s'...\n", final_plugin_id);
                        char hook_cmd[2048];
                        snprintf(hook_cmd, sizeof(hook_cmd), "cd \"%s\" && %s", temp_target, post_install);
                        int hook_rc = system(hook_cmd);
                        if (hook_rc != 0) {
                            fprintf(stderr, "Warning: post-install hook for '%s' exited with status %d\n",
                                    final_plugin_id, hook_rc);
                        }
                    }
                    apex_free_metadata(meta);
                }
            }

            fprintf(stderr, "Installed plugin '%s' into %s\n", final_plugin_id, temp_target);
            if (normalized_repo) free(normalized_repo);
            if (final_plugin_id) free(final_plugin_id);
            apex_remote_free_plugins(plist);
            return 0;
        }
    }

    /* Handle filter listing/installation/uninstallation before normal conversion.
     * Filters are distributed from the apex-filters directory:
     *   https://github.com/ApexMarkdown/apex-filters
     * and installed into:
     *   $XDG_CONFIG_HOME/apex/filters or ~/.config/apex/filters
     */
    if (list_filters || install_filter_id || uninstall_filter_id) {
        if (install_filter_id && uninstall_filter_id) {
            fprintf(stderr, "Error: --install-filter and --uninstall-filter cannot be combined.\n");
            return 1;
        }

        /* Determine filters root: $XDG_CONFIG_HOME/apex/filters or ~/.config/apex/filters */
        const char *xdg = getenv("XDG_CONFIG_HOME");
        char root[1024];
        if (xdg && *xdg) {
            snprintf(root, sizeof(root), "%s/apex/filters", xdg);
        } else {
            const char *home = getenv("HOME");
            if (!home || !*home) {
                fprintf(stderr, "Error: HOME not set; cannot determine filter install directory.\n");
                return 1;
            }
            snprintf(root, sizeof(root), "%s/.config/apex/filters", home);
        }

        /* Uninstall filter */
        if (uninstall_filter_id) {
            char target[1200];
            snprintf(target, sizeof(target), "%s/%s", root, uninstall_filter_id);

            struct stat st;
            if (stat(target, &st) != 0) {
                /* Try with common extensions (e.g. code-includes.lua from path install) */
                const char *exts[] = { ".lua", ".py", ".rb" };
                int found = 0;
                for (size_t e = 0; e < sizeof(exts)/sizeof(exts[0]); e++) {
                    snprintf(target, sizeof(target), "%s/%s%s", root, uninstall_filter_id, exts[e]);
                    if (stat(target, &st) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    snprintf(target, sizeof(target), "%s/%s", root, uninstall_filter_id);
                    fprintf(stderr, "Error: filter '%s' is not installed at %s\n", uninstall_filter_id, target);
                    return 1;
                }
            }

            fprintf(stderr, "About to remove filter:\n  %s\n", target);
            fprintf(stderr, "Proceed? [y/N]: ");
            fflush(stderr);

            char answer[16];
            if (!fgets(answer, sizeof(answer), stdin)) {
                fprintf(stderr, "Aborted.\n");
                return 1;
            }
            if (answer[0] != 'y' && answer[0] != 'Y') {
                fprintf(stderr, "Aborted.\n");
                return 1;
            }

            if (S_ISDIR(st.st_mode)) {
                char rm_cmd[1400];
                snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", target);
                int rm_rc = system(rm_cmd);
                if (rm_rc != 0) {
                    fprintf(stderr, "Error: failed to remove filter directory '%s'.\n", target);
                    return 1;
                }
            } else {
                if (unlink(target) != 0) {
                    fprintf(stderr, "Error: failed to remove filter '%s'.\n", target);
                    return 1;
                }
            }

            fprintf(stderr, "Uninstalled filter '%s' from %s\n", uninstall_filter_id, target);
            return 0;
        }

        /* List filters: installed from root + available from remote directory */
        if (list_filters) {
            const char *dir_url = "https://raw.githubusercontent.com/ApexMarkdown/apex-filters/refs/heads/main/apex-filters.json";
            apex_remote_plugin_list *flist = apex_remote_fetch_filters_directory(dir_url);

            /* Collect installed filter ids (files and directories in root) */
            char *installed_ids[128];
            size_t installed_count = 0;
            DIR *dir = opendir(root);
            if (dir) {
                struct dirent *ent;
                while ((ent = readdir(dir)) != NULL && installed_count < 128) {
                    if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
                        continue;
                    if (strncmp(ent->d_name, ".apex_", 6) == 0)
                        continue;
                    installed_ids[installed_count] = strdup(ent->d_name);
                    if (installed_ids[installed_count])
                        installed_count++;
                }
                closedir(dir);
            }

            printf("## Installed Filters\n");
            if (installed_count == 0) {
                printf("(none)\n");
            } else {
                for (size_t i = 0; i < installed_count; i++)
                    printf("%s\n", installed_ids[i]);
            }

            printf("---\n");
            printf("## Available Filters\n");
            apex_remote_print_plugins_filtered(flist, (const char **)installed_ids, installed_count, "filters");

            for (size_t i = 0; i < installed_count; i++)
                free(installed_ids[i]);
            if (flist) apex_remote_free_plugins(flist);
            return 0;
        }

        /* Install filter */
        if (install_filter_id) {
        /* Check if install_filter_id is a direct URL/shorthand (GitHub repo) */
        char *normalized_repo = normalize_plugin_repo_url(install_filter_id);
        const char *repo = NULL;
        char *final_filter_id = NULL;
        char *filter_path = NULL;  /* optional: single file path inside repo (e.g. "src/code-includes.lua") */

        if (normalized_repo) {
            repo = normalized_repo;

            fprintf(stderr,
                    "Apex filters execute unverified code. Only install filters from trusted sources.\n"
                    "Continue? (y/n) ");
            fflush(stderr);
            char answer[8] = {0};
            if (!fgets(answer, sizeof(answer), stdin) ||
                (answer[0] != 'y' && answer[0] != 'Y')) {
                fprintf(stderr, "Aborted filter install.\n");
                free(normalized_repo);
                return 1;
            }
        } else {
            /* Not a URL - look up in the remote filters directory */
            const char *dir_url = "https://raw.githubusercontent.com/ApexMarkdown/apex-filters/refs/heads/main/apex-filters.json";
            char *json = apex_remote_fetch_json(dir_url);
            if (!json) {
                fprintf(stderr, "Error: failed to fetch filter directory from %s\n", dir_url);
                return 1;
            }

            /* Very small JSON scan: find entry with matching "id" and extract "repo" */
            const char *p = json;
            int found = 0;
            while ((p = strstr(p, "\"id\"")) != NULL) {
                const char *id_start = strchr(p, ':');
                if (!id_start) break;
                id_start++;
                while (*id_start == ' ' || *id_start == '\t') id_start++;
                if (*id_start != '\"') { p = id_start; continue; }
                id_start++;
                const char *id_end = strchr(id_start, '\"');
                if (!id_end) break;
                size_t id_len = (size_t)(id_end - id_start);

                if (strlen(install_filter_id) == id_len &&
                    strncmp(install_filter_id, id_start, id_len) == 0) {
                    /* Found matching id; search for "repo" and optional "path" in this object */
                    const char *obj_start = p;
                    char *repo_val = apex_remote_extract_string(obj_start, "repo");
                    if (!repo_val) {
                        fprintf(stderr, "Error: filter '%s' missing repo URL in directory.\n", install_filter_id);
                        free(json);
                        return 1;
                    }
                    repo = repo_val;
                    final_filter_id = strdup(install_filter_id);
                    filter_path = apex_remote_extract_string(obj_start, "path");
                    found = 1;
                    break;
                }
                p = id_end;
            }

            if (!found) {
                fprintf(stderr, "Error: filter '%s' not found in directory.\n", install_filter_id);
                free(json);
                return 1;
            }
            free(json);
        }

        /* Ensure root directory exists */
        char mkdir_cmd[1200];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", root);
        int mkrc = system(mkdir_cmd);
        if (mkrc != 0) {
            fprintf(stderr, "Error: failed to create filter directory '%s'.\n", root);
            if (normalized_repo) free(normalized_repo);
            if (final_filter_id) free(final_filter_id);
            if (filter_path) free(filter_path);
            if (repo && repo != normalized_repo) free((void *)repo);
            return 1;
        }

        /* Single-file install: clone to temp, copy path to root/<basename(path)>, remove temp */
        if (filter_path && final_filter_id) {
            const char *path_basename = strrchr(filter_path, '/');
            path_basename = path_basename ? (path_basename + 1) : filter_path;
            char final_file[1200];
            snprintf(final_file, sizeof(final_file), "%s/%s", root, path_basename);
            char test_cmd[1300];
            snprintf(test_cmd, sizeof(test_cmd), "[ -e \"%s\" ]", final_file);
            if (system(test_cmd) == 0) {
                fprintf(stderr, "Error: filter '%s' already exists at %s. Remove it first to reinstall.\n", final_filter_id, final_file);
                if (normalized_repo) free(normalized_repo);
                if (final_filter_id) free(final_filter_id);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
            char temp_target[1200];
            snprintf(temp_target, sizeof(temp_target), "%s/.apex_install_%s", root, final_filter_id);
            char clone_cmd[2048];
            snprintf(clone_cmd, sizeof(clone_cmd), "git clone --depth 1 \"%s\" \"%s\"", repo, temp_target);
            int git_rc = system(clone_cmd);
            if (git_rc != 0) {
                fprintf(stderr, "Error: git clone failed for '%s'. Is git installed and the URL correct?\n", repo);
                if (normalized_repo) free(normalized_repo);
                if (final_filter_id) free(final_filter_id);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
            char src_file[1800];
            snprintf(src_file, sizeof(src_file), "%s/%s", temp_target, filter_path);
            char cp_cmd[3000];
            snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", src_file, final_file);
            if (system(cp_cmd) != 0) {
                fprintf(stderr, "Error: failed to copy '%s' from repo to %s\n", filter_path, final_file);
                snprintf(cp_cmd, sizeof(cp_cmd), "rm -rf \"%s\"", temp_target);
                system(cp_cmd);
                if (normalized_repo) free(normalized_repo);
                if (final_filter_id) free(final_filter_id);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
            char chmod_cmd[1300];
            snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x \"%s\"", final_file);
            system(chmod_cmd);
            snprintf(cp_cmd, sizeof(cp_cmd), "rm -rf \"%s\"", temp_target);
            system(cp_cmd);
            fprintf(stderr, "Installed filter '%s' into %s\n", final_filter_id, final_file);
            if (normalized_repo) free(normalized_repo);
            if (final_filter_id) free(final_filter_id);
            if (filter_path) free(filter_path);
            if (repo && repo != normalized_repo) free((void *)repo);
            return 0;
        }

        /* Determine temporary or final target directory */
        char temp_target[1200];
        if (!final_filter_id) {
            /* Derive a temporary name from repo URL */
            const char *last_slash = strrchr(repo, '/');
            const char *name_start = last_slash ? (last_slash + 1) : repo;
            const char *name_end = strstr(name_start, ".git");
            if (!name_end) name_end = name_start + strlen(name_start);
            size_t name_len = name_end - name_start;
            if (name_len > 0 && name_len < 200) {
                char temp_name[256];
                memcpy(temp_name, name_start, name_len);
                temp_name[name_len] = '\0';
                snprintf(temp_target, sizeof(temp_target), "%s/.apex_install_%s", root, temp_name);
            } else {
                snprintf(temp_target, sizeof(temp_target), "%s/.apex_install_temp", root);
            }
        } else {
            snprintf(temp_target, sizeof(temp_target), "%s/%s", root, final_filter_id);
        }

        /* Refuse to overwrite existing directory when using a final id */
        if (final_filter_id) {
            char test_cmd[1300];
            snprintf(test_cmd, sizeof(test_cmd), "[ -d \"%s\" ]", temp_target);
            int exists_rc = system(test_cmd);
            if (exists_rc == 0) {
                fprintf(stderr, "Error: filter directory '%s' already exists. Remove it first to reinstall.\n", temp_target);
                if (normalized_repo) free(normalized_repo);
                if (final_filter_id) free(final_filter_id);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
        }

        /* Clone repo using git */
        char clone_cmd[2048];
        snprintf(clone_cmd, sizeof(clone_cmd), "git clone \"%s\" \"%s\"", repo, temp_target);
        int git_rc = system(clone_cmd);
        if (git_rc != 0) {
            fprintf(stderr, "Error: git clone failed for '%s'. Is git installed and the URL correct?\n", repo);
            if (normalized_repo) free(normalized_repo);
            if (final_filter_id) free(final_filter_id);
            if (filter_path) free(filter_path);
            if (repo && repo != normalized_repo) free((void *)repo);
            return 1;
        }

        /* If we didn't get a final id from the directory, use install_filter_id as the final name */
        if (!final_filter_id) {
            final_filter_id = strdup(install_filter_id);
            if (!final_filter_id) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                if (normalized_repo) free(normalized_repo);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
        }

        char final_target[1200];
        snprintf(final_target, sizeof(final_target), "%s/%s", root, final_filter_id);

        /* If temp_target != final_target, move into place */
        if (strcmp(temp_target, final_target) != 0) {
            char final_test_cmd[1300];
            snprintf(final_test_cmd, sizeof(final_test_cmd), "[ -d \"%s\" ]", final_target);
            int final_exists_rc = system(final_test_cmd);
            if (final_exists_rc == 0) {
                fprintf(stderr, "Error: filter directory '%s' already exists. Remove it first to reinstall.\n", final_target);
                char rm_cmd[1300];
                snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
                system(rm_cmd);
                free(final_filter_id);
                if (normalized_repo) free(normalized_repo);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }

            char mv_cmd[2500];
            snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", temp_target, final_target);
            int mv_rc = system(mv_cmd);
            if (mv_rc != 0) {
                fprintf(stderr, "Error: failed to move filter to final location '%s'.\n", final_target);
                char rm_cmd[1300];
                snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", temp_target);
                system(rm_cmd);
                free(final_filter_id);
                if (normalized_repo) free(normalized_repo);
                if (filter_path) free(filter_path);
                if (repo && repo != normalized_repo) free((void *)repo);
                return 1;
            }
        }

        fprintf(stderr, "Installed filter '%s' into %s\n", final_filter_id, final_target);

        if (normalized_repo) free(normalized_repo);
        if (final_filter_id) free(final_filter_id);
        if (filter_path) free(filter_path);
        if (repo && repo != normalized_repo) free((void *)repo);
        return 0;
        }
    }

    /* mmd-merge mode: emulate MultiMarkdown mmd_merge.pl and exit */
    if (mmd_merge_mode) {
        FILE *out = stdout;
        if (output_file) {
            out = fopen(output_file, "w");
            if (!out) {
                fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                return 1;
            }
        }

        if (mmd_merge_file_count == 0) {
            fprintf(stderr, "Error: --mmd-merge requires at least one index file\n");
            if (out != stdout) fclose(out);
            return 1;
        }

        if (cli_info) {
            apex_cli_print_info(stderr, &options, plugins_cli_override, plugins_cli_value, meta_file, cmdline_metadata);
        }

        int rc = 0;
        for (size_t i = 0; i < mmd_merge_file_count; i++) {
            const char *path = mmd_merge_files[i];
            if (!path) continue;
            if (apex_cli_mmd_merge_index(path, out) != 0) {
                rc = 1;
                break;
            }
        }

        if (out != stdout) {
            fclose(out);
        }
        return rc;
    }

    /* Combine mode: concatenate Markdown files (with includes expanded) and exit */
    if (combine_mode) {
        FILE *out = stdout;
        if (output_file) {
            out = fopen(output_file, "w");
            if (!out) {
                fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
                return 1;
            }
        }

        if (cli_info) {
            apex_cli_print_info(stderr, &options, plugins_cli_override, plugins_cli_value, meta_file, cmdline_metadata);
        }

        int rc = 0;
        bool needs_separator = false;

        for (size_t i = 0; i < combine_file_count; i++) {
            const char *path = combine_files[i];
            if (!path) continue;

            /* Detect GitBook SUMMARY.md by basename */
            char *path_copy = strdup(path);
            if (!path_copy) {
                rc = 1;
                break;
            }
            char *base = basename(path_copy);
            bool is_summary = (base && strcasecmp(base, "SUMMARY.md") == 0);

            if (is_summary) {
                if (apex_cli_combine_from_summary(path, out) != 0) {
                    rc = 1;
                    free(path_copy);
                    break;
                }
                /* SUMMARY already handles its own separation */
                needs_separator = true;
            } else {
                char *processed = apex_cli_combine_process_file(path);
                if (!processed) {
                    fprintf(stderr, "Warning: Skipping unreadable file '%s'\n", path);
                } else {
                    apex_cli_write_combined_chunk(out, processed, &needs_separator);
                    free(processed);
                }
            }

            free(path_copy);
        }

        if (out != stdout) {
            fclose(out);
        }
        return rc;
    }

    /* Set base_directory from input file if not already set */
    if (input_file && !options.base_directory) {
        char *input_path_copy = strdup(input_file);
        if (input_path_copy) {
            char *dir = dirname(input_path_copy);
            if (dir && dir[0] != '\0' && strcmp(dir, ".") != 0) {
                allocated_base_dir = strdup(dir);
                options.base_directory = allocated_base_dir;
            }
            free(input_path_copy);
        }
    }

    if (cli_info && input_file && !combine_mode && !mmd_merge_mode) {
        apex_cli_print_info(stderr, &options, plugins_cli_override, plugins_cli_value, meta_file, cmdline_metadata);
    }

    /* Set input_file_path for plugins (APEX_FILE_PATH) */
    if (input_file) {
        /* When a file is provided, use the original path (as passed in) */
        options.input_file_path = input_file;
    } else {
        /* When reading from stdin:
         * - Prefer an explicit base_directory, if set.
         * - Otherwise, leave input_file_path empty (plugins see APEX_FILE_PATH="").
         */
        if (options.base_directory && options.base_directory[0] != '\0') {
            options.input_file_path = options.base_directory;
        } else {
            options.input_file_path = NULL;
        }
    }

    /* Read input */
    size_t input_len;
    char *markdown;

    PROFILE_START(cli_total);
    if (input_file) {
        markdown = read_file(input_file, &input_len);
    } else {
        PROFILE_START(stdin_read);
        markdown = read_stdin(&input_len);
        PROFILE_END(stdin_read);
    }

    if (!markdown) {
        if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
        return 1;
    }

    /* Load metadata from:
     *   - Global config:   $XDG_CONFIG_HOME/apex/config.yml or ~/.config/apex/config.yml
     *   - Project config:  .apex/config.yml (CWD/base_directory/git root)
     *   - Explicit --meta-file (if provided)
     *
     * These are merged (global < project < explicit) and then merged with
     * document metadata and command-line metadata below.
     */
    PROFILE_START(metadata_file_load);
    apex_metadata_item *file_metadata = NULL;
    apex_metadata_item *global_config_meta = NULL;
    apex_metadata_item *project_config_meta = NULL;
    apex_metadata_item *explicit_file_meta = NULL;

    char *global_config_path = apex_cli_find_global_config();
    char *project_config_path = apex_cli_find_project_config(&options);

    if (global_config_path) {
        global_config_meta = apex_load_metadata_from_file(global_config_path);
        if (!global_config_meta) {
            fprintf(stderr, "Warning: Could not load metadata from global config '%s'\n", global_config_path);
        }
    }

    if (project_config_path) {
        project_config_meta = apex_load_metadata_from_file(project_config_path);
        if (!project_config_meta) {
            fprintf(stderr, "Warning: Could not load metadata from project config '%s'\n", project_config_path);
        }
    }

    if (meta_file) {
        explicit_file_meta = apex_load_metadata_from_file(meta_file);
        if (!explicit_file_meta) {
            fprintf(stderr, "Warning: Could not load metadata from file '%s'\n", meta_file);
        }
    }

    if (global_config_meta || project_config_meta || explicit_file_meta) {
        apex_metadata_item *lists[] = { global_config_meta, project_config_meta, explicit_file_meta };
        file_metadata = apex_merge_metadata_lists(lists, sizeof(lists) / sizeof(lists[0]));
    }

    if (global_config_meta) apex_free_metadata(global_config_meta);
    if (project_config_meta) apex_free_metadata(project_config_meta);
    if (explicit_file_meta) apex_free_metadata(explicit_file_meta);
    if (global_config_path) free(global_config_path);
    if (project_config_path) free(project_config_path);

    PROFILE_END(metadata_file_load);

    /* Extract document metadata to merge with external sources
     * We'll extract it here and then inject the merged result */
    PROFILE_START(metadata_extract_cli);
    apex_metadata_item *doc_metadata = NULL;
    size_t doc_metadata_end = 0;

    if (options.mode == APEX_MODE_MULTIMARKDOWN ||
        options.mode == APEX_MODE_KRAMDOWN ||
        apex_mode_is_unified_family(options.mode)) {
        /* Make a copy to extract metadata without modifying original */
        char *doc_copy = malloc(input_len + 1);
        if (doc_copy) {
            memcpy(doc_copy, markdown, input_len);
            doc_copy[input_len] = '\0';
            char *doc_ptr = doc_copy;
            doc_metadata = apex_extract_metadata(&doc_ptr);
            if (doc_metadata) {
                /* Calculate where metadata ended in original */
                doc_metadata_end = doc_ptr - doc_copy;
            }
            free(doc_copy);
        }
    }
    PROFILE_END(metadata_extract_cli);

    /* Merge metadata in priority order: file -> document -> command-line */
    PROFILE_START(metadata_merge);
    apex_metadata_item *merged_metadata = NULL;
    if (file_metadata || doc_metadata || cmdline_metadata) {
        apex_metadata_item *lists[] = { file_metadata, doc_metadata, cmdline_metadata };
        merged_metadata = apex_merge_metadata_lists(lists, sizeof(lists) / sizeof(lists[0]));
    }
    PROFILE_END(metadata_merge);

    /* Set bibliography files in options (NULL-terminated array) */
    if (bibliography_count > 0) {
        bibliography_files = realloc(bibliography_files, (bibliography_count + 1) * sizeof(char*));
        if (bibliography_files) {
            bibliography_files[bibliography_count] = NULL;  /* NULL terminator */
            options.bibliography_files = bibliography_files;
        }
    }

    /* Set concordance files in options (NULL-terminated array) */
    if (concordance_count > 0) {
        concordance_files = realloc(concordance_files, (concordance_count + 1) * sizeof(char*));
        if (concordance_files) {
            concordance_files[concordance_count] = NULL;
            options.concordance_files = concordance_files;
            options.enable_indices = true;
            options.enable_textindex_syntax = true;
        }
    }

    /* Set stylesheet files in options (NULL-terminated array) */
    if (stylesheet_count > 0) {
        stylesheet_files = realloc(stylesheet_files, (stylesheet_count + 1) * sizeof(char*));
        if (stylesheet_files) {
            stylesheet_files[stylesheet_count] = NULL;  /* NULL terminator */
            options.stylesheet_paths = (const char **)stylesheet_files;
            options.stylesheet_count = stylesheet_count;
        }
    }

    /* Apply metadata to options - allows per-document control of command-line options */
    /* Note: Bibliography file loading from metadata will be handled in citations extension */
    if (merged_metadata) {
        /* Snapshot argv-resolved options after wiring bibliography/stylesheet; merged metadata must not override explicit CLI flags. */
        cli_options_snapshot = options;
        apex_apply_metadata_to_options(merged_metadata, &options);
        apex_cli_restore_argv_options(&options, &cli_options_snapshot, &cli_opt_mask);
    }

    /* Re-apply explicit CLI override for plugins so it wins over metadata. */
    if (plugins_cli_override) {
        options.enable_plugins = plugins_cli_value;
    }

    /* Build enhanced markdown with merged metadata as YAML front matter.
     * CommonMark and GFM never strip front matter, so injecting it there
     * would render the metadata as document text. Decide after metadata
     * has been applied, since it can change the mode. */
    bool mode_reads_front_matter = options.mode == APEX_MODE_MULTIMARKDOWN ||
                                   apex_mode_is_kramdown_or_unified_family(options.mode);
    PROFILE_START(metadata_yaml_build);
    char *enhanced_markdown = NULL;
    size_t enhanced_len = input_len;

    if (merged_metadata && mode_reads_front_matter) {
        bool has_existing_metadata = (doc_metadata_end > 0);
        size_t metadata_start_pos = 0;
        size_t metadata_end_pos = doc_metadata_end;

        char *yaml_buf = NULL;
        size_t yaml_sz = 0;
        FILE *ym = open_memstream(&yaml_buf, &yaml_sz);
        if (ym) {
            fprintf(ym, "---\n");
            apex_metadata_fprint_yaml_mapping(ym, merged_metadata);
            fprintf(ym, "---\n");
            fclose(ym);
        }

        if (yaml_buf && yaml_sz > 0) {
            size_t yaml_pos = yaml_sz;

            if (has_existing_metadata) {
                /* Replace existing metadata */
                size_t before_len = metadata_start_pos;
                size_t after_len = input_len - metadata_end_pos;
                enhanced_len = before_len + yaml_pos + after_len;
                enhanced_markdown = malloc(enhanced_len + 1);
                if (enhanced_markdown) {
                    if (before_len > 0) {
                        memcpy(enhanced_markdown, markdown, before_len);
                    }
                    memcpy(enhanced_markdown + before_len, yaml_buf, yaml_pos);
                    if (after_len > 0) {
                        memcpy(enhanced_markdown + before_len + yaml_pos, markdown + metadata_end_pos, after_len);
                    }
                    enhanced_markdown[enhanced_len] = '\0';
                }
            } else {
                /* Prepend metadata */
                enhanced_len = yaml_pos + input_len;
                enhanced_markdown = malloc(enhanced_len + 1);
                if (enhanced_markdown) {
                    memcpy(enhanced_markdown, yaml_buf, yaml_pos);
                    memcpy(enhanced_markdown + yaml_pos, markdown, input_len);
                    enhanced_markdown[enhanced_len] = '\0';
                }
            }
            free(yaml_buf);
        }
    }
    PROFILE_END(metadata_yaml_build);

    /* Resolve AST filters configured from CLI into absolute command paths.
     * Filters live in $XDG_CONFIG_HOME/apex/filters or ~/.config/apex/filters.
     */
    char **ast_filter_commands = NULL;
    size_t ast_filter_count = 0;
    if (run_all_filters_dir || ast_filter_name_count > 0 || lua_filter_count > 0) {
        const char *xdg = getenv("XDG_CONFIG_HOME");
        char root[1024];
        if (xdg && *xdg) {
            snprintf(root, sizeof(root), "%s/apex/filters", xdg);
        } else {
            const char *home = getenv("HOME");
            if (!home || !*home) {
                fprintf(stderr, "Error: HOME not set; cannot determine filters directory.\n");
                return 1;
            }
            snprintf(root, sizeof(root), "%s/.config/apex/filters", home);
        }

        /* Ensure root directory exists when running --filters or --filter */
        struct stat st_root;
        if (stat(root, &st_root) != 0 || !S_ISDIR(st_root.st_mode)) {
            if (run_all_filters_dir || ast_filter_name_count > 0) {
                fprintf(stderr, "Error: filters directory '%s' does not exist.\n", root);
                return 1;
            }
        }

        /* Collect all filters from directory when --filters is set */
        if (run_all_filters_dir) {
            DIR *d = opendir(root);
            if (!d) {
                fprintf(stderr, "Error: cannot open filters directory '%s'\n", root);
                return 1;
            }
            struct dirent *ent;
            size_t capacity = 8;
            ast_filter_commands = malloc(capacity * sizeof(char *));
            if (!ast_filter_commands) {
                closedir(d);
                fprintf(stderr, "Error: Memory allocation failed\n");
                return 1;
            }
            while ((ent = readdir(d)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                char path[1200];
                snprintf(path, sizeof(path), "%s/%s", root, ent->d_name);
                struct stat st;
                if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
                    continue;
                }
                if (ast_filter_count == capacity) {
                    capacity *= 2;
                    char **tmp = realloc(ast_filter_commands, capacity * sizeof(char *));
                    if (!tmp) {
                        closedir(d);
                        fprintf(stderr, "Error: Memory allocation failed\n");
                        return 1;
                    }
                    ast_filter_commands = tmp;
                }
                ast_filter_commands[ast_filter_count] = strdup(path);
                if (!ast_filter_commands[ast_filter_count]) {
                    closedir(d);
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_count++;
            }
            closedir(d);
        }

        /* Resolve individual --filter NAME entries to absolute paths.
         *
         * Resolution rules:
         *   - If root/NAME is a regular file, use that directly.
         *   - If root/NAME is a directory, look for a script inside it:
         *       root/NAME/NAME
         *       root/NAME/NAME.lua
         *       root/NAME/NAME.py
         *       root/NAME/NAME.rb
         *     and use the first regular file found.
         */
        if (ast_filter_name_count > 0) {
            size_t capacity = (ast_filter_commands ? ast_filter_count : 0) + ast_filter_name_count;
            if (!ast_filter_commands) {
                ast_filter_commands = malloc(capacity * sizeof(char *));
                if (!ast_filter_commands) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else {
                char **tmp = realloc(ast_filter_commands, capacity * sizeof(char *));
                if (!tmp) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_commands = tmp;
            }

            for (size_t i = 0; i < ast_filter_name_count; i++) {
                const char *name = ast_filter_names[i];
                char path[1200];
                snprintf(path, sizeof(path), "%s/%s", root, name);
                struct stat st;
                int path_exists = (stat(path, &st) == 0);

                char resolved[1400];
                int  resolved_ok = 0;

                /* Try root/NAME as regular file first */
                if (path_exists && S_ISREG(st.st_mode)) {
                    snprintf(resolved, sizeof(resolved), "%s", path);
                    resolved_ok = 1;
                }
                /* Else try root/NAME.lua, root/NAME.py, root/NAME.rb as regular files (single-file installs) */
                if (!resolved_ok) {
                    const char *exts[] = { ".lua", ".py", ".rb" };
                    for (size_t e = 0; e < sizeof(exts)/sizeof(exts[0]); e++) {
                        char with_ext[1400];
                        snprintf(with_ext, sizeof(with_ext), "%s/%s%s", root, name, exts[e]);
                        struct stat ste;
                        if (stat(with_ext, &ste) == 0 && S_ISREG(ste.st_mode)) {
                            snprintf(resolved, sizeof(resolved), "%s", with_ext);
                            resolved_ok = 1;
                            break;
                        }
                    }
                }
                /* Else if root/NAME is a directory, look for script inside */
                if (!resolved_ok && path_exists && S_ISDIR(st.st_mode)) {
                    const char *candidates[4];
                    char buf0[1400], buf1[1400], buf2[1400], buf3[1400];

                    snprintf(buf0, sizeof(buf0), "%s/%s", path, name);
                    snprintf(buf1, sizeof(buf1), "%s/%s.lua", path, name);
                    snprintf(buf2, sizeof(buf2), "%s/%s.py", path, name);
                    snprintf(buf3, sizeof(buf3), "%s/%s.rb", path, name);

                    candidates[0] = buf0;
                    candidates[1] = buf1;
                    candidates[2] = buf2;
                    candidates[3] = buf3;

                    for (size_t c = 0; c < 4; c++) {
                        struct stat stc;
                        if (stat(candidates[c], &stc) == 0 && S_ISREG(stc.st_mode)) {
                            snprintf(resolved, sizeof(resolved), "%s", candidates[c]);
                            resolved_ok = 1;
                            break;
                        }
                    }

                    if (!resolved_ok) {
                        fprintf(stderr,
                                "Error: filter '%s' is a directory at %s but no executable script was found inside.\n",
                                name, path);
                        fprintf(stderr,
                                "Tried: %s/%s, %s/%s.lua, %s/%s.py, %s/%s.rb\n",
                                path, name, path, name, path, name, path, name);
                        return 1;
                    }
                }
                if (!resolved_ok) {
                    fprintf(stderr, "Error: filter '%s' not found at %s (or %s.lua, %s.py, %s.rb)\n",
                            name, path, path, path, path);
                    return 1;
                }

                /* Lua scripts without shebang: run via `lua "path"` */
                size_t rlen = strlen(resolved);
                int is_lua = (rlen > 4 && strcmp(resolved + rlen - 4, ".lua") == 0);
                if (is_lua) {
                    char cmd[1400];
                    snprintf(cmd, sizeof(cmd), "lua \"%s\"", resolved);
                    ast_filter_commands[ast_filter_count] = strdup(cmd);
                } else {
                    ast_filter_commands[ast_filter_count] = strdup(resolved);
                }
                if (!ast_filter_commands[ast_filter_count]) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_count++;
            }
        }

        /* Append explicit Lua filters as commands: `lua <script>` */
        if (lua_filter_count > 0) {
            size_t capacity = (ast_filter_commands ? ast_filter_count : 0) + lua_filter_count;
            if (!ast_filter_commands) {
                ast_filter_commands = malloc(capacity * sizeof(char *));
                if (!ast_filter_commands) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else {
                char **tmp = realloc(ast_filter_commands, capacity * sizeof(char *));
                if (!tmp) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_commands = tmp;
            }

            for (size_t i = 0; i < lua_filter_count; i++) {
                const char *script = lua_filter_paths[i];
                /* Build a simple 'lua "<script>"' command */
                char cmd[1400];
                snprintf(cmd, sizeof(cmd), "lua \"%s\"", script);
                ast_filter_commands[ast_filter_count] = strdup(cmd);
                if (!ast_filter_commands[ast_filter_count]) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                ast_filter_count++;
            }
        }

        if (ast_filter_count > 0) {
            options.ast_filter_commands = (const char **)ast_filter_commands;
            options.ast_filter_count = ast_filter_count;
            options.ast_filter_strict = ast_filters_strict;
        }
    }

    /* Attach any collected script tags to options as a NULL-terminated array */
    if (script_tags) {
        /* Ensure NULL terminator */
        script_tags = realloc(script_tags, (script_tag_count + 1) * sizeof(char *));
        if (!script_tags) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return 1;
        }
        script_tags[script_tag_count] = NULL;
        options.script_tags = script_tags;
    }

    /* Use enhanced markdown if we created it, otherwise use original */
    char *final_markdown = enhanced_markdown ? enhanced_markdown : markdown;
    size_t final_len = enhanced_markdown ? enhanced_len : input_len;

    /* Set progress callback if enabled */
    if (progress_enabled) {
        options.progress_callback = progress_callback;
        options.progress_user_data = NULL;
        /* Reset start time when we begin processing */
        progress_start_time = get_time_ms();
        progress_shown = false;
        last_stage = NULL;
    }

    /* Man page output must keep -- as literal double hyphen; option names must not become en-dash */
    if (options.output_format == APEX_OUTPUT_MAN || options.output_format == APEX_OUTPUT_MAN_HTML) {
        options.enable_smart_typography = false;
    }

    if (no_terminal_images_cli) {
        options.terminal_inline_images = false;
    }
    if (terminal_image_width_cli > 0) {
        options.terminal_image_width = terminal_image_width_cli;
    }
    if (no_paginate_cli) {
        options.paginate = false;
        options.paginate_symbols = false;
    } else if (paginate_symbols_cli) {
        options.paginate = true;
        options.paginate_symbols = true;
    } else if (paginate_cli) {
        options.paginate = true;
        options.paginate_symbols = false;
    }

    /* Convert to output (HTML, Markdown, terminal, etc.) */
    char *html = apex_markdown_to_html(final_markdown, final_len, &options);

    /* Check if we should show delayed progress (in case processing took > 1s but no progress was shown) */
    if (progress_enabled) {
        check_delayed_progress();
        /* Also force an update to show current progress */
        update_progress_if_needed();
    }

    /* Clear progress line before output */
    clear_progress();

    /* Cleanup */
    if (enhanced_markdown) free(enhanced_markdown);
    free(markdown);
    if (allocated_input_file_path) free(allocated_input_file_path);
    if (file_metadata) apex_free_metadata(file_metadata);
    if (doc_metadata) apex_free_metadata(doc_metadata);
    if (cmdline_metadata) apex_free_metadata(cmdline_metadata);
    if (merged_metadata) apex_free_metadata(merged_metadata);

    if (!html) {
        fprintf(stderr, "Error: Conversion failed\n");
        return 1;
    }

    bool is_terminal_output = (options.output_format == APEX_OUTPUT_TERMINAL ||
                               options.output_format == APEX_OUTPUT_TERMINAL256);
    size_t html_len = 0;
    if (html) {
        html_len = is_terminal_output ? apex_terminal_output_length() : strlen(html);
        if (html_len == 0) {
            html_len = strlen(html);
        }
    }

    /* For terminal output, optionally wrap to a fixed width when requested.
     * Precedence: CLI --width > metadata/config terminal.width > theme default.
     * (Theme files are currently not consulted for width.)
     */
    if (is_terminal_output) {
        int effective_width = 0;
        if (width_override > 0) {
            effective_width = width_override;
        } else if (options.terminal_width > 0) {
            effective_width = options.terminal_width;
        }
        if (effective_width > 0) {
            char *wrapped = wrap_ansi_to_width(html, html_len, effective_width);
            if (wrapped) {
                apex_free_string(html);
                html = wrapped;
                html_len = strlen(html);
            }
        }
    }

    /* Determine whether to paginate terminal output.
     * Pagination is only applied for terminal/terminal256 output when writing to stdout.
     * Precedence: CLI -p/--paginate OR config/metadata paginate:true.
     */
    bool paginate_effective = false;
    if (!output_file &&
        (options.output_format == APEX_OUTPUT_TERMINAL ||
         options.output_format == APEX_OUTPUT_TERMINAL256)) {
        if (!no_paginate_cli && (paginate_cli || options.paginate)) {
            paginate_effective = true;
        }
    }

    if (paginate_effective && is_terminal_output && !options.paginate_symbols &&
        terminal_output_has_graphics(html, html_len)) {
        if (getenv("APEX_DEBUG_TERMINAL")) {
            fprintf(stderr, "[APEX_DEBUG_TERMINAL] pager disabled (inline graphics in output)\n");
        }
        fprintf(stderr,
                "apex: skipping pager (inline terminal graphics require direct TTY output; "
                "less and most pagers only support ANSI color)\n");
        paginate_effective = false;
    }

    /* Write output (optionally via pager) */
    PROFILE_START(file_write);
    if (paginate_effective) {
        const char *pager_cmd = getenv("APEX_PAGER");
        if (!pager_cmd || !*pager_cmd) {
            pager_cmd = getenv("PAGER");
        }
        if (!pager_cmd || !*pager_cmd) {
            pager_cmd = "less -R";
        }

        FILE *pager = popen(pager_cmd, "w");
        if (!pager) {
            /* Fall back to direct stdout if pager cannot be started */
            fwrite(html, 1, html_len, stdout);
        } else {
            fwrite(html, 1, html_len, pager);
            pclose(pager);
        }
    } else if (output_file) {
        FILE *fp = fopen(output_file, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
            apex_free_string(html);
            return 1;
        }
        fwrite(html, 1, html_len, fp);
        fclose(fp);
    } else {
        fwrite(html, 1, html_len, stdout);
        /* Don't fflush - let the system buffer for better performance */
    }
    PROFILE_END(file_write);

    PROFILE_END(cli_total);

    apex_free_string(html);

    /* Free bibliography files array */
    if (bibliography_files) {
        free(bibliography_files);
    }

    /* Free concordance files array */
    if (concordance_files) {
        free(concordance_files);
    }

    /* Free script tags array and contents */
    if (script_tags) {
        for (size_t i = 0; i < script_tag_count; i++) {
            free(script_tags[i]);
        }
        free(script_tags);
    }

    /* Free AST filter command paths allocated for this run */
    if (ast_filter_commands) {
        for (size_t i = 0; i < ast_filter_count; i++) {
            free(ast_filter_commands[i]);
        }
        free(ast_filter_commands);
    }

    /* lua_filter_paths entries are argv pointers; no need to free them here */

    /* Free base_directory if we allocated it */
    if (allocated_base_dir) {
        free(allocated_base_dir);
    }

    return 0;
}
