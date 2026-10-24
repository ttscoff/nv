#ifndef APEX_PLUGINS_H
#define APEX_PLUGINS_H

#include "../include/apex/apex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Discover and load plugins from project and user config dirs.
 * Returns NULL if no plugins are found or an error occurs. */
apex_plugin_manager *apex_plugins_load(const apex_options *options);

/* Free all plugin resources. */
void apex_plugins_free(apex_plugin_manager *manager);

/* Run all text-based plugins for the given phase over the provided text.
 * Returns newly allocated string on modification, or NULL if no changes.
 */
char *apex_plugins_run_text_phase(apex_plugin_manager *manager,
                                  apex_plugin_phase_mask phase,
                                  const char *text,
                                  const apex_options *options);

#ifdef __cplusplus
}
#endif

#endif /* APEX_PLUGINS_H */
