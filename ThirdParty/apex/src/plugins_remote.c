#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugins_remote.h"

/* Simple structures for remote plugin directory entries */

void apex_remote_free_plugins(apex_remote_plugin_list *list) {
    if (!list) return;
    apex_remote_plugin *p = list->head;
    while (p) {
        apex_remote_plugin *next = p->next;
        free(p->id);
        free(p->title);
        free(p->description);
        free(p->author);
        free(p->homepage);
        free(p->repo);
        free(p);
        p = next;
    }
    free(list);
}

/* Fetch JSON from a URL using curl. Returns malloc'd buffer or NULL. */
char *apex_remote_fetch_json(const char *url) {
    if (!url) return NULL;
    /* Use curl -fsSL to fail on HTTP errors and be quiet except for data. */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -fsSL \"%s\"", url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Error: failed to run curl. Is it installed?\n");
        return NULL;
    }

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        pclose(fp);
        return NULL;
    }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                pclose(fp);
                return NULL;
            }
            buf = nb;
        }
    }
    buf[len] = '\0';
    int rc = pclose(fp);
    if (rc != 0) {
        /* curl failed; treat as error */
        free(buf);
        fprintf(stderr, "Error: curl exited with status %d while fetching plugin directory.\n", rc);
        return NULL;
    }
    return buf;
}

/* Very small JSON helper: extract string value for a key from an object snippet.
 * Assumes JSON is well-formed and keys/values are double-quoted.
 */
char *apex_remote_extract_string(const char *obj, const char *key) {
    if (!obj || !key) return NULL;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(obj, pattern);
    if (!p) return NULL;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\"') return NULL;
    p++;
    const char *start = p;
    while (*p && *p != '\"') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
        } else {
            p++;
        }
    }
    if (*p != '\"') return NULL;
    size_t len = (size_t)(p - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* Parse array of objects from JSON; array_key is e.g. "\"plugins\"" or "\"filters\"" */
static apex_remote_plugin_list *apex_remote_parse_array(const char *json, const char *array_key) {
    if (!json || !array_key) return NULL;
    const char *p = strstr(json, array_key);
    if (!p) return NULL;
    p = strchr(p, '[');
    if (!p) return NULL;
    p++; /* move past '[' */

    apex_remote_plugin_list *list = calloc(1, sizeof(apex_remote_plugin_list));
    if (!list) return NULL;

    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) p++;
        if (!*p || *p == ']') break;
        if (*p != '{') {
            break;
        }
        const char *obj_start = p;
        int depth = 0;
        while (*p) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) {
                    p++;
                    break;
                }
            }
            p++;
        }
        if (depth != 0) {
            apex_remote_free_plugins(list);
            return NULL;
        }
        size_t obj_len = (size_t)(p - obj_start);
        char *obj = malloc(obj_len + 1);
        if (!obj) {
            apex_remote_free_plugins(list);
            return NULL;
        }
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        apex_remote_plugin *rp = calloc(1, sizeof(apex_remote_plugin));
        if (!rp) {
            free(obj);
            apex_remote_free_plugins(list);
            return NULL;
        }
        rp->id = apex_remote_extract_string(obj, "id");
        rp->title = apex_remote_extract_string(obj, "title");
        rp->description = apex_remote_extract_string(obj, "description");
        rp->author = apex_remote_extract_string(obj, "author");
        rp->homepage = apex_remote_extract_string(obj, "homepage");
        rp->repo = apex_remote_extract_string(obj, "repo");
        free(obj);

        if (!rp->id || !rp->repo) {
            /* id and repo are required for use; drop this entry */
            free(rp->id);
            free(rp->title);
            free(rp->description);
            free(rp->author);
            free(rp->homepage);
            free(rp->repo);
            free(rp);
            continue;
        }

        rp->next = list->head;
        list->head = rp;
    }

    return list;
}

/* Parse { \"plugins\": [ ... ] } */
static apex_remote_plugin_list *apex_remote_parse_directory(const char *json) {
    if (!json) return NULL;
    if (!strstr(json, "\"plugins\"")) {
        fprintf(stderr, "Error: plugin directory JSON missing \"plugins\" key.\n");
        return NULL;
    }
    return apex_remote_parse_array(json, "\"plugins\"");
}

/* Parse { \"filters\": [ ... ] } - same shape as plugins (id, title, description, author, homepage, repo) */
static apex_remote_plugin_list *apex_remote_parse_filters_directory(const char *json) {
    if (!json) return NULL;
    const char *p = strstr(json, "\"filters\"");
    if (!p) {
        fprintf(stderr, "Error: filter directory JSON missing \"filters\" key.\n");
        return NULL;
    }
    return apex_remote_parse_array(json, "\"filters\"");
}

/* Public helpers used by CLI */

apex_remote_plugin_list *apex_remote_fetch_directory(const char *url) {
    char *json = apex_remote_fetch_json(url);
    if (!json) return NULL;
    apex_remote_plugin_list *list = apex_remote_parse_directory(json);
    free(json);
    return list;
}

apex_remote_plugin_list *apex_remote_fetch_filters_directory(const char *url) {
    char *json = apex_remote_fetch_json(url);
    if (!json) return NULL;
    apex_remote_plugin_list *list = apex_remote_parse_filters_directory(json);
    free(json);
    return list;
}

void apex_remote_print_plugins_filtered(apex_remote_plugin_list *list,
                                        const char **installed_ids,
                                        size_t installed_count,
                                        const char *noun) {
    if (!list || !list->head) {
        fprintf(stderr, "No %s found in remote directory.\n", noun ? noun : "plugins");
        return;
    }
    for (apex_remote_plugin *p = list->head; p; p = p->next) {
        int skip = 0;
        if (installed_ids && installed_count > 0 && p->id) {
            for (size_t i = 0; i < installed_count; i++) {
                if (installed_ids[i] && strcmp(installed_ids[i], p->id) == 0) {
                    skip = 1;
                    break;
                }
            }
        }
        if (skip) continue;

        const char *title = p->title ? p->title : p->id;
        const char *author = p->author ? p->author : "";
        printf("%-20s - %s", p->id, title);
        if (author && *author) {
            printf("  (author: %s)", author);
        }
        printf("\n");
        if (p->description && *p->description) {
            printf("    %s\n", p->description);
        }
        if (p->homepage && *p->homepage) {
            printf("    homepage: %s\n", p->homepage);
        } else if (p->repo && *p->repo) {
            printf("    repo: %s\n", p->repo);
        }
    }
}

void apex_remote_print_plugins(apex_remote_plugin_list *list) {
    apex_remote_print_plugins_filtered(list, NULL, 0, NULL);
}

apex_remote_plugin *apex_remote_find_plugin(apex_remote_plugin_list *list, const char *id) {
    if (!list || !id) return NULL;
    for (apex_remote_plugin *p = list->head; p; p = p->next) {
        if (p->id && strcmp(p->id, id) == 0) {
            return p;
        }
    }
    return NULL;
}

const char *apex_remote_plugin_repo(apex_remote_plugin *p) {
    if (!p) return NULL;
    return p->repo;
}

static char *apex_plugin_dup_optional(const char *s) {
    return s ? strdup(s) : NULL;
}

static apex_plugin_info apex_plugin_info_from_remote(const apex_remote_plugin *p) {
    apex_plugin_info info;
    memset(&info, 0, sizeof(info));
    if (!p) return info;
    info.id = apex_plugin_dup_optional(p->id);
    info.title = apex_plugin_dup_optional(p->title);
    info.description = apex_plugin_dup_optional(p->description);
    info.author = apex_plugin_dup_optional(p->author);
    info.homepage = apex_plugin_dup_optional(p->homepage);
    info.repo = apex_plugin_dup_optional(p->repo);
    return info;
}

static apex_plugin_catalog *apex_plugin_catalog_from_remote_list(apex_remote_plugin_list *list) {
    if (!list) return NULL;

    size_t count = 0;
    for (apex_remote_plugin *p = list->head; p; p = p->next) {
        count++;
    }
    if (count == 0) {
        apex_plugin_catalog *empty = calloc(1, sizeof(apex_plugin_catalog));
        return empty;
    }

    apex_plugin_catalog *catalog = calloc(1, sizeof(apex_plugin_catalog));
    if (!catalog) return NULL;

    catalog->items = calloc(count, sizeof(apex_plugin_info));
    if (!catalog->items) {
        free(catalog);
        return NULL;
    }

    /* Linked list is built head-first; reverse into stable array order. */
    apex_remote_plugin **nodes = calloc(count, sizeof(apex_remote_plugin *));
    if (!nodes) {
        free(catalog->items);
        free(catalog);
        return NULL;
    }
    size_t i = 0;
    for (apex_remote_plugin *p = list->head; p; p = p->next) {
        nodes[i++] = p;
    }
    for (size_t j = 0; j < count; j++) {
        catalog->items[j] = apex_plugin_info_from_remote(nodes[count - 1 - j]);
    }
    catalog->count = count;
    free(nodes);
    return catalog;
}

void apex_plugin_catalog_free(apex_plugin_catalog *catalog) {
    if (!catalog) return;
    if (catalog->items) {
        for (size_t i = 0; i < catalog->count; i++) {
            free(catalog->items[i].id);
            free(catalog->items[i].title);
            free(catalog->items[i].description);
            free(catalog->items[i].author);
            free(catalog->items[i].homepage);
            free(catalog->items[i].repo);
        }
        free(catalog->items);
    }
    free(catalog);
}

apex_plugin_catalog *apex_plugin_catalog_fetch_url(const char *url) {
    if (!url) return NULL;
    apex_remote_plugin_list *list = apex_remote_fetch_directory(url);
    if (!list) return NULL;
    apex_plugin_catalog *catalog = apex_plugin_catalog_from_remote_list(list);
    apex_remote_free_plugins(list);
    return catalog;
}

apex_plugin_catalog *apex_plugin_catalog_fetch_default(void) {
    return apex_plugin_catalog_fetch_url(APEX_PLUGIN_DIRECTORY_URL);
}

