/**
 * GitHub Emoji Extension for Apex
 * Complete implementation with 861 emoji mappings
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "emoji_data.h"

/* Forward declarations */
static void normalize_emoji_name(char *name);
static int is_table_alignment_pattern(const char *start, const char *end);
static int is_inside_html_attribute(const char *pos, const char *start);
static int emoji_shortcode_is_bounded(const char *text_start, const char *open_colon,
                                      const char *close_colon);
static int emoji_name_is_all_digits(const char *name, size_t len);
static int emoji_fuzzy_max_distance(size_t name_len);

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static int looks_like_list_marker(const char *p) {
    if (!*p) return 0;
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return 0;
}

/** True if we're at the start of a line that is an indented code block (4+ spaces or tab). */
static int line_is_indented_code_block(const char *read) {
    if (!*read) return 0;
    if (*read == '\t')
        return !looks_like_list_marker(read + 1);
    if (read[0] != ' ' || read[1] != ' ' || read[2] != ' ' || read[3] != ' ')
        return 0;
    const char *content = read + 4;
    while (*content == ' ') content++;
    return *content && !looks_like_list_marker(content);
}

/**
 * True if :name: is a bounded shortcode (not inside a word or digit run).
 * Left of the opening colon and right of the closing colon must not be alphanumeric.
 */
static int emoji_shortcode_is_bounded(const char *text_start, const char *open_colon,
                                      const char *close_colon) {
    if (!text_start || !open_colon || !close_colon || open_colon < text_start) return 0;
    if (open_colon > text_start && isalnum((unsigned char)open_colon[-1])) return 0;
    if (close_colon[1] != '\0' && isalnum((unsigned char)close_colon[1])) return 0;
    return 1;
}

/** True if name is non-empty and consists only of ASCII digits. */
static int emoji_name_is_all_digits(const char *name, size_t len) {
    if (!name || len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)name[i])) return 0;
    }
    return 1;
}

/**
 * Cap fuzzy Levenshtein distance by name length. Distance 4 on 2-char tokens
 * (e.g. "00" -> "100") is what corrupted timecodes. Names shorter than 3
 * characters are exact-match only; longer names allow roughly one edit per
 * three characters (so :smlie: can still reach :smile: at distance 2).
 */
static int emoji_fuzzy_max_distance(size_t name_len) {
    if (name_len < 3) return 0; /* exact matches only for very short names */
    int scaled = (int)((name_len + 2) / 3); /* 3-4 -> 1, 5-7 -> 2, ... */
    if (scaled < 1) scaled = 1;
    if (scaled > 4) scaled = 4;
    return scaled;
}

/**
 * Find emoji entry by name
 * Returns pointer to emoji_entry or NULL if not found
 */
static const emoji_entry *find_emoji_entry(const char *name, int len) {
    for (int i = 0; complete_emoji_map[i].name; i++) {
        if (strlen(complete_emoji_map[i].name) == (size_t)len &&
            strncmp(complete_emoji_map[i].name, name, len) == 0) {
            return &complete_emoji_map[i];
        }
    }
    return NULL;
}

/**
 * Check if we're inside a header tag (h1-h6)
 * Simple state machine to track if we're between <h1> and </h1> etc.
 */
static int is_in_header(const char *pos, const char *start) {
    /* Look backwards for opening header tag */
    const char *p = pos;
    int depth = 0;

    while (p >= start) {
        if (p[0] == '>' && p > start + 3) {
            /* Check if this is a closing tag */
            if (p[-1] == '/' && p[-2] == '<') {
                /* Check for </h1> through </h6> */
                if (p >= start + 5) {
                    if ((p[-3] == 'h' || p[-3] == 'H') &&
                        p[-4] >= '1' && p[-4] <= '6') {
                        depth--;
                        if (depth < 0) return 0; /* Outside header */
                    }
                }
            } else if (p > start + 2) {
                /* Check for <h1> through <h6> */
                if (p[-1] == 'h' || p[-1] == 'H') {
                    if (p >= start + 3 && p[-2] >= '1' && p[-2] <= '6' && p[-3] == '<') {
                        depth++;
                        if (depth > 0) return 1; /* Inside header */
                    }
                }
            }
        }
        p--;
    }
    return 0;
}

/**
 * Check if we're inside an HTML tag attribute
 * Returns 1 if inside an attribute value, 0 otherwise
 *
 * Handles both raw HTML (<img>) and HTML-encoded tags (&lt;img&gt;)
 */
static int is_inside_html_attribute(const char *pos, const char *start) {
    if (!pos || !start || pos < start) return 0;

    /* First, check if we're inside a tag (between < and > or &lt; and &gt;) */
    const char *p = pos - 1;
    const char *tag_start = NULL;
    const char *tag_end = NULL;

    /* Find nearest < or > before pos, or &lt; or &gt; */
    while (p >= start) {
        if (*p == '>') {
            tag_end = p;
            break;
        } else if (*p == '<') {
            tag_start = p;
            break;
        } else if (p >= start + 3 && p[-2] == '&' && p[-1] == 'l' && *p == 't' && p[1] == ';') {
            /* Found &lt; (encoded <) - tag_start is at the & */
            tag_start = p - 2;
            break;
        } else if (p >= start + 3 && p[-2] == '&' && p[-1] == 'g' && *p == 't' && p[1] == ';') {
            /* Found &gt; (encoded >) - tag_end is at the & */
            tag_end = p - 2;
            break;
        }
        p--;
    }

    /* If > comes before <, we're outside any tag */
    if (tag_end && (!tag_start || tag_end > tag_start)) {
        return 0;
    }

    /* If we're not inside a tag, we can't be in an attribute */
    if (!tag_start) {
        return 0;
    }

    /* Now look backwards from pos to find the nearest = sign within this tag */
    p = pos - 1;
    const char *equals_pos = NULL;
    while (p > tag_start) {
        if (*p == '=') {
            equals_pos = p;
            break;
        } else if (*p == '>') {
            /* Hit tag end, no = found before this */
            return 0;
        } else if (p >= start + 3 && p[-2] == '&' && p[-1] == 'g' && *p == 't' && p[1] == ';') {
            /* Hit &gt; (encoded tag end), no = found before this */
            return 0;
        }
        p--;
    }

    if (!equals_pos) {
        return 0;  /* No = found, not in an attribute */
    }

    /* Check what comes after the = */
    const char *after_equals = equals_pos + 1;
    /* Skip whitespace */
    while (after_equals < pos && isspace((unsigned char)*after_equals)) {
        after_equals++;
    }

    if (after_equals >= pos) {
        return 0;  /* pos is at or before the value starts */
    }

    /* Check if it's a quoted attribute */
    if (*after_equals == '"' || *after_equals == '\'') {
        char quote = *after_equals;
        const char *value_start = after_equals + 1;  /* After opening quote */

        /* If pos is after the opening quote, check if we're inside */
        if (pos > value_start) {
            /* Look for the closing quote - scan forward from value_start */
            const char *quote_end = value_start;
            while (quote_end < pos && *quote_end != quote && *quote_end != '\0') {
                quote_end++;
            }

            /* If we haven't found the closing quote by the time we reach pos, we're inside */
            if (quote_end >= pos || *quote_end != quote) {
                return 1;
            }
            /* If quote_end < pos and *quote_end == quote, we found the closing quote before pos, so we're not inside */
        } else if (pos == value_start) {
            /* pos is exactly at the start of the value (right after opening quote) */
            /* Check if there's a closing quote immediately */
            if (pos[0] == quote) {
                return 0;  /* Empty attribute value */
            }
            /* Otherwise we're inside (at the start of the value) */
            return 1;
        }
    } else {
        /* Unquoted attribute - value is between = and next space or > or &gt; */
        const char *value_start = after_equals;
        if (pos > value_start) {
            const char *value_end = value_start;
            while (value_end < pos && !isspace((unsigned char)*value_end) && *value_end != '>') {
                /* Also check for &gt; */
                if (value_end >= start + 3 && value_end[-2] == '&' && value_end[-1] == 'g' && *value_end == 't' && value_end[1] == ';') {
                    break;
                }
                value_end++;
            }
            /* If pos is in this unquoted value, return 1 */
            if (pos <= value_end) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Find emoji name from unicode emoji (reverse lookup)
 * Compares the unicode string against the emoji map
 */
const char *apex_find_emoji_name(const char *unicode, size_t unicode_len) {
    if (!unicode || unicode_len == 0) return NULL;

    const char *best_match = NULL;
    size_t best_match_len = 0;

    for (int i = 0; complete_emoji_map[i].name; i++) {
        const char *emoji_unicode = complete_emoji_map[i].unicode;
        if (emoji_unicode) {
            size_t emoji_len = strlen(emoji_unicode);
            /* Check if the unicode matches (exact match) */
            if (emoji_len == unicode_len &&
                strncmp(emoji_unicode, unicode, unicode_len) == 0) {
                size_t name_len = strlen(complete_emoji_map[i].name);
                /* Prefer longer names (more descriptive) over shorter ones (like "+1" vs "thumbsup") */
                if (!best_match || name_len > best_match_len) {
                    best_match = complete_emoji_map[i].name;
                    best_match_len = name_len;
                }
            }
        }
    }
    return best_match;
}

/**
 * Replace :emoji: patterns in HTML
 * Handles both unicode and image-based emojis
 */
char *apex_replace_emoji(const char *html) {
    if (!html) return NULL;

    size_t capacity = strlen(html) * 3;  /* Extra space for image tags */
    char *output = malloc(capacity);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_tag = false;  /* Skip emoji inside <code>...</code> and <pre>...</pre> */

    while (*read) {
        /* Track <code> and <pre> tags - skip emoji replacement inside code */
        if (*read == '<' && read[1]) {
            if (read[6] && read[1] == '/' && read[2] == 'c' && read[3] == 'o' && read[4] == 'd' && read[5] == 'e' && read[6] == '>') {
                in_code_tag = false;
            } else if (read[5] && read[1] == '/' && read[2] == 'p' && read[3] == 'r' && read[4] == 'e' && read[5] == '>') {
                in_code_tag = false;
            } else if (read[5] && read[1] == 'c' && read[2] == 'o' && read[3] == 'd' && read[4] == 'e' &&
                       (read[5] == '>' || read[5] == ' ' || read[5] == '\t')) {
                in_code_tag = true;
            } else if (read[4] && read[1] == 'p' && read[2] == 'r' && read[3] == 'e' &&
                       (read[4] == '>' || read[4] == ' ' || read[4] == '\t')) {
                in_code_tag = true;
            }
        }
        if (in_code_tag) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        /* Skip emoji inside HTML comments (citation placeholders are
         * <!--CITE:key-->; a key like "Wayne:" contains ":Wayne:" and would
         * otherwise fuzzy-match to :wave: → <!--CITE👋-->). */
        if (read[0] == '<' && read[1] == '!' && read[2] == '-' && read[3] == '-') {
            const char *comment_end = strstr(read + 4, "-->");
            if (comment_end) {
                size_t comment_len = (size_t)(comment_end + 3 - read);
                if (comment_len <= remaining) {
                    memcpy(write, read, comment_len);
                    write += comment_len;
                    remaining -= comment_len;
                    read = comment_end + 3;
                    continue;
                }
            }
        }

        /* Check if we're inside an index placeholder <!--IDX:...--> - if so, skip emoji processing */
        if (read >= html + 7 && strncmp(read - 7, "<!--IDX:", 8) == 0) {
            /* Find the end of the placeholder */
            const char *placeholder_end = strstr(read, "-->");
            if (placeholder_end) {
                /* Copy the entire placeholder as-is */
                size_t placeholder_len = placeholder_end + 3 - read;
                if (placeholder_len <= remaining) {
                    memcpy(write, read, placeholder_len);
                    write += placeholder_len;
                    remaining -= placeholder_len;
                    read = placeholder_end + 3;
                    continue;
                }
            }
        }

        if (*read == ':') {
            /* Check if we're inside an HTML tag attribute - if so, skip emoji processing */
            if (is_inside_html_attribute(read, html)) {
                /* Inside HTML attribute, copy as-is */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Also check if we're inside an <img> tag (even if not in an attribute) */
            /* This prevents processing emoji patterns that are part of img tag content */
            /* Handles both raw HTML (<img>) and HTML-encoded tags (&lt;img&gt;) */
            const char *img_check = read - 1;
            while (img_check >= html && img_check > read - 50) {
                if (*img_check == '>') {
                    break;  /* Hit tag end, we're outside */
                } else if (*img_check == '<') {
                    /* Check if this is an <img> tag */
                    if (img_check + 4 < read &&
                        (img_check[1] == 'i' || img_check[1] == 'I') &&
                        (img_check[2] == 'm' || img_check[2] == 'M') &&
                        (img_check[3] == 'g' || img_check[3] == 'G') &&
                        (isspace((unsigned char)img_check[4]) || img_check[4] == '>')) {
                        /* We're inside an <img> tag - skip emoji processing */
                        if (remaining > 0) {
                            *write++ = *read++;
                            remaining--;
                        } else {
                            read++;
                        }
                        continue;
                    }
                    break;
                } else if (img_check >= html + 3 && img_check[-2] == '&' && img_check[-1] == 'l' && *img_check == 't' && img_check[1] == ';') {
                    /* Found &lt; (encoded <) - check if this is an &lt;img&gt; tag */
                    const char *tag_start = img_check - 2;
                    if (tag_start + 6 < read &&
                        (tag_start[3] == 'i' || tag_start[3] == 'I') &&
                        (tag_start[4] == 'm' || tag_start[4] == 'M') &&
                        (tag_start[5] == 'g' || tag_start[5] == 'G') &&
                        (isspace((unsigned char)tag_start[6]) || (tag_start + 6 < read && tag_start[6] == '&'))) {
                        /* We're inside an &lt;img&gt; tag - skip emoji processing */
                        if (remaining > 0) {
                            *write++ = *read++;
                            remaining--;
                        } else {
                            read++;
                        }
                        continue;
                    }
                    break;
                }
                img_check--;
            }

            /* Look for closing : */
            const char *end = strchr(read + 1, ':');
            if (end && (end - read) < 50 && emoji_shortcode_is_bounded(html, read, end)) {
                /* Extract emoji name */
                int name_len = (int)(end - (read + 1));
                const char *name_start = read + 1;

                /* Validate: must have at least one character and no spaces */
                if (name_len > 0) {
                    /* Check for spaces in the name */
                    int has_space = 0;
                    for (int i = 0; i < name_len; i++) {
                        if (name_start[i] == ' ' || name_start[i] == '\t' || name_start[i] == '\n') {
                            has_space = 1;
                            break;
                        }
                    }

                    /* Skip if it contains only table alignment characters (pipes, dashes, colons) */
                    if (!has_space && is_table_alignment_pattern(name_start, end)) {
                        /* This is a table alignment pattern like :---:, :|:, :|---:, etc. */
                        /* Copy the colon pair as-is */
                        size_t pattern_len = end - read + 1;
                        if (pattern_len < remaining) {
                            memcpy(write, read, pattern_len);
                            write += pattern_len;
                            remaining -= pattern_len;
                        }
                        read = end + 1;
                        continue;
                    }

                    if (!has_space) {
                        /* Normalize the name for comparison */
                        char normalized[64];
                        if ((size_t)name_len >= sizeof(normalized)) {
                            name_len = sizeof(normalized) - 1;
                        }
                        memcpy(normalized, name_start, name_len);
                        normalized[name_len] = '\0';
                        normalize_emoji_name(normalized);
                        size_t normalized_len = strlen(normalized);

                        const emoji_entry *entry = find_emoji_entry(normalized, (int)normalized_len);

                        if (entry) {
                            int in_header = is_in_header(read, html);

                            if (entry->unicode) {
                                /* Unicode emoji */
                                size_t emoji_len = strlen(entry->unicode);
                                if (emoji_len < remaining) {
                                    memcpy(write, entry->unicode, emoji_len);
                                    write += emoji_len;
                                    remaining -= emoji_len;
                                    read = end + 1;
                                    continue;
                                } else {
                                    /* Not enough space, copy original pattern as-is */
                                    size_t pattern_len = end - read + 1;
                                    if (pattern_len < remaining) {
                                        memcpy(write, read, pattern_len);
                                        write += pattern_len;
                                        remaining -= pattern_len;
                                    }
                                    read = end + 1;
                                    continue;
                                }
                            } else if (entry->image_url) {
                                /* Image-based emoji */
                                const char *img_tag;
                                if (in_header) {
                                    /* In header: use em units for sizing */
                                    img_tag = "<img class=\"emoji\" src=\"%s\" alt=\":%s:\" style=\"height: 1em; width: auto; vertical-align: middle;\">";
                                } else {
                                    /* Regular text: use fixed size */
                                    img_tag = "<img class=\"emoji\" src=\"%s\" alt=\":%s:\" height=\"20\" width=\"20\" align=\"absmiddle\">";
                                }

                                int needed = snprintf(write, remaining, img_tag, entry->image_url, entry->name);
                                if (needed > 0 && (size_t)needed < remaining) {
                                    write += needed;
                                    remaining -= needed;
                                    read = end + 1;
                                    continue;
                                } else {
                                    /* Not enough space, copy original pattern as-is */
                                    size_t pattern_len = end - read + 1;
                                    if (pattern_len < remaining) {
                                        memcpy(write, read, pattern_len);
                                        write += pattern_len;
                                        remaining -= pattern_len;
                                    }
                                    read = end + 1;
                                    continue;
                                }
                            }
                        } else {
                            /* No match found, copy the entire pattern as-is */
                            size_t pattern_len = end - read + 1;
                            if (pattern_len < remaining) {
                                memcpy(write, read, pattern_len);
                                write += pattern_len;
                                remaining -= pattern_len;
                            }
                            read = end + 1;
                            continue;
                        }
                    }
                }
            }
        }

        /* Not an emoji pattern, copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Replace :emoji: patterns in plain text with Unicode emoji only.
 *
 * This variant is intended for non-HTML outputs (e.g. terminal rendering)
 * where we do not want to emit <img> tags. It reuses the same emoji table
 * but only substitutes entries that have a Unicode representation; image-
 * only emoji names are left as their original :name: patterns.
 */
char *apex_replace_emoji_text(const char *text) {
    if (!text) return NULL;

    size_t capacity = strlen(text) * 2 + 16;  /* Enough for most unicode expansions */
    char *output = malloc(capacity);
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_indented_code_block = false;

    while (*read) {
        /* At line start: indented code block only if 4+ spaces/tab and not a list line */
        if (read == text || read[-1] == '\n') {
            in_indented_code_block = line_is_indented_code_block(read);
        }

        /* Track fenced code blocks (```) and inline code (`) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Skip emoji replacement inside any code context */
        if (in_code_block || in_inline_code || in_indented_code_block) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        if (*read == ':') {
            /* Look for closing : */
            const char *end = strchr(read + 1, ':');
            if (end && (end - read) < 50 && emoji_shortcode_is_bounded(text, read, end)) {
                /* Extract emoji name */
                int name_len = (int)(end - (read + 1));
                const char *name_start = read + 1;

                if (name_len > 0) {
                    /* Reject names containing whitespace */
                    int has_space = 0;
                    for (int i = 0; i < name_len; i++) {
                        char ch = name_start[i];
                        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                            has_space = 1;
                            break;
                        }
                    }

                    /* Skip common table alignment patterns like :---: */
                    if (!has_space && is_table_alignment_pattern(name_start, end)) {
                        size_t pattern_len = (size_t)(end - read + 1);
                        if (pattern_len <= remaining) {
                            memcpy(write, read, pattern_len);
                            write += pattern_len;
                            remaining -= pattern_len;
                        }
                        read = end + 1;
                        continue;
                    }

                    if (!has_space) {
                        /* Normalize name and look up in emoji table */
                        char normalized[64];
                        if ((size_t)name_len >= sizeof(normalized)) {
                            name_len = (int)sizeof(normalized) - 1;
                        }
                        memcpy(normalized, name_start, (size_t)name_len);
                        normalized[name_len] = '\0';
                        normalize_emoji_name(normalized);
                        size_t normalized_len = strlen(normalized);

                        const emoji_entry *entry = find_emoji_entry(normalized, (int)normalized_len);
                        if (entry && entry->unicode) {
                            /* Substitute Unicode emoji */
                            size_t emoji_len = strlen(entry->unicode);
                            if (emoji_len <= remaining) {
                                memcpy(write, entry->unicode, emoji_len);
                                write += emoji_len;
                                remaining -= emoji_len;
                                read = end + 1;
                                continue;
                            }
                            /* If not enough space, fall through and copy pattern as-is */
                        }
                    }
                }
            }
        }

        /* Default: copy single byte */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    if (remaining > 0) {
        *write = '\0';
    } else {
        output[capacity - 1] = '\0';
    }

    return output;
}

/**
 * Normalize emoji name: lowercase, hyphens to underscores, remove colons
 */
static void normalize_emoji_name(char *name) {
    size_t len = strlen(name);
    size_t write_pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (name[i] == ':') {
            /* Skip colons */
            continue;
        } else if (name[i] == '-') {
            name[write_pos++] = '_';
        } else {
            name[write_pos++] = (char)tolower((unsigned char)name[i]);
        }
    }
    name[write_pos] = '\0';
}

/**
 * Calculate Levenshtein distance between two strings
 */
static int levenshtein_distance(const char *s1, size_t len1, const char *s2, size_t len2) {
    if (len1 == 0) return (int)len2;
    if (len2 == 0) return (int)len1;

    /* Use dynamic programming with minimal memory */
    int *prev_row = malloc((len2 + 1) * sizeof(int));
    int *curr_row = malloc((len2 + 1) * sizeof(int));
    if (!prev_row || !curr_row) {
        free(prev_row);
        free(curr_row);
        return (int)(len1 > len2 ? len1 : len2); /* Fallback: return max length */
    }

    /* Initialize first row */
    for (size_t i = 0; i <= len2; i++) {
        prev_row[i] = (int)i;
    }

    /* Fill the matrix */
    for (size_t i = 0; i < len1; i++) {
        curr_row[0] = (int)(i + 1);
        for (size_t j = 0; j < len2; j++) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            int deletion = prev_row[j + 1] + 1;
            int insertion = curr_row[j] + 1;
            int substitution = prev_row[j] + cost;

            curr_row[j + 1] = (deletion < insertion) ?
                (deletion < substitution ? deletion : substitution) :
                (insertion < substitution ? insertion : substitution);
        }

        /* Swap rows */
        int *temp = prev_row;
        prev_row = curr_row;
        curr_row = temp;
    }

    int result = prev_row[len2];
    free(prev_row);
    free(curr_row);
    return result;
}

/**
 * Find best emoji match using fuzzy matching
 * Returns the shortest matching emoji name within max_distance, or NULL if no match
 */
static const char *find_best_emoji_match(const char *name, size_t name_len, int max_distance) {
    char normalized[64];
    if (name_len >= sizeof(normalized)) {
        name_len = sizeof(normalized) - 1;
    }
    memcpy(normalized, name, name_len);
    normalized[name_len] = '\0';
    normalize_emoji_name(normalized);
    size_t normalized_len = strlen(normalized);

    /* Check exact match first (allows :100: / :1234: via exact lookup only) */
    const emoji_entry *exact = find_emoji_entry(normalized, (int)normalized_len);
    if (exact) {
        return exact->name;
    }

    /* Never fuzzy-match all-digit tokens (e.g. "00" -> "100") */
    if (emoji_name_is_all_digits(normalized, normalized_len)) {
        return NULL;
    }

    if (max_distance <= 0) {
        return NULL;
    }

    /* Find fuzzy matches */
    int best_distance = max_distance + 1;
    size_t best_length = SIZE_MAX;
    const char *best_match = NULL;

    for (int i = 0; complete_emoji_map[i].name; i++) {
        const char *emoji_name = complete_emoji_map[i].name;
        size_t emoji_len = strlen(emoji_name);

        int distance = levenshtein_distance(normalized, normalized_len, emoji_name, emoji_len);

        if (distance <= max_distance) {
            if (distance < best_distance ||
                (distance == best_distance && emoji_len < best_length)) {
                best_distance = distance;
                best_length = emoji_len;
                best_match = emoji_name;
            }
        }
    }

    return best_match;
}

/**
 * Check if a colon pair contains only table-related characters (pipes, dashes, colons)
 * This helps identify table alignment markers like :---:, :|:, :|---:, etc.
 * Note: Patterns with spaces are already filtered out before this check.
 */
static int is_table_alignment_pattern(const char *start, const char *end) {
    if (!start || !end || start >= end) return 0;

    /* Check if content between colons contains only pipes, dashes, or colons */
    /* (spaces are already checked separately and rejected) */
    for (const char *p = start; p < end; p++) {
        if (*p != '|' && *p != '-' && *p != ':') {
            /* Found a character that's not part of table alignment syntax */
            return 0;
        }
    }

    /* If we only have pipes, dashes, or colons, it's a table alignment pattern */
    return 1;
}

/**
 * Autocorrect emoji names in markdown text
 * Processes :emoji_name: patterns and corrects typos using fuzzy matching
 */
char *apex_autocorrect_emoji_names(const char *text) {
    if (!text) return NULL;

    size_t capacity = strlen(text) * 2;
    char *output = malloc(capacity);
    if (!output) return strdup(text);

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_indented_code_block = false;

    while (*read) {
        /* At line start: indented code block only if 4+ spaces/tab and not a list line */
        if (read == text || read[-1] == '\n') {
            in_indented_code_block = line_is_indented_code_block(read);
        }

        /* Track fenced code blocks (```) and inline code (`) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Skip emoji processing inside any code context */
        if (in_code_block || in_inline_code || in_indented_code_block) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        if (*read == ':') {
            /* Look for closing : */
            const char *end = strchr(read + 1, ':');
            if (end && (end - read) < 50 && emoji_shortcode_is_bounded(text, read, end)) {
                /* Extract emoji name */
                int name_len = (int)(end - (read + 1));
                const char *name_start = read + 1;

                /* Validate: must have at least one character and no spaces */
                if (name_len > 0) {
                    /* Check for spaces in the name */
                    int has_space = 0;
                    for (int i = 0; i < name_len; i++) {
                        if (name_start[i] == ' ' || name_start[i] == '\t' || name_start[i] == '\n') {
                            has_space = 1;
                            break;
                        }
                    }

                    /* Skip if it contains only table alignment characters (pipes, dashes, colons) */
                    if (!has_space && is_table_alignment_pattern(name_start, end)) {
                        /* This is a table alignment pattern like :---:, :|:, :|---:, etc. */
                        /* Copy the colon pair as-is */
                        size_t pattern_len = end - read + 1;
                        if (pattern_len < remaining) {
                            memcpy(write, read, pattern_len);
                            write += pattern_len;
                            remaining -= pattern_len;
                        }
                        read = end + 1;
                        continue;
                    }

                    if (!has_space) {
                        /* Normalize the name for comparison */
                        char normalized[64];
                        if ((size_t)name_len >= sizeof(normalized)) {
                            name_len = sizeof(normalized) - 1;
                        }
                        memcpy(normalized, name_start, name_len);
                        normalized[name_len] = '\0';
                        normalize_emoji_name(normalized);
                        size_t normalized_len = strlen(normalized);

                        /* Check if it's already correct (using normalized name) */
                        const emoji_entry *entry = find_emoji_entry(normalized, (int)normalized_len);

                        if (entry) {
                            /* Already correct (after normalization), write normalized version */
                            /* Check if we have enough space for :name: */
                            if (normalized_len + 2 <= remaining) {
                                *write++ = ':';
                                remaining--;
                                memcpy(write, normalized, normalized_len);
                                write += normalized_len;
                                remaining -= normalized_len;
                                *write++ = ':';
                                remaining--;
                            } else {
                                /* Not enough space, copy original pattern as-is */
                                size_t pattern_len = end - read + 1;
                                if (pattern_len < remaining) {
                                    memcpy(write, read, pattern_len);
                                    write += pattern_len;
                                    remaining -= pattern_len;
                                }
                            }
                            read = end + 1;
                            continue;
                        } else {
                            /* Try fuzzy matching with length-scaled distance */
                            int max_dist = emoji_fuzzy_max_distance(normalized_len);
                            const char *best_match = find_best_emoji_match(name_start, (size_t)name_len, max_dist);
                            if (best_match) {
                                /* Replace with corrected name */
                                size_t match_len = strlen(best_match);
                                /* Check if we have enough space for :name: */
                                if (match_len + 2 <= remaining) {
                                    *write++ = ':';
                                    remaining--;
                                    memcpy(write, best_match, match_len);
                                    write += match_len;
                                    remaining -= match_len;
                                    *write++ = ':';
                                    remaining--;
                                } else {
                                    /* Not enough space, copy original pattern as-is */
                                    size_t pattern_len = end - read + 1;
                                    if (pattern_len < remaining) {
                                        memcpy(write, read, pattern_len);
                                        write += pattern_len;
                                        remaining -= pattern_len;
                                    }
                                }
                                read = end + 1;
                                continue;
                            }
                            /* No match found, copy the entire pattern as-is */
                            size_t pattern_len = end - read + 1;
                            if (pattern_len < remaining) {
                                memcpy(write, read, pattern_len);
                                write += pattern_len;
                                remaining -= pattern_len;
                            }
                            read = end + 1;
                            continue;
                        }
                    }
                }
            }
        }

        /* Not an emoji pattern, copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}
