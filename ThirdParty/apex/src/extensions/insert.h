/**
 * Insert Extension
 * Handles ++text++ syntax (converts to <ins>text</ins>)
 * Supports IAL attributes: ++text++{: .class} → <ins markdown="span" class="class">text</ins>
 */

#ifndef APEX_INSERT_H
#define APEX_INSERT_H

/**
 * Process ++insert++ syntax in text
 * Converts ++text++ to <ins>text</ins>
 * If followed by IAL, converts to <ins markdown="span" ...>text</ins>
 * Does not interfere with CriticMarkup {++text++} syntax
 */
char *apex_process_inserts(const char *text);

#endif
