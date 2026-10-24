/**
 * Critic Markup Extension for Apex
 *
 * Supports CriticMarkup syntax for track changes:
 * {++addition++}       - added text
 * {--deletion--}       - deleted text
 * {~~old~>new~~}       - substitution
 * {==highlight==}      - highlighted text
 * {>>comment<<}        - comment/annotation
 */

#ifndef APEX_CRITIC_H
#define APEX_CRITIC_H

#include <stdbool.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Critic Markup rendering mode
 */
typedef enum {
    CRITIC_ACCEPT,      /* Accept all changes */
    CRITIC_REJECT,      /* Reject all changes */
    CRITIC_MARKUP       /* Show markup with classes */
} critic_mode_t;

/**
 * Process Critic Markup in an AST via postprocessing
 */
void apex_process_critic_markup_in_tree(cmark_node *document, critic_mode_t mode);

/**
 * Process Critic Markup in raw text (preprocessing approach)
 * Returns newly allocated string with critic markup converted to HTML
 */
char *apex_process_critic_markup_text(const char *text, critic_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* APEX_CRITIC_H */

