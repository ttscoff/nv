/**
 * Header ID Generation Extension
 * Implementation
 */

#include "header_ids.h"
#include "cmark-gfm.h"
#include "emoji.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

/**
 * Convert diacritic characters to ASCII equivalents
 * Handles common Latin diacritics
 */
static char normalize_char(unsigned char c) {
    /* Basic ASCII alphanumeric */
    if (isalnum(c)) {
        return tolower(c);
    }

    /* Common diacritics → ASCII */
    /* This is a simplified mapping - full Unicode normalization would require ICU */
    switch (c) {
        /* Latin-1 Supplement */
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return 'a'; /* ÀÁÂÃÄÅ */
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return 'a'; /* àáâãäå */
        case 0xC7: return 'c'; /* Ç */
        case 0xE7: return 'c'; /* ç */
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'e'; /* ÈÉÊË */
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e'; /* èéêë */
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'i'; /* ÌÍÎÏ */
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i'; /* ìíîï */
        case 0xD1: return 'n'; /* Ñ */
        case 0xF1: return 'n'; /* ñ */
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return 'o'; /* ÒÓÔÕÖØ */
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return 'o'; /* òóôõöø */
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'u'; /* ÙÚÛÜ */
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u'; /* ùúûü */
        case 0xDD: case 0xFD: case 0xFF: return 'y'; /* Ýýÿ */
        case 0xDF: return 's'; /* ß */
        default: return 0; /* Not a valid character to keep */
    }
}

/**
 * Generate header ID from text
 */
char *apex_generate_header_id(const char *text, apex_id_format_t format) {
    if (!text) return strdup("");

    size_t len = strlen(text);
    size_t capacity = len * 3 + 1;  /* Extra space for UTF-8 expansion */
    char *id = malloc(capacity);
    if (!id) return strdup("");

    char *write = id;
    bool last_was_dash = false;
    bool first_char = true;
    bool last_was_punct_dash = false;  /* Track if last dash came from punctuation (for kramdown) */

    for (const char *read = text; *read; read++) {
        unsigned char c = (unsigned char)*read;

        /* Skip if already processed as part of UTF-8 sequence */
        if ((c & 0xC0) == 0x80) continue;

        /* Check for em dash (—) U+2014: 0xE2 0x80 0x94 */
        if (c == 0xE2 && read[1] != '\0' && read[2] != '\0' &&
            (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x94) {
            if (format == APEX_ID_FORMAT_MMD) {
                /* MMD: preserve em dash as-is */
                *write++ = 0xE2;
                *write++ = 0x80;
                *write++ = 0x94;
                last_was_dash = false;
                first_char = false;
            }
            /* GFM and Kramdown: remove em dash (skip it) */
            read += 2;  /* Skip the next 2 bytes (for loop will increment past 0x94) */
            continue;
        }

        /* Check for en dash (–) U+2013: 0xE2 0x80 0x93 */
        if (c == 0xE2 && read[1] != '\0' && read[2] != '\0' &&
            (unsigned char)read[1] == 0x80 && (unsigned char)read[2] == 0x93) {
            if (format == APEX_ID_FORMAT_MMD) {
                /* MMD: preserve en dash as-is */
                *write++ = 0xE2;
                *write++ = 0x80;
                *write++ = 0x93;
                last_was_dash = false;
                first_char = false;
            }
            /* GFM and Kramdown: remove en dash (skip it) */
            read += 2;  /* Skip the next 2 bytes (for loop will increment past 0x93) */
            continue;
        }

        /* Check for apostrophes: curly (') U+2019: 0xE2 0x80 0x99, left quote (') U+2018: 0xE2 0x80 0x98 */
        if (c == 0xE2 && read[1] != '\0' && read[2] != '\0' &&
            (unsigned char)read[1] == 0x80 &&
            ((unsigned char)read[2] == 0x99 || (unsigned char)read[2] == 0x98)) {
            /* Remove apostrophes in all formats - they break anchor links */
            read += 2;
            continue;
        }

        if (format == APEX_ID_FORMAT_MMD) {
            /* MMD format: preserve dashes, lowercase alphanumerics, preserve diacritics, skip spaces/punctuation */
            if (c == '-') {
                /* Regular dash: preserve as-is */
                *write++ = '-';
                last_was_dash = false;
                first_char = false;
            } else if (isalnum((unsigned char)c)) {
                /* ASCII alphanumeric: lowercase */
                *write++ = tolower(c);
                last_was_dash = false;
                first_char = false;
            } else if (c == ' ') {
                /* Space: skip (remove) */
                /* Do nothing */
            } else if (c < 0x80) {
                /* ASCII non-alphanumeric (punctuation, etc.): skip */
                /* Do nothing */
            } else {
                /* UTF-8 character (diacritics, etc.): preserve as-is, but lowercase if it's a letter */
                /* Check if this is the start of a UTF-8 sequence */
                int utf8_len = 0;
                if ((c & 0xE0) == 0xC0) utf8_len = 2;  /* 2-byte sequence */
                else if ((c & 0xF0) == 0xE0) utf8_len = 3;  /* 3-byte sequence */
                else if ((c & 0xF8) == 0xF0) utf8_len = 4;  /* 4-byte sequence */

                if (utf8_len > 0) {
                    /* Copy the entire UTF-8 sequence */
                    for (int i = 0; i < utf8_len && read[i] != '\0'; i++) {
                        *write++ = read[i];
                    }
                    read += utf8_len - 1;  /* -1 because for loop will increment */
                    last_was_dash = false;
                    first_char = false;
                } else {
                    /* Invalid UTF-8 or single byte: skip */
                }
            }
        } else if (format == APEX_ID_FORMAT_KRAMDOWN) {
            /* Kramdown format: spaces→dashes (not collapsed), remove diacritics, remove em/en dashes */
            /* Trailing punctuation is removed, not converted to dash */
            /* If punctuation is followed by space, skip the space */
            const char *next = read + 1;
            while (*next && (*next == ' ' || *next == '\t' || *next == '\n' || *next == '\r')) next++;
            bool is_trailing = (*next == '\0');

            if (c == '-') {
                /* Regular dash: preserve as-is */
                *write++ = '-';
                last_was_dash = false;
                last_was_punct_dash = false;
                first_char = false;
            } else if (isalnum((unsigned char)c)) {
                /* ASCII alphanumeric: lowercase */
                *write++ = tolower(c);
                last_was_dash = false;
                last_was_punct_dash = false;
                first_char = false;
            } else if (c == ' ') {
                /* Space: check if it follows punctuation that became a dash */
                if (last_was_punct_dash) {
                    /* Space after punctuation: skip (don't convert to dash) */
                    last_was_punct_dash = false;
                    /* Do nothing */
                } else {
                    /* Regular space: convert to dash (don't collapse multiple spaces) */
                    *write++ = '-';
                    last_was_dash = false;
                    last_was_punct_dash = false;
                    first_char = false;
                }
            } else if (c < 0x80) {
                /* ASCII non-alphanumeric (punctuation, etc.) */
                if (is_trailing) {
                    /* Trailing punctuation: remove (skip) */
                    last_was_punct_dash = false;
                    /* Do nothing */
                } else {
                    /* Middle punctuation: convert to dash */
                    *write++ = '-';
                    last_was_dash = false;
                    last_was_punct_dash = true;  /* Mark that this dash came from punctuation */
                    first_char = false;
                }
            } else {
                /* UTF-8 character (diacritics, etc.): remove (skip) */
                /* Check if this is the start of a UTF-8 sequence */
                int utf8_len = 0;
                if ((c & 0xE0) == 0xC0) utf8_len = 2;  /* 2-byte sequence */
                else if ((c & 0xF0) == 0xE0) utf8_len = 3;  /* 3-byte sequence */
                else if ((c & 0xF8) == 0xF0) utf8_len = 4;  /* 4-byte sequence */

                if (utf8_len > 0) {
                    /* Skip the entire UTF-8 sequence */
                    read += utf8_len - 1;  /* -1 because for loop will increment */
                }
                /* Otherwise skip the byte */
            }
        } else {
            /* GFM format: spaces become dashes, other whitespace/punctuation removed, normalize diacritics */
            /* Check for emoji (UTF-8 sequences) and replace with name */
            bool emoji_found = false;
            if (c >= 0x80) {
                /* Potential UTF-8 sequence - check if it's an emoji */
                int utf8_len = 0;
                if ((c & 0xE0) == 0xC0) utf8_len = 2;  /* 2-byte sequence */
                else if ((c & 0xF0) == 0xE0) utf8_len = 3;  /* 3-byte sequence */
                else if ((c & 0xF8) == 0xF0) utf8_len = 4;  /* 4-byte sequence */

                if (utf8_len > 0) {
                    /* Check if we have enough bytes */
                    bool has_enough_bytes = true;
                    for (int i = 1; i < utf8_len; i++) {
                        if (read[i] == '\0' || ((unsigned char)read[i] & 0xC0) != 0x80) {
                            has_enough_bytes = false;
                            break;
                        }
                    }

                    if (has_enough_bytes) {
                        /* Try to find emoji name */
                        const char *emoji_name = apex_find_emoji_name(read, utf8_len);
                        if (emoji_name) {
                            /* Replace emoji with its name */
                            size_t name_len = strlen(emoji_name);
                            /* Ensure we have enough space */
                            size_t current_pos = write - id;
                            size_t needed = current_pos + name_len + 1;
                            if (needed > capacity) {
                                /* Reallocate if needed */
                                size_t old_pos = write - id;
                                size_t new_cap = needed * 2;
                                char *new_id = realloc(id, new_cap);
                                if (new_id) {
                                    id = new_id;
                                    write = id + old_pos;
                                    capacity = new_cap;
                                }
                            }
                            /* Write emoji name */
                            memcpy(write, emoji_name, name_len);
                            write += name_len;
                            read += utf8_len - 1;  /* -1 because for loop will increment */
                            last_was_dash = false;
                            first_char = false;
                            emoji_found = true;
                        } else {
                            /* Valid UTF-8 sequence but not an emoji - check if it's a diacritic */
                            /* Check for diacritics before skipping */
                            char normalized = 0;
                            if (utf8_len == 2 && (c & 0xE0) == 0xC0) {
                                /* 2-byte UTF-8 sequence - check for common Latin diacritics */
                                if (read[1] != '\0' && ((unsigned char)read[1] & 0xC0) == 0x80) {
                                    unsigned char byte2 = (unsigned char)read[1];
                                    /* Check for É (0xC3 0x89) */
                                    if (c == 0xC3 && byte2 == 0x89) {
                                        normalized = 'e';
                                    } else if (c == 0xC3 && byte2 >= 0x80 && byte2 <= 0x85) {
                                        /* ÀÁÂÃÄÅ (0xC3 0x80-0x85) */
                                        normalized = 'a';
                                    } else if (c == 0xC3 && (byte2 == 0x87 || byte2 == 0xA7)) {
                                        /* Ç (0xC7) or ç (0xE7) */
                                        normalized = 'c';
                                    } else if (c == 0xC3 && byte2 >= 0x88 && byte2 <= 0x8B) {
                                        /* ÈÉÊË (0xC8-0xCB) */
                                        normalized = 'e';
                                    } else if (c == 0xC3 && byte2 >= 0x8C && byte2 <= 0x8F) {
                                        /* ÌÍÎÏ (0xCC-0xCF) */
                                        normalized = 'i';
                                    } else if (c == 0xC3 && (byte2 == 0x91 || byte2 == 0xB1)) {
                                        /* Ñ (0xD1) or ñ (0xF1) */
                                        normalized = 'n';
                                    } else if (c == 0xC3 && byte2 >= 0x92 && byte2 <= 0x98) {
                                        /* ÒÓÔÕÖØ (0xD2-0xD8) */
                                        normalized = 'o';
                                    } else if (c == 0xC3 && byte2 >= 0x99 && byte2 <= 0x9C) {
                                        /* ÙÚÛÜ (0xD9-0xDC) */
                                        normalized = 'u';
                                    } else if (c == 0xC3 && (byte2 == 0x9D || byte2 == 0xBD || byte2 == 0xFF)) {
                                        /* Ýýÿ (0xDD, 0xFD, 0xFF) */
                                        normalized = 'y';
                                    } else if (c == 0xC3 && byte2 == 0x9F) {
                                        /* ß (0xDF) */
                                        normalized = 's';
                                    }
                                }
                            }

                            if (normalized) {
                                /* Found a diacritic - write the normalized character */
                                *write++ = normalized;
                                read += utf8_len - 1;  /* -1 because for loop will increment */
                                last_was_dash = false;
                                first_char = false;
                                emoji_found = true;  /* Mark as found so we don't process it again */
                            } else {
                                /* Not an emoji and not a diacritic - skip it (GFM removes non-ASCII non-emoji) */
                                read += utf8_len - 1;  /* -1 because for loop will increment */
                                emoji_found = true;  /* Mark as found so we don't process it */
                            }
                        }
                    }
                }
            }

            if (!emoji_found) {
                /* Check for ASCII diacritics (single-byte) or other characters */
                char normalized = 0;
                if (c < 0x80) {
                    /* ASCII character - use normalize_char */
                    normalized = normalize_char(c);
                }

                if (normalized) {
                    /* Valid alphanumeric character (normalized diacritic) */
                    *write++ = normalized;
                    last_was_dash = false;
                    first_char = false;
                } else if (isalnum(c)) {
                    /* ASCII alphanumeric not in our diacritic map */
                    *write++ = tolower(c);
                    last_was_dash = false;
                    first_char = false;
                } else if (c == ' ') {
                    /* Space: convert to dash (collapsed) */
                    if (!last_was_dash && !first_char) {
                        *write++ = '-';
                        last_was_dash = true;
                    }
                } else if (c == '-') {
                    /* Regular dash: preserve (but don't add multiple consecutive) */
                    if (!last_was_dash && !first_char) {
                        *write++ = '-';
                        last_was_dash = true;
                    }
                } else {
                    /* Other whitespace and punctuation: remove (skip) */
                    /* Do nothing */
                }
            }
        }
    }

    *write = '\0';

    /* Trim dashes from start and end */
    if (format == APEX_ID_FORMAT_GFM) {
        /* GFM: trim both leading and trailing dashes */
        char *start = id;
        char *end = write - 1;

        while (*start == '-') start++;
        while (end > start && *end == '-') end--;

        if (start > id) {
            size_t new_len = end - start + 1;
            memmove(id, start, new_len);
            id[new_len] = '\0';
        } else if (end < write - 1) {
            *(end + 1) = '\0';
        }
    } else if (format == APEX_ID_FORMAT_KRAMDOWN) {
        /* Kramdown: trim only leading dashes, preserve trailing */
        char *start = id;
        while (*start == '-') start++;

        if (start > id) {
            size_t new_len = write - start;
            memmove(id, start, new_len);
            id[new_len] = '\0';
        }
    }
    /* MMD format: preserve leading/trailing dashes */

    /* If result is empty, use "header" */
    if (strlen(id) == 0) {
        free(id);
        return strdup("header");
    }

    return id;
}

/**
 * Recursively append literal text from node and its descendants to buffer.
 * Handles TEXT, CODE, and recurses into inline containers (EMPH, STRONG, etc.)
 * so "### *Processing* modes" yields "Processing modes" matching rendered HTML.
 */
static void append_literal(char **text, char **write, size_t *capacity, size_t *remaining,
                          const char *literal) {
    if (!literal) return;
    size_t len = strlen(literal);

    size_t used = (size_t)(*write - *text);
    size_t required = used + len + 1; /* +1 for NUL terminator */
    if (required > *capacity) {
        size_t new_capacity = *capacity;
        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2) {
                return;
            }
            new_capacity *= 2;
        }

        char *new_text = realloc(*text, new_capacity);
        if (!new_text) return;

        *text = new_text;
        *write = *text + used;
        *capacity = new_capacity;
        *remaining = *capacity - used;
    }

    memcpy(*write, literal, len);
    *write += len;
    *remaining -= len;
}

static void extract_heading_text_recursive(cmark_node *node, char **text, char **write,
                                           size_t *capacity, size_t *remaining) {
    cmark_node_type type = cmark_node_get_type(node);

    if (type == CMARK_NODE_TEXT || type == CMARK_NODE_CODE) {
        append_literal(text, write, capacity, remaining, cmark_node_get_literal(node));
        return;
    }
    /* HTML_INLINE has literal (e.g. "&") - needed for "Documentation & resources" */
    if (type == CMARK_NODE_HTML_INLINE) {
        append_literal(text, write, capacity, remaining, cmark_node_get_literal(node));
        return;
    }

    /* Recurse into inline containers (EMPH, STRONG, LINK, etc.) */
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        extract_heading_text_recursive(child, text, write, capacity, remaining);
        child = cmark_node_next(child);
    }
}

/**
 * Extract text content from a heading node
 */
char *apex_extract_heading_text(cmark_node *heading_node) {
    if (!heading_node || cmark_node_get_type(heading_node) != CMARK_NODE_HEADING) {
        return strdup("");
    }

    size_t capacity = 256;
    char *text = malloc(capacity);
    if (!text) return strdup("");
    char *write = text;
    size_t remaining = capacity;

    cmark_node *child = cmark_node_first_child(heading_node);
    while (child) {
        extract_heading_text_recursive(child, &text, &write, &capacity, &remaining);
        child = cmark_node_next(child);
    }

    *write = '\0';
    return text;
}

/**
 * Extract manual header ID from heading text
 * Supports:
 * - MultiMarkdown: "Heading [id]" -> returns "id", removes "[id]" from text
 * - Kramdown: "Heading {#id}" -> returns "id", removes "{#id}" from text
 *
 * Note: IAL format "Heading {: #id}" is handled separately by IAL processor
 *
 * @param heading_text Heading text (will be modified to remove ID syntax)
 * @param manual_id_out Output parameter for extracted ID (must be freed by caller)
 * @return true if manual ID was found and extracted
 */
bool apex_extract_manual_header_id(char **heading_text, char **manual_id_out) {
    if (!heading_text || !*heading_text || !manual_id_out) {
        return false;
    }

    char *text = *heading_text;
    size_t len = strlen(text);
    if (len == 0) return false;

    /* Try MultiMarkdown format: [id] at the end */
    const char *mmd_start = strrchr(text, '[');
    if (mmd_start) {
        const char *mmd_end = strchr(mmd_start, ']');
        if (mmd_end && mmd_end > mmd_start + 1) {
            /* Skip if content starts with % (metadata variable like [%title]) */
            if (mmd_start[1] == '%') {
                /* This is a metadata variable, not a header ID */
                mmd_start = NULL;
            }
        }
        if (mmd_start && mmd_end && mmd_end > mmd_start + 1) {
            /* Check nothing after ] except whitespace */
            const char *after = mmd_end + 1;
            while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '\0') {
                /* Extract ID */
                size_t id_len = mmd_end - mmd_start - 1;
                if (id_len > 0) {
                    *manual_id_out = malloc(id_len + 1);
                    if (*manual_id_out) {
                        memcpy(*manual_id_out, mmd_start + 1, id_len);
                        (*manual_id_out)[id_len] = '\0';

                        /* Remove [id] from text */
                        size_t prefix_len = mmd_start - text;
                        char *new_text = malloc(prefix_len + 1);
                        if (new_text) {
                            memcpy(new_text, text, prefix_len);
                            new_text[prefix_len] = '\0';

                            /* Trim trailing whitespace */
                            char *end = new_text + prefix_len - 1;
                            while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';

                            free(*heading_text);
                            *heading_text = new_text;
                            return true;
                        } else {
                            free(*manual_id_out);
                            *manual_id_out = NULL;
                        }
                    }
                }
            }
        }
    }

    /* Try Kramdown format: {#id} at the end */
    const char *kramdown_start = strrchr(text, '{');
    if (kramdown_start && kramdown_start[1] == '#') {
        const char *kramdown_end = strchr(kramdown_start, '}');
        if (kramdown_end && kramdown_end > kramdown_start + 2) {
            /* Check nothing after } except whitespace */
            const char *after = kramdown_end + 1;
            while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '\0') {
                /* Extract ID */
                size_t id_len = kramdown_end - kramdown_start - 2;  /* Skip {# */
                if (id_len > 0) {
                    *manual_id_out = malloc(id_len + 1);
                    if (*manual_id_out) {
                        memcpy(*manual_id_out, kramdown_start + 2, id_len);
                        (*manual_id_out)[id_len] = '\0';

                        /* Remove {#id} from text */
                        size_t prefix_len = kramdown_start - text;
                        char *new_text = malloc(prefix_len + 1);
                        if (new_text) {
                            memcpy(new_text, text, prefix_len);
                            new_text[prefix_len] = '\0';

                            /* Trim trailing whitespace */
                            char *end = new_text + prefix_len - 1;
                            while (end >= new_text && isspace((unsigned char)*end)) *end-- = '\0';

                            free(*heading_text);
                            *heading_text = new_text;
                            return true;
                        } else {
                            free(*manual_id_out);
                            *manual_id_out = NULL;
                        }
                    }
                }
            }
        }
    }

    return false;
}

/**
 * Extract plain text from a link node (for simple [ref] style).
 * Returns allocated string or NULL.
 */
static char *get_link_label_text(cmark_node *link_node) {
    if (!link_node || cmark_node_get_type(link_node) != CMARK_NODE_LINK) return NULL;
    cmark_node *child = cmark_node_first_child(link_node);
    if (!child || cmark_node_get_type(child) != CMARK_NODE_TEXT) return NULL;
    const char *literal = cmark_node_get_literal(child);
    return literal ? strdup(literal) : NULL;
}

/**
 * Check if a string is a valid MMD heading ID (no spaces, no metadata %).
 */
static bool is_valid_mmd_id(const char *s) {
    if (!s || !*s) return false;
    for (; *s; s++) {
        if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '%') return false;
    }
    return true;
}

/**
 * Process manual header IDs in a heading node
 * Extracts MMD [id] or Kramdown {#id} syntax and stores ID in user_data
 * Updates the heading text node to remove the manual ID syntax
 *
 * Walks ALL text children (not just first) so headings split by "&" etc.
 * (e.g. TEXT + HTML_INLINE + TEXT) are handled - the IAL may be in a later child.
 *
 * Edge case: When [id] matches a link reference and would render as a link,
 * but [id] is the last element in the heading with other content before it,
 * treat it as MMD heading ID (not a link). This avoids the conflict where
 * "# Heading [mermaid]" with "[mermaid]: URL" would wrongly render mermaid as
 * a link. If the heading is ONLY [id] (e.g. "# [mermaid]"), keep it as a link
 * to avoid empty headings.
 */
bool apex_process_manual_header_id(cmark_node *heading_node) {
    if (!heading_node || cmark_node_get_type(heading_node) != CMARK_NODE_HEADING) {
        return false;
    }

    /* Check each TEXT child for manual ID - "&" etc. can split content across nodes.
       Prefer the rightmost (last) match to align with IAL behavior. */
    cmark_node *match_node = NULL;
    char *match_text = NULL;
    char *match_id = NULL;

    for (cmark_node *child = cmark_node_first_child(heading_node); child;
         child = cmark_node_next(child)) {
        if (cmark_node_get_type(child) != CMARK_NODE_TEXT) continue;

        const char *literal = cmark_node_get_literal(child);
        if (!literal) continue;

        char *text_copy = strdup(literal);
        if (!text_copy) continue;

        char *manual_id = NULL;
        bool found = apex_extract_manual_header_id(&text_copy, &manual_id);

        if (found && manual_id) {
            /* Discard previous match - we want the rightmost */
            free(match_text);
            free(match_id);
            match_node = child;
            match_text = text_copy;
            match_id = manual_id;
        } else {
            free(text_copy);
            if (manual_id) free(manual_id);
        }
    }

    if (match_node && match_id) {
        /* Store ID in user_data as id="..." */
        char *id_attr = malloc(strlen(match_id) + 6);  /* id="" + null */
        if (id_attr) {
            sprintf(id_attr, "id=\"%s\"", match_id);

            /* Merge with existing user_data if present (e.g. from IAL) */
            char *existing = (char *)cmark_node_get_user_data(heading_node);
            if (existing) {
                char *combined = malloc(strlen(existing) + strlen(id_attr) + 2);
                if (combined) {
                    sprintf(combined, "%s %s", existing, id_attr);
                    cmark_node_set_user_data(heading_node, combined);
                    free(id_attr);
                } else {
                    cmark_node_set_user_data(heading_node, id_attr);
                }
            } else {
                cmark_node_set_user_data(heading_node, id_attr);
            }
        }

        cmark_node_set_literal(match_node, match_text);
        free(match_id);
        free(match_text);
        return true;
    }

    /* Edge case: [id] was parsed as a link (ref existed). If it's the last
     * element and there's other content, treat as MMD heading ID. */
    cmark_node *last = NULL;
    cmark_node *child = cmark_node_first_child(heading_node);
    while (child) {
        cmark_node_type t = cmark_node_get_type(child);
        if (t != CMARK_NODE_SOFTBREAK && t != CMARK_NODE_LINEBREAK) {
            last = child;
        }
        child = cmark_node_next(child);
    }

    if (!last || cmark_node_get_type(last) != CMARK_NODE_LINK) return false;

    /* Must have at least one sibling before the link (avoid empty headings) */
    cmark_node *prev = cmark_node_previous(last);
    if (!prev) return false;

    char *link_text = get_link_label_text(last);
    if (!link_text || !is_valid_mmd_id(link_text)) {
        free(link_text);
        return false;
    }

    /* Replace link with text node, set heading id */
    cmark_node *text_replacement = cmark_node_new(CMARK_NODE_TEXT);
    if (!text_replacement) {
        free(link_text);
        return false;
    }
    cmark_node_set_literal(text_replacement, link_text);
    cmark_node_insert_before(last, text_replacement);
    cmark_node_unlink(last);
    cmark_node_free(last);

    char *id_attr = malloc(strlen(link_text) + 6);
    if (id_attr) {
        sprintf(id_attr, "id=\"%s\"", link_text);
        char *existing = (char *)cmark_node_get_user_data(heading_node);
        if (existing) {
            char *combined = malloc(strlen(existing) + strlen(id_attr) + 2);
            if (combined) {
                sprintf(combined, "%s %s", existing, id_attr);
                cmark_node_set_user_data(heading_node, combined);
                free(id_attr);
            } else {
                cmark_node_set_user_data(heading_node, id_attr);
            }
        } else {
            cmark_node_set_user_data(heading_node, id_attr);
        }
    }
    free(link_text);
    return true;
}

