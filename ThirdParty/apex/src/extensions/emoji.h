/**
 * GitHub Emoji Extension for Apex
 */

#ifndef APEX_EMOJI_H
#define APEX_EMOJI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Replace :emoji: patterns with Unicode emoji or image tags
 */
char *apex_replace_emoji(const char *html);

/**
 * Replace :emoji: patterns in plain text with Unicode emoji only.
 *
 * This is suitable for non-HTML outputs (e.g. terminal rendering) where
 * image-based emoji tags are not desired. If an emoji entry has no
 * Unicode representation (image-only), the original :emoji: pattern is
 * left unchanged.
 *
 * @param text Plain text to process (UTF-8)
 * @return Newly allocated string with emoji replacements applied, or NULL
 *         on error. Caller must free the returned string.
 */
char *apex_replace_emoji_text(const char *text);

/**
 * Find emoji name from unicode emoji (reverse lookup)
 * @param unicode The unicode emoji string (UTF-8)
 * @param unicode_len Length of the unicode string in bytes
 * @return Emoji name if found, NULL otherwise
 */
const char *apex_find_emoji_name(const char *unicode, size_t unicode_len);

/**
 * Autocorrect emoji names in markdown text
 * Processes :emoji_name: patterns and corrects typos using fuzzy matching
 * @param text The markdown text to process
 * @return New string with corrected emoji names (caller must free), or NULL on error
 */
char *apex_autocorrect_emoji_names(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APEX_EMOJI_H */

