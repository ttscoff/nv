#ifndef APEX_PLUGINS_REMOTE_H
#define APEX_PLUGINS_REMOTE_H

#include <stddef.h>
#include "../include/apex/plugins.h"

typedef struct apex_remote_plugin {
    char *id;
    char *title;
    char *description;
    char *author;
    char *homepage;
    char *repo;
    struct apex_remote_plugin *next;
} apex_remote_plugin;

typedef struct apex_remote_plugin_list {
    apex_remote_plugin *head;
} apex_remote_plugin_list;

apex_remote_plugin_list *apex_remote_fetch_directory(const char *url);
apex_remote_plugin_list *apex_remote_fetch_filters_directory(const char *url);
void apex_remote_print_plugins(apex_remote_plugin_list *list);
void apex_remote_print_plugins_filtered(apex_remote_plugin_list *list,
                                        const char **installed_ids,
                                        size_t installed_count,
                                        const char *noun);
apex_remote_plugin *apex_remote_find_plugin(apex_remote_plugin_list *list, const char *id);
void apex_remote_free_plugins(apex_remote_plugin_list *list);
const char *apex_remote_plugin_repo(apex_remote_plugin *p);
char *apex_remote_fetch_json(const char *url);
char *apex_remote_extract_string(const char *obj, const char *key);

#endif /* APEX_PLUGINS_REMOTE_H */
