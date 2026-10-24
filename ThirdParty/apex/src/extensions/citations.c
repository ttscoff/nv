/**
 * Citations Extension for Apex
 * Implementation
 */

#include "citations.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

/* Citation placeholder prefix - we'll use a unique marker */
#define CITATION_PLACEHOLDER_PREFIX "<!--CITE:"
#define CITATION_PLACEHOLDER_SUFFIX "-->"

/**
 * Check if character is valid in citation key
 * Citation keys can contain: alphanumerics, _, and internal punctuation (:.#$%&-+?<>~/)
 */
static bool is_valid_citation_char(char c) {
    return isalnum(c) || c == '_' || c == ':' || c == '.' || c == '#' ||
           c == '$' || c == '%' || c == '&' || c == '-' || c == '+' ||
           c == '?' || c == '<' || c == '>' || c == '~' || c == '/';
}

static int apex_ptrdiff_to_int(ptrdiff_t v) {
    if (v <= 0) return 0;
    if (v > INT_MAX) return INT_MAX;
    return (int)v;
}

static int apex_size_to_int(size_t v) {
    if (v > (size_t)INT_MAX) return INT_MAX;
    return (int)v;
}

/**
 * Extract citation key from text starting at pos
 * Returns length of key, or 0 if not valid
 */
static int extract_citation_key(const char *text, int pos, int len, char **key_out) {
    if (pos >= len) return 0;

    const char *start = text + pos;
    const char *p = start;

    /* Key must start with letter, digit, or _ */
    if (!isalnum(*p) && *p != '_') {
        /* Check for special keys that need curly braces */
        if (*p == '{') {
            p++;  /* Skip { */
            start = p;
            /* Find closing } */
            while (*p && *p != '}' && p - text < len) {
                if (!is_valid_citation_char(*p) && *p != '}') {
                    return 0;  /* Invalid character */
                }
                p++;
            }
            if (*p != '}') return 0;  /* No closing brace */
        } else {
            return 0;
        }
    }

    /* Collect key characters */
    const char *key_start = start;
    while (p - text < len && is_valid_citation_char(*p)) {
        p++;
    }

    if (p == key_start) return 0;  /* No key found */

    size_t key_len = p - key_start;
    *key_out = malloc(key_len + 1);
    if (!*key_out) return 0;

    memcpy(*key_out, key_start, key_len);
    (*key_out)[key_len] = '\0';

    /* If we started with {, skip past it */
    if (text[pos] == '{') {
        return apex_ptrdiff_to_int((p - text) - pos + 1);  /* +1 for closing } */
    }

    return apex_ptrdiff_to_int(p - start);
}

/**
 * Check if text matches RFC/BCP/STD/I-D/W3C pattern (mmark syntax)
 */
static bool is_mmark_pattern(const char *text, int pos, int len) {
    if (pos + 3 >= len) return false;

    const char *p = text + pos;

    /* Check for RFC, BCP, STD, I-D, or W3C prefix */
    if (strncmp(p, "RFC", 3) == 0 && isdigit(p[3])) return true;
    if (strncmp(p, "BCP", 3) == 0 && isdigit(p[3])) return true;
    if (strncmp(p, "STD", 3) == 0 && isdigit(p[3])) return true;
    if (strncmp(p, "I-D.", 4) == 0) return true;
    if (strncmp(p, "W3C.", 4) == 0) return true;

    return false;
}

/**
 * Quarto cross-ref keys (fig-/sec-/tbl-/eq-) use bare @ syntax and must not be
 * parsed as author-in-text citations when quarto-xrefs is enabled.
 */
static bool apex_citation_key_is_quarto_xref(const char *key) {
    if (!key) {
        return false;
    }
    static const char *prefixes[] = { "fig-", "sec-", "tbl-", "eq-", NULL };
    for (int i = 0; prefixes[i]; i++) {
        size_t n = strlen(prefixes[i]);
        if (strncmp(key, prefixes[i], n) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Parse Pandoc citation: [@key] or @key or [see @key, pp. 33-35]
 */
static int parse_pandoc_citation(const char *text, int pos, int len,
                                  apex_citation **citation_out, const apex_options *options) {
    if (pos >= len) return 0;

    const char *p = text + pos;
    bool in_brackets = false;
    bool author_suppressed = false;
    char *prefix = NULL;
    char *locator = NULL;
    char *suffix = NULL;
    char *key = NULL;

    /* Check for @key (author-in-text, no brackets).
     * Same boundary as the early-out scan: start of text, whitespace, or '('.
     * Mid-token '@' (ah-original@2x.png, user@example.com) is not a citation. */
    if (*p == '@' &&
        (p == text || isspace((unsigned char)p[-1]) || p[-1] == '(')) {
        p++;
        int key_len = extract_citation_key(text, apex_ptrdiff_to_int(p - text), len, &key);
        if (key_len > 0 && key) {
            /* `@Name: …` in prose (iA Writer Annotations dumps) must not swallow
             * the trailing colon into the key — that yields <!--CITE:Name:--> which
             * emoji then corrupts, and leaves the rest of the line as orphan text. */
            size_t key_strlen = strlen(key);
            if (key_strlen > 0 && key[key_strlen - 1] == ':') {
                const char *after_colon_key = p + key_len;
                if (after_colon_key >= text + len ||
                    isspace((unsigned char)*after_colon_key) ||
                    isdigit((unsigned char)*after_colon_key)) {
                    key[key_strlen - 1] = '\0';
                    key_len--;
                }
            }
            if (options && options->enable_quarto_xrefs && apex_citation_key_is_quarto_xref(key)) {
                free(key);
                return 0;
            }
            /* Check if there's a locator after: @key [p. 33] */
            const char *after_key = p + key_len;
            while (after_key < text + len && isspace(*after_key)) after_key++;
            if (after_key < text + len && *after_key == '[') {
                /* Extract locator from [p. 33] */
                const char *loc_start = after_key + 1;
                const char *loc_end = strchr(loc_start, ']');
                if (loc_end) {
                    size_t loc_len = loc_end - loc_start;
                    locator = malloc(loc_len + 1);
                    if (locator) {
                        memcpy(locator, loc_start, loc_len);
                        locator[loc_len] = '\0';
                    }
                    after_key = loc_end + 1;
                }
            }

            apex_citation *cite = apex_citation_new(key, APEX_CITATION_PANDOC);
            if (cite) {
                cite->author_in_text = true;
                cite->locator = locator;
                *citation_out = cite;
                free(key);
                return apex_ptrdiff_to_int(after_key - (text + pos));
            }
            free(key);
            free(locator);
            return 0;
        }
        return 0;
    }

    /* Check for [-@key] (author suppressed) */
    if (*p == '[' && p + 1 < text + len && p[1] == '-' && p[2] == '@') {
        author_suppressed = true;
        in_brackets = true;
        p += 3;  /* Skip [-@ */
    }
    /* Check for [@key] (normal citation) */
    else if (*p == '[' && p + 1 < text + len && p[1] == '@') {
        in_brackets = true;
        p += 2;  /* Skip [@ */
    } else {
        return 0;  /* Not a Pandoc citation */
    }

    /* Extract key */
    int key_len = extract_citation_key(text, apex_ptrdiff_to_int(p - text), len, &key);
    if (key_len == 0 || !key) {
        return 0;
    }
    p += key_len;

    /* If we're in brackets, look for prefix, locator, suffix */
    if (in_brackets) {
        /* Skip whitespace */
        while (p < text + len && isspace(*p)) p++;

        /* Check for prefix before @ (e.g., "see @key") */
        if (p > text + pos + 2) {
            const char *prefix_start = text + pos + 1;
            const char *at_pos = strchr(prefix_start, '@');
            if (at_pos && at_pos < p) {
                size_t prefix_len = at_pos - prefix_start;
                while (prefix_len > 0 && isspace(prefix_start[prefix_len - 1])) prefix_len--;
                if (prefix_len > 0) {
                    prefix = malloc(prefix_len + 1);
                    if (prefix) {
                        memcpy(prefix, prefix_start, prefix_len);
                        prefix[prefix_len] = '\0';
                    }
                }
            }
        }

        /* Look for locator and suffix after key */
        while (p < text + len && *p != ']') {
            if (*p == ',') {
                p++;
                while (p < text + len && isspace(*p)) p++;

                /* Try to extract locator (e.g., "pp. 33-35") */
                const char *loc_start = p;
                while (p < text + len && *p != ']' && *p != ';') p++;

                if (p > loc_start) {
                    size_t loc_len = p - loc_start;
                    /* Check if this looks like a locator (contains page/chapter indicators) */
                    const char *loc_test = loc_start;
                    bool looks_like_locator = false;
                    while (loc_test < loc_start + loc_len) {
                        if (strncmp(loc_test, "p.", 2) == 0 || strncmp(loc_test, "pp.", 3) == 0 ||
                            strncmp(loc_test, "chap.", 5) == 0 || strncmp(loc_test, "chapter", 7) == 0 ||
                            strncmp(loc_test, "sec.", 4) == 0 || strncmp(loc_test, "section", 7) == 0) {
                            looks_like_locator = true;
                            break;
                        }
                        loc_test++;
                    }

                    if (looks_like_locator || isdigit(*loc_start)) {
                        locator = malloc(loc_len + 1);
                        if (locator) {
                            memcpy(locator, loc_start, loc_len);
                            locator[loc_len] = '\0';
                        }
                    } else {
                        /* Might be suffix */
                        suffix = malloc(loc_len + 1);
                        if (suffix) {
                            memcpy(suffix, loc_start, loc_len);
                            suffix[loc_len] = '\0';
                        }
                    }
                }
            } else if (*p == ';') {
                /* Multiple citations - for now, just handle first one */
                break;
            } else {
                p++;
            }
        }

        if (p < text + len && *p == ']') {
            p++;  /* Skip closing ] */
        }
    }

    apex_citation *cite = apex_citation_new(key, APEX_CITATION_PANDOC);
    if (cite) {
        cite->author_suppressed = author_suppressed;
        cite->prefix = prefix;
        cite->locator = locator;
        cite->suffix = suffix;
        *citation_out = cite;
        free(key);
        return apex_ptrdiff_to_int(p - (text + pos));
    }

    free(key);
    free(prefix);
    free(locator);
    free(suffix);
    return 0;
}

/**
 * Parse MultiMarkdown citation: [#key] or [p. 23][#key]
 */
static int parse_mmd_citation(const char *text, int pos, int len,
                               apex_citation **citation_out, const apex_options *options) {
    (void)options;  /* Unused for now */
    if (pos + 2 >= len) return 0;

    const char *p = text + pos;

    /* Must start with [# */
    if (*p != '[' || p[1] != '#') return 0;

    p += 2;  /* Skip [# */

    /* Extract key */
    char *key = NULL;
    int key_len = extract_citation_key(text, apex_ptrdiff_to_int(p - text), len, &key);
    if (key_len == 0 || !key) {
        return 0;
    }
    p += key_len;

    /* Must end with ] */
    if (p >= text + len || *p != ']') {
        free(key);
        return 0;
    }
    p++;  /* Skip ] */

    /* Check for locator before: [p. 23][#key] */
    char *locator = NULL;
    if (pos > 0 && text[pos - 1] == ']') {
        /* Look backwards for opening [ */
        const char *loc_start = text + pos - 1;
        while (loc_start > text && *loc_start != '[') loc_start--;
        if (*loc_start == '[') {
            loc_start++;  /* Skip [ */
            size_t loc_len = (text + pos - 1) - loc_start;
            if (loc_len > 0) {
                locator = malloc(loc_len + 1);
                if (locator) {
                    memcpy(locator, loc_start, loc_len);
                    locator[loc_len] = '\0';
                }
            }
        }
    }

    apex_citation *cite = apex_citation_new(key, APEX_CITATION_MMD);
    if (cite) {
        cite->locator = locator;
        *citation_out = cite;
        free(key);
        return apex_ptrdiff_to_int(p - (text + pos));
    }

    free(key);
    free(locator);
    return 0;
}

/**
 * Parse mmark citation: [@RFC2535] or [@!RFC1034] or [@-RFC1000]
 */
static int parse_mmark_citation(const char *text, int pos, int len,
                                 apex_citation **citation_out, const apex_options *options) {
    (void)options;  /* Unused for now */
    if (pos + 3 >= len) return 0;

    const char *p = text + pos;

    /* Must start with [@ */
    if (*p != '[' || p[1] != '@') return 0;

    p += 2;  /* Skip [@ */

    /* Check for modifier: ! (normative), ? (informative), - (suppressed) */
    bool author_suppressed = false;
    if (*p == '!') {
        p++;  /* Normative - for now just skip */
    } else if (*p == '?') {
        p++;  /* Informative - for now just skip */
    } else if (*p == '-') {
        author_suppressed = true;
        p++;  /* Suppressed */
    }

    /* Check if it's an RFC/BCP/STD/I-D/W3C pattern */
    if (!is_mmark_pattern(text, apex_ptrdiff_to_int(p - text), len)) {
        return 0;  /* Not mmark pattern, might be Pandoc */
    }

    /* Extract key (RFC1234, BCP123, etc.) */
    const char *key_start = p;
    while (p < text + len && (isalnum(*p) || *p == '.' || *p == '-' || *p == '#')) {
        p++;
    }

    if (p == key_start) {
        return 0;  /* No key found */
    }

    size_t key_len = p - key_start;
    char *key = malloc(key_len + 1);
    if (!key) return 0;
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    /* Check for multiple citations: [@RFC1034;@RFC1035] */
    /* For now, just handle first one */
    if (p < text + len && *p == ';') {
        p++;  /* Skip ; */
    }

    /* Must end with ] */
    if (p >= text + len || *p != ']') {
        free(key);
        return 0;
    }
    p++;  /* Skip ] */

    apex_citation *cite = apex_citation_new(key, APEX_CITATION_MMARK);
    if (cite) {
        cite->author_suppressed = author_suppressed;
        *citation_out = cite;
        free(key);
        return apex_ptrdiff_to_int(p - (text + pos));
    }

    free(key);
    return 0;
}

/**
 * Process citations in text
 * Returns modified text with citations replaced by placeholders
 */
char *apex_process_citations(const char *text, apex_citation_registry *registry, const apex_options *options) {
    if (!text || !registry || !options) return NULL;

    /* Citations only enabled in certain modes */
    if (options->mode != APEX_MODE_MULTIMARKDOWN &&
        !apex_mode_is_unified_family(options->mode)) {
        return NULL;  /* Citations not enabled in this mode */
    }

    if (!options->enable_citations) {
        return NULL;  /* Citations disabled */
    }

    size_t len = strlen(text);

    /* Quick scan: check if any citation patterns exist before processing */
    /* Look for various citation patterns:
     * - [@key] or [@!key] or [@?key] (Pandoc-style)
     * - @key (author-in-text)
     * - [#key] (MultiMarkdown-style)
     */
    bool has_citation_pattern = false;
    const char *p = text;
    while (*p && p < text + len - 1) {
        if ((*p == '[' && p[1] == '@') ||  /* Pandoc [@...] */
            (*p == '[' && p[1] == '#') ||  /* MultiMarkdown [#...] */
            (*p == '@' && (p == text || isspace((unsigned char)p[-1]) || p[-1] == '('))) {  /* Author-in-text @... */
            has_citation_pattern = true;
            break;
        }
        p++;
    }

    /* Early exit if no citation patterns found */
    if (!has_citation_pattern) {
        return NULL;
    }

    size_t capacity = len * 2;  /* Room for placeholders */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;
    bool in_code_block = false;
    bool in_inline_code = false;

    while (*read && read - text < (int)len) {
        /* Track code blocks */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Skip citations inside code */
        if (in_code_block || in_inline_code) {
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }

        int pos = apex_ptrdiff_to_int(read - text);
        apex_citation *citation = NULL;
        int consumed = 0;

        /* Try mmark first (most specific pattern) */
        if (apex_mode_is_unified_family(options->mode)) {
            consumed = parse_mmark_citation(text, pos, apex_size_to_int(len), &citation, options);
        }

        /* Try MultiMarkdown */
        if (!citation && (options->mode == APEX_MODE_MULTIMARKDOWN || apex_mode_is_unified_family(options->mode))) {
            consumed = parse_mmd_citation(text, pos, apex_size_to_int(len), &citation, options);
        }

        /* Try Pandoc (most common) */
        if (!citation && apex_mode_is_unified_family(options->mode)) {
            consumed = parse_pandoc_citation(text, pos, apex_size_to_int(len), &citation, options);
        }

        if (citation && consumed > 0) {
            /* Add citation to registry */
            citation->position = pos;
            citation->next = registry->citations;
            registry->citations = citation;
            registry->count++;

            /* Insert placeholder */
            char placeholder[256];
            snprintf(placeholder, sizeof(placeholder), "%s%s%s",
                    CITATION_PLACEHOLDER_PREFIX, citation->key, CITATION_PLACEHOLDER_SUFFIX);
            size_t placeholder_len = strlen(placeholder);

            if (placeholder_len < remaining) {
                memcpy(write, placeholder, placeholder_len);
                write += placeholder_len;
                remaining -= placeholder_len;
            }

            read += consumed;
        } else {
            /* Copy character */
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
        }
    }

    *write = '\0';
    return output;
}

/**
 * Render citations in HTML
 * Replaces placeholders with formatted HTML
 */
char *apex_render_citations(const char *html, apex_citation_registry *registry, const apex_options *options) {
    if (!html || !registry || !options) return NULL;

    if (!options->enable_citations) {
        return NULL;
    }

    /* For now, just replace placeholders with simple formatted citations */
    size_t len = strlen(html);
    size_t capacity = len * 3 + 1;  /* Room for HTML tags + null terminator */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;
    size_t prefix_len = strlen(CITATION_PLACEHOLDER_PREFIX);
    size_t suffix_len = strlen(CITATION_PLACEHOLDER_SUFFIX);

    while (*read) {
        if (strncmp(read, CITATION_PLACEHOLDER_PREFIX, prefix_len) == 0) {
            /* Found placeholder */
            const char *key_start = read + prefix_len;
            const char *key_end = strstr(key_start, CITATION_PLACEHOLDER_SUFFIX);

            if (key_end) {
                size_t key_len = key_end - key_start;
                char *key = malloc(key_len + 1);
                if (key) {
                    memcpy(key, key_start, key_len);
                    key[key_len] = '\0';

                    /* Find citation in registry */
                    apex_citation *cite = registry->citations;
                    while (cite) {
                        if (cite->key && strcmp(cite->key, key) == 0) {
                            /* Try to find bibliography entry for better formatting */
                            apex_bibliography_entry *bib_entry = NULL;
                            if (registry->bibliography) {
                                bib_entry = apex_find_bibliography_entry(registry->bibliography, key);
                            }

                            /* Format citation HTML */
                            char citation_html[512];
                            char citation_text[256] = "";

                            if (bib_entry) {
                                /* Use bibliography data for formatting */
                                if (cite->author_in_text) {
                                    if (bib_entry->author && bib_entry->year) {
                                        snprintf(citation_text, sizeof(citation_text), "%s (%s)",
                                                bib_entry->author, bib_entry->year);
                                    } else if (bib_entry->author) {
                                        strncpy(citation_text, bib_entry->author, sizeof(citation_text) - 1);
                                    } else {
                                        strncpy(citation_text, cite->key, sizeof(citation_text) - 1);
                                    }
                                } else if (cite->author_suppressed) {
                                    if (bib_entry->year) {
                                        snprintf(citation_text, sizeof(citation_text), "(%s)", bib_entry->year);
                                    } else {
                                        snprintf(citation_text, sizeof(citation_text), "(%s)", cite->key);
                                    }
                                } else {
                                    if (bib_entry->author && bib_entry->year) {
                                        snprintf(citation_text, sizeof(citation_text), "(%s %s)",
                                                bib_entry->author, bib_entry->year);
                                    } else if (bib_entry->year) {
                                        snprintf(citation_text, sizeof(citation_text), "(%s)", bib_entry->year);
                                    } else {
                                        snprintf(citation_text, sizeof(citation_text), "(%s)", cite->key);
                                    }
                                }
                            } else {
                                /* No bibliography entry, use simple format */
                                if (cite->author_in_text) {
                                    strncpy(citation_text, cite->key, sizeof(citation_text) - 1);
                                } else {
                                    snprintf(citation_text, sizeof(citation_text), "(%s)", cite->key);
                                }
                            }

                            /* Generate HTML with or without link */
                            if (options->link_citations) {
                                snprintf(citation_html, sizeof(citation_html),
                                        "<a href=\"#ref-%s\" class=\"citation\" data-cites=\"%s\">%s</a>",
                                        cite->key, cite->key, citation_text);
                            } else {
                                snprintf(citation_html, sizeof(citation_html),
                                        "<span class=\"citation\" data-cites=\"%s\">%s</span>",
                                        cite->key, citation_text);
                            }

                            size_t html_len = strlen(citation_html);
                            if (html_len < remaining) {
                                memcpy(write, citation_html, html_len);
                                write += html_len;
                                remaining -= html_len;
                            }
                            break;
                        }
                        cite = cite->next;
                    }

                    free(key);
                }

                read = key_end + suffix_len;
                continue;
            }
        }

        /* Copy character */
        if (remaining > 1) {  /* Reserve 1 byte for null terminator */
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    /* Null terminate - ensure we have space */
    if (remaining > 0) {
        *write = '\0';
    } else {
        /* Should never happen, but be safe */
        free(output);
        return strdup(html);
    }
    return output;
}

/**
 * Format a bibliography entry as HTML
 */
static char *format_bibliography_entry(apex_bibliography_entry *entry) {
    if (!entry) return NULL;

    size_t capacity = 2048;  /* Larger capacity for full entries */
    char *html = malloc(capacity);
    if (!html) return NULL;

    char *write = html;
    size_t remaining = capacity;
    int written;

    /* Start entry div */
    written = snprintf(write, remaining, "<div id=\"ref-%s\" class=\"csl-entry\">", entry->id ? entry->id : "");
    if (written < 0 || (size_t)written >= remaining) {
        free(html);
        return NULL;
    }
    write += written;
    remaining -= written;

    /* Format entry content - author-date style */
    bool has_content = false;

    /* Author */
    if (entry->author && entry->author[0] != '\0') {
        written = snprintf(write, remaining, "%s", entry->author);
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Year */
    if (entry->year && entry->year[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, " %s", entry->year);
        } else {
            written = snprintf(write, remaining, "%s", entry->year);
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Title */
    if (entry->title && entry->title[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, ". ");
        } else {
            written = snprintf(write, remaining, "");
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
        }
        written = snprintf(write, remaining, "<em>%s</em>", entry->title);
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Container title (journal) */
    if (entry->container_title && entry->container_title[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, ". ");
        } else {
            written = snprintf(write, remaining, "");
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
        }
        written = snprintf(write, remaining, "<em>%s</em>", entry->container_title);
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Volume and pages */
    if (entry->volume && entry->volume[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, " ");
        } else {
            written = snprintf(write, remaining, "");
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
        }
        written = snprintf(write, remaining, "%s", entry->volume);
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    if (entry->page && entry->page[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, ": %s", entry->page);
        } else {
            written = snprintf(write, remaining, "%s", entry->page);
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Publisher */
    if (entry->publisher && entry->publisher[0] != '\0') {
        if (has_content) {
            written = snprintf(write, remaining, ". ");
        } else {
            written = snprintf(write, remaining, "");
        }
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
        }
        written = snprintf(write, remaining, "%s", entry->publisher);
        if (written > 0 && (size_t)written < remaining) {
            write += written;
            remaining -= written;
            has_content = true;
        }
    }

    /* Close entry div */
    written = snprintf(write, remaining, "</div>\n");
    if (written > 0 && (size_t)written < remaining) {
        write += written;
    }

    return html;
}

/**
 * Generate bibliography HTML from cited entries
 */
char *apex_generate_bibliography(apex_citation_registry *registry, const apex_options *options) {
    if (!registry || !options) return NULL;

    if (options->suppress_bibliography) {
        return NULL;
    }

    if (!registry->bibliography) {
        return NULL;
    }

    if (registry->bibliography->count == 0) {
        return NULL;
    }

    /* Collect unique cited entries */
    size_t cited_count = 0;
    size_t cited_capacity = 16;
    apex_bibliography_entry **cited_entries = malloc(cited_capacity * sizeof(apex_bibliography_entry*));
    if (!cited_entries) {
        return NULL;
    }

    /* Build list of cited entries */
    apex_citation *cite = registry->citations;
    while (cite) {
        if (cite->key) {
            apex_bibliography_entry *entry = apex_find_bibliography_entry(registry->bibliography, cite->key);
            if (entry) {
                /* Check if already in list */
                bool already_added = false;
                for (size_t i = 0; i < cited_count; i++) {
                    if (cited_entries[i] == entry) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    /* Add to list */
                    if (cited_count >= cited_capacity) {
                        cited_capacity *= 2;
                        apex_bibliography_entry **new_entries = realloc(cited_entries, cited_capacity * sizeof(apex_bibliography_entry*));
                        if (!new_entries) {
                            free(cited_entries);
                            return NULL;
                        }
                        cited_entries = new_entries;
                    }
                    cited_entries[cited_count++] = entry;
                }
            }
        }
        cite = cite->next;
    }

    if (cited_count == 0) {
        if (cited_entries) free(cited_entries);
        /* No cited entries found - bibliography might not match citations */
        /* Return NULL so insertion doesn't add empty bibliography */
        return NULL;
    }

    /* Generate bibliography HTML */
    size_t capacity = 4096;
    char *html = malloc(capacity);
    if (!html) {
        free(cited_entries);
        return NULL;
    }

    char *write = html;
    size_t remaining = capacity;

    /* Start bibliography div */
    snprintf(write, remaining, "<div id=\"refs\" class=\"references csl-bib-body\">\n");
    size_t len = strlen(write);
    write += len;
    remaining -= len;

    /* Add each entry */
    for (size_t i = 0; i < cited_count; i++) {
        if (!cited_entries[i]) {
            continue;
        }
        char *entry_html = format_bibliography_entry(cited_entries[i]);
        if (entry_html) {
            size_t entry_len = strlen(entry_html);
            if (entry_len >= remaining) {
                size_t used = write - html;
                capacity = (used + entry_len + 1) * 2;
                char *new_html = realloc(html, capacity);
                if (!new_html) {
                    free(html);
                    free(entry_html);
                    free(cited_entries);
                    return NULL;
                }
                html = new_html;
                write = html + used;
                remaining = capacity - used;
            }
            memcpy(write, entry_html, entry_len);
            write += entry_len;
            remaining -= entry_len;
            free(entry_html);
        }
    }

    /* Close bibliography div */
    snprintf(write, remaining, "</div>\n");
    write += strlen(write);

    free(cited_entries);
    return html;
}

/**
 * Insert bibliography at <!-- REFERENCES --> marker or end of document
 */
char *apex_insert_bibliography(const char *html, apex_citation_registry *registry, const apex_options *options) {
    if (!html || !registry || !options) return NULL;

    if (options->suppress_bibliography) {
        return strdup(html);
    }

    /* Generate bibliography */
    char *bibliography_html = apex_generate_bibliography(registry, options);
    if (!bibliography_html) {
        return strdup(html);  /* No bibliography to insert */
    }

    size_t bib_len = strlen(bibliography_html);
    size_t html_len = strlen(html);
    size_t output_capacity = html_len + bib_len + 100;
    char *output = malloc(output_capacity);
    if (!output) {
        free(bibliography_html);
        return strdup(html);
    }

    /* Look for <!-- REFERENCES --> marker */
    const char *refs_marker = strstr(html, "<!-- REFERENCES -->");
    if (refs_marker) {
        /* Insert at marker */
        size_t before_len = refs_marker - html;
        const char *after = refs_marker + strlen("<!-- REFERENCES -->");
        size_t after_len = strlen(after);

        memcpy(output, html, before_len);
        memcpy(output + before_len, bibliography_html, bib_len);
        memcpy(output + before_len + bib_len, after, after_len + 1);
    } else {
        /* Look for {backmatter} marker (mmark style) */
        const char *backmatter = strstr(html, "{backmatter}");
        if (backmatter) {
            size_t before_len = backmatter - html;
            const char *after = backmatter + strlen("{backmatter}");
            size_t after_len = strlen(after);

            memcpy(output, html, before_len);
            memcpy(output + before_len, bibliography_html, bib_len);
            memcpy(output + before_len + bib_len, after, after_len + 1);
        } else {
            /* Look for <div id="refs"> */
            const char *refs_div = strstr(html, "<div id=\"refs\">");
            if (refs_div) {
                /* Insert before closing </div> */
                const char *div_end = strstr(refs_div, "</div>");
                if (div_end) {
                    size_t before_len = div_end - html;
                    size_t after_len = strlen(div_end);

                    memcpy(output, html, before_len);
                    memcpy(output + before_len, bibliography_html, bib_len);
                    memcpy(output + before_len + bib_len, div_end, after_len + 1);
                } else {
                    /* No closing div, append to end */
                    memcpy(output, html, html_len);
                    memcpy(output + html_len, bibliography_html, bib_len + 1);
                }
            } else {
                /* Append to end of document */
                memcpy(output, html, html_len);
                memcpy(output + html_len, bibliography_html, bib_len + 1);
            }
        }
    }

    free(bibliography_html);
    return output;
}

/**
 * Create citation extension (stub - uses preprocessing)
 */
cmark_syntax_extension *create_citations_extension(void) {
    return NULL;  /* Citations handled via preprocessing */
}

/**
 * Create a new citation
 */
apex_citation *apex_citation_new(const char *key, apex_citation_syntax_t syntax_type) {
    if (!key) return NULL;

    apex_citation *cite = malloc(sizeof(apex_citation));
    if (!cite) return NULL;

    cite->key = strdup(key);
    cite->prefix = NULL;
    cite->locator = NULL;
    cite->suffix = NULL;
    cite->author_suppressed = false;
    cite->author_in_text = false;
    cite->syntax_type = syntax_type;
    cite->position = 0;
    cite->next = NULL;

    return cite;
}

/**
 * Free a citation
 */
void apex_citation_free(apex_citation *citation) {
    if (!citation) return;

    free(citation->key);
    free(citation->prefix);
    free(citation->locator);
    free(citation->suffix);
    free(citation);
}

/**
 * Free citation registry
 */
void apex_free_citation_registry(apex_citation_registry *registry) {
    if (!registry) return;

    apex_citation *cite = registry->citations;
    while (cite) {
        apex_citation *next = cite->next;
        apex_citation_free(cite);
        cite = next;
    }

    if (registry->bibliography) {
        apex_free_bibliography_registry(registry->bibliography);
    }

    registry->citations = NULL;
    registry->count = 0;
    registry->bibliography = NULL;
}

/**
 * Read file into buffer
 */
static char *read_bibliography_file(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0 || file_size > 10 * 1024 * 1024) {  /* Max 10MB */
        fclose(fp);
        return NULL;
    }

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    return buffer;
}

/**
 * Resolve bibliography file path relative to base directory
 */
static char *resolve_bibliography_path(const char *filepath, const char *base_directory) {
    if (!filepath) return NULL;

    /* If absolute path or starts with ./ or ../, use as-is */
    if (filepath[0] == '/' || (filepath[0] == '.' && (filepath[1] == '/' || filepath[1] == '.'))) {
        return strdup(filepath);
    }

    /* If base_directory is provided, resolve relative to it */
    if (base_directory && base_directory[0] != '\0') {
        size_t base_len = strlen(base_directory);
        size_t file_len = strlen(filepath);
        char *resolved = malloc(base_len + file_len + 2);
        if (resolved) {
            strcpy(resolved, base_directory);
            if (resolved[base_len - 1] != '/') {
                resolved[base_len] = '/';
                base_len++;
            }
            strcpy(resolved + base_len, filepath);
        }
        return resolved;
    }

    return strdup(filepath);
}

/**
 * Detect bibliography format from file extension
 */
typedef enum {
    BIB_FORMAT_BIBTEX,
    BIB_FORMAT_CSL_JSON,
    BIB_FORMAT_CSL_YAML,
    BIB_FORMAT_UNKNOWN
} bibliography_format_t;

static bibliography_format_t detect_bibliography_format(const char *filepath) {
    if (!filepath) return BIB_FORMAT_UNKNOWN;

    const char *ext = strrchr(filepath, '.');
    if (!ext) return BIB_FORMAT_UNKNOWN;

    ext++;  /* Skip the dot */

    if (strcasecmp(ext, "bib") == 0 || strcasecmp(ext, "bibtex") == 0) {
        return BIB_FORMAT_BIBTEX;
    } else if (strcasecmp(ext, "json") == 0) {
        return BIB_FORMAT_CSL_JSON;
    } else if (strcasecmp(ext, "yaml") == 0 || strcasecmp(ext, "yml") == 0) {
        return BIB_FORMAT_CSL_YAML;
    }

    return BIB_FORMAT_UNKNOWN;
}

/**
 * Create a new bibliography entry
 */
static apex_bibliography_entry *bibliography_entry_new(const char *id) {
    if (!id) return NULL;

    apex_bibliography_entry *entry = malloc(sizeof(apex_bibliography_entry));
    if (!entry) return NULL;

    entry->id = strdup(id);
    entry->type = NULL;
    entry->title = NULL;
    entry->author = NULL;
    entry->year = NULL;
    entry->container_title = NULL;
    entry->publisher = NULL;
    entry->volume = NULL;
    entry->page = NULL;
    entry->raw_data = NULL;
    entry->next = NULL;

    return entry;
}

/**
 * Free bibliography entry
 */
void apex_bibliography_entry_free(apex_bibliography_entry *entry) {
    if (!entry) return;

    free(entry->id);
    free(entry->type);
    free(entry->title);
    free(entry->author);
    free(entry->year);
    free(entry->container_title);
    free(entry->publisher);
    free(entry->volume);
    free(entry->page);
    free(entry->raw_data);
    free(entry);
}

/**
 * Free bibliography registry
 */
void apex_free_bibliography_registry(apex_bibliography_registry *registry) {
    if (!registry) return;

    apex_bibliography_entry *entry = registry->entries;
    while (entry) {
        apex_bibliography_entry *next = entry->next;
        apex_bibliography_entry_free(entry);
        entry = next;
    }

    registry->entries = NULL;
    registry->count = 0;
}

/**
 * Find bibliography entry by ID
 */
apex_bibliography_entry *apex_find_bibliography_entry(apex_bibliography_registry *registry, const char *id) {
    if (!registry || !id) return NULL;

    apex_bibliography_entry *entry = registry->entries;
    while (entry) {
        if (entry->id && strcmp(entry->id, id) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/**
 * Trim whitespace from string (in-place)
 */
static char *trim_string(char *str) {
    if (!str) return NULL;

    /* Trim leading whitespace */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

/**
 * Remove braces from BibTeX value
 */
static char *remove_braces(const char *value) {
    if (!value) return NULL;

    size_t len = strlen(value);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    const char *src = value;
    char *dst = result;
    while (*src) {
        if (*src != '{' && *src != '}') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    return trim_string(result);
}

/**
 * Parse BibTeX entry type
 */
static const char *parse_bibtex_entry_type(const char *text, const char **entry_start) {
    if (!text) return NULL;

    const char *p = text;

    /* Skip whitespace */
    while (*p && isspace(*p)) p++;

    /* Must start with @ */
    if (*p != '@') return NULL;
    p++;

    /* Extract entry type (between @ and {) */
    const char *type_start = p;
    while (*p && *p != '{' && !isspace(*p)) p++;

    if (*p != '{') return NULL;

    /* Extract entry type */
    const char *type_end = p;  /* At { */

    size_t type_len = type_end - type_start;
    if (type_len == 0) return NULL;

    char *type = malloc(type_len + 1);
    if (!type) return NULL;
    memcpy(type, type_start, type_len);
    type[type_len] = '\0';

    /* Convert to lowercase */
    for (char *t = type; *t; t++) {
        *t = tolower(*t);
    }

    if (entry_start) *entry_start = p + 1;  /* After { */

    return type;
}

/**
 * Parse BibTeX field: key = {value} or key = value
 * Returns true if successful, and sets *end_pos to position after the field
 */
static bool parse_bibtex_field(const char *text, char **key_out, char **value_out, const char **end_pos) {
    if (!text) return false;

    const char *p = text;

    /* Skip whitespace */
    while (*p && isspace(*p)) p++;
    if (!*p) return false;

    /* Find = */
    const char *key_start = p;
    const char *equals = strchr(p, '=');
    if (!equals) return false;

    /* Extract key */
    const char *key_end = equals;
    while (key_end > key_start && isspace(key_end[-1])) key_end--;

    size_t key_len = key_end - key_start;
    if (key_len == 0) return false;

    *key_out = malloc(key_len + 1);
    if (!*key_out) return false;
    memcpy(*key_out, key_start, key_len);
    (*key_out)[key_len] = '\0';
    trim_string(*key_out);

    /* Find value */
    p = equals + 1;
    while (*p && isspace(*p)) p++;

    if (*p == '{') {
        /* Braced value */
        p++;
        const char *value_start = p;
        int brace_depth = 1;

        while (*p && brace_depth > 0) {
            if (*p == '{') brace_depth++;
            else if (*p == '}') brace_depth--;
            p++;
        }

        if (brace_depth == 0) {
            size_t value_len = (p - 1) - value_start;  /* Exclude closing } */
            *value_out = malloc(value_len + 1);
            if (*value_out) {
                memcpy(*value_out, value_start, value_len);
                (*value_out)[value_len] = '\0';
                char *cleaned = remove_braces(*value_out);
                if (cleaned) {
                    free(*value_out);
                    *value_out = cleaned;
                }
            }
            if (end_pos) {
                *end_pos = p;  /* Position after closing } */
            }
        } else {
            /* Unmatched braces, return failure */
            if (end_pos) *end_pos = NULL;
            return false;
        }
    } else {
        /* Unbraced value (until comma or closing brace) */
        const char *value_start = p;
        while (*p && *p != ',' && *p != '}') p++;

        size_t value_len = p - value_start;
        while (value_len > 0 && isspace(value_start[value_len - 1])) value_len--;

        *value_out = malloc(value_len + 1);
        if (*value_out) {
            memcpy(*value_out, value_start, value_len);
            (*value_out)[value_len] = '\0';
            trim_string(*value_out);
        }
        if (end_pos) {
            *end_pos = p;  /* Position after the field value */
        }
    }

    return (*value_out != NULL);

    return (*value_out != NULL);
}

/**
 * Map BibTeX entry type to CSL type
 */
static const char *bibtex_to_csl_type(const char *bibtex_type) {
    if (!bibtex_type) return "article";

    /* Common BibTeX types */
    if (strcmp(bibtex_type, "article") == 0) return "article-journal";
    if (strcmp(bibtex_type, "book") == 0) return "book";
    if (strcmp(bibtex_type, "inbook") == 0) return "chapter";
    if (strcmp(bibtex_type, "incollection") == 0) return "chapter";
    if (strcmp(bibtex_type, "inproceedings") == 0) return "paper-conference";
    if (strcmp(bibtex_type, "phdthesis") == 0) return "thesis";
    if (strcmp(bibtex_type, "mastersthesis") == 0) return "thesis";
    if (strcmp(bibtex_type, "techreport") == 0) return "report";

    return "article";  /* Default */
}

/**
 * Parse BibTeX file
 */
apex_bibliography_registry *apex_parse_bibtex(const char *content) {
    if (!content) return NULL;

    apex_bibliography_registry *registry = malloc(sizeof(apex_bibliography_registry));
    if (!registry) return NULL;
    registry->entries = NULL;
    registry->count = 0;

    const char *p = content;

    while (*p) {
        /* Find @ entry */
        while (*p && *p != '@') p++;
        if (!*p) break;

        const char *entry_start;
        const char *type_str = parse_bibtex_entry_type(p, &entry_start);
        if (!type_str) {
            p++;
            continue;
        }

        /* Find entry key (after {) */
        const char *key_start = entry_start;
        while (*key_start && (isspace(*key_start) || *key_start == ',')) key_start++;

        const char *key_end = key_start;
        while (*key_end && *key_end != ',' && *key_end != '}') key_end++;

        if (key_end == key_start) {
            free((void*)type_str);
            p++;
            continue;
        }

        size_t key_len = key_end - key_start;
        char *entry_id = malloc(key_len + 1);
        if (!entry_id) {
            free((void*)type_str);
            p++;
            continue;
        }
        memcpy(entry_id, key_start, key_len);
        entry_id[key_len] = '\0';
        trim_string(entry_id);

        /* Create entry */
        apex_bibliography_entry *entry = bibliography_entry_new(entry_id);
        free(entry_id);

        if (entry) {
            entry->type = strdup(bibtex_to_csl_type(type_str));

            /* Parse fields */
            const char *field_start = key_end;
            while (*field_start && *field_start != '}') {
                if (*field_start == ',') field_start++;
                while (*field_start && isspace(*field_start)) field_start++;
                if (*field_start == '}') break;

                char *field_key = NULL;
                char *field_value = NULL;
                const char *field_end = NULL;

                if (parse_bibtex_field(field_start, &field_key, &field_value, &field_end)) {
                    if (field_key && field_value) {
                        /* Map BibTeX fields to CSL fields */
                        if (strcasecmp(field_key, "title") == 0) {
                            entry->title = field_value;
                            field_value = NULL;  /* Don't free, we're using it */
                        } else if (strcasecmp(field_key, "author") == 0) {
                            entry->author = field_value;
                            field_value = NULL;
                        } else if (strcasecmp(field_key, "year") == 0) {
                            entry->year = field_value;
                            field_value = NULL;
                        } else if (strcasecmp(field_key, "journal") == 0) {
                            entry->container_title = field_value;
                            field_value = NULL;
                        } else if (strcasecmp(field_key, "publisher") == 0) {
                            entry->publisher = field_value;
                            field_value = NULL;
                        } else if (strcasecmp(field_key, "volume") == 0) {
                            entry->volume = field_value;
                            field_value = NULL;
                        } else if (strcasecmp(field_key, "pages") == 0) {
                            entry->page = field_value;
                            field_value = NULL;
                        }

                        if (field_value) free(field_value);
                    }
                    free(field_key);

                    /* Advance past this field */
                    if (field_end) {
                        field_start = field_end;
                        /* Skip comma if present */
                        while (*field_start && (*field_start == ',' || isspace(*field_start))) {
                            field_start++;
                        }
                    } else {
                        /* Fallback: find next comma or closing brace */
                        field_start = strchr(field_start, ',');
                        if (field_start) {
                            field_start++;
                        } else {
                            field_start = strchr(field_start ? field_start : content, '}');
                            break;  /* End of entry */
                        }
                    }
                } else {
                    break;  /* Can't parse more fields */
                }
            }

            /* Add to registry */
            entry->next = registry->entries;
            registry->entries = entry;
            registry->count++;
        }

        free((void*)type_str);

        /* Find next entry */
        p = strchr(p, '}');
        if (p) p++;
    }

    return registry;
}

/**
 * Parse CSL JSON file (basic implementation)
 * For now, just extract id, type, title, author, year
 */
apex_bibliography_registry *apex_parse_csl_json(const char *content) {
    if (!content) return NULL;

    /* Very basic JSON parsing - just look for "id" fields */
    /* For a full implementation, we'd use a JSON library like cJSON */
    /* This is a minimal implementation for MVP */

    apex_bibliography_registry *registry = malloc(sizeof(apex_bibliography_registry));
    if (!registry) return NULL;
    registry->entries = NULL;
    registry->count = 0;

    /* TODO: Implement proper JSON parsing */
    /* For now, return empty registry - will be enhanced later */

    return registry;
}

/**
 * Get indentation level (number of leading spaces)
 */
static int get_indent_level(const char *line) {
    int indent = 0;
    while (*line == ' ') {
        indent++;
        line++;
    }
    return indent;
}

/**
 * Extract YAML value (handles quoted and unquoted strings)
 */
static char *extract_yaml_value(const char *value_str) {
    if (!value_str) return NULL;

    /* Skip leading whitespace */
    while (*value_str && isspace(*value_str)) value_str++;
    if (!*value_str) return NULL;

    /* Check for quotes */
    char quote_char = 0;
    if (*value_str == '"' || *value_str == '\'') {
        quote_char = *value_str;
        value_str++;
    }

    const char *value_start = value_str;
    const char *value_end = value_str;

    if (quote_char) {
        /* Find closing quote */
        while (*value_end && *value_end != quote_char) {
            if (*value_end == '\\' && value_end[1]) value_end++;  /* Skip escaped chars */
            value_end++;
        }
    } else {
        /* Find end of value (newline or comment) */
        while (*value_end && *value_end != '\n' && *value_end != '\r' && *value_end != '#') {
            value_end++;
        }
        /* Trim trailing whitespace */
        while (value_end > value_start && isspace(value_end[-1])) value_end--;
    }

    size_t value_len = value_end - value_start;
    if (value_len == 0) return NULL;

    char *value = malloc(value_len + 1);
    if (!value) return NULL;
    memcpy(value, value_start, value_len);
    value[value_len] = '\0';

    return trim_string(value);
}

/**
 * Parse author from YAML structure
 * Handles: author: {family: Doe, given: John} or author: [{family: Doe, given: John}]
 * Also handles multi-line format:
 *   author:
 *     - family: Doe
 *       given: John
 */
static char *parse_yaml_author(const char *content, int base_indent) {
    if (!content) return NULL;

    const char *p = content;
    char *family = NULL;
    char *given = NULL;

    /* Skip whitespace and opening brace/bracket */
    while (*p && (isspace(*p) || *p == '{' || *p == '[')) p++;

    /* Look for family: and given: fields */
    while (*p) {
        /* Check for list item marker */
        if (*p == '-' && isspace(p[1])) {
            p++;
            while (*p && isspace(*p)) p++;
        }

        if (strncmp(p, "family:", 7) == 0) {
            p += 7;
            while (*p && isspace(*p)) p++;
            family = extract_yaml_value(p);
            if (family) {
                /* Find end of this line */
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
            }
        } else if (strncmp(p, "given:", 6) == 0) {
            p += 6;
            while (*p && isspace(*p)) p++;
            given = extract_yaml_value(p);
            if (given) {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
            }
        } else if (*p == '\n') {
            /* Check next line for continuation */
            p++;
            int next_indent = get_indent_level(p);
            if (next_indent <= base_indent) {
                break;  /* End of author block */
            }
            /* Continue parsing on next line */
        } else if (*p == '}' || *p == ']') {
            break;  /* End of structure */
        } else {
            p++;
        }
    }

    /* Format author name */
    if (family || given) {
        size_t len = (family ? strlen(family) : 0) + (given ? strlen(given) : 0) + 3;
        char *author = malloc(len);
        if (author) {
            if (family && given) {
                snprintf(author, len, "%s, %s", family, given);
            } else if (family) {
                strcpy(author, family);
            } else {
                strcpy(author, given);
            }
        }
        free(family);
        free(given);
        return author;
    }

    free(family);
    free(given);
    return NULL;
}

/**
 * Parse date from YAML structure
 * Handles: issued: {date-parts: [[1999]]} or year: 1999
 * Also handles multi-line format:
 *   issued:
 *     date-parts:
 *       - - 1999
 */
static char *parse_yaml_date(const char *content, int base_indent) {
    if (!content) return NULL;

    const char *p = content;
    bool in_date_parts = false;
    int list_depth = 0;

    /* Look for date-parts: */
    while (*p) {
        if (strncmp(p, "date-parts:", 11) == 0) {
            in_date_parts = true;
            p += 11;
            while (*p && isspace(*p) && *p != '\n') p++;
            if (*p == '\n') {
                p++;
                /* Multi-line format, continue on next line */
            }
        } else if (in_date_parts) {
            if (*p == '[') {
                list_depth++;
                p++;
            } else if (*p == ']') {
                list_depth--;
                p++;
                if (list_depth == 0) break;
            } else if (*p == '-' && isspace(p[1]) && list_depth >= 1) {
                /* List item in date-parts */
                p++;
                while (*p && isspace(*p)) p++;
                if (*p == '-') {
                    /* Nested list item (year) */
                    p++;
                    while (*p && isspace(*p)) p++;
                    const char *year_start = p;
                    while (*p && isdigit(*p)) p++;
                    size_t year_len = p - year_start;
                    if (year_len > 0) {
                        char *year = malloc(year_len + 1);
                        if (year) {
                            memcpy(year, year_start, year_len);
                            year[year_len] = '\0';
                            return year;
                        }
                    }
                }
            } else if (*p == '\n') {
                p++;
                int next_indent = get_indent_level(p);
                if (next_indent <= base_indent && list_depth == 0) {
                    break;  /* End of date block */
                }
            } else {
                p++;
            }
        } else if (strncmp(p, "year:", 5) == 0) {
            /* Simple year field */
            p += 5;
            while (*p && isspace(*p)) p++;
            const char *year_start = p;
            while (*p && isdigit(*p)) p++;
            size_t year_len = p - year_start;
            if (year_len > 0) {
                char *year = malloc(year_len + 1);
                if (year) {
                    memcpy(year, year_start, year_len);
                    year[year_len] = '\0';
                    return year;
                }
            }
            break;
        } else {
            p++;
        }
    }

    return NULL;
}

/**
 * Parse CSL YAML file
 */
apex_bibliography_registry *apex_parse_csl_yaml(const char *content) {
    if (!content) return NULL;

    apex_bibliography_registry *registry = malloc(sizeof(apex_bibliography_registry));
    if (!registry) return NULL;
    registry->entries = NULL;
    registry->count = 0;

    const char *p = content;
    apex_bibliography_entry *current_entry = NULL;
    bool in_entry = false;

    /* Parse line by line */
    while (*p) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        size_t line_len = line_end - line_start;
        char line[1024];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        char *trimmed = trim_string(line);
        if (!trimmed || *trimmed == '\0' || *trimmed == '#') {
            p = line_end + 1;
            continue;
        }

        int indent = get_indent_level(line);

        /* Check for list item start (new entry) */
        if (trimmed[0] == '-' && indent == 0) {
            /* Start new entry */
            if (current_entry && current_entry->id) {
                /* Add previous entry to registry */
                current_entry->next = registry->entries;
                registry->entries = current_entry;
                registry->count++;
            }
            current_entry = NULL;
            in_entry = true;
            p = line_end + 1;
            continue;
        }

        if (!in_entry) {
            p = line_end + 1;
            continue;
        }

        /* Parse key: value pairs */
        char *colon = strchr(trimmed, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim_string(trimmed);
            char *value_str = trim_string(colon + 1);

            if (!current_entry && strcmp(key, "id") == 0) {
                /* Create new entry with ID */
                char *id_value = extract_yaml_value(value_str);
                if (id_value) {
                    current_entry = bibliography_entry_new(id_value);
                    free(id_value);
                }
            } else if (current_entry) {
                /* Set entry fields */
                if (strcmp(key, "type") == 0) {
                    char *type_value = extract_yaml_value(value_str);
                    if (type_value) {
                        current_entry->type = type_value;
                    }
                } else if (strcmp(key, "title") == 0) {
                    char *title_value = extract_yaml_value(value_str);
                    if (title_value) {
                        current_entry->title = title_value;
                    }
                } else if (strcmp(key, "author") == 0) {
                    /* Parse author structure - may be multi-line */
                    const char *author_start = value_str;
                    if (*author_start == '\0' && line_end[1]) {
                        /* Multi-line author, look at next line */
                        const char *next_line = line_end + 1;
                        if (*next_line == '-' || (*next_line == ' ' && next_line[1] == ' ')) {
                            author_start = next_line;
                        }
                    }
                    char *author = parse_yaml_author(author_start, indent);
                    if (author) {
                        current_entry->author = author;
                    }
                } else if (strcmp(key, "issued") == 0 || strcmp(key, "year") == 0) {
                    /* Parse date - may be multi-line */
                    const char *date_start = value_str;
                    if (*date_start == '\0' && line_end[1]) {
                        const char *next_line = line_end + 1;
                        if (*next_line == ' ' || *next_line == '-') {
                            date_start = next_line;
                        }
                    }
                    char *year = parse_yaml_date(date_start, indent);
                    if (year) {
                        current_entry->year = year;
                    }
                } else if (strcmp(key, "container-title") == 0) {
                    char *container_value = extract_yaml_value(value_str);
                    if (container_value) {
                        current_entry->container_title = container_value;
                    }
                } else if (strcmp(key, "publisher") == 0) {
                    char *publisher_value = extract_yaml_value(value_str);
                    if (publisher_value) {
                        current_entry->publisher = publisher_value;
                    }
                } else if (strcmp(key, "volume") == 0) {
                    char *volume_value = extract_yaml_value(value_str);
                    if (volume_value) {
                        current_entry->volume = volume_value;
                    }
                } else if (strcmp(key, "page") == 0) {
                    char *page_value = extract_yaml_value(value_str);
                    if (page_value) {
                        current_entry->page = page_value;
                    }
                }
            }
        }

        p = line_end + 1;
    }

    /* Add last entry if exists */
    if (current_entry && current_entry->id) {
        current_entry->next = registry->entries;
        registry->entries = current_entry;
        registry->count++;
    }

    return registry;
}

/**
 * Load bibliography from a single file
 */
apex_bibliography_registry *apex_load_bibliography_file(const char *filepath) {
    if (!filepath) return NULL;

    char *content = read_bibliography_file(filepath);
    if (!content) return NULL;

    bibliography_format_t format = detect_bibliography_format(filepath);
    apex_bibliography_registry *registry = NULL;

    switch (format) {
        case BIB_FORMAT_BIBTEX:
            registry = apex_parse_bibtex(content);
            break;
        case BIB_FORMAT_CSL_JSON:
            registry = apex_parse_csl_json(content);
            break;
        case BIB_FORMAT_CSL_YAML:
            registry = apex_parse_csl_yaml(content);
            break;
        default:
            /* Try to auto-detect from content */
            if (strstr(content, "@") && strstr(content, "{")) {
                registry = apex_parse_bibtex(content);
            } else if (strstr(content, "[") && strstr(content, "\"id\"")) {
                registry = apex_parse_csl_json(content);
            }
            break;
    }

    free(content);
    return registry;
}

/**
 * Load bibliography from multiple files
 */
apex_bibliography_registry *apex_load_bibliography(const char **files, const char *base_directory) {
    if (!files) return NULL;

    apex_bibliography_registry *merged_registry = malloc(sizeof(apex_bibliography_registry));
    if (!merged_registry) return NULL;
    merged_registry->entries = NULL;
    merged_registry->count = 0;

    /* Load each file and merge entries */
    for (int i = 0; files[i] != NULL; i++) {
        char *resolved_path = resolve_bibliography_path(files[i], base_directory);
        if (!resolved_path) continue;

        apex_bibliography_registry *file_registry = apex_load_bibliography_file(resolved_path);
        free(resolved_path);

        if (file_registry) {
            /* Merge entries into merged_registry */
            apex_bibliography_entry *entry = file_registry->entries;
            while (entry) {
                apex_bibliography_entry *next = entry->next;

                /* Check if entry with same ID already exists */
                apex_bibliography_entry *existing = apex_find_bibliography_entry(merged_registry, entry->id);
                if (!existing) {
                    /* Add to merged registry */
                    entry->next = merged_registry->entries;
                    merged_registry->entries = entry;
                    merged_registry->count++;
                } else {
                    /* Entry already exists, skip (or could update) */
                    apex_bibliography_entry_free(entry);
                }

                entry = next;
            }

            /* Free the file registry structure (entries were moved) */
            free(file_registry);
        }
    }

    return merged_registry;
}
