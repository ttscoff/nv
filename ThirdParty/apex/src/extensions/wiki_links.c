/**
 * Wiki Links Extension for Apex
 * Implementation
 */

#include "wiki_links.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include "parser.h"
#include "inlines.h"
#include "html.h"

/* Special inline character for wiki links */
__attribute__((unused))
static const char WIKI_OPEN_CHAR = '[';

/* Default configuration */
static wiki_link_config default_config = {
    .base_path = "",
    .extension = "",
    .space_mode = WIKILINK_SPACE_DASH,  /* Default: convert spaces to dashes */
    .sanitize = false
};

/**
 * Check if we have a wiki link at the current position
 * Returns the number of characters consumed, or 0 if not a wiki link
 */
static int scan_wiki_link(const char *input, int len) {
    if (len < 4) return 0;  /* Need at least [[x]] */

    /* Must start with [[ */
    if (input[0] != '[' || input[1] != '[') return 0;

    /* Find the closing ]] */
    for (int i = 2; i < len - 1; i++) {
        if (input[i] == ']' && input[i + 1] == ']') {
            /* Found closing ]] */
            return i + 2;  /* Return position after ]] */
        }
    }

    return 0;  /* No closing ]] found */
}

/**
 * Parse wiki link content
 * Format: PageName or PageName|DisplayText or PageName#Section
 */
static void parse_wiki_link(const char *content, int len,
                            char **page, char **display, char **section) {
    *page = NULL;
    *display = NULL;
    *section = NULL;

    if (len <= 0) return;

    /* Copy content for parsing */
    char *text = malloc(len + 1);
    if (!text) return;
    memcpy(text, content, len);
    text[len] = '\0';

    /* Check for | separator (display text) */
    char *pipe = strchr(text, '|');
    if (pipe) {
        *pipe = '\0';
        *page = strdup(text);
        *display = strdup(pipe + 1);
    } else {
        *page = strdup(text);
    }

    /* Check for # separator (section) */
    if (*page) {
        char *hash = strchr(*page, '#');
        if (hash) {
            *hash = '\0';
            *section = strdup(hash + 1);
        }
    }

    free(text);
}

/**
 * Map Latin-1 supplement accented characters (U+00C0 - U+00FF) to ASCII base.
 * Returns the ASCII base letter for accented chars, or 0 if not applicable.
 * The index is the low byte (0x80-0xFF) of the 2-byte UTF-8 sequence 0xC3 0xXX.
 */
static char latin1_to_ascii(unsigned char low_byte) {
    /* Latin-1 supplement: UTF-8 0xC3 0x80-0xBF maps to U+00C0-U+00FF */
    /* This covers: À-Ö (0x80-0x96), Ø-ö (0x98-0xB6), ø-ÿ (0xB8-0xBF) */
    static const char map[64] = {
        /*  À    Á    Â    Ã    Ä    Å    Æ    Ç    È    É    Ê    Ë    Ì    Í    Î    Ï  */
           'a', 'a', 'a', 'a', 'a', 'a',  0 , 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
        /*  Ð    Ñ    Ò    Ó    Ô    Õ    Ö    ×    Ø    Ù    Ú    Û    Ü    Ý    Þ    ß  */
           'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 , 'o', 'u', 'u', 'u', 'u', 'y',  0 ,  0 ,
        /*  à    á    â    ã    ä    å    æ    ç    è    é    ê    ë    ì    í    î    ï  */
           'a', 'a', 'a', 'a', 'a', 'a',  0 , 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
        /*  ð    ñ    ò    ó    ô    õ    ö    ÷    ø    ù    ú    û    ü    ý    þ    ÿ  */
           'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 , 'o', 'u', 'u', 'u', 'u', 'y',  0 , 'y',
    };
    if (low_byte >= 0x80 && low_byte <= 0xBF) {
        return map[low_byte - 0x80];
    }
    return 0;
}

/**
 * Convert page name to URL
 *
 * When sanitize is enabled:
 * - Removes apostrophes
 * - Converts A-Z to a-z
 * - Removes accents from Latin-1 characters (both NFC and NFD forms)
 * - Replaces non-alphanumeric characters with the space_mode character
 * - Removes duplicate space_mode characters
 * - Removes leading and trailing space_mode characters
 */
static char *page_to_url(const char *page, const char *section, wiki_link_config *config) {
    if (!page) return NULL;

    /* Default configuration if none provided */
    if (!config) config = &default_config;

    /* Calculate URL length - worst case: all spaces become dashes/underscores */
    size_t page_len = strlen(page);
    size_t url_len = strlen(config->base_path) + page_len +
                     (section ? strlen(section) + 1 : 0) +
                     (config->extension ? strlen(config->extension) : 0) + 10;

    char *url = malloc(url_len);
    if (!url) return NULL;

    char *p = url;

    /* Add base path */
    strcpy(p, config->base_path);
    p += strlen(config->base_path);

    char *page_start = p;  /* Remember where page name starts for trailing trim */

    if (config->sanitize) {
        /* Sanitize in one pass */
        char space_char;
        switch (config->space_mode) {
            case WIKILINK_SPACE_DASH:       space_char = '-'; break;
            case WIKILINK_SPACE_NONE:       space_char = '\0'; break;
            case WIKILINK_SPACE_UNDERSCORE: space_char = '_'; break;
            case WIKILINK_SPACE_SPACE:      space_char = ' '; break;
            default:                        space_char = '-'; break;
        }
        bool last_was_space_char = true;  /* Start true to skip leading space chars */

        for (const char *s = page; *s; s++) {
            unsigned char c = (unsigned char)*s;

            /* Remove apostrophes, quotes, and similar punctuation (ASCII and Unicode) */
            if (c == '\'' || c == '"' || c == '`') {
                continue;
            }
            /* Skip acute accent: ´ (U+00B4) = 0xC2 0xB4 */
            if (c == 0xC2 && s[1] && (unsigned char)s[1] == 0xB4) {
                s += 1;  /* Skip the 2-byte sequence (loop will advance 1 more) */
                continue;
            }
            /* Skip curly apostrophes and quotes (3-byte UTF-8 sequences starting with 0xE2 0x80):
             * ' (U+2018) = 0xE2 0x80 0x98, ' (U+2019) = 0xE2 0x80 0x99
             * " (U+201C) = 0xE2 0x80 0x9C, " (U+201D) = 0xE2 0x80 0x9D */
            if (c == 0xE2 && s[1] && s[2] &&
                (unsigned char)s[1] == 0x80 &&
                ((unsigned char)s[2] == 0x98 || (unsigned char)s[2] == 0x99 ||
                 (unsigned char)s[2] == 0x9C || (unsigned char)s[2] == 0x9D)) {
                s += 2;  /* Skip the 3-byte sequence (loop will advance 1 more) */
                continue;
            }

            /* Handle NFD: Skip combining diacritical marks (U+0300-U+036F).
             * In UTF-8: 0xCC 0x80-0xBF (U+0300-U+033F) and 0xCD 0x80-0xAF (U+0340-U+036F) */
            if (c == 0xCC && s[1] && (unsigned char)s[1] >= 0x80 && (unsigned char)s[1] <= 0xBF) {
                s += 1;  /* Skip combining mark */
                continue;
            }
            if (c == 0xCD && s[1] && (unsigned char)s[1] >= 0x80 && (unsigned char)s[1] <= 0xAF) {
                s += 1;  /* Skip combining mark */
                continue;
            }

            /* Handle NFC: Convert Latin-1 supplement accented chars to ASCII base.
             * UTF-8 sequence 0xC3 0x80-0xBF maps to U+00C0-U+00FF */
            if (c == 0xC3 && s[1]) {
                unsigned char next = (unsigned char)s[1];

                /* Handle ligatures that expand to multiple characters:
                 * Æ (U+00C6) = 0xC3 0x86 → "ae"
                 * æ (U+00E6) = 0xC3 0xA6 → "ae"
                 * ß (U+00DF) = 0xC3 0x9F → "ss"
                 * note that the utf-8 encoded latin=-1 ligatures and the two
                 * ASCII characters always take up the same number of bytes
                 * so there's no risk of buffer overflow */
                if (next == 0x86 || next == 0xA6) {
                    s += 1;  /* Skip the 2-byte sequence */
                    *p++ = 'a';
                    *p++ = 'e';
                    last_was_space_char = false;
                    continue;
                }
                if (next == 0x9F) {
                    s += 1;  /* Skip the 2-byte sequence */
                    *p++ = 's';
                    *p++ = 's';
                    last_was_space_char = false;
                    continue;
                }

                char base = latin1_to_ascii(next);
                if (base) {
                    s += 1;  /* Skip the 2-byte sequence (loop advances 1 more) */
                    c = (unsigned char)base;
                    /* Fall through to normal processing */
                } else {
                    /* Unknown Latin-1 char, skip both bytes and treat as space */
                    s += 1;
                    if (space_char != '\0' && !last_was_space_char) {
                        *p++ = space_char;
                        last_was_space_char = true;
                    }
                    continue;
                }
            }

            /* Lowercase A-Z */
            if (c >= 'A' && c <= 'Z') {
                c = (unsigned char)(c + ('a' - 'A'));
            }

            /* Replace non-alphanumeric with space_mode char (except / and .) */
            if (!isalnum(c) && c != '/' && c != '.') {
                if (space_char == '\0') {
                    /* WIKILINK_SPACE_NONE: skip non-alphanumeric entirely */
                    continue;
                }
                c = space_char;
            }

            /* Remove duplicate space_mode characters */
            if (space_char != '\0' && c == space_char) {
                if (last_was_space_char) {
                    continue;  /* Skip duplicate */
                }
                last_was_space_char = true;
            } else {
                last_was_space_char = false;
            }

            *p++ = c;
        }

        /* Remove trailing space_mode character */
        if (space_char != '\0' && p > page_start && *(p - 1) == space_char) {
            p--;
        }
    } else {
        /* basic behavior: only handle spaces */
        for (const char *s = page; *s; s++) {
            if (*s == ' ') {
                switch (config->space_mode) {
                    case WIKILINK_SPACE_DASH:
                        *p++ = '-';
                        break;
                    case WIKILINK_SPACE_NONE:
                        /* Skip space - don't add anything */
                        break;
                    case WIKILINK_SPACE_UNDERSCORE:
                        *p++ = '_';
                        break;
                    case WIKILINK_SPACE_SPACE:
                        *p++ = ' ';
                        break;
                }
            } else {
                *p++ = *s;
            }
        }
    }

    /* Add extension (with leading dot if provided) */
    if (config->extension && config->extension[0] != '\0') {
        /* If extension doesn't start with dot, add one */
        if (config->extension[0] != '.') {
            *p++ = '.';
        }
        strcpy(p, config->extension);
        p += strlen(config->extension);
    }

    /* Add section anchor */
    if (section) {
        *p++ = '#';
        strcpy(p, section);
    } else {
        *p = '\0';
    }

    return url;
}

/**
 * Match function - called when we encounter [
 * Need to check for [[ specifically to avoid conflicting with standard markdown links
 */
__attribute__((unused))
static cmark_node *match_wiki_link(cmark_syntax_extension *self,
                                    cmark_parser *parser,
                                    cmark_node *parent,
                                    unsigned char character,
                                    cmark_inline_parser *inline_parser) {
    (void)self;
    (void)parent;
    if (character != '[') return NULL;

    /* Get current position and remaining input */
    int pos = cmark_inline_parser_get_offset(inline_parser);
    cmark_chunk *chunk = cmark_inline_parser_get_chunk(inline_parser);

    if (pos >= chunk->len) return NULL;

    const char *input = (const char *)chunk->data + pos;
    int remaining = chunk->len - pos;

    /* Do not parse Obsidian embeds ![[...]] as regular wiki links. */
    if (pos > 0 && chunk->data[pos - 1] == '!') {
        return NULL;
    }

    /* CRITICAL: Must check for [[ to distinguish from regular markdown links [text](url) */
    if (remaining < 2 || input[0] != '[' || input[1] != '[') {
        return NULL;  /* Not a wiki link, let standard link parser handle it */
    }

    /* Check if this is a wiki link */
    int consumed = scan_wiki_link(input, remaining);
    if (consumed == 0) return NULL;

    /* Extract the content between [[ and ]] */
    const char *content = input + 2;  /* Skip [[ */
    int content_len = consumed - 4;    /* Remove [[ and ]] */

    if (content_len <= 0) return NULL;

    /* Parse the wiki link */
    char *page = NULL;
    char *display = NULL;
    char *section = NULL;
    parse_wiki_link(content, content_len, &page, &display, &section);

    if (!page) return NULL;

    /* Get configuration */
    wiki_link_config *config = (wiki_link_config *)cmark_syntax_extension_get_private(self);
    if (!config) config = &default_config;

    /* Create URL */
    char *url = page_to_url(page, section, config);
    if (!url) {
        free(page);
        free(display);
        free(section);
        return NULL;
    }

    /* Create link node using parser memory */
    cmark_node *link = cmark_node_new_with_mem(CMARK_NODE_LINK, parser->mem);
    cmark_node_set_url(link, url);

    /* Create text node for display */
    cmark_node *text = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
    const char *link_text = display ? display : page;
    cmark_node_set_literal(text, link_text);

    /* Add text as child of link */
    cmark_node_append_child(link, text);

    /* Set line/column info */
    link->start_line = text->start_line =
        link->end_line = text->end_line = cmark_inline_parser_get_line(inline_parser);
    link->start_column = text->start_column = cmark_inline_parser_get_column(inline_parser) - 1;
    link->end_column = text->end_column = cmark_inline_parser_get_column(inline_parser) + consumed - 1;

    /* Advance the parser by setting new offset */
    cmark_inline_parser_set_offset(inline_parser, pos + consumed);

    /* Clean up */
    free(page);
    free(display);
    free(section);
    free(url);

    return link;
}

/**
 * Set wiki link configuration
 */
void wiki_links_set_config(cmark_syntax_extension *ext, wiki_link_config *config) {
    if (ext && config) {
        cmark_syntax_extension_set_private(ext, config, NULL);
    }
}

/**
 * Process wiki links in text nodes via AST walking (postprocessing approach)
 * This avoids conflicts with standard markdown link syntax
 */
void apex_process_wiki_links_in_tree(cmark_node *node, wiki_link_config *config) {
    if (!node) return;

    /* Process current node if it's text */
    if (cmark_node_get_type(node) == CMARK_NODE_TEXT) {
        const char *literal = cmark_node_get_literal(node);
        if (!literal) goto recurse;

        /* Fast path: no wiki markers present */
        const char *first_marker = strstr(literal, "[[");
        if (!first_marker) goto recurse;

        /* Default configuration if none provided */
        if (!config) config = &default_config;

        /* Rebuild this text node in a single pass to avoid repeated rescans */
        const char *cursor = literal;
        cmark_node *insert_after = NULL;
        bool changed = false;

        while (1) {
            const char *open = strstr(cursor, "[[");
            if (!open) {
                /* Append any trailing text */
                size_t tail_len = strlen(cursor);
                if (tail_len > 0) {
                    char *tail = malloc(tail_len + 1);
                    if (tail) {
                        memcpy(tail, cursor, tail_len);
                        tail[tail_len] = '\0';
                        cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                        cmark_node_set_literal(text, tail);
                        free(tail);
                        if (!insert_after) {
                            cmark_node_insert_before(node, text);
                        } else {
                            cmark_node_insert_after(insert_after, text);
                        }
                        insert_after = text;
                        changed = true;
                    }
                }
                break;
            }

            /* Copy text preceding the wiki link */
            size_t prefix_len = (size_t)(open - cursor);
            if (prefix_len > 0) {
                char *prefix = malloc(prefix_len + 1);
                if (prefix) {
                    memcpy(prefix, cursor, prefix_len);
                    prefix[prefix_len] = '\0';
                    cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(text, prefix);
                    free(prefix);
                    if (!insert_after) {
                        cmark_node_insert_before(node, text);
                    } else {
                        cmark_node_insert_after(insert_after, text);
                    }
                    insert_after = text;
                    changed = true;
                }
            }

            /* Look for closing marker after [[ */
            const char *close = strstr(open + 2, "]]");
            if (!close) {
                /* No closing marker - treat the rest as plain text */
                size_t remaining_len = strlen(open);
                char *remaining = malloc(remaining_len + 1);
                if (remaining) {
                    memcpy(remaining, open, remaining_len);
                    remaining[remaining_len] = '\0';
                    cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(text, remaining);
                    free(remaining);
                    if (!insert_after) {
                        cmark_node_insert_before(node, text);
                    } else {
                        cmark_node_insert_after(insert_after, text);
                    }
                    insert_after = text;
                    changed = true;
                }
                break;
            }

            size_t content_len = (size_t)(close - (open + 2));

            /* Preserve Obsidian embeds ![[...]] as literal text so include
             * preprocessing can handle them, and avoid rendering !<a ...>. */
            if (open > literal && open[-1] == '!') {
                size_t raw_len = (size_t)((close + 2) - open);
                char *raw = malloc(raw_len + 1);
                if (raw) {
                    memcpy(raw, open, raw_len);
                    raw[raw_len] = '\0';
                    cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(text, raw);
                    free(raw);
                    if (!insert_after) {
                        cmark_node_insert_before(node, text);
                    } else {
                        cmark_node_insert_after(insert_after, text);
                    }
                    insert_after = text;
                    changed = true;
                }
                cursor = close + 2;
                continue;
            }

            /* If empty content, keep literal text and continue */
            if (content_len == 0) {
                size_t raw_len = (size_t)((close + 2) - open);
                char *raw = malloc(raw_len + 1);
                if (raw) {
                    memcpy(raw, open, raw_len);
                    raw[raw_len] = '\0';
                    cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(text, raw);
                    free(raw);
                    if (!insert_after) {
                        cmark_node_insert_before(node, text);
                    } else {
                        cmark_node_insert_after(insert_after, text);
                    }
                    insert_after = text;
                    changed = true;
                }
                cursor = close + 2;
                continue;
            }

            /* Parse wiki link content */
            char *page = NULL;
            char *display = NULL;
            char *section = NULL;
            parse_wiki_link(open + 2, (int)content_len, &page, &display, &section);

            if (page) {
                char *url = page_to_url(page, section, config);
                if (url) {
                    cmark_node *link = cmark_node_new(CMARK_NODE_LINK);
                    cmark_node_set_url(link, url);

                    cmark_node *link_text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(link_text, display ? display : page);
                    cmark_node_append_child(link, link_text);

                    if (!insert_after) {
                        cmark_node_insert_before(node, link);
                    } else {
                        cmark_node_insert_after(insert_after, link);
                    }
                    insert_after = link;
                    changed = true;
                    free(url);
                } else {
                    /* Fallback: keep literal text if URL creation fails */
                    size_t raw_len = (size_t)((close + 2) - open);
                    char *raw = malloc(raw_len + 1);
                    if (raw) {
                        memcpy(raw, open, raw_len);
                        raw[raw_len] = '\0';
                        cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                        cmark_node_set_literal(text, raw);
                        free(raw);
                        if (!insert_after) {
                            cmark_node_insert_before(node, text);
                        } else {
                            cmark_node_insert_after(insert_after, text);
                        }
                        insert_after = text;
                        changed = true;
                    }
                }
            } else {
                /* Fallback: keep literal text if parsing fails */
                size_t raw_len = (size_t)((close + 2) - open);
                char *raw = malloc(raw_len + 1);
                if (raw) {
                    memcpy(raw, open, raw_len);
                    raw[raw_len] = '\0';
                    cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(text, raw);
                    free(raw);
                    if (!insert_after) {
                        cmark_node_insert_before(node, text);
                    } else {
                        cmark_node_insert_after(insert_after, text);
                    }
                    insert_after = text;
                    changed = true;
                }
            }

            free(page);
            free(display);
            free(section);

            /* Move past the current wiki link */
            cursor = close + 2;
        }

        if (changed) {
            /* Remove the original text node after rebuilding */
            /* Unlink the node first, then free it. Since we're returning immediately
             * and the parent iteration already has the next sibling, this is safe. */
            cmark_node_unlink(node);
            cmark_node_free(node);
            return;  /* Don't recurse into children after modifying tree */
        }
    }

recurse:
    /* Recursively process children */
    /* Get next sibling before processing to avoid issues if child modifies/frees tree */
    for (cmark_node *child = cmark_node_first_child(node); child; ) {
        cmark_node *next = cmark_node_next(child);
        apex_process_wiki_links_in_tree(child, config);
        child = next;
    }
}

/**
 * Create the wiki links extension (simplified - actual processing done via postprocessing)
 */
cmark_syntax_extension *create_wiki_links_extension(void) {
    /* Return NULL - we handle wiki links via postprocessing now */
    return NULL;
}

