#include "apex/apex.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

/* cmark-gfm headers */
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "cmark-gfm-core-extensions.h"

/* Apex extensions */
#include "extensions/metadata.h"
#include "extensions/wiki_links.h"
#include "extensions/math.h"
#include "extensions/critic.h"
#include "extensions/callouts.h"
#include "extensions/raw_content.h"
#include "extensions/code_fence_attrs.h"
#include "extensions/quarto_diagrams.h"
#include "extensions/quarto_shortcodes.h"
#include "extensions/quarto_polish.h"
#include "extensions/quarto_lists.h"
#include "extensions/includes.h"
#include "extensions/toc.h"
#include "extensions/abbreviations.h"
#include "extensions/emoji.h"
#include "extensions/special_markers.h"
#include "extensions/inline_tables.h"
#include "extensions/ial.h"
#include "extensions/definition_list.h"
#include "extensions/advanced_footnotes.h"
#include "extensions/advanced_tables.h"
#include "extensions/html_markdown.h"
#include "extensions/inline_footnotes.h"
#include "extensions/highlight.h"
#include "extensions/insert.h"
#include "extensions/sup_sub.h"
#include "extensions/header_ids.h"
#include "extensions/relaxed_tables.h"
#include "extensions/grid_tables.h"
#include "extensions/citations.h"
#include "extensions/index.h"
#include "extensions/fenced_divs.h"
#include "extensions/syntax_highlight.h"
#include "plugins.h"
#include "ast_json.h"
#include "apex/ast_markdown.h"
#include "apex/ast_terminal.h"
#include "apex/ast_man.h"
#include "apex/ast_rtf.h"
#include "filters_ast.h"

/* Custom renderer */
#include "html_renderer.h"

/**
 * Encode a string as hexadecimal HTML entities (&#xNN;)
 * Caller must free the returned buffer.
 */
static char *apex_encode_hex_entities(const char *text, size_t len) {
    if (!text || len == 0) return NULL;
    /* Each char becomes &#xNN; => up to 6 chars */
    size_t cap = len * 6 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    char *w = out;
    for (size_t i = 0; i < len; i++) {
        int written = snprintf(w, cap - (size_t)(w - out), "&#x%02X;", (unsigned char)text[i]);
        if (written <= 0 || (size_t)written >= cap - (size_t)(w - out)) {
            free(out);
            return NULL;
        }
        w += written;
    }
    *w = '\0';
    return out;
}

/**
 * Escape string for safe HTML attribute usage.
 */
static char *apex_escape_html_attr(const char *input) {
    if (!input) return strdup("");

    size_t len = strlen(input);
    size_t max_len = len * 6 + 1; /* Worst case for &quot; */
    char *out = malloc(max_len);
    if (!out) return NULL;

    char *w = out;
    for (const char *p = input; *p; p++) {
        switch (*p) {
            case '&':
                memcpy(w, "&amp;", 5);
                w += 5;
                break;
            case '<':
                memcpy(w, "&lt;", 4);
                w += 4;
                break;
            case '>':
                memcpy(w, "&gt;", 4);
                w += 4;
                break;
            case '"':
                memcpy(w, "&quot;", 6);
                w += 6;
                break;
            case '\'':
                memcpy(w, "&#39;", 5);
                w += 5;
                break;
            default:
                *w++ = *p;
                break;
        }
    }
    *w = '\0';
    return out;
}

/**
 * Normalize metadata key by removing spaces and lowercasing.
 */
static char *apex_normalize_meta_key(const char *key) {
    if (!key) return NULL;
    size_t len = strlen(key);
    char *normalized = malloc(len + 1);
    if (!normalized) return NULL;

    char *out = normalized;
    for (const char *in = key; *in; in++) {
        if (!isspace((unsigned char)*in)) {
            *out++ = (char)tolower((unsigned char)*in);
        }
    }
    *out = '\0';
    return normalized;
}

/**
 * Keys handled elsewhere (title/lang/css/html header/footer/etc.) should not
 * be emitted as generic <meta name="..."> tags.
 */
static bool apex_skip_generic_meta_key(const char *key) {
    char *normalized = apex_normalize_meta_key(key);
    if (!normalized) return false;

    static const char *skip_keys[] = {
        "title",
        "css",
        "language",
        "htmlheader",
        "htmlfooter",
        "htmlheaderlevel",
        "baseheaderlevel",
        "quoteslanguage",
        NULL
    };

    bool skip = false;
    for (int i = 0; skip_keys[i]; i++) {
        if (strcmp(normalized, skip_keys[i]) == 0) {
            skip = true;
            break;
        }
    }
    free(normalized);
    return skip;
}

/**
 * Render metadata list to newline-separated generic HTML meta tags.
 */
static char *apex_render_generic_meta_tags(apex_metadata_item *metadata) {
    if (!metadata) return NULL;

    size_t capacity = 256;
    size_t used = 0;
    char *out = malloc(capacity);
    if (!out) return NULL;
    out[0] = '\0';

    /* Metadata entries are prepended during parsing; reverse iteration restores
     * source declaration order in generated head tags. */
    size_t item_count = 0;
    for (apex_metadata_item *it = metadata; it; it = it->next) item_count++;
    if (item_count == 0) {
        free(out);
        return NULL;
    }
    apex_metadata_item **items = malloc(item_count * sizeof(apex_metadata_item *));
    if (!items) {
        free(out);
        return NULL;
    }
    size_t item_index = 0;
    for (apex_metadata_item *it = metadata; it; it = it->next) {
        items[item_index++] = it;
    }

    for (size_t i = item_count; i > 0; i--) {
        apex_metadata_item *item = items[i - 1];
        if (!item->key || !item->value || apex_skip_generic_meta_key(item->key)) {
            continue;
        }

        char *escaped_key = apex_escape_html_attr(item->key);
        char *escaped_value = apex_escape_html_attr(item->value);
        if (!escaped_key || !escaped_value) {
            if (escaped_key) free(escaped_key);
            if (escaped_value) free(escaped_value);
            free(items);
            free(out);
            return NULL;
        }

        size_t needed = strlen(escaped_key) + strlen(escaped_value) + 36;
        if (used + needed + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            while (used + needed + 1 > new_capacity) {
                new_capacity *= 2;
            }
            char *new_out = realloc(out, new_capacity);
            if (!new_out) {
                free(escaped_key);
                free(escaped_value);
                free(items);
                free(out);
                return NULL;
            }
            out = new_out;
            capacity = new_capacity;
        }

        int written = snprintf(out + used, capacity - used,
                               "  <meta name=\"%s\" content=\"%s\"/>\n",
                               escaped_key, escaped_value);
        free(escaped_key);
        free(escaped_value);
        if (written < 0) {
            free(items);
            free(out);
            return NULL;
        }
        used += (size_t)written;
    }
    free(items);

    if (used == 0) {
        free(out);
        return NULL;
    }

    return out;
}

/**
 * Base64 encode binary data
 * Caller must free the returned buffer.
 */
static char *apex_base64_encode(const unsigned char *data, size_t len) {
    if (!data || len == 0) return NULL;

    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    /* Base64 encoding increases size by ~33% (4 chars per 3 bytes) */
    size_t encoded_len = ((len + 2) / 3) * 4;
    char *encoded = malloc(encoded_len + 1);
    if (!encoded) return NULL;

    char *out = encoded;
    size_t i = 0;

    while (i < len) {
        unsigned char byte1 = data[i++];
        unsigned char byte2 = (i < len) ? data[i++] : 0;
        unsigned char byte3 = (i < len) ? data[i++] : 0;

        unsigned int combined = (byte1 << 16) | (byte2 << 8) | byte3;

        *out++ = base64_chars[(combined >> 18) & 0x3F];
        *out++ = base64_chars[(combined >> 12) & 0x3F];

        /* Check if we have at least 2 bytes (byte1 and byte2) */
        if (i >= 2) {
            *out++ = base64_chars[(combined >> 6) & 0x3F];
        } else {
            *out++ = '=';
        }

        /* Check if we have all 3 bytes */
        if (i >= 3) {
            *out++ = base64_chars[combined & 0x3F];
        } else {
            *out++ = '=';
        }
    }

    *out = '\0';
    return encoded;
}

/**
 * Obfuscate mailto: links in rendered HTML by converting href/text
 * characters to hexadecimal HTML entities.
 */
static char *apex_obfuscate_email_links(const char *html) {
    if (!html) return NULL;

    size_t cap = strlen(html) * 6 + 1; /* generous for expansion */
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *p = html;
    char *w = out;
    bool in_mailto = false;

    while (*p) {
        /* Obfuscate href="mailto:... */
        if (!in_mailto && strncmp(p, "href=\"mailto:", 13) == 0) {
            const char *addr_start = p + 6; /* keep mailto: prefix in output */
            const char *addr_end = strchr(addr_start, '"');
            if (!addr_end) {
                *w++ = *p++;
                continue;
            }

            char *encoded = apex_encode_hex_entities(addr_start, (size_t)(addr_end - addr_start));
            if (encoded) {
                size_t needed = 6 + strlen(encoded) + 1; /* href=" + encoded + closing quote */
                size_t used = (size_t)(w - out);
                if (used + needed >= cap) {
                    cap = (used + needed + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) {
                        free(out);
                        free(encoded);
                        return NULL;
                    }
                    out = new_out;
                    w = out + used;
                }
                memcpy(w, "href=\"", 6); w += 6;
                memcpy(w, encoded, strlen(encoded)); w += strlen(encoded);
                *w++ = '"';
                free(encoded);
                p = addr_end + 1;
                in_mailto = true;
                continue;
            }
        }

        /* Encode visible link text for mailto links */
        if (in_mailto && *p == '>') {
            *w++ = *p++;
            const char *text_start = p;
            while (*p && *p != '<') p++;
            char *encoded_text = apex_encode_hex_entities(text_start, (size_t)(p - text_start));
            if (encoded_text) {
                size_t needed = strlen(encoded_text);
                size_t used = (size_t)(w - out);
                if (used + needed >= cap) {
                    cap = (used + needed + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) {
                        free(out);
                        free(encoded_text);
                        return NULL;
                    }
                    out = new_out;
                    w = out + used;
                }
                memcpy(w, encoded_text, needed);
                w += needed;
                free(encoded_text);
                continue;
            }
        }

        /* Detect end of link */
        if (in_mailto && strncmp(p, "</a", 3) == 0) {
            in_mailto = false;
        }

        *w++ = *p++;
    }

    *w = '\0';
    return out;
}

/**
 * Detect MIME type from file extension
 * Handles URLs with query parameters by stopping at ? or #
 */
static const char *apex_detect_mime_type(const char *filepath) {
    if (!filepath) return "image/png";  /* Default */

    const char *ext = strrchr(filepath, '.');
    if (!ext) return "image/png";  /* Default */
    ext++;

    /* Find the end of the extension (stop at ? or # for URLs) */
    const char *ext_end = ext;
    while (*ext_end && *ext_end != '?' && *ext_end != '#' && *ext_end != '/' && *ext_end != ' ') {
        ext_end++;
    }
    size_t ext_len = ext_end - ext;

    /* Case-insensitive comparison */
    if (ext_len == 3 && (strncasecmp(ext, "jpg", 3) == 0 || strncasecmp(ext, "jpeg", 3) == 0)) {
        return "image/jpeg";
    } else if (ext_len == 4 && strncasecmp(ext, "jpeg", 4) == 0) {
        return "image/jpeg";
    } else if (ext_len == 3 && strncasecmp(ext, "png", 3) == 0) {
        return "image/png";
    } else if (ext_len == 3 && strncasecmp(ext, "gif", 3) == 0) {
        return "image/gif";
    } else if (ext_len == 4 && strncasecmp(ext, "webp", 4) == 0) {
        return "image/webp";
    } else if (ext_len == 3 && strncasecmp(ext, "svg", 3) == 0) {
        return "image/svg+xml";
    } else if (ext_len == 3 && strncasecmp(ext, "bmp", 3) == 0) {
        return "image/bmp";
    } else if (ext_len == 3 && strncasecmp(ext, "ico", 3) == 0) {
        return "image/x-icon";
    }

    return "image/png";  /* Default */
}

/**
 * Resolve relative path from base directory
 */
char *apex_resolve_local_image_path(const char *filepath, const char *base_dir) {
    if (!filepath) return NULL;

    /* If absolute path, return as-is */
    if (filepath[0] == '/') {
        return strdup(filepath);
    }

    /* Relative path - combine with base_dir */
    if (!base_dir || !*base_dir) {
        return strdup(filepath);
    }

    size_t len = strlen(base_dir) + strlen(filepath) + 2;
    char *resolved = malloc(len);
    if (!resolved) return NULL;

    snprintf(resolved, len, "%s/%s", base_dir, filepath);
    return resolved;
}

/**
 * Read local image file and encode as base64
 */
static char *apex_read_and_encode_image(const char *filepath) {
    if (!filepath) return NULL;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0 || size > 10 * 1024 * 1024) {  /* Limit to 10MB */
        fclose(fp);
        return NULL;
    }

    /* Read content */
    unsigned char *content = malloc(size);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    fclose(fp);

    if (read != (size_t)size) {
        free(content);
        return NULL;
    }

    /* Encode to base64 */
    char *encoded = apex_base64_encode(content, size);
    free(content);
    return encoded;
}

/**
 * Embed images as base64 data URLs in HTML
 * Only supports local images - remote images are not embedded
 */
static char *apex_embed_images(const char *html, const apex_options *options, const char *base_directory) {
    if (!html || !options->embed_images) {
        return html ? strdup(html) : NULL;
    }

    size_t html_len = strlen(html);
    /* Allocate generous buffer - base64 encoding increases size by ~33% */
    size_t cap = html_len * 3 + 1024;
    char *output = malloc(cap);
    if (!output) return strdup(html);

    const char *read = html;
    char *write = output;
    size_t remaining = cap;

    while (*read) {
        /* Look for <img tag */
        if (*read == '<' && strncmp(read, "<img", 4) == 0) {
            const char *img_start = read;
            const char *img_end = strchr(img_start, '>');
            if (!img_end) {
                /* Malformed tag, copy as-is */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Check if already a data URL */
            const char *src_attr = strstr(img_start, "src=\"");
            if (!src_attr || src_attr > img_end) {
                src_attr = strstr(img_start, "src='");
            }

            if (src_attr && src_attr < img_end) {
                const char quote_char = (src_attr[4] == '"') ? '"' : '\'';
                const char *url_start = src_attr + 5;
                const char *url_end = strchr(url_start, quote_char);
                if (url_end && url_end < img_end) {
                    size_t url_len = url_end - url_start;
                    char *url = malloc(url_len + 1);
                    if (url) {
                        memcpy(url, url_start, url_len);
                        url[url_len] = '\0';

                        /* Check if already a data URL */
                        bool is_data_url = (strncmp(url, "data:", 5) == 0);
                        bool is_remote = (strncmp(url, "http://", 7) == 0 ||
                                        strncmp(url, "https://", 8) == 0 ||
                                        strncmp(url, "//", 2) == 0);

                        char *data_url = NULL;
                        const char *mime_type = NULL;

                        if (!is_data_url && !is_remote && options->embed_images) {
                            /* Local image */
                            char *resolved_path = apex_resolve_local_image_path(url, base_directory);
                            if (resolved_path) {
                                struct stat st;
                                if (stat(resolved_path, &st) == 0 && S_ISREG(st.st_mode)) {
                                    char *encoded = apex_read_and_encode_image(resolved_path);
                                    if (encoded) {
                                        mime_type = apex_detect_mime_type(resolved_path);
                                        size_t data_url_len = strlen("data:") + strlen(mime_type) + strlen(";base64,") + strlen(encoded) + 1;
                                        data_url = malloc(data_url_len);
                                        if (data_url) {
                                            snprintf(data_url, data_url_len, "data:%s;base64,%s", mime_type, encoded);
                                        }
                                        free(encoded);
                                    }
                                }
                                free(resolved_path);
                            }
                        }

                        if (data_url) {
                            /* Replace the src attribute */
                            size_t before_src = src_attr - img_start;
                            size_t after_url = img_end - url_end;

                            /* Calculate new size needed */
                            size_t new_len = before_src + strlen("src=\"") + strlen(data_url) + 1 + after_url + 1;
                            if (new_len > remaining) {
                                size_t written = write - output;
                                cap = (written + new_len + 1) * 2;
                                char *new_output = realloc(output, cap);
                                if (!new_output) {
                                    free(data_url);
                                    free(url);
                                    free(output);
                                    return strdup(html);
                                }
                                output = new_output;
                                write = output + written;
                                remaining = cap - written;
                            }

                            /* Copy up to src= */
                            memcpy(write, img_start, before_src);
                            write += before_src;
                            remaining -= before_src;

                            /* Write new src attribute */
                            memcpy(write, "src=\"", 5);
                            write += 5;
                            remaining -= 5;
                            size_t data_url_len = strlen(data_url);
                            memcpy(write, data_url, data_url_len);
                            write += data_url_len;
                            remaining -= data_url_len;
                            *write++ = '"';
                            remaining--;

                            /* Copy rest of tag */
                            memcpy(write, url_end + 1, after_url);
                            write += after_url;
                            remaining -= after_url;

                            read = img_end + 1;
                            free(data_url);
                            free(url);
                            continue;
                        }

                        /* No data URL created, clean up */
                        free(url);
                    }
                }
            }

            /* No replacement, copy tag as-is */
            size_t tag_len = img_end - img_start + 1;
            if (tag_len > remaining) {
                size_t written = write - output;
                cap = (written + tag_len + 1) * 2;
                char *new_output = realloc(output, cap);
                if (!new_output) {
                    free(output);
                    return strdup(html);
                }
                output = new_output;
                write = output + written;
                remaining = cap - written;
            }
            memcpy(write, img_start, tag_len);
            write += tag_len;
            remaining -= tag_len;
            read = img_end + 1;
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
 * Track HTML tag / quoted-attribute state while scanning source text.
 * State reflects characters already consumed, matching the previous
 * "rescan from document start" semantics without the O(n^2) cost.
 */
static void apex_autolink_html_consume(char c, bool *in_html_tag,
                                       bool *html_quote_active,
                                       char *html_quote_char) {
    if (!*in_html_tag) {
        if (c == '<') {
            *in_html_tag = true;
            *html_quote_active = false;
            *html_quote_char = '\0';
        }
        return;
    }

    if (*html_quote_active) {
        if (c == *html_quote_char) {
            *html_quote_active = false;
            *html_quote_char = '\0';
        }
        return;
    }

    if (c == '"' || c == '\'') {
        *html_quote_active = true;
        *html_quote_char = c;
    } else if (c == '>') {
        *in_html_tag = false;
    }
}

static void apex_autolink_html_consume_range(const char *s, size_t n,
                                            bool *in_html_tag,
                                            bool *html_quote_active,
                                            char *html_quote_char) {
    for (size_t i = 0; i < n; i++) {
        apex_autolink_html_consume(s[i], in_html_tag, html_quote_active,
                                   html_quote_char);
    }
}

/**
 * Preprocess angle-bracket autolinks (<http://...>) into explicit links
 * and convert bare URLs/emails to explicit links so they survive
 * custom rendering paths.
 * Skips processing inside code spans (`...`) and code blocks (```...```).
 */
static char *apex_preprocess_autolinks(const char *text, const apex_options *options) {
    if (!text || !options || !options->enable_autolink) return NULL;

    size_t len = strlen(text);
    /* Worst case: every character becomes part of a [url](url) */
    size_t cap = len * 4 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *r = text;
    char *w = out;
    bool in_code_block = false;
    bool in_inline_code = false;
    int code_block_backticks = 0;  /* Count of consecutive backticks for code blocks */
    bool in_liquid = false;
    bool in_markdown_link_url = false;  /* Track if we're inside [text](url) URL part */
    int bracket_count = 0;  /* Count unmatched [ for markdown links */
    int paren_count = 0;  /* Count unmatched ( inside markdown link URLs */
    bool in_html_tag = false;
    bool html_quote_active = false;
    char html_quote_char = '\0';

    while (*r) {
        const char *loop_start = r;

        /* Handle Liquid tags: copy {% ... %} without processing */
        if (!in_liquid && *r == '{' && r[1] == '%') {
            in_liquid = true;
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            continue;
        }
        if (in_liquid) {
            if (*r == '%' && r[1] == '}') {
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++;
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++;
                in_liquid = false;
            } else {
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++;
            }
            continue;
        }

        /* At start of line: handle reference definitions and indented code blocks */
        if (r == text || r[-1] == '\n') {
            const char *line_start = r;

            /* First: skip indented code blocks (4+ spaces or a leading tab) entirely */
            int indent_spaces = 0;
            while (*line_start == ' ' && indent_spaces < 4) {
                line_start++;
                indent_spaces++;
            }
            if (indent_spaces == 4 || *line_start == '\t') {
                const char *line_end = strchr(r, '\n');
                if (!line_end) line_end = r + strlen(r);
                size_t line_len = line_end - r;
                if ((size_t)(w - out) + line_len + 1 > cap) {
                    size_t used = (size_t)(w - out);
                    cap = (used + line_len + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) { free(out); return NULL; }
                    out = new_out;
                    w = out + used;
                }
                memcpy(w, r, line_len);
                w += line_len;
                apex_autolink_html_consume_range(r, line_len, &in_html_tag,
                                                &html_quote_active,
                                                &html_quote_char);
                r = line_end;
                continue;
            }

            /* Then: check for reference link definitions: [id]: URL */
            line_start = r;
            /* Skip leading whitespace and blockquote markers */
            while (*line_start == ' ' || *line_start == '\t' || *line_start == '>') {
                line_start++;
            }
            /* Check for [id]: pattern */
            if (*line_start == '[') {
                const char *id_end = strchr(line_start + 1, ']');
                if (id_end && id_end[1] == ':') {
                    /* This is a reference link definition - skip autolinking for this line */
                    const char *line_end = strchr(r, '\n');
                    if (!line_end) line_end = r + strlen(r);
                    /* Copy the entire line without processing */
                    size_t line_len = line_end - r;
                    if ((size_t)(w - out) + line_len + 1 > cap) {
                        size_t used = (size_t)(w - out);
                        cap = (used + line_len + 1) * 2;
                        char *new_out = realloc(out, cap);
                        if (!new_out) { free(out); return NULL; }
                        out = new_out;
                        w = out + used;
                    }
                    memcpy(w, r, line_len);
                    w += line_len;
                    apex_autolink_html_consume_range(r, line_len, &in_html_tag,
                                                    &html_quote_active,
                                                    &html_quote_char);
                    r = line_end;
                    continue;
                }
            }
        }
        /* Track code blocks (```...```) */
        if (*r == '`') {
            int backtick_count = 1;
            const char *p = r + 1;
            while (*p == '`' && backtick_count < 10) {
                backtick_count++;
                p++;
            }

            if (backtick_count >= 3) {
                /* Code block marker */
                if (!in_code_block) {
                    in_code_block = true;
                    code_block_backticks = backtick_count;
                } else if (backtick_count >= code_block_backticks) {
                    /* End of code block */
                    in_code_block = false;
                    code_block_backticks = 0;
                }
                /* Copy the backticks */
                for (int i = 0; i < backtick_count; i++) {
                    apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                               &html_quote_char);
                    *w++ = *r++;
                }
                continue;
            } else if (backtick_count == 1) {
                /* Inline code span - toggle state and copy the backtick */
                in_inline_code = !in_inline_code;
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++;
                continue;
            } else {
                /* Multiple backticks but less than 3 - copy them as-is (shouldn't happen in valid markdown) */
                for (int i = 0; i < backtick_count; i++) {
                    apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                               &html_quote_char);
                    *w++ = *r++;
                }
                continue;
            }
        }

        /* Skip processing inside code blocks or inline code */
        if (in_code_block || in_inline_code) {
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            continue;
        }

        /* Skip any autolink processing while inside an HTML tag (<...>),
         * including quoted attributes. */
        if (in_html_tag) {
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            continue;
        }

        /* Track markdown links: [text](url) - skip autolinking inside URL part */
        if (*r == '[' && !in_markdown_link_url) {
            bracket_count++;
        } else if (*r == ']' && bracket_count > 0) {
            /* Check if next character is '(' - if so, we're entering a link URL */
            if (r[1] == '(') {
                in_markdown_link_url = true;
                paren_count = 1;  /* Count the opening '(' */
                /* Copy ']' and '(' */
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++; /* ']' */
                apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                           &html_quote_char);
                *w++ = *r++; /* '(' */
                continue;
            }
            bracket_count--;
            if (bracket_count < 0) bracket_count = 0;
        } else if (*r == '(' && in_markdown_link_url) {
            /* Nested '(' inside URL - track it */
            paren_count++;
        } else if (*r == ')' && in_markdown_link_url) {
            /* End of markdown link URL (or nested level) */
            paren_count--;
            if (paren_count == 0) {
                in_markdown_link_url = false;
            }
        }

        /* Skip autolinking while inside markdown link text [ ... ] */
        if (bracket_count > 0 && !in_markdown_link_url) {
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            continue;
        }

        /* Skip autolinking if we're inside a markdown link URL */
        if (in_markdown_link_url) {
            apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                       &html_quote_char);
            *w++ = *r++;
            continue;
        }

        /* Handle angle-bracket autolink */
        if (*r == '<') {
            const char *start = r + 1;
            const char *end = strchr(start, '>');
            if (end && start != end) {
                size_t url_len = (size_t)(end - start);
                if ((url_len > 6 && strncmp(start, "http://", 7) == 0) ||
                    (url_len > 7 && strncmp(start, "https://", 8) == 0) ||
                    (url_len > 7 && strncmp(start, "mailto:", 7) == 0)) {
                    size_t needed = 2 + url_len + 3 + url_len + 2; /* [url](url) */
                    if ((size_t)(w - out) + needed + 1 > cap) {
                        size_t used = (size_t)(w - out);
                        cap = (used + needed + 1) * 2;
                        char *new_out = realloc(out, cap);
                        if (!new_out) { free(out); return NULL; }
                        out = new_out;
                        w = out + used;
                    }
                    *w++ = '[';
                    memcpy(w, start, url_len); w += url_len;
                    *w++ = ']'; *w++ = '(';
                    memcpy(w, start, url_len); w += url_len;
                    *w++ = ')';
                    /* Rewritten away from HTML; net tag state is unchanged. */
                    r = end + 1;
                    continue;
                }
            }
        }

        /* Skip HTML comments (like citation placeholders <!--CITE:key-->) */
        if (*r == '<' && r[1] == '!' && r[2] == '-' && r[3] == '-') {
            /* Find end of comment --> */
            const char *comment_end = strstr(r, "-->");
            if (comment_end) {
                size_t comment_len = (comment_end + 3) - r;
                if ((size_t)(w - out) + comment_len + 1 > cap) {
                    size_t used = (size_t)(w - out);
                    cap = (used + comment_len + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) { free(out); return NULL; }
                    out = new_out;
                    w = out + used;
                }
                memcpy(w, r, comment_len);
                w += comment_len;
                apex_autolink_html_consume_range(r, comment_len, &in_html_tag,
                                                &html_quote_active,
                                                &html_quote_char);
                r = comment_end + 3;
                continue;
            }
        }

        /* Handle bare URL or mailto/email */
        bool is_url_start = false;
        bool is_email_start = false;

        if (!isspace((unsigned char)*r)) {
            /* Check for URL protocols */
            if (strncmp(r, "http://", 7) == 0 || strncmp(r, "https://", 8) == 0 || strncmp(r, "mailto:", 7) == 0) {
                is_url_start = true;
            }
            /* Check for email address: must start at word boundary and have @ in current token */
            /* Skip if @ is part of a citation placeholder (<!--CITE:...) or citation syntax [@ */
            else if ((r == text || isspace((unsigned char)r[-1]) || r[-1] == '(' || r[-1] == '[') &&
                     !(r > text && r[-1] == '[' && *r == '@')) {  /* Skip [@ which is citation syntax */
                /* Scan forward to find end of current token */
                const char *token_end = r;
                while (*token_end && !isspace((unsigned char)*token_end) && *token_end != '<' && *token_end != '>') {
                    token_end++;
                }
                /* Check if @ exists within this token (not just anywhere in text) */
                const char *at_pos = r;
                while (at_pos < token_end && *at_pos != '@') {
                    at_pos++;
                }
                /* If @ found within token, and it's not at start/end, it might be an email */
                /* Also skip if @ is immediately after [ (citation syntax) */
                if (at_pos < token_end && at_pos > r && (at_pos + 1) < token_end &&
                    !(at_pos > text && at_pos[-1] == '[')) {  /* Not [@ */
                    /* Require that the character immediately before @ is alphanumeric.
                     * This prevents matching @ in URLs like https://example.com/@user */
                    if (at_pos > r && isalnum((unsigned char)at_pos[-1])) {
                        /* Ignore image-density suffixes like @2x.jpg, @3x.avif, etc. */
                        const char *suffix = at_pos + 1;
                        if (suffix < token_end && isdigit((unsigned char)*suffix)) {
                            while (suffix < token_end && isdigit((unsigned char)*suffix)) {
                                suffix++;
                            }
                            if (suffix < token_end && *suffix == 'x') {
                                suffix++;
                                if (suffix < token_end && *suffix == '.') {
                                    suffix++;
                                    if (suffix < token_end && isalnum((unsigned char)*suffix)) {
                                        while (suffix < token_end && isalnum((unsigned char)*suffix)) {
                                            suffix++;
                                        }
                                        if (suffix == token_end) {
                                            /* This token is an image filename density suffix,
                                             * not an email address. */
                                            goto skip_email_candidate;
                                        }
                                    }
                                }
                            }
                        }

                        /* Validate that email has a TLD (at least one dot followed by alphanumeric) */
                        const char *after_at = at_pos + 1;
                        bool has_tld = false;
                        while (after_at < token_end) {
                            if (*after_at == '.' && (after_at + 1) < token_end &&
                                isalnum((unsigned char)after_at[1])) {
                                has_tld = true;
                                break;
                            }
                            if (!isalnum((unsigned char)*after_at) && *after_at != '-' && *after_at != '_') {
                                break;
                            }
                            after_at++;
                        }

                        if (has_tld) {
                            /* Basic validation passed: has chars before @, @, and TLD after @ */
                            is_email_start = true;
                        }
                    }
                }
            }
        }
skip_email_candidate:

        if (is_url_start || is_email_start) {
            const char *start = r;
            /* simple token end: whitespace or angle bracket */
            const char *end = start;
            while (*end && !isspace((unsigned char)*end) && *end != '<' && *end != '>') end++;
            size_t url_len = (size_t)(end - start);

            /* Trim trailing punctuation that should not be part of the link */
            while (url_len > 0 &&
                   (start[url_len - 1] == '.' ||
                    start[url_len - 1] == ',' ||
                    start[url_len - 1] == ';' ||
                    start[url_len - 1] == ':')) {
                url_len--;
                end--;
            }

            /* Prepare link and href text */
            const char *link_text = start;
            size_t link_text_len = url_len;
            const char *href_text = start;
            size_t href_len = url_len;
            bool needs_mailto_prefix = is_email_start &&
                                       !(url_len >= 7 && strncmp(start, "mailto:", 7) == 0);
            char *mailto_buf = NULL;

            if (needs_mailto_prefix) {
                href_len += 7; /* "mailto:" */
                mailto_buf = malloc(href_len + 1);
                if (mailto_buf) {
                    memcpy(mailto_buf, "mailto:", 7);
                    memcpy(mailto_buf + 7, start, url_len);
                    mailto_buf[href_len] = '\0';
                    href_text = mailto_buf;
                }
            }

            /* Heuristic: skip if preceded by '(' or '[' (likely already a link) */
            /* Also skip if this is a single '#' at start of line (header marker) */
            if (url_len > 0 &&
                !(r > text && (r[-1] == '(' || r[-1] == '[')) &&
                !(r == text && *r == '#' && (r[1] == ' ' || r[1] == '\t' || r[1] == '\n'))) {
                size_t needed = 2 + link_text_len + 3 + href_len + 2; /* [text](href) */
                if ((size_t)(w - out) + needed + 1 > cap) {
                    size_t used = (size_t)(w - out);
                    cap = (used + needed + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) { free(out); return NULL; }
                    out = new_out;
                    w = out + used;
                }
                *w++ = '[';
                memcpy(w, link_text, link_text_len); w += link_text_len;
                *w++ = ']'; *w++ = '(';
                memcpy(w, href_text, href_len); w += href_len;
                *w++ = ')';
                apex_autolink_html_consume_range(start, (size_t)(end - start),
                                                &in_html_tag, &html_quote_active,
                                                &html_quote_char);
                r = end;
                free(mailto_buf);
                continue;
            }
            free(mailto_buf);
        }

        apex_autolink_html_consume(*r, &in_html_tag, &html_quote_active,
                                   &html_quote_char);
        *w++ = *r++;

        /* Safety: ensure we always advance */
        if (r == loop_start) {
            r++;
        }
    }
    *w = '\0';
    return out;
}

/* ------------------------------------------------------------------------- */
/* Liquid tag protection                                                     */
/*                                                                           */
/* Any text between {% and %} is Liquid templating syntax and should be      */
/* ignored by Apex processing (math, autolinks, etc.). We implement this by  */
/* temporarily replacing Liquid tags with unique placeholder tokens before   */
/* parsing, then restoring the original tags in the final HTML.              */
/* ------------------------------------------------------------------------- */

static char *apex_protect_liquid_tags(const char *text,
                                      char ***out_tags,
                                      size_t *out_count) {
    if (!text || !out_tags || !out_count) {
        return NULL;
    }

    size_t len = strlen(text);
    /* Start with a modest expansion factor; we can grow as needed. */
    size_t capacity = (len > 0) ? len + 64 : 64;
    char *output = malloc(capacity);
    if (!output) {
        return NULL;
    }

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    char **tags = NULL;
    size_t tag_count = 0;
    size_t tag_capacity = 0;

    const char *ph_prefix = "APEX_LIQUID_TAG_";
    size_t ph_prefix_len = strlen(ph_prefix);

    while (*read) {
        const char *start = strstr(read, "{%");
        if (!start) {
            /* No more Liquid tags, copy remainder */
            size_t chunk_len = strlen(read);
            if (chunk_len >= remaining) {
                size_t used = (size_t)(write - output);
                capacity = used + chunk_len + 1;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    /* Cleanup on failure */
                    for (size_t i = 0; i < tag_count; i++) {
                        free(tags[i]);
                    }
                    free(tags);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            memcpy(write, read, chunk_len);
            write += chunk_len;
            remaining -= chunk_len;
            break;
        }

        /* Copy text before the tag */
        size_t prefix_copy_len = (size_t)(start - read);
        if (prefix_copy_len >= remaining) {
            size_t used = (size_t)(write - output);
            capacity = used + prefix_copy_len + 64;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                for (size_t i = 0; i < tag_count; i++) {
                    free(tags[i]);
                }
                free(tags);
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }
        memcpy(write, read, prefix_copy_len);
        write += prefix_copy_len;
        remaining -= prefix_copy_len;

        /* Find end of Liquid tag */
        const char *end = strstr(start + 2, "%}");
        if (!end) {
            /* No closing %}; copy rest as-is and stop */
            size_t chunk_len = strlen(start);
            if (chunk_len >= remaining) {
                size_t used = (size_t)(write - output);
                capacity = used + chunk_len + 1;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    for (size_t i = 0; i < tag_count; i++) {
                        free(tags[i]);
                    }
                    free(tags);
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            memcpy(write, start, chunk_len);
            write += chunk_len;
            remaining -= chunk_len;
            break;
        }

        size_t tag_len = (size_t)((end + 2) - start);

        /* Store original Liquid tag */
        if (tag_count == tag_capacity) {
            size_t new_cap = tag_capacity ? tag_capacity * 2 : 8;
            char **new_tags = realloc(tags, new_cap * sizeof(char *));
            if (!new_tags) {
                for (size_t i = 0; i < tag_count; i++) {
                    free(tags[i]);
                }
                free(tags);
                free(output);
                return NULL;
            }
            tags = new_tags;
            tag_capacity = new_cap;
        }

        tags[tag_count] = malloc(tag_len + 1);
        if (!tags[tag_count]) {
            for (size_t i = 0; i < tag_count; i++) {
                free(tags[i]);
            }
            free(tags);
            free(output);
            return NULL;
        }
        memcpy(tags[tag_count], start, tag_len);
        tags[tag_count][tag_len] = '\0';

        /* Build placeholder: APEX_LIQUID_TAG_<index> */
        char placeholder[64];
        int ph_len = snprintf(placeholder, sizeof(placeholder), "%s%zu", ph_prefix, tag_count);
        if (ph_len <= 0 || (size_t)ph_len >= sizeof(placeholder)) {
            /* Cleanup on formatting error */
            for (size_t i = 0; i <= tag_count; i++) {
                free(tags[i]);
            }
            free(tags);
            free(output);
            return NULL;
        }

        size_t placeholder_len = (size_t)ph_len;
        if (placeholder_len >= remaining) {
            size_t used = (size_t)(write - output);
            capacity = used + placeholder_len + 64;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                for (size_t i = 0; i <= tag_count; i++) {
                    free(tags[i]);
                }
                free(tags);
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }

        memcpy(write, placeholder, placeholder_len);
        write += placeholder_len;
        remaining -= placeholder_len;

        tag_count++;
        read = end + 2;
    }

    *write = '\0';
    *out_tags = tags;
    *out_count = tag_count;

    /* If we didn't actually find any Liquid tags, clean up and signal no-op */
    if (tag_count == 0) {
        free(output);
        *out_tags = NULL;
        *out_count = 0;
        return NULL;
    }

    (void)ph_prefix_len; /* silence unused warning */
    return output;
}

/**
 * Preprocess table captions to normalize different syntaxes before parsing.
 *
 * Handles two cases:
 * 1. Contiguous [Caption] lines immediately following a table row:
 *    - Detects a caption line like "[Caption]" directly after a line
 *      containing table cells (with '|').
 *    - Inserts a blank line between the table row and the caption line so
 *      cmark-gfm parses the caption as a separate paragraph, which our
 *      existing caption logic already understands.
 *
 * 2. Pandoc-style captions using "Table: Caption" after a table:
 *    - When a line starting with "Table:" immediately follows a table row,
 *      converts it to a MultiMarkdown-style caption "[Caption]" and inserts
      a blank line before it.
 *    - The caption text is trimmed of surrounding whitespace.
 *
 * NOTE: This is a text-level transform that runs before any table parsing
 * or advanced table processing. It intentionally skips fenced code blocks.
 */
static char *apex_preprocess_table_captions(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Allow room for extra blank lines and brackets when converting captions */
    size_t cap = len * 2 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *read = text;
    char *write = out;
    bool prev_line_was_table_row = false;
    bool prev_line_was_blank = false;
    bool in_code_block = false;
    /* Track if we buffered a blank line that should be skipped if next line is a caption */
    bool buffered_blank_line = false;
    /* Track if we're in a table section (after a table, across blank lines and captions) */
    bool in_table_section = false;

    while (*read) {
        const char *line_start = read;
        /* Find line ending - handle CRLF, CR, and LF */
        const char *line_end = NULL;
        const char *cr_pos = strchr(read, '\r');
        const char *lf_pos = strchr(read, '\n');

        /* Determine which line ending we have (prefer CRLF, then CR, then LF) */
        if (cr_pos && lf_pos && cr_pos < lf_pos && (lf_pos - cr_pos) == 1) {
            /* CRLF sequence - line ends at CR, LF follows */
            line_end = cr_pos;
        } else if (cr_pos && (!lf_pos || cr_pos < lf_pos)) {
            /* CR without following LF, or CR comes before LF */
            line_end = cr_pos;
        } else if (lf_pos) {
            /* LF only */
            line_end = lf_pos;
        } else {
            /* No line ending found - end of string */
            line_end = read + strlen(read);
        }

        /* has_newline indicates whether we found any line ending character */
        bool has_newline = (line_end != NULL && line_end < read + strlen(read));
        /* Determine the type of line ending for proper handling */
        bool has_crlf = (has_newline && line_end[0] == '\r' && line_end[1] == '\n');

        /* Determine line properties */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) {
            p++;
        }

        /* Track fenced code blocks (``` or ~~~) and skip caption transforms inside */
        if (!in_code_block &&
            (line_end - p) >= 3 &&
            ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
             (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = true;
        } else if (in_code_block &&
                   (line_end - p) >= 3 &&
                   ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
                    (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = false;
        }

        bool is_table_row_line = false;
        bool is_bracket_caption_line = false;
        bool is_pandoc_caption_line = false;
        bool is_colon_caption_line = false;
        bool is_blank_line = false;

        if (!in_code_block) {
            /* Treat any line containing '|' as a candidate table row */
            for (const char *q = line_start; q < line_end; q++) {
                if (*q == '|') {
                    is_table_row_line = true;
                    break;
                }
            }

            if (p >= line_end) {
                is_blank_line = true;
            } else if (*p == '[') {
                is_bracket_caption_line = true;
            } else if ((size_t)(line_end - p) >= 6 &&
                       (strncmp(p, "Table:", 6) == 0 || strncmp(p, "table:", 6) == 0)) {
                /* Table: at start of line (after whitespace) - always treat as caption */
                is_pandoc_caption_line = true;
            } else if (prev_line_was_table_row || prev_line_was_blank || in_table_section) {
                /* Also check for Table: later in the line (when it appears immediately after a table row) */
                const char *table_check = p;
                while (table_check < line_end - 5) {
                    if (strncmp(table_check, "Table:", 6) == 0 || strncmp(table_check, "table:", 6) == 0) {
                        is_pandoc_caption_line = true;
                        break;
                    }
                    table_check++;
                }
                /* Check for : Caption format (Pandoc-style, only in table context)
                 * Require prev_line_was_table_row or in_table_section - NOT prev_line_was_blank alone.
                 * prev_line_was_blank alone would wrongly convert "Term\n\n: definition 1" (def list) to caption. */
                if ((prev_line_was_table_row || in_table_section) &&
                    !is_pandoc_caption_line) {
                    const char *check = p;
                    int spaces = 0;
                    while (spaces < 3 && check < line_end && *check == ' ') {
                        spaces++;
                        check++;
                    }
                    if (check < line_end && *check == ':' &&
                        (check + 1) < line_end &&
                        (check[1] == ' ' || check[1] == '\t')) {
                        is_colon_caption_line = true;
                    }
                }
            } else {
                /* Check for : Caption BEFORE table (next non-blank line is a table row) */
                const char *check = p;
                int spaces = 0;
                while (spaces < 3 && check < line_end && *check == ' ') {
                    spaces++;
                    check++;
                }
                if (check < line_end && *check == ':' &&
                    (check + 1) < line_end &&
                    (check[1] == ' ' || check[1] == '\t')) {
                    /* Peek ahead: is next non-blank line a table row? */
                    const char *next = line_end;
                    if (next < text + len && *next == '\n') next++;
                    if (next < text + len && *next == '\r') next++;
                    while (next < text + len && (*next == '\n' || *next == '\r' || *next == ' ' || *next == '\t')) {
                        if (*next == '\n' || *next == '\r') {
                            next++;
                            if (next < text + len && next[-1] == '\r' && *next == '\n') next++;
                        } else {
                            next++;
                        }
                    }
                    if (next < text + len && *next == '|') {
                        is_colon_caption_line = true;
                    }
                }
            }
        }

        size_t line_len = (size_t)(line_end - line_start);

        if (!in_code_block &&
            prev_line_was_table_row &&
            is_bracket_caption_line) {
            /* Case 1: [Caption] immediately after table row -> ALWAYS insert blank line */
            /* This prevents the caption from being parsed as part of the table */
            *write++ = '\n';
            memcpy(write, line_start, line_len);
            write += line_len;
            /* Write line ending - always use LF for output consistency */
            if (has_newline) {
                *write++ = '\n';
            }
        } else if (!in_code_block &&
                   is_pandoc_caption_line) {
            /* Case 2: Pandoc-style 'Table: Caption {#id .class}' -> convert to '[Caption {#id .class}]' */
            /* Note: We check is_pandoc_caption_line without requiring prev_line_was_table_row
             * because Table: format is unambiguous and should work even after blank lines.
             * However, when it appears immediately after a table row (prev_line_was_table_row=true),
             * we need to ensure the table ends before the caption, so we always insert a blank line. */
            /* Find where 'Table:' or 'table:' appears in the line (might not be at the start) */
            const char *table_marker = strstr(line_start, "Table:");
            if (!table_marker || table_marker >= line_end) {
                table_marker = strstr(line_start, "table:");
                if (!table_marker || table_marker >= line_end) {
                    table_marker = p; /* Fallback to p if not found */
                }
            }
            const char *caption_start = table_marker + 6; /* after 'Table:' or 'table:' */
            while (caption_start < line_end &&
                   (*caption_start == ' ' || *caption_start == '\t')) {
                caption_start++;
            }

            /* Find end of caption (before IAL if present, or end of line) */
            const char *caption_end = line_end;
            /* Look for IAL pattern from the end */
            const char *search = caption_end - 1;
            while (search >= caption_start) {
                if (*search == '}') {
                    /* Found closing brace, look backwards for opening brace */
                    const char *open = search;
                    while (open >= caption_start && *open != '{') {
                        open--;
                    }
                    if (open >= caption_start && *open == '{') {
                        /* Check if it's a valid IAL pattern */
                        if ((open[1] == ':' || open[1] == '#' || open[1] == '.') &&
                            search > open) {
                            caption_end = open; /* Caption ends before IAL */
                            break;
                        }
                    }
                }
                search--;
            }

            /* Trim whitespace from caption */
            while (caption_end > caption_start &&
                   (caption_end[-1] == ' ' || caption_end[-1] == '\t' ||
                    caption_end[-1] == '\r')) {
                caption_end--;
            }

            if (caption_end > caption_start) {
                size_t caption_len = (size_t)(caption_end - caption_start);
                /* If we buffered a blank line, skip it (don't write it) since caption follows */
                buffered_blank_line = false; /* Discard the buffered blank line */

                /* If Table: appears in the middle of the line (after table row content),
                 * write the table row part first, then the caption */
                if (table_marker > line_start) {
                    /* Write the table row part (everything before Table:) */
                    size_t table_part_len = (size_t)(table_marker - line_start);
                    memcpy(write, line_start, table_part_len);
                    write += table_part_len;
                    /* ALWAYS write a newline to close the table row, even if the original line didn't have one */
                    *write++ = '\n';
                }

                /* ALWAYS insert blank line to prevent caption from being parsed as table row */
                *write++ = '\n';      /* blank line between table and caption */
                *write++ = '[';
                memcpy(write, caption_start, caption_len);
                write += caption_len;
                *write++ = ']';
                /* Clear table section flag after processing Table: caption */
                in_table_section = false;

                /* Write IAL if present (from original line, after the bracket) */
                if (caption_end < line_end) {
                    /* There's IAL after the caption */
                    const char *ial_start = caption_end;
                    while (ial_start < line_end && isspace((unsigned char)*ial_start)) {
                        ial_start++;
                    }
                    if (ial_start < line_end) {
                        /* Add space before IAL */
                        *write++ = ' ';
                        size_t ial_len = (size_t)(line_end - ial_start);
                        memcpy(write, ial_start, ial_len);
                        write += ial_len;
                    }
                }

                if (has_newline) {
                    *write++ = '\n';
                }
            } else {
                /* Empty caption text - fall back to copying the line as-is */
                memcpy(write, line_start, line_len);
                write += line_len;
                if (has_newline) {
                    *write++ = '\n';
                }
            }
        } else if (!in_code_block && is_colon_caption_line) {
            /* Case 3: Pandoc-style ': Caption {#id .class}' -> convert to '[Caption {#id .class}]'
             * Handles both: (a) after table, (b) before table (next line is | table row) */
            /* Skip leading whitespace (up to 3 spaces) */
            const char *caption_start = p;
            int spaces = 0;
            while (spaces < 3 && caption_start < line_end && *caption_start == ' ') {
                spaces++;
                caption_start++;
            }
            /* Skip ': ' */
            if (caption_start < line_end && *caption_start == ':' &&
                (caption_start + 1) < line_end &&
                (caption_start[1] == ' ' || caption_start[1] == '\t')) {
                caption_start += 2; /* Skip ': ' */
            }

            /* Find end of caption (before IAL if present, or end of line) */
            const char *caption_end = line_end;
            /* Look for IAL pattern from the end */
            const char *search = caption_end - 1;
            while (search >= caption_start) {
                if (*search == '}') {
                    /* Found closing brace, look backwards for opening brace */
                    const char *open = search;
                    while (open >= caption_start && *open != '{') {
                        open--;
                    }
                    if (open >= caption_start && *open == '{') {
                        /* Check if it's a valid IAL pattern */
                        if ((open[1] == ':' || open[1] == '#' || open[1] == '.') &&
                            search > open) {
                            caption_end = open; /* Caption ends before IAL */
                            break;
                        }
                    }
                }
                search--;
            }

            /* Trim whitespace from caption */
            while (caption_start < caption_end && isspace((unsigned char)*caption_start)) {
                caption_start++;
            }
            while (caption_end > caption_start && isspace((unsigned char)*(caption_end - 1))) {
                caption_end--;
            }

            /* If we buffered a blank line, skip it (don't write it) since caption follows */
            buffered_blank_line = false; /* Discard the buffered blank line */
            /* ALWAYS insert blank line to prevent caption from being parsed as table row */
            *write++ = '\n';
            *write++ = '[';

            /* Write caption text */
            if (caption_end > caption_start) {
                size_t caption_len = (size_t)(caption_end - caption_start);
                memcpy(write, caption_start, caption_len);
                write += caption_len;
            }

            *write++ = ']';
            /* Keep in_table_section true after colon caption, in case there's a Table: caption next */
            in_table_section = true;

            /* Write IAL if present (from original line, after the bracket) */
            if (caption_end < line_end) {
                /* There's IAL after the caption */
                const char *ial_start = caption_end;
                while (ial_start < line_end && isspace((unsigned char)*ial_start)) {
                    ial_start++;
                }
                if (ial_start < line_end) {
                    /* Add space before IAL */
                    *write++ = ' ';
                    size_t ial_len = (size_t)(line_end - ial_start);
                    memcpy(write, ial_start, ial_len);
                    write += ial_len;
                }
            }
            if (has_newline) {
                *write++ = '\n';
            }
        } else if (!in_code_block && is_blank_line && (prev_line_was_table_row || prev_line_was_blank || in_table_section)) {
            /* Blank line after table - might be before a caption, so buffer it */
            /* Also buffer if previous line was blank (chain of blank lines after table) */
            /* Or if we're in a table section (after processing a caption, still in table context) */
            /* We'll check the next line to see if it's a caption before writing this blank line */
            /* Update state to preserve prev_line_was_table_row */
            prev_line_was_blank = true;
            /* Mark that we have a buffered blank line */
            buffered_blank_line = true;
            /* Keep in_table_section true */
            in_table_section = true;
            /* Advance to next line, handling CRLF, CR, or LF */
            if (has_newline) {
                read = line_end + 1;
                /* Skip LF if we had CRLF */
                if (has_crlf) {
                    read++;
                }
            } else {
                read = line_end;
            }
            /* Don't write this blank line yet - continue to check next line */
            continue; /* Skip writing this line for now */
        } else {
            /* Default: copy line unchanged */
            /* If we buffered a blank line and this isn't a caption, write it now */
            if (buffered_blank_line &&
                !is_bracket_caption_line &&
                !is_pandoc_caption_line &&
                !is_colon_caption_line) {
                /* Write the buffered blank line at current position */
                *write++ = '\n';
                buffered_blank_line = false;
            }
            memcpy(write, line_start, line_len);
            write += line_len;
            /* Write line ending - always use LF for output consistency */
            if (has_newline) {
                *write++ = '\n';
            }
            buffered_blank_line = false; /* Clear buffer since we wrote the line */
        }

        /* Advance to next line, handling CRLF, CR, or LF */
        if (has_newline) {
            read = line_end + 1;
            /* Skip LF if we had CRLF */
            if (has_crlf) {
                read++;
            }
        } else {
            read = line_end;
        }
        if (!in_code_block) {
            if (is_table_row_line) {
                /* Remember that the last non-blank, table-looking line was a row */
                prev_line_was_table_row = true;
                prev_line_was_blank = false;
                in_table_section = true; /* Enter table section */
            } else if (!is_blank_line) {
                /* Any non-blank, non-table line clears the table-row context */
                /* But don't clear in_table_section if we just processed a caption */
                if (!is_bracket_caption_line && !is_pandoc_caption_line && !is_colon_caption_line) {
                    prev_line_was_table_row = false;
                    in_table_section = false; /* Only clear if not a caption */
                }
                prev_line_was_blank = false;
            } else {
                /* Blank line - preserve prev_line_was_table_row and set prev_line_was_blank */
                prev_line_was_blank = true;
                /* Blank lines preserve prev_line_was_table_row so that
                 * 'Table: Caption' can appear after one or more blank lines.
                 * Note: prev_line_was_table_row is NOT modified here - it remains
                 * whatever it was from the previous iteration. */
                /* Also preserve in_table_section across blank lines */
            }
        }
    }

    /* Ensure output always ends with a newline (required for cmark-gfm table parsing) */
    if (write > out && write[-1] != '\n' && write[-1] != '\r') {
        *write++ = '\n';
    }

    *write = '\0';
    return out;
}

static char *apex_restore_liquid_tags(const char *html,
                                      char **tags,
                                      size_t tag_count) {
    if (!html || !tags || tag_count == 0) {
        return NULL;
    }

    const char *ph_prefix = "APEX_LIQUID_TAG_";
    size_t ph_prefix_len = strlen(ph_prefix);
    size_t html_len = strlen(html);

    /* Allocate generously: HTML + space for tags being longer than placeholders */
    size_t capacity = html_len + tag_count * 64;
    if (capacity < html_len + 1) {
        capacity = html_len + 1;
    }

    char *output = malloc(capacity);
    if (!output) {
        return NULL;
    }

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        const char *ph_start = strstr(read, ph_prefix);
        if (!ph_start) {
            /* Copy remainder */
            size_t chunk_len = strlen(read);
            if (chunk_len >= remaining) {
                size_t used = (size_t)(write - output);
                capacity = used + chunk_len + 1;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            memcpy(write, read, chunk_len);
            write += chunk_len;
            remaining -= chunk_len;
            break;
        }

        /* Copy text before the placeholder */
        size_t prefix_copy_len = (size_t)(ph_start - read);
        if (prefix_copy_len >= remaining) {
            size_t used = (size_t)(write - output);
            capacity = used + prefix_copy_len + 64;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }
        memcpy(write, read, prefix_copy_len);
        write += prefix_copy_len;
        remaining -= prefix_copy_len;

        /* Parse index after prefix */
        const char *idx_start = ph_start + ph_prefix_len;
        size_t idx = 0;
        const char *p = idx_start;
        if (!(*p >= '0' && *p <= '9')) {
            /* Not actually a placeholder, copy prefix char and continue */
            if (remaining > 0) {
                *write++ = *ph_start;
                remaining--;
            }
            read = ph_start + 1;
            continue;
        }
        while (*p >= '0' && *p <= '9') {
            idx = idx * 10 + (size_t)(*p - '0');
            p++;
        }

        if (idx >= tag_count) {
            /* Out-of-range index, treat as normal text */
            size_t placeholder_len = (size_t)(p - ph_start);
            if (placeholder_len >= remaining) {
                size_t used = (size_t)(write - output);
                capacity = used + placeholder_len + 64;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            memcpy(write, ph_start, placeholder_len);
            write += placeholder_len;
            remaining -= placeholder_len;
            read = p;
            continue;
        }

        /* Insert original Liquid tag */
        size_t tag_len = strlen(tags[idx]);
        if (tag_len >= remaining) {
            size_t used = (size_t)(write - output);
            capacity = used + tag_len + 64;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }
        memcpy(write, tags[idx], tag_len);
        write += tag_len;
        remaining -= tag_len;

        read = p;
    }

    *write = '\0';
    (void)ph_prefix_len; /* silence unused warning */
    return output;
}

/**
 * Preprocess table rows to convert consecutive pipes without whitespace into << markers
 * for colspan detection.
 *
 * This distinguishes between:
 * - `| 1 |  |  |` - whitespace between pipes, should create separate empty cells
 * - `| 1 |||` - consecutive pipes, should create colspan
 *
 * Converts consecutive pipes (||) to | << | pattern so the existing colspan logic
 * can recognize them.
 */
static char *apex_preprocess_table_colspans(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Worst case: every || becomes | << | (grows by 4 chars per occurrence) */
    size_t cap = len * 2 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    const char *read = text;
    char *write = out;
    size_t remaining = cap;
    bool in_code_block = false;

    while (*read) {
        const char *line_start = read;
        /* Find line ending - handle CRLF, CR, and LF */
        const char *line_end = NULL;
        const char *cr_pos = strchr(read, '\r');
        const char *lf_pos = strchr(read, '\n');

        /* Determine which line ending we have (prefer CRLF, then CR, then LF) */
        if (cr_pos && lf_pos && cr_pos < lf_pos && (lf_pos - cr_pos) == 1) {
            line_end = cr_pos;
        } else if (cr_pos && (!lf_pos || cr_pos < lf_pos)) {
            line_end = cr_pos;
        } else if (lf_pos) {
            line_end = lf_pos;
        } else {
            line_end = read + strlen(read);
        }

        bool has_newline = (line_end != NULL && line_end < read + strlen(read));
        bool has_crlf = (has_newline && line_end[0] == '\r' && line_end[1] == '\n');

        /* Track fenced code blocks (``` or ~~~) and skip processing inside */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) {
            p++;
        }

        if (!in_code_block &&
            (line_end - p) >= 3 &&
            ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
             (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = true;
        } else if (in_code_block &&
                   (line_end - p) >= 3 &&
                   ((p[0] == '`' && p[1] == '`' && p[2] == '`') ||
                    (p[0] == '~' && p[1] == '~' && p[2] == '~'))) {
            in_code_block = false;
        }

        /* Check if this line contains a table row (has | character) */
        bool is_table_row = false;
        if (!in_code_block) {
            for (const char *q = line_start; q < line_end; q++) {
                if (*q == '|') {
                    is_table_row = true;
                    break;
                }
            }
        }

        if (is_table_row && !in_code_block) {
            /* Process this table row to convert consecutive pipes */
            const char *line_cur = line_start;
            while (line_cur < line_end) {
                if (*line_cur == '|') {
                    /* Count consecutive pipes */
                    int pipe_count = 1;
                    const char *check = line_cur + 1;
                    while (check < line_end && *check == '|') {
                        pipe_count++;
                        check++;
                    }

                    if (pipe_count >= 2) {
                        /* Found consecutive pipes with no whitespace - convert to | << | pattern */
                        /* For || (2 pipes), convert to | << | (one colspan marker) */
                        /* For ||| (3 pipes), convert to | << | << | (two colspan markers) */
                        /* This distinguishes ||| from |  |  | */
                        /* Write the first pipe */
                        if (remaining < 5) {
                            /* Need more space */
                            size_t written = write - out;
                            cap = (written + (line_end - line_cur) * 2 + 1) * 2;
                            char *new_out = realloc(out, cap);
                            if (!new_out) {
                                free(out);
                                return NULL;
                            }
                            write = new_out + written;
                            out = new_out;
                            remaining = cap - written;
                        }
                        *write++ = '|';
                        remaining--;

                        /* For each additional pipe beyond the first, add | << | */
                        /* pipe_count=2 (||) -> add 1 marker */
                        /* pipe_count=3 (|||) -> add 2 markers */
                        for (int i = 1; i < pipe_count; i++) {
                            if (remaining < 5) {
                                size_t written = write - out;
                                cap = (written + (line_end - line_cur) * 2 + 1) * 2;
                                char *new_out = realloc(out, cap);
                                if (!new_out) {
                                    free(out);
                                    return NULL;
                                }
                                write = new_out + written;
                                out = new_out;
                                remaining = cap - written;
                            }
                            *write++ = ' ';
                            remaining--;
                            *write++ = '<';
                            remaining--;
                            *write++ = '<';
                            remaining--;
                            *write++ = ' ';
                            remaining--;
                            *write++ = '|';
                            remaining--;
                        }
                        line_cur += pipe_count; /* Skip all consecutive pipes */
                        continue;
                    }
                }
                /* Copy character as-is */
                if (remaining > 0) {
                    *write++ = *line_cur++;
                    remaining--;
                } else {
                    /* Need more space */
                    size_t written = write - out;
                    cap = (written + (line_end - line_cur) + 1) * 2;
                    char *new_out = realloc(out, cap);
                    if (!new_out) {
                        free(out);
                        return NULL;
                    }
                    write = new_out + written;
                    out = new_out;
                    remaining = cap - written;
                    *write++ = *line_cur++;
                    remaining--;
                }
            }
        } else {
            /* Not a table row or in code block - copy line as-is */
            size_t line_len = line_end - line_start;
            if (line_len > remaining) {
                size_t written = write - out;
                cap = (written + line_len + 1) * 2;
                char *new_out = realloc(out, cap);
                if (!new_out) {
                    free(out);
                    return NULL;
                }
                write = new_out + written;
                out = new_out;
                remaining = cap - written;
            }
            memcpy(write, line_start, line_len);
            write += line_len;
            remaining -= line_len;
        }

        /* Copy line ending */
        if (has_newline) {
            if (remaining > 2) {
                if (has_crlf) {
                    *write++ = '\r';
                    *write++ = '\n';
                    remaining -= 2;
                    read = line_end + 2;
                } else if (line_end[0] == '\r') {
                    *write++ = '\r';
                    remaining--;
                    read = line_end + 1;
                } else {
                    *write++ = '\n';
                    remaining--;
                    read = line_end + 1;
                }
            } else {
                /* Need more space */
                size_t written = write - out;
                cap = (written + 10) * 2;
                char *new_out = realloc(out, cap);
                if (!new_out) {
                    free(out);
                    return NULL;
                }
                write = new_out + written;
                out = new_out;
                remaining = cap - written;
                if (has_crlf) {
                    *write++ = '\r';
                    *write++ = '\n';
                    remaining -= 2;
                    read = line_end + 2;
                } else if (line_end[0] == '\r') {
                    *write++ = '\r';
                    remaining--;
                    read = line_end + 1;
                } else {
                    *write++ = '\n';
                    remaining--;
                    read = line_end + 1;
                }
            }
        } else {
            read = line_end;
        }
    }

    *write = '\0';
    return out;
}

/**
 * Preprocess alpha list markers (a., b., c. and A., B., C.)
 * Converts them to numbered markers (1., 2., 3.) and inserts an HTML comment (indented like
 * the list items) so the HTML pass can apply list-style-type; bracket tokens are not used
 * because the Markdown parser strips them before HTML is produced.
 */
static char *apex_preprocess_alpha_lists(const char *text,
                                         bool *inserted_synthetic_nested_alpha_break,
                                         bool *saw_explicit_nested_alpha_break) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 3;  /* Extra capacity for markers and conversions */
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;
    bool in_alpha_list = false;
    size_t alpha_list_indent = 0;
    char expected_lower = 'a';
    char expected_upper = 'A';
    bool is_upper = false;
    int item_number = 1;
    int blank_lines_since_alpha = 0;  /* Track blank lines to detect list breaks */
    if (inserted_synthetic_nested_alpha_break) *inserted_synthetic_nested_alpha_break = false;
    if (saw_explicit_nested_alpha_break) *saw_explicit_nested_alpha_break = false;

    while (*read) {
        const char *line_start = read;
        const char *line_end = strchr(read, '\n');
        if (!line_end) line_end = read + strlen(read);
        bool has_newline = (*line_end == '\n');

        /* Check for line start */
        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        size_t current_indent = (size_t)(p - line_start);

        /* Check if line starts with alpha marker */
        bool is_alpha_marker = false;
        char alpha_char = 0;
        bool alpha_is_upper = false;

        if (p < line_end) {
            if (*p >= 'a' && *p <= 'z' && p + 1 < line_end && p[1] == '.' &&
                (p + 2 >= line_end || (p[2] == ' ' || p[2] == '\t'))) {
                is_alpha_marker = true;
                alpha_char = *p;
                alpha_is_upper = false;
            } else if (*p >= 'A' && *p <= 'Z' && p + 1 < line_end && p[1] == '.' &&
                       (p + 2 >= line_end || (p[2] == ' ' || p[2] == '\t'))) {
                is_alpha_marker = true;
                alpha_char = *p;
                alpha_is_upper = true;
            }
        }

        if (is_alpha_marker) {
            bool nested_alpha_after_item = false;
            if (in_alpha_list && alpha_is_upper == is_upper && current_indent > alpha_list_indent) {
                if (alpha_is_upper) {
                    nested_alpha_after_item = (alpha_char == expected_upper);
                } else {
                    nested_alpha_after_item = (alpha_char == expected_lower);
                }
            }
            /* Check if this continues an existing alpha list */
            bool continues_list = false;
            if (in_alpha_list) {
                if (alpha_is_upper == is_upper && current_indent == alpha_list_indent) {
                    if (alpha_is_upper) {
                        if (alpha_char == expected_upper) {
                            continues_list = true;
                        }
                    } else {
                        if (alpha_char == expected_lower) {
                            continues_list = true;
                        }
                    }
                }
            }

            if (!continues_list) {
                if (nested_alpha_after_item) {
                    if (blank_lines_since_alpha > 0) {
                        if (saw_explicit_nested_alpha_break) *saw_explicit_nested_alpha_break = true;
                    } else {
                        if (inserted_synthetic_nested_alpha_break) *inserted_synthetic_nested_alpha_break = true;
                    }
                }
                /* Start new alpha list */
                in_alpha_list = true;
                is_upper = alpha_is_upper;
                alpha_list_indent = current_indent;
                item_number = 1;
                blank_lines_since_alpha = 0;
                if (alpha_is_upper) {
                    expected_upper = alpha_char;
                } else {
                    expected_lower = alpha_char;
                }
                /* HTML comment + indent: bracket markers are stripped by the parser; comments survive in unsafe HTML
                 * and must be indented like the following list items to stay inside a parent <li>. */
                int needed = snprintf(write, remaining, "%.*s<!-- apex-alpha-list-%s -->\n\n",
                                      (int)(p - line_start), line_start, alpha_is_upper ? "upper" : "lower");
                if (needed > 0 && needed < (int)remaining) {
                    write += needed;
                    remaining -= needed;
                }
            } else {
                blank_lines_since_alpha = 0;  /* Reset on continuation */
            }

            /* Convert alpha marker to numbered marker */
            int needed = snprintf(write, remaining, "%.*s%d. ", (int)(p - line_start), line_start, item_number);
            if (needed > 0 && needed < (int)remaining) {
                write += needed;
                remaining -= needed;
            }

            /* Advance past the alpha marker and copy the rest of the line */
            const char *line_rest = p + 2;  /* Skip "a." or "A." */
            size_t line_rest_len = line_end - line_rest;
            if (has_newline) {
                line_rest_len++;  /* Include newline */
            }
            if (line_rest_len > remaining) {
                /* Buffer too small, but try to copy what we can */
                line_rest_len = remaining;
            }
            if (line_rest_len > 0) {
                memcpy(write, line_rest, line_rest_len);
                write += line_rest_len;
                remaining -= line_rest_len;
            }
            read = line_end;
            if (has_newline && *read == '\n') read++;
            item_number++;

            /* Update expected next character */
            if (alpha_is_upper) {
                expected_upper++;
                if (expected_upper > 'Z') expected_upper = 'A';  /* Wrap around */
            } else {
                expected_lower++;
                if (expected_lower > 'z') expected_lower = 'a';  /* Wrap around */
            }
            continue;  /* Skip the else block since we've handled this line */
        } else {
            /* Not an alpha marker - check if we should end the list */
            if (in_alpha_list) {
                /* Blank line - count it */
                if (line_end == line_start || (p >= line_end)) {
                    blank_lines_since_alpha++;
                    /* If we have 2+ blank lines, end the alpha list */
                    if (blank_lines_since_alpha >= 2) {
                        in_alpha_list = false;
                    }
                } else if (current_indent > alpha_list_indent) {
                    /* Nested content inside current alpha list item; keep list open. */
                    blank_lines_since_alpha = 0;
                } else {
                    /* Check if it's a numbered list marker starting with "1." after blank lines */
                    bool had_blank_lines = (blank_lines_since_alpha > 0);
                    /* Reset blank line counter on non-blank line */
                    blank_lines_since_alpha = 0;

                    if (*p >= '0' && *p <= '9') {
                        /* Parse the number */
                        int num = 0;
                        const char *num_p = p;
                        while (num_p < line_end && *num_p >= '0' && *num_p <= '9') {
                            num = num * 10 + (*num_p - '0');
                            num_p++;
                        }
                        /* If it's "1. " after blank lines (from ^ marker), end alpha list */
                        if (num == 1 && num_p < line_end && *num_p == '.' &&
                            (num_p + 1 >= line_end || num_p[1] == ' ' || num_p[1] == '\t') &&
                            had_blank_lines) {
                            in_alpha_list = false;
                            /* Insert a paragraph with a space to force block separation */
                            /* The parser will see this as a block break and create separate lists */
                            int needed = snprintf(write, remaining, "\n\n \n\n");
                            if (needed > 0 && needed < (int)remaining) {
                                write += needed;
                                remaining -= needed;
                            }
                        } else {
                            /* Other numbered markers also end alpha list */
                            in_alpha_list = false;
                        }
                    } else if (*p == '*' || *p == '-' || *p == '+') {
                        /* Bullet list marker - ends alpha list */
                        in_alpha_list = false;
                    } else {
                        /* Non-list content ends the alpha list */
                        in_alpha_list = false;
                    }
                }
            }

            /* Copy line as-is */
            size_t line_len = line_end - line_start;
            if (line_end < read + strlen(read)) line_len++;  /* Include newline */
            if (line_len > remaining) line_len = remaining;
            memcpy(write, line_start, line_len);
            write += line_len;
            remaining -= line_len;
            read = line_end;
            if (*read == '\n') read++;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Insert a blank line before indented ordered sublists that directly follow
 * a parent list item line, so they parse as nested <ol> blocks.
 */
static char *apex_preprocess_nested_ordered_sublists(const char *text,
                                                     bool *inserted_synthetic_break,
                                                     bool *saw_explicit_break) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    size_t output_capacity = text_len * 2 + 1;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = output_capacity;

    bool prev_line_was_blank = true;
    bool prev_line_was_list_item = false;
    size_t prev_line_indent = 0;

    if (inserted_synthetic_break) *inserted_synthetic_break = false;
    if (saw_explicit_break) *saw_explicit_break = false;

    while (*read) {
        const char *line_start = read;
        const char *line_end = strchr(read, '\n');
        if (!line_end) line_end = read + strlen(read);
        bool has_newline = (*line_end == '\n');

        const char *p = line_start;
        while (p < line_end && (*p == ' ' || *p == '\t')) p++;
        size_t current_indent = (size_t)(p - line_start);

        bool current_line_blank = (p >= line_end);
        bool current_line_ordered_marker = false;
        if (!current_line_blank && *p >= '0' && *p <= '9') {
            const char *num_p = p;
            while (num_p < line_end && *num_p >= '0' && *num_p <= '9') num_p++;
            if (num_p < line_end && *num_p == '.' &&
                (num_p + 1 >= line_end || num_p[1] == ' ' || num_p[1] == '\t')) {
                current_line_ordered_marker = true;
            }
        }

        bool current_line_list_item = false;
        if (!current_line_blank) {
            if (current_line_ordered_marker) {
                current_line_list_item = true;
            } else if ((*p == '-' || *p == '*' || *p == '+') &&
                       (p + 1 >= line_end || p[1] == ' ' || p[1] == '\t')) {
                current_line_list_item = true;
            }
        }

        bool nested_ordered_after_item = (prev_line_was_list_item &&
                                          current_line_ordered_marker &&
                                          current_indent > prev_line_indent);
        if (nested_ordered_after_item && prev_line_was_blank) {
            if (saw_explicit_break) *saw_explicit_break = true;
        }

        if (!prev_line_was_blank && nested_ordered_after_item) {
            if (remaining < 1) {
                size_t used = (size_t)(write - output);
                size_t new_capacity = output_capacity * 2 + 64;
                char *new_output = realloc(output, new_capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = new_capacity - used;
                output_capacity = new_capacity;
            }
            *write++ = '\n';
            remaining--;
            if (inserted_synthetic_break) *inserted_synthetic_break = true;
            prev_line_was_blank = true;
        }

        size_t line_len = (size_t)(line_end - line_start) + (has_newline ? 1 : 0);
        if (remaining < line_len) {
            size_t used = (size_t)(write - output);
            size_t needed = used + line_len + 1;
            size_t new_capacity = output_capacity;
            while (new_capacity < needed) new_capacity *= 2;
            char *new_output = realloc(output, new_capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = new_capacity - used;
            output_capacity = new_capacity;
        }

        memcpy(write, line_start, line_len);
        write += line_len;
        remaining -= line_len;

        prev_line_was_blank = current_line_blank;
        prev_line_was_list_item = current_line_list_item;
        prev_line_indent = current_indent;

        read = line_end;
        if (has_newline) read++;
    }

    *write = '\0';
    return output;
}

/* cmark-gfm's tasklist scanner matches from the start of the physical line,
 * so "> - [ ] item" (including callout bodies) is never recognized. Convert
 * those items after parsing. */
static void apex_fix_blockquote_tasklists(cmark_node *node, cmark_syntax_extension *tasklist_ext,
                                          int blockquote_depth) {
    if (!node || !tasklist_ext) return;

    cmark_node_type type = cmark_node_get_type(node);
    if (type == CMARK_NODE_BLOCK_QUOTE) {
        blockquote_depth++;
    } else if (type == CMARK_NODE_ITEM && blockquote_depth > 0 &&
               cmark_node_get_syntax_extension(node) != tasklist_ext) {
        cmark_node *para = cmark_node_first_child(node);
        cmark_node *text = para && cmark_node_get_type(para) == CMARK_NODE_PARAGRAPH
                               ? cmark_node_first_child(para) : NULL;
        const char *lit = text && cmark_node_get_type(text) == CMARK_NODE_TEXT
                              ? cmark_node_get_literal(text) : NULL;
        if (lit && lit[0] == '[' && (lit[1] == ' ' || lit[1] == 'x' || lit[1] == 'X') &&
            lit[2] == ']' && (lit[3] == ' ' || lit[3] == '\t' ||
                              (lit[3] == '\0' && !cmark_node_next(text)))) {
            bool checked = lit[1] != ' ';
            const char *rest = lit + 3;
            while (*rest == ' ' || *rest == '\t') rest++;
            if (*rest) {
                char *copy = strdup(rest);
                if (copy) {
                    cmark_node_set_literal(text, copy);
                    free(copy);
                }
            } else {
                cmark_node_free(text);
            }
            cmark_node_set_syntax_extension(node, tasklist_ext);
            cmark_gfm_extensions_set_tasklist_item_checked(node, checked);
        }
    }

    for (cmark_node *child = cmark_node_first_child(node); child; child = cmark_node_next(child)) {
        apex_fix_blockquote_tasklists(child, tasklist_ext, blockquote_depth);
    }
}

/* Force tight rendering for lists synthesized from nested ordered sublist fixes. */
static void apex_tighten_nested_ordered_list_items(cmark_node *node) {
    if (!node) return;

    for (cmark_node *child = cmark_node_first_child(node); child; ) {
        cmark_node *next = cmark_node_next(child);
        apex_tighten_nested_ordered_list_items(child);
        child = next;
    }

    if (cmark_node_get_type(node) != CMARK_NODE_LIST) return;

    bool has_para_then_sublist_item = false;
    for (cmark_node *item = cmark_node_first_child(node); item; item = cmark_node_next(item)) {
        if (cmark_node_get_type(item) != CMARK_NODE_ITEM) continue;
        cmark_node *first = cmark_node_first_child(item);
        cmark_node *second = first ? cmark_node_next(first) : NULL;
        if (first && second &&
            cmark_node_get_type(first) == CMARK_NODE_PARAGRAPH &&
            cmark_node_get_type(second) == CMARK_NODE_LIST) {
            has_para_then_sublist_item = true;
            break;
        }
    }
    if (has_para_then_sublist_item) {
        cmark_node_set_list_tight(node, 1);
    }
}

/**
 * Post-process HTML to add style attributes to alpha lists
 * Finds HTML comments like <!-- apex-alpha-list-lower --> or <!-- apex-alpha-list-upper -->
 * and adds style="list-style-type: lower-alpha" or style="list-style-type: upper-alpha"
 * to the following <ol> tag.
 */
static char *apex_postprocess_alpha_lists_html(const char *html) {
    if (!html) return NULL;

    /* Early exit: bracket markers (legacy) or HTML comments from preprocessor */
    if (strstr(html, "[apex-alpha-list:") == NULL &&
        strstr(html, "<!-- apex-alpha-list-") == NULL) {
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t output_capacity = html_len + 1024;  /* Extra space for style attributes */
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = output_capacity;

    /* Helper macro for buffer expansion */
    #define ENSURE_SPACE(needed) do { \
        if (remaining < (needed)) { \
            size_t used = write - output; \
            size_t new_capacity = (used + (needed) + 1) * 2; \
            char *new_output = realloc(output, new_capacity); \
            if (!new_output) { \
                free(output); \
                return NULL; \
            } \
            output = new_output; \
            write = output + used; \
            remaining = new_capacity - used; \
        } \
    } while(0)

    /* Single-pass: HTML comments (current), <p>[apex-alpha-list:…]</p> (legacy), or stray brackets */
    const char *read_start = read;
    static const char alpha_comment_lower[] = "<!-- apex-alpha-list-lower -->";
    static const char alpha_comment_upper[] = "<!-- apex-alpha-list-upper -->";
    const size_t alpha_comment_lower_len = sizeof(alpha_comment_lower) - 1;
    const size_t alpha_comment_upper_len = sizeof(alpha_comment_upper) - 1;

    while (*read) {
        /* Strip HTML comment markers and style the following <ol> (not copied to output). */
        bool comment_upper = false;
        size_t comment_len = 0;
        if ((size_t)(html + html_len - read) >= alpha_comment_lower_len &&
            strncmp(read, alpha_comment_lower, alpha_comment_lower_len) == 0) {
            comment_len = alpha_comment_lower_len;
        } else if ((size_t)(html + html_len - read) >= alpha_comment_upper_len &&
                   strncmp(read, alpha_comment_upper, alpha_comment_upper_len) == 0) {
            comment_upper = true;
            comment_len = alpha_comment_upper_len;
        }
        if (comment_len > 0) {
            size_t copy_len = (size_t)(read - read_start);
            ENSURE_SPACE(copy_len);
            if (copy_len > 0) {
                memcpy(write, read_start, copy_len);
                write += copy_len;
                remaining -= copy_len;
            }
            read += comment_len;
            read_start = read;
            while (*read == ' ' || *read == '\t' || *read == '\n' || *read == '\r') {
                read++;
            }
            read_start = read;
            if (read[0] == '<' && read[1] == 'o' && read[2] == 'l' &&
                (read[3] == '>' || read[3] == ' ' || read[3] == '\t' || read[3] == '\n')) {
                const char *ol_start = read;
                const char *tag_end = strchr(ol_start, '>');

                if (tag_end) {
                    bool has_style = false;
                    for (const char *p = ol_start; p < tag_end; p++) {
                        if (strncmp(p, "style=", 6) == 0) {
                            has_style = true;
                            break;
                        }
                    }
                    size_t tag_len = (size_t)(tag_end - ol_start); /* before '>' */
                    ENSURE_SPACE(tag_len + 50);
                    memcpy(write, ol_start, tag_len);
                    write += tag_len;
                    remaining -= tag_len;
                    if (!has_style) {
                        const char *style = comment_upper
                            ? " style=\"list-style-type: upper-alpha\">"
                            : " style=\"list-style-type: lower-alpha\">";
                        size_t style_len = strlen(style);
                        memcpy(write, style, style_len);
                        write += style_len;
                        remaining -= style_len;
                    } else {
                        ENSURE_SPACE(1);
                        *write++ = '>';
                        remaining--;
                    }
                    read = tag_end + 1;
                    read_start = read;
                    continue;
                }
            }
            continue;
        }

        /* Drop any stray raw markers that did not form a standalone paragraph. */
        if (strncmp(read, "[apex-alpha-list:lower]", 23) == 0 ||
            strncmp(read, "[apex-alpha-list:upper]", 23) == 0) {
            size_t copy_len = read - read_start;
            ENSURE_SPACE(copy_len);
            if (copy_len > 0) {
                memcpy(write, read_start, copy_len);
                write += copy_len;
                remaining -= copy_len;
            }
            read += 23;
            read_start = read;
            continue;
        }

        /* Check for marker patterns */
        if (read[0] == '<' && read[1] == 'p' && read[2] == '>' && read[3] == '[') {
            /* Potential marker start */
            if (strncmp(read + 4, "apex-alpha-list:", 16) == 0) {
                bool is_upper = false;
                const char *marker_end = NULL;

                /* Check for lower or upper */
                if (read + 20 < html + html_len && strncmp(read + 20, "lower]</p>", 9) == 0) {
                    marker_end = read + 29;  /* "<p>[apex-alpha-list:lower]</p>" */
                    is_upper = false;
                } else if (read + 20 < html + html_len && strncmp(read + 20, "upper]</p>", 9) == 0) {
                    marker_end = read + 29;  /* "<p>[apex-alpha-list:upper]</p>" */
                    is_upper = true;
                }

                if (marker_end) {
                    /* Found a marker - copy everything up to it */
                    size_t copy_len = read - read_start;
                    ENSURE_SPACE(copy_len);
                    if (copy_len > 0) {
                        memcpy(write, read_start, copy_len);
                        write += copy_len;
                        remaining -= copy_len;
                    }
                    read_start = marker_end + 1;
                    read = marker_end + 1;

                    /* Skip whitespace (but copy it) */
                    const char *whitespace_start = read;
                    while (*read && (*read == ' ' || *read == '\t' || *read == '\n' || *read == '\r')) {
                        read++;
                    }
                    /* Copy whitespace in batch */
                    size_t whitespace_len = read - whitespace_start;
                    if (whitespace_len > 0) {
                        ENSURE_SPACE(whitespace_len);
                        memcpy(write, whitespace_start, whitespace_len);
                        write += whitespace_len;
                        remaining -= whitespace_len;
                    }
                    read_start = read;

                    /* Look for <ol> tag (single character check, then verify) */
                    if (read[0] == '<' && read[1] == 'o' && read[2] == 'l' &&
                        (read[3] == '>' || read[3] == ' ' || read[3] == '\t' || read[3] == '\n')) {
                        const char *ol_start = read;
                        const char *tag_end = strchr(ol_start, '>');

                        if (tag_end) {
                            /* Check if already has style attribute */
                            bool has_style = false;
                            for (const char *p = ol_start; p < tag_end; p++) {
                                if (strncmp(p, "style=", 6) == 0) {
                                    has_style = true;
                                    break;
                                }
                            }

                            if (!has_style) {
                                /* Copy "<ol" and attributes */
                                size_t tag_len = tag_end - ol_start;
                                ENSURE_SPACE(tag_len + 50);  /* Extra for style attribute */
                                memcpy(write, ol_start, tag_len);
                                write += tag_len;
                                remaining -= tag_len;

                                /* Add style attribute */
                                const char *style = is_upper
                                    ? " style=\"list-style-type: upper-alpha\">"
                                    : " style=\"list-style-type: lower-alpha\">";
                                size_t style_len = strlen(style);
                                memcpy(write, style, style_len);
                                write += style_len;
                                remaining -= style_len;
                                read = tag_end + 1;
                                read_start = read;
                                continue;
                            }
                        }
                    }
                }
            }
        }

        /* Normal character - just advance */
        read++;
    }

    /* Copy any remaining characters */
    size_t remaining_len = read - read_start;
    if (remaining_len > 0) {
        ENSURE_SPACE(remaining_len);
        memcpy(write, read_start, remaining_len);
        write += remaining_len;
        remaining -= remaining_len;
    }

    #undef ENSURE_SPACE

    *write = '\0';
    return output;
}

/**
 * Remove empty paragraphs that contain only zero-width spaces (from ^ markers)
 */
static char *apex_remove_empty_paragraphs(const char *html) {
    if (!html) return NULL;

    size_t html_len = strlen(html);
    size_t output_capacity = html_len + 1;
    char *output = malloc(output_capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = output_capacity;

    while (*read) {
        /* Look for <p> with zero-width space or empty content followed by </p> */
        if (strncmp(read, "<p>", 3) == 0) {
            const char *p_end = strstr(read, "</p>");
            if (p_end) {
                const char *content_start = read + 3;
                size_t content_len = p_end - content_start;

                /* Check if content is just zero-width space entity, whitespace, or empty */
                bool is_empty = true;
                const char *c = content_start;
                while (c < p_end) {
                    if (strncmp(c, "&#8203;", 7) == 0) {
                        c += 7;  /* Skip the entity */
                        continue;
                    }
                    if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') {
                        /* Check for UTF-8 zero-width space (E2 80 8B) */
                        if ((unsigned char)*c == 0xE2 && c + 2 < p_end &&
                            (unsigned char)c[1] == 0x80 && (unsigned char)c[2] == 0x8B) {
                            c += 3;
                            continue;
                        }
                        is_empty = false;
                        break;
                    }
                    c++;
                }

                if (is_empty && content_len > 0) {
                    /* Skip this paragraph */
                    read = p_end + 4;  /* Skip </p> */
                    continue;
                }
            }
        }

        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            break;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Merge adjacent lists with mixed markers at the same level
 * When allow_mixed_list_markers is true, lists with different marker types
 * at the same indentation level should inherit the type from the first list.
 */
static void apex_merge_mixed_list_markers(cmark_node *node) {
    if (!node) return;

    /* Process children first (depth-first) */
    /* Get next sibling before processing to avoid issues if child modifies tree */
    /* Note: If child merges with its next sibling during recursive processing,
     * that sibling will be freed. So we need to get the next sibling from the
     * parent's perspective after the recursive call, not use the pre-call next. */
    for (cmark_node *child = cmark_node_first_child(node); child; ) {
        /* Get next sibling BEFORE recursive call as a hint, but we'll re-get it after
         * because the recursive call might have merged child with next and freed it. */
        apex_merge_mixed_list_markers(child);
        /* After recursive call, get next sibling from parent's perspective.
         * If child was merged with its next sibling, that sibling is now freed,
         * but child's position in the parent's child list hasn't changed, so
         * we can safely get the next sibling. */
        child = cmark_node_next(child);
    }

    /* Check if current node is a list */
    if (cmark_node_get_type(node) != CMARK_NODE_LIST) {
        return;
    }

    /* Look for adjacent lists at the same level */
    cmark_node *sibling = cmark_node_next(node);
    while (sibling) {
        /* Skip non-list nodes (paragraphs, etc.) - these indicate list separation */
        if (cmark_node_get_type(sibling) != CMARK_NODE_LIST) {
            break;  /* Non-list node means lists are separated, don't merge */
        }

        cmark_list_type first_type = cmark_node_get_list_type(node);
        cmark_list_type second_type = cmark_node_get_list_type(sibling);

        /* If they have different types, merge them */
        if (first_type != second_type) {
            /* Use the first list's type (this is what we'll use) */

            /* Move all items from the second list to the first */
            cmark_node *item = cmark_node_first_child(sibling);
            while (item) {
                cmark_node *next_item = cmark_node_next(item);
                cmark_node_unlink(item);
                cmark_node_append_child(node, item);
                item = next_item;
            }

            /* Update list start number if needed (for ordered lists) */
            /* Note: We keep the original start number from the first list.
             * The items are already numbered correctly by the parser. */

            /* Remove the now-empty second list */
            cmark_node *next_sibling = cmark_node_next(sibling);
            cmark_node_unlink(sibling);
            cmark_node_free(sibling);
            sibling = next_sibling;
        } else {
            /* Same type, move to next sibling */
            sibling = cmark_node_next(sibling);
        }
    }
}

/**
 * Get default options with all features enabled (unified mode)
 */
apex_options apex_options_default(void) {
    apex_options opts;
    opts.mode = APEX_MODE_UNIFIED;

    /* Enable all features by default in unified mode; plugins are opt-in */
    opts.enable_plugins = false;
    opts.allow_external_plugin_detection = true;
    opts.plugin_register = NULL;

    opts.enable_tables = true;
    opts.enable_footnotes = true;
    opts.enable_definition_lists = true;
    opts.enable_smart_typography = true;
    opts.enable_math = true;
    opts.enable_critic_markup = true;
    opts.enable_wiki_links = false;  /* Disabled by default - use --wikilinks to enable */
    opts.enable_task_lists = true;
    opts.enable_attributes = true;
    opts.enable_callouts = true;
    opts.enable_py_callouts = false;
    opts.enable_quarto_callouts = false;
    opts.enable_quarto_extensions = false;
    opts.enable_quarto_raw = false;
    opts.enable_quarto_example_lists = false;
    opts.enable_quarto_line_blocks = false;
    opts.enable_quarto_roman_lists = false;
    opts.enable_quarto_code_attrs = false;
    opts.enable_quarto_diagrams = false;
    opts.enable_quarto_shortcodes = false;
    opts.enable_quarto_strict_lists = false;
    opts.enable_quarto_xrefs = false;
    opts.enable_marked_extensions = true;
    opts.enable_divs = true;  /* Enabled by default in unified mode */
    opts.enable_spans = true;  /* Enabled by default in unified mode */
    opts.enable_grid_tables = false;  /* Disabled by default - enable with --grid-tables */

    /* Critic markup mode (0=accept, 1=reject, 2=markup) */
    opts.critic_mode = 2;  /* Default: show markup */

    /* Metadata */
    opts.strip_metadata = true;
    opts.enable_metadata_variables = true;
    opts.enable_metadata_transforms = true;  /* Enabled by default in unified mode */

    /* File inclusion */
    opts.enable_file_includes = true;
    opts.max_include_depth = 10;
    opts.base_directory = NULL;

    /* Output options */
    opts.output_format = APEX_OUTPUT_HTML;  /* Default: HTML output */
    opts.unsafe = true;
    opts.validate_utf8 = true;
    opts.github_pre_lang = true;
    opts.standalone = false;
    opts.pretty = false;
    opts.xhtml = false;
    opts.strict_xhtml = false;
    opts.stylesheet_paths = NULL;
    opts.stylesheet_count = 0;
    opts.document_title = NULL;

    /* Line breaks */
    opts.hardbreaks = false;
    opts.nobreaks = false;

    /* Header IDs */
    opts.generate_header_ids = true;
    opts.header_anchors = false;  /* Use header IDs by default, not anchor tags */
    opts.id_format = 0;  /* GFM format (with dashes) */
    opts.toc_min = 1;
    opts.toc_max = 3;
    opts.toc_entries_out = NULL;
    opts.toc_entries_count_out = NULL;

    /* Table options */
    opts.relaxed_tables = true;  /* Default: enabled in unified mode (can be disabled with --no-relaxed-tables) */
    opts.caption_position = 1;  /* Default: below (1=below, 0=above) */
    opts.per_cell_alignment = true;  /* Default: enabled in unified mode (can be disabled with --no-per-cell-alignment) */

    /* List options */
    /* Since default mode is unified, enable these by default */
    opts.allow_mixed_list_markers = true;  /* Unified mode default: inherit type from first item */
    opts.allow_alpha_lists = true;  /* Unified mode default: support alpha lists */

    /* Superscript and subscript */
    opts.enable_sup_sub = true;  /* Default: enabled in unified mode */

    /* Strikethrough (GFM-style ~~text~~) */
    opts.enable_strikethrough = true;  /* Default: enabled in unified mode */

    /* Autolink options */
    opts.enable_autolink = true;  /* Default: enabled in unified mode */
    opts.obfuscate_emails = false; /* Default: plaintext emails */

    /* Image options */
    opts.embed_images = false;           /* Default: disabled */
    opts.enable_image_captions = true;   /* Default: enabled (unified mode defaults) */
    opts.title_captions_only = false;    /* Default: use title or alt for caption */

    /* Citation options */
    opts.enable_citations = false;  /* Disabled by default - enable with --bibliography */
    opts.bibliography_files = NULL;
    opts.csl_file = NULL;
    opts.suppress_bibliography = false;
    opts.link_citations = false;
    opts.show_tooltips = false;
    opts.nocite = NULL;

    /* Index options */
    opts.enable_indices = false;
    opts.enable_mmark_index_syntax = false;
    opts.enable_textindex_syntax = false;
    opts.enable_leanpub_index_syntax = false;
    opts.suppress_index = false;
    opts.group_index_by_letter = true;
    opts.concordance_files = NULL;

    /* Wiki link options */
    opts.wikilink_space = 0;  /* Default: dash (0=dash, 1=none, 2=underscore, 3=space) */
    opts.wikilink_extension = NULL;  /* Default: no extension */
    opts.wikilink_sanitize = false;  /* Default: no sanitization */

    /* Script injection options */
    opts.script_tags = NULL;

    /* Stylesheet embedding options */
    opts.embed_stylesheet = false;

    /* ARIA accessibility options */
    opts.enable_aria = false;

    /* Emoji options */
    opts.enable_emoji = true;  /* :name: replacement on by default (unified); modes may turn off */
    opts.enable_emoji_autocorrect = true;  /* Enabled by default in unified mode */

    /* Syntax highlighting options */
    opts.code_highlighter = NULL;          /* Default: no external syntax highlighting */
    opts.code_line_numbers = false;        /* Default: no line numbers */
    opts.highlight_language_only = false;  /* Default: highlight all code blocks */
    opts.code_highlight_theme = NULL;      /* Default: no explicit theme */

    /* Marked / integration-specific options (unified defaults) */
    opts.enable_widont = false;
    opts.code_is_poetry = false;
    opts.enable_markdown_in_html = true; /* Enabled by default in unified mode */
    opts.random_footnote_ids = false;
    opts.enable_hashtags = false;
    opts.style_hashtags = false;
    opts.proofreader_mode = false;
    opts.hr_page_break = false;
    opts.title_from_h1 = false;
    opts.page_break_before_footnotes = false;

    /* Source file information (used by plugins via APEX_FILE_PATH) */
    opts.input_file_path = NULL;

    /* AST filter options (Pandoc-style JSON filters) */
    opts.ast_filter_commands = NULL;
    opts.ast_filter_count = 0;
    opts.ast_filter_strict = true; /* Default: fail fast on filter errors */

    /* Progress reporting */
    opts.progress_callback = NULL;
    opts.progress_user_data = NULL;

    /* Custom cmark extension callback */
    opts.cmark_init = NULL;
    opts.cmark_done = NULL;
    opts.cmark_user_data = NULL;

    /* Terminal theme and width (for -t terminal/terminal256) */
    opts.theme_name = NULL;
    opts.terminal_width = 0;
    opts.paginate = false;
    opts.paginate_symbols = false;
    opts.terminal_inline_images = true;
    opts.terminal_image_width = 50;

    return opts;
}

/**
 * Get options configured for a specific processor mode
 */
apex_options apex_options_for_mode(apex_mode_t mode) {
    apex_options opts = apex_options_default();
    opts.mode = mode;

    switch (mode) {
        case APEX_MODE_COMMONMARK:
            /* Pure CommonMark - disable extensions */
            opts.enable_tables = false;
            opts.enable_footnotes = false;
            opts.enable_definition_lists = false;
            opts.enable_smart_typography = false;
            opts.enable_math = false;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_py_callouts = false;
            opts.enable_quarto_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_divs = false;
            opts.enable_spans = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.enable_metadata_transforms = false;
            opts.unsafe = false;  /* CommonMark mode: replace HTML comments with "raw HTML omitted" */
            opts.hardbreaks = false;
            opts.id_format = 0;  /* GFM format (default) */
            opts.relaxed_tables = false;  /* CommonMark has no tables */
            opts.per_cell_alignment = false;  /* CommonMark: no per-cell alignment */
            opts.allow_mixed_list_markers = false;  /* CommonMark: current behavior (separate lists) */
            opts.allow_alpha_lists = false;  /* CommonMark: no alpha lists */
            opts.enable_sup_sub = false;  /* CommonMark: no sup/sub */
            opts.enable_strikethrough = false;  /* CommonMark: no strikethrough */
            opts.enable_autolink = false;  /* CommonMark: no autolinks */
            opts.enable_image_captions = false; /* CommonMark: no automatic image figure captions */
            opts.enable_citations = false;  /* CommonMark: no citations */
            opts.enable_emoji = false;  /* CommonMark: no :name: emoji replacement */
            opts.enable_emoji_autocorrect = false;  /* CommonMark: no emoji autocorrect */
            /* Disable HTML markdown processing in strict CommonMark */
            opts.enable_markdown_in_html = false;
            break;

        case APEX_MODE_GFM:
            /* GFM - tables, task lists, strikethrough, autolinks */
            opts.enable_tables = true;
            opts.enable_task_lists = true;
            opts.enable_footnotes = false;
            opts.enable_definition_lists = false;
            opts.enable_smart_typography = false;
            opts.enable_math = false;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_py_callouts = false;
            opts.enable_quarto_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_divs = false;
            opts.enable_spans = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.enable_metadata_transforms = false;
            opts.unsafe = false;  /* GFM mode: replace HTML comments with "raw HTML omitted" */
            opts.hardbreaks = true;  /* GFM treats newlines as hard breaks */
            opts.id_format = 0;  /* GFM format */
            opts.relaxed_tables = false;  /* GFM uses standard table syntax */
            opts.per_cell_alignment = false;  /* GFM: no per-cell alignment */
            opts.allow_mixed_list_markers = false;  /* GFM: current behavior (separate lists) */
            opts.allow_alpha_lists = false;  /* GFM: no alpha lists */
            opts.enable_sup_sub = false;  /* GFM: no sup/sub */
            opts.enable_strikethrough = true;  /* GFM: strikethrough enabled */
            opts.enable_autolink = true;  /* GFM: autolinks enabled */
            opts.enable_image_captions = false; /* GFM: no automatic image figure captions */
            opts.enable_citations = false;  /* GFM: no citations */
            opts.enable_emoji = true;  /* GFM: :name: emoji replacement enabled */
            opts.enable_emoji_autocorrect = false;  /* GFM: no emoji autocorrect by default */
            /* Disable HTML markdown processing in GFM mode */
            opts.enable_markdown_in_html = false;
            break;

        case APEX_MODE_MULTIMARKDOWN:
            /* MultiMarkdown - metadata, footnotes, tables, etc. */
            opts.enable_tables = true;
            opts.enable_footnotes = true;
            opts.relaxed_tables = false;  /* MMD uses standard table syntax */
            opts.per_cell_alignment = false;  /* MMD: no per-cell alignment */
            opts.enable_definition_lists = true;
            opts.enable_smart_typography = true;
            opts.enable_math = true;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = false;
            opts.enable_callouts = false;
            opts.enable_py_callouts = false;
            opts.enable_quarto_callouts = false;
            opts.enable_marked_extensions = false;
            opts.enable_divs = false;
            opts.enable_spans = false;
            opts.enable_file_includes = true;
            opts.enable_metadata_variables = true;
            opts.enable_metadata_transforms = false;
            opts.hardbreaks = false;
            opts.id_format = 1;  /* MMD format */
            opts.allow_mixed_list_markers = true;  /* MultiMarkdown: inherit type from first item */
            opts.allow_alpha_lists = false;  /* MultiMarkdown: no alpha lists */
            opts.enable_citations = true;  /* MultiMarkdown: citations enabled (if bibliography provided) */
            opts.enable_sup_sub = true;  /* MultiMarkdown: support sup/sub */
            opts.enable_strikethrough = false;  /* MultiMarkdown: no strikethrough by default */
            opts.enable_autolink = true;  /* MultiMarkdown: autolinks enabled */
            opts.enable_indices = false;  /* Indices disabled by default - use --indices to enable */
            opts.enable_mmark_index_syntax = false;  /* Disabled by default - use --indices to enable */
            opts.enable_textindex_syntax = false;  /* Disabled by default - use --indices to enable */
            opts.enable_leanpub_index_syntax = false;  /* Disabled by default - use --indices to enable */
            opts.enable_emoji = false;  /* MMD: no :name: emoji replacement by default */
            opts.enable_emoji_autocorrect = false;  /* MMD: no emoji autocorrect by default */
            opts.enable_image_captions = true; /* MultiMarkdown: image captions enabled by default */
            break;

        case APEX_MODE_KRAMDOWN:
            /* Kramdown - attributes, definition lists, footnotes */
            opts.enable_tables = true;
            opts.enable_footnotes = true;
            opts.enable_definition_lists = true;
            opts.enable_smart_typography = true;
            opts.enable_math = true;
            opts.enable_critic_markup = false;
            opts.enable_wiki_links = false;
            opts.enable_task_lists = false;
            opts.enable_attributes = true;
            opts.enable_callouts = false;
            opts.enable_py_callouts = false;
            opts.enable_quarto_callouts = false;
            /* Enable Marked-style extensions (including TOC) so that
             * Kramdown documents can use <!--TOC--> and {:toc} syntax
             * for table-of-contents generation. */
            opts.enable_marked_extensions = true;
            opts.enable_divs = false;
            opts.enable_spans = false;
            opts.enable_file_includes = false;
            opts.enable_metadata_variables = false;
            opts.enable_metadata_transforms = false;
            opts.hardbreaks = false;
            opts.id_format = 2;  /* Kramdown format */
            opts.relaxed_tables = true;  /* Kramdown supports relaxed tables */
            opts.per_cell_alignment = false;  /* Kramdown: no per-cell alignment */
            opts.allow_mixed_list_markers = false;  /* Kramdown: current behavior (separate lists) */
            opts.allow_alpha_lists = false;  /* Kramdown: no alpha lists */
            opts.enable_sup_sub = false;  /* Kramdown: no sup/sub */
            opts.enable_strikethrough = false;  /* Kramdown: no strikethrough by default */
            opts.enable_autolink = true;  /* Kramdown: autolinks enabled */
            opts.enable_citations = false;  /* Kramdown: no citations (different system) */
            opts.enable_emoji = false;  /* Kramdown: no :name: emoji replacement by default */
            opts.enable_emoji_autocorrect = false;  /* Kramdown: no emoji autocorrect by default */
            opts.enable_image_captions = false; /* Kramdown: no automatic image figure captions */
            break;

        case APEX_MODE_UNIFIED:
            /* All features enabled - already the default */
            /* Unified mode should have everything on */
            opts.enable_wiki_links = false;  /* Disabled by default - use --wikilinks to enable */
            opts.enable_math = true;
            opts.id_format = 0;  /* GFM format (default, can be overridden with --id-format) */
            opts.relaxed_tables = true;  /* Unified mode supports relaxed tables */
            opts.per_cell_alignment = true;  /* Unified mode: per-cell alignment enabled by default */
            opts.allow_mixed_list_markers = true;  /* Unified: inherit type from first item */
            opts.allow_alpha_lists = true;  /* Unified: support alpha lists */
            opts.enable_sup_sub = true;  /* Unified: support sup/sub (default: true) */
            opts.enable_strikethrough = true;  /* Unified: strikethrough enabled (default: true) */
            opts.unsafe = true;  /* Unified mode: allow raw HTML by default */
            opts.enable_citations = true;  /* Unified: citations enabled (if bibliography provided) */
            opts.enable_indices = true;  /* Unified: indices enabled */
            opts.enable_mmark_index_syntax = true;  /* Unified: mmark index syntax */
            opts.enable_textindex_syntax = true;  /* Unified: TextIndex syntax enabled */
            opts.enable_leanpub_index_syntax = true;  /* Unified: Leanpub index syntax enabled */
            opts.enable_py_callouts = false;  /* Unified default: off unless explicitly enabled */
            opts.enable_quarto_callouts = false;  /* Unified default: off unless explicitly enabled */
            opts.enable_divs = true;  /* Unified: Pandoc fenced divs enabled */
            opts.enable_spans = true;  /* Unified: bracketed spans enabled */
            opts.enable_emoji = true;  /* Unified: :name: emoji replacement enabled */
            opts.enable_emoji_autocorrect = true;  /* Unified: emoji autocorrect enabled */
            opts.enable_image_captions = true;     /* Unified: image captions enabled by default */
            break;

        case APEX_MODE_QUARTO:
            /* Pandoc/Quarto markdown: unified-family defaults tuned for Quarto docs */
            opts = apex_options_for_mode(APEX_MODE_UNIFIED);
            opts.mode = APEX_MODE_QUARTO;
            opts.enable_quarto_extensions = true;
            opts.enable_quarto_raw = true;
            opts.enable_quarto_example_lists = true;
            opts.enable_quarto_line_blocks = true;
            opts.enable_quarto_roman_lists = true;
            opts.enable_quarto_code_attrs = true;
            opts.enable_quarto_diagrams = true;
            opts.enable_quarto_shortcodes = true;
            opts.enable_quarto_xrefs = true;
            opts.enable_quarto_strict_lists = false;
            opts.enable_quarto_callouts = true;
            opts.enable_wiki_links = false;
            opts.enable_marked_extensions = false;
            opts.enable_py_callouts = false;
            opts.enable_indices = false;
            opts.enable_mmark_index_syntax = false;
            opts.enable_textindex_syntax = false;
            opts.enable_leanpub_index_syntax = false;
            break;
    }

    return opts;
}

bool apex_options_apply_meta_file(apex_options *options, const char *path) {
    if (!options || !path || !path[0]) {
        return false;
    }

    apex_options original_options = *options;
    apex_metadata_item *metadata = apex_load_metadata_from_file(path);
    if (!metadata) {
        return false;
    }

    apex_apply_metadata_to_options(metadata, options);

    bool owns_csl_file = options->csl_file &&
                         options->csl_file != original_options.csl_file;
    bool owns_document_title = options->document_title &&
                               options->document_title != original_options.document_title;
    bool owns_base_directory = options->base_directory &&
                               options->base_directory != original_options.base_directory;
    bool owns_wikilink_extension = options->wikilink_extension &&
                                   options->wikilink_extension != original_options.wikilink_extension;
    bool owns_theme_name = options->theme_name &&
                           options->theme_name != original_options.theme_name;
    bool owns_code_highlight_theme = options->code_highlight_theme &&
                                     options->code_highlight_theme != original_options.code_highlight_theme;
    bool owns_stylesheet_paths = options->stylesheet_paths &&
                                 options->stylesheet_paths != original_options.stylesheet_paths;

    char *csl_file = owns_csl_file ? strdup(options->csl_file) : NULL;
    char *document_title = owns_document_title ? strdup(options->document_title) : NULL;
    char *base_directory = owns_base_directory ? strdup(options->base_directory) : NULL;
    char *wikilink_extension = owns_wikilink_extension ? strdup(options->wikilink_extension) : NULL;
    char *theme_name = owns_theme_name ? strdup(options->theme_name) : NULL;
    char *code_highlight_theme = owns_code_highlight_theme ? strdup(options->code_highlight_theme) : NULL;
    const char **stylesheet_paths = NULL;
    bool copy_failed = (owns_csl_file && !csl_file) ||
                       (owns_document_title && !document_title) ||
                       (owns_base_directory && !base_directory) ||
                       (owns_wikilink_extension && !wikilink_extension) ||
                       (owns_theme_name && !theme_name) ||
                       (owns_code_highlight_theme && !code_highlight_theme);

    if (owns_stylesheet_paths) {
        stylesheet_paths = calloc(options->stylesheet_count + 1, sizeof(*stylesheet_paths));
        if (!stylesheet_paths) {
            copy_failed = true;
        } else {
            for (size_t i = 0; i < options->stylesheet_count; i++) {
                if (options->stylesheet_paths[i]) {
                    stylesheet_paths[i] = strdup(options->stylesheet_paths[i]);
                    if (!stylesheet_paths[i]) {
                        copy_failed = true;
                        break;
                    }
                }
            }
        }
    }

    if (copy_failed) {
        free(csl_file);
        free(document_title);
        free(base_directory);
        free(wikilink_extension);
        free(theme_name);
        free(code_highlight_theme);
        if (stylesheet_paths) {
            for (size_t i = 0; i < options->stylesheet_count; i++) {
                free((void *)stylesheet_paths[i]);
            }
            free(stylesheet_paths);
        }
        if (owns_stylesheet_paths) {
            free(options->stylesheet_paths);
        }
        apex_free_metadata(metadata);
        *options = original_options;
        return false;
    }

    if (owns_stylesheet_paths) {
        free(options->stylesheet_paths);
    }
    apex_free_metadata(metadata);

    if (owns_csl_file) options->csl_file = csl_file;
    if (owns_document_title) options->document_title = document_title;
    if (owns_base_directory) options->base_directory = base_directory;
    if (owns_wikilink_extension) options->wikilink_extension = wikilink_extension;
    if (owns_theme_name) options->theme_name = theme_name;
    if (owns_code_highlight_theme) options->code_highlight_theme = code_highlight_theme;
    if (owns_stylesheet_paths) options->stylesheet_paths = stylesheet_paths;
    return true;
}

static bool apex_quarto_feature(const apex_options *options, bool feature_enabled) {
    return feature_enabled &&
           (options->enable_quarto_extensions || options->mode == APEX_MODE_QUARTO);
}

/**
 * Convert cmark-gfm option flags based on Apex options
 */
static int apex_to_cmark_options(const apex_options *options) {
    int cmark_opts = CMARK_OPT_DEFAULT;

    if (options->validate_utf8) {
        cmark_opts |= CMARK_OPT_VALIDATE_UTF8;
    }

    if (options->unsafe) {
        cmark_opts |= CMARK_OPT_UNSAFE;
        /* Also enable liberal HTML tag interpretation to prevent encoding of inline HTML tags */
        cmark_opts |= CMARK_OPT_LIBERAL_HTML_TAG;
    }

    if (options->hardbreaks) {
        cmark_opts |= CMARK_OPT_HARDBREAKS;
    }

    if (options->nobreaks) {
        cmark_opts |= CMARK_OPT_NOBREAKS;
    }

    if (options->github_pre_lang) {
        cmark_opts |= CMARK_OPT_GITHUB_PRE_LANG;
    }

    if (options->enable_footnotes) {
        cmark_opts |= CMARK_OPT_FOOTNOTES;
    }

    if (options->enable_smart_typography) {
        cmark_opts |= CMARK_OPT_SMART;
    }

    /* Table style preference (use CSS classes instead of inline styles) */
    if (options->enable_tables) {
        /* Tables are handled via extension registration, not options */
        /* We could add CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES here if needed */
    }

    return cmark_opts;
}

/* One-time cmark-gfm core extension registration. Not thread-safe on its own. */
void apex_ensure_cmark_extensions(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, cmark_gfm_core_extensions_ensure_registered);
}

/**
 * Register cmark-gfm extensions based on Apex options
 */
static void apex_register_extensions(cmark_parser *parser, const apex_options *options) {
    /* Ensure core extensions are registered */
    apex_ensure_cmark_extensions();

    /* Note: Metadata is handled via preprocessing, not as an extension */

    /* Add GFM extensions as needed */
    if (options->enable_tables) {
        cmark_syntax_extension *table_ext = cmark_find_syntax_extension("table");
        if (table_ext) {
            cmark_parser_attach_syntax_extension(parser, table_ext);
        }
    }

    if (options->enable_task_lists) {
        cmark_syntax_extension *tasklist_ext = cmark_find_syntax_extension("tasklist");
        if (tasklist_ext) {
            cmark_parser_attach_syntax_extension(parser, tasklist_ext);
        }
    }

    /* GFM strikethrough (~~text~~) - controlled by enable_strikethrough option */
    if (options->enable_strikethrough) {
        cmark_syntax_extension *strike_ext = cmark_find_syntax_extension("strikethrough");
        if (strike_ext) {
            cmark_parser_attach_syntax_extension(parser, strike_ext);
        }
    }

    /* GFM autolink - enable for GFM and Unified modes if autolink is enabled */
    if (options->enable_autolink && (options->mode == APEX_MODE_GFM || apex_mode_is_unified_family(options->mode))) {
        cmark_syntax_extension *autolink_ext = cmark_find_syntax_extension("autolink");
        if (autolink_ext) {
            cmark_parser_attach_syntax_extension(parser, autolink_ext);
        }
    }

    /* Tag filter (GFM security)
     * In Unified mode we allow raw HTML/autolinks, so only enable in GFM.
     */
    if (options->mode == APEX_MODE_GFM) {
        cmark_syntax_extension *tagfilter_ext = cmark_find_syntax_extension("tagfilter");
        if (tagfilter_ext) {
            cmark_parser_attach_syntax_extension(parser, tagfilter_ext);
        }
    }

    /* Note: Wiki links are handled via postprocessing, not as an extension */

    /* Math support (LaTeX) */
    if (options->enable_math) {
        cmark_syntax_extension *math_ext = create_math_extension();
        if (math_ext) {
            cmark_parser_attach_syntax_extension(parser, math_ext);
        }
    }

    /* Definition lists (one-line format: Term :: Definition) - handled by preprocessing only */

    /* Advanced footnotes (block-level content support) */
    if (options->enable_footnotes) {
        cmark_syntax_extension *adv_footnotes_ext = create_advanced_footnotes_extension();
        if (adv_footnotes_ext) {
            cmark_parser_attach_syntax_extension(parser, adv_footnotes_ext);
        }
    }

    /* Advanced tables (colspan, rowspan, captions) */
    if (options->enable_tables) {
        cmark_syntax_extension *adv_tables_ext = create_advanced_tables_extension(options->per_cell_alignment);
        if (adv_tables_ext) {
            cmark_parser_attach_syntax_extension(parser, adv_tables_ext);
        }
    }
}

/**
 * Main conversion function using cmark-gfm
 */
/* Profiling helpers */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static bool profiling_enabled(void) {
    const char *env = getenv("APEX_PROFILE");
    return env && (strcmp(env, "1") == 0 || strcmp(env, "yes") == 0 || strcmp(env, "true") == 0);
}

#define PROFILE_START(name) \
    double name##_start = 0; \
    if (profiling_enabled()) { name##_start = get_time_ms(); }

#define PROFILE_END(name) \
    if (profiling_enabled()) { \
        double name##_elapsed = get_time_ms() - name##_start; \
        fprintf(stderr, "[PROFILE] %-30s: %8.2f ms\n", #name, name##_elapsed); \
    }

/* Progress reporting helper macro */
#define PROGRESS_REPORT(stage, percent) \
    do { \
        if (options && options->progress_callback) { \
            options->progress_callback(stage, percent, options->progress_user_data); \
        } \
    } while (0)

/**
 * Add 'poetry' class to code blocks without a language class.
 * Handles both fenced code blocks (<pre><code class="language-X">) and
 * indented code blocks (<pre><code>).
 * Returns a newly allocated string, or NULL on error.
 */
static char *apex_add_poetry_class_to_code_blocks(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    size_t capacity = len * 2 + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for <pre><code pattern */
        const char *pre_code = strstr(read, "<pre><code");
        if (!pre_code) {
            /* Copy the rest */
            size_t tail_len = strlen(read);
            if (tail_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tail_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, tail_len);
            write += tail_len;
            remaining -= tail_len;
            break;
        }

        /* Copy up to the <pre><code */
        size_t chunk_len = (size_t)(pre_code - read);
        if (chunk_len >= remaining) {
            size_t written = (size_t)(write - output);
            capacity = (written + chunk_len + 100) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }
        memcpy(write, read, chunk_len);
        write += chunk_len;
        remaining -= chunk_len;

        /* Check if this code block has a language class */
        const char *code_tag = pre_code + 9;  /* Skip "<pre><code" */
        bool has_language = false;

        /* Look for class="language- or class='language- */
        const char *class_pos = strstr(code_tag, "class=");
        if (class_pos) {
            /* Check if it's a language class */
            const char *lang_check = strstr(class_pos, "language-");
            if (lang_check) {
                has_language = true;
            }
        }

        /* Find the closing > of the <code> tag */
        const char *code_end = strchr(code_tag, '>');
        if (!code_end) {
            /* Malformed, copy rest as-is */
            size_t tail_len = strlen(pre_code);
            if (tail_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tail_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, pre_code, tail_len);
            write += tail_len;
            remaining -= tail_len;
            break;
        }

        if (!has_language) {
            /* No language class, add poetry class */
            /* Copy <pre><code */
            size_t prefix_len = (size_t)(code_end - pre_code);
            if (prefix_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + prefix_len + 20) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, pre_code, prefix_len);
            write += prefix_len;
            remaining -= prefix_len;

            /* Add class="poetry" */
            const char *poetry_class = " class=\"poetry\"";
            size_t poetry_len = strlen(poetry_class);
            if (poetry_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + poetry_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, poetry_class, poetry_len);
            write += poetry_len;
            remaining -= poetry_len;

            /* Copy the closing > */
            if (remaining < 10) {
                size_t written = (size_t)(write - output);
                capacity = (written + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            *write++ = '>';
            remaining--;
            read = code_end + 1;
        } else {
            /* Has language class, copy as-is */
            size_t tag_len = (size_t)(code_end + 1 - pre_code);
            if (tag_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tag_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, pre_code, tag_len);
            write += tag_len;
            remaining -= tag_len;
            read = code_end + 1;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Extract text from the first H1 heading in HTML (strips HTML tags).
 * Returns a newly allocated string, or NULL if no H1 found or on error.
 * Caller must free the returned string.
 */
static char *apex_extract_first_h1_text(const char *html) {
    if (!html) return NULL;

    /* Find first <h1> tag */
    const char *h1_start = strstr(html, "<h1");
    if (!h1_start) return NULL;

    /* Find the closing > of the opening tag */
    const char *tag_end = strchr(h1_start, '>');
    if (!tag_end) return NULL;

    /* Find the closing </h1> tag */
    const char *h1_close = strstr(tag_end + 1, "</h1>");
    if (!h1_close) return NULL;

    /* Extract text between tags, stripping HTML */
    size_t text_len = (size_t)(h1_close - tag_end - 1);
    char *text = malloc(text_len + 1);
    if (!text) return NULL;

    const char *read = tag_end + 1;
    char *write = text;
    bool in_tag = false;
    size_t written = 0;

    while (read < h1_close && written < text_len) {
        if (*read == '<') {
            in_tag = true;
        } else if (*read == '>') {
            in_tag = false;
        } else if (!in_tag) {
            *write++ = *read;
            written++;
        }
        read++;
    }
    *write = '\0';

    /* Trim whitespace */
    while (written > 0 && (text[written - 1] == ' ' || text[written - 1] == '\t' || text[written - 1] == '\n' || text[written - 1] == '\r')) {
        text[--written] = '\0';
    }
    char *start = text;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    if (strlen(text) == 0) {
        free(text);
        return NULL;
    }

    return text;
}

/**
 * Compute a simple 8-character hexadecimal hash from document content.
 * Uses djb2 hash algorithm. Returns a newly allocated string.
 */
static char *apex_compute_document_hash(const char *content, size_t len) {
    if (!content || len == 0) return NULL;

    /* djb2 hash */
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)content[i]; /* hash * 33 + c */
    }

    /* Convert to 8-character hex string */
    char *hash_str = malloc(9);
    if (!hash_str) return NULL;
    snprintf(hash_str, 9, "%08lx", hash);
    return hash_str;
}

/**
 * Add hash prefix to footnote IDs to avoid collisions when combining documents.
 * Returns a newly allocated string, or NULL on error.
 */
static char *apex_add_hash_to_footnote_ids(const char *html, const char *hash_prefix) {
    if (!html || !hash_prefix) return NULL;

    size_t len = strlen(html);
    size_t hash_len = strlen(hash_prefix);
    /* Allow for expansion: each fn- or fnref- gets hash- prefix added */
    size_t capacity = len + (hash_len + 1) * 100 + 1;  /* Estimate 100 footnotes max */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for id="fn- or id="fnref- */
        if (strncmp(read, "id=\"fn-", 7) == 0) {
            /* Copy up to fn- */
            size_t prefix_len = 7;  /* "id=\"fn-" */
            if (prefix_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + prefix_len + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, prefix_len);
            write += prefix_len;
            remaining -= prefix_len;
            read += prefix_len;

            /* Insert hash prefix */
            if (hash_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, hash_prefix, hash_len);
            write += hash_len;
            remaining -= hash_len;
            *write++ = '-';
            remaining--;
        } else if (strncmp(read, "id=\"fnref-", 10) == 0) {
            /* Copy up to fnref- */
            size_t prefix_len = 10;  /* "id=\"fnref-" */
            if (prefix_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + prefix_len + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, prefix_len);
            write += prefix_len;
            remaining -= prefix_len;
            read += prefix_len;

            /* Insert hash prefix */
            if (hash_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, hash_prefix, hash_len);
            write += hash_len;
            remaining -= hash_len;
            *write++ = '-';
            remaining--;
        } else if (strncmp(read, "href=\"#fn-", 10) == 0) {
            /* Copy up to #fn- */
            size_t prefix_len = 10;  /* "href=\"#fn-" */
            if (prefix_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + prefix_len + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, prefix_len);
            write += prefix_len;
            remaining -= prefix_len;
            read += prefix_len;

            /* Insert hash prefix */
            if (hash_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, hash_prefix, hash_len);
            write += hash_len;
            remaining -= hash_len;
            *write++ = '-';
            remaining--;
        } else if (strncmp(read, "href=\"#fnref-", 13) == 0) {
            /* Copy up to #fnref- */
            size_t prefix_len = 13;  /* "href=\"#fnref-" */
            if (prefix_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + prefix_len + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, prefix_len);
            write += prefix_len;
            remaining -= prefix_len;
            read += prefix_len;

            /* Insert hash prefix */
            if (hash_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + hash_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, hash_prefix, hash_len);
            write += hash_len;
            remaining -= hash_len;
            *write++ = '-';
            remaining--;
        } else {
            /* Normal character, copy as-is */
            if (remaining < 10) {
                size_t written = (size_t)(write - output);
                capacity = (written + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Replace <hr> elements in HTML with Marked-style page break divs.
 * Returns a newly allocated string, or NULL on error.
 */
static char *apex_replace_hr_with_pagebreak(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    /* Page break divs are significantly longer than <hr>, allow generous expansion */
    size_t capacity = len * 4 + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    const char *replacement =
        "<div class=\"mkpagebreak manualbreak\" "
        "title=\"Page break created from HR\" "
        "data-description=\"PAGE (HR)\" "
        "style=\"page-break-after:always\">"
        "<span style=\"display:none\">&nbsp;</span></div>";
    size_t repl_len = strlen(replacement);

    while (*read) {
        const char *hr = strstr(read, "<hr");
        if (!hr) {
            /* Copy the rest */
            size_t tail_len = strlen(read);
            if (tail_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tail_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, read, tail_len);
            write += tail_len;
            remaining -= tail_len;
            break;
        }

        /* Copy up to the <hr */
        size_t chunk_len = (size_t)(hr - read);
        if (chunk_len >= remaining) {
            size_t written = (size_t)(write - output);
            capacity = (written + chunk_len + repl_len + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }
        memcpy(write, read, chunk_len);
        write += chunk_len;
        remaining -= chunk_len;

        /* Find end of the <hr ...> tag */
        const char *tag_end = strchr(hr, '>');
        if (!tag_end) {
            /* Malformed tag, copy the rest as-is */
            size_t tail_len = strlen(hr);
            if (tail_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tail_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, hr, tail_len);
            write += tail_len;
            remaining -= tail_len;
            break;
        }

        /* Skip the entire <hr ...> tag */
        read = tag_end + 1;

        /* Write replacement */
        if (repl_len >= remaining) {
            size_t written = (size_t)(write - output);
            capacity = (written + repl_len + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
            write = output + written;
            remaining = capacity - written;
        }
        memcpy(write, replacement, repl_len);
        write += repl_len;
        remaining -= repl_len;
    }

    *write = '\0';
    return output;
}

/**
 * Apply widont to headings: replace spaces with &nbsp; between trailing words
 * when their combined length (including spaces) is <= 10 characters.
 * Returns a newly allocated string, or NULL on error.
 */
static char *apex_apply_widont_to_headings(const char *html) {
    if (!html) return NULL;

    size_t len = strlen(html);
    size_t capacity = len * 2 + 1;  /* Allow for expansion with &nbsp; */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for heading tags: <h1>, <h2>, ..., <h6> */
        if (read[0] == '<' && read[1] == 'h' && read[2] >= '1' && read[2] <= '6') {
            const char *tag_start = read;
            int level = read[2] - '0';

            /* Find the closing > of the opening tag (may have attributes) */
            const char *tag_end = strchr(tag_start, '>');
            if (!tag_end) {
                /* Malformed tag, copy rest as-is */
                size_t tail_len = strlen(read);
                if (tail_len >= remaining) {
                    size_t written = (size_t)(write - output);
                    capacity = (written + tail_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }
                memcpy(write, read, tail_len);
                write += tail_len;
                remaining -= tail_len;
                break;
            }

            /* Copy the opening tag */
            size_t tag_len = (size_t)(tag_end + 1 - tag_start);
            if (tag_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + tag_len + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, tag_start, tag_len);
            write += tag_len;
            remaining -= tag_len;

            read = tag_end + 1;

            /* Find the closing </hN> tag */
            char close_tag[6];
            snprintf(close_tag, sizeof(close_tag), "</h%d>", level);
            const char *close_pos = strstr(read, close_tag);
            if (!close_pos) {
                /* No closing tag found, copy rest as-is */
                size_t tail_len = strlen(read);
                if (tail_len >= remaining) {
                    size_t written = (size_t)(write - output);
                    capacity = (written + tail_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }
                memcpy(write, read, tail_len);
                write += tail_len;
                remaining -= tail_len;
                break;
            }

            /* Extract heading text (between opening and closing tags) */
            size_t heading_text_len = (size_t)(close_pos - read);
            if (heading_text_len > 0) {
                /* Extract plain text (ignoring HTML tags) to calculate word lengths */
                char *plain_text = malloc(heading_text_len + 1);
                if (!plain_text) {
                    free(output);
                    return NULL;
                }
                const char *p = read;
                char *pt_write = plain_text;
                bool in_tag = false;
                while (p < close_pos) {
                    if (*p == '<') {
                        in_tag = true;
                    } else if (*p == '>') {
                        in_tag = false;
                    } else if (!in_tag) {
                        *pt_write++ = *p;
                    }
                    p++;
                }
                *pt_write = '\0';
                size_t plain_len = (size_t)(pt_write - plain_text);

                if (plain_len > 0) {
                    /* Work backwards to find trailing words that need widont.
                     * We want to include words until the combined length > 10,
                     * so that the trailing portion won't be a short widow (<= 10 chars).
                     */
                    size_t word_end = plain_len;
                    size_t word_start = plain_len;
                    size_t combined_len = 0;
                    size_t first_word_start = plain_len;

                    /* Find words from the end */
                    for (size_t i = plain_len; i > 0; i--) {
                        char c = plain_text[i - 1];
                        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                            if (word_start < plain_len) {
                                /* Found a word */
                                size_t word_len = word_end - word_start;
                                if (combined_len == 0) {
                                    /* First word from the end */
                                    first_word_start = word_start;
                                    combined_len = word_len;
                                } else {
                                    /* Add this word to the trailing group */
                                    size_t new_combined = combined_len + 1 + word_len;  /* +1 for space */
                                    combined_len = new_combined;
                                    first_word_start = word_start;
                                    /* If we've exceeded 10 chars, we're done - we have enough to prevent a short widow */
                                    if (new_combined > 10) {
                                        break;  /* Stop here */
                                    }
                                }
                                word_end = i - 1;
                                word_start = i - 1;
                            } else {
                                word_end = i - 1;
                                word_start = i - 1;
                            }
                        } else {
                            if (word_start == plain_len) {
                                word_end = i;
                            }
                            word_start = i - 1;
                        }
                    }

                    /* Handle the first word if we haven't processed it yet */
                    if (word_start < plain_len && combined_len == 0) {
                        size_t word_len = word_end - word_start;
                        first_word_start = word_start;
                        combined_len = word_len;
                    } else if (word_start < plain_len && combined_len > 0 && combined_len <= 10) {
                        /* If we haven't exceeded 10 yet, try to add the first word */
                        size_t word_len = word_end - word_start;
                        size_t new_combined = combined_len + 1 + word_len;
                        combined_len = new_combined;
                        first_word_start = word_start;
                    }

                    /* If we found words to combine (combined_len > 0 and first_word_start < plain_len)
                     * Apply widont if:
                     * 1. The trailing portion is <= 10 chars (needs protection from short widow)
                     * 2. OR we've included enough words to exceed 10 (the algorithm includes the word
                     *    that pushes us over 10, so combined_len > 10 means we have a protected trailing portion) */
                    if (combined_len > 0 && first_word_start < plain_len) {
                        /* Map plain text position back to HTML position */
                        /* We need to find where first_word_start corresponds in the HTML */
                        size_t plain_pos = 0;
                        size_t html_pos = 0;
                        bool in_html_tag = false;
                        size_t trailing_start_html = 0;

                        /* Find the HTML position corresponding to first_word_start in plain text */
                        const char *html_p = read;
                        while (html_p < close_pos && plain_pos < first_word_start) {
                            if (*html_p == '<') {
                                in_html_tag = true;
                            } else if (*html_p == '>') {
                                in_html_tag = false;
                            } else if (!in_html_tag) {
                                plain_pos++;
                            }
                            html_p++;
                            html_pos++;
                        }
                        trailing_start_html = html_pos;

                        /* Now copy heading text, replacing spaces with &nbsp; in trailing section */
                        const char *html_p2 = read;
                        size_t html_pos2 = 0;
                        bool in_tag2 = false;
                        while (html_p2 < close_pos) {
                            char c = *html_p2;
                            if (c == '<') {
                                in_tag2 = true;
                                if (remaining < 10) {
                                    size_t written = (size_t)(write - output);
                                    capacity = (written + 100) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(plain_text);
                                        free(output);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }
                                *write++ = c;
                                remaining--;
                            } else if (c == '>') {
                                in_tag2 = false;
                                if (remaining < 10) {
                                    size_t written = (size_t)(write - output);
                                    capacity = (written + 100) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(plain_text);
                                        free(output);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }
                                *write++ = c;
                                remaining--;
                            } else if (in_tag2) {
                                if (remaining < 10) {
                                    size_t written = (size_t)(write - output);
                                    capacity = (written + 100) * 2;
                                    char *new_output = realloc(output, capacity);
                                    if (!new_output) {
                                        free(plain_text);
                                        free(output);
                                        return NULL;
                                    }
                                    output = new_output;
                                    write = output + written;
                                    remaining = capacity - written;
                                }
                                *write++ = c;
                                remaining--;
                            } else {
                                /* Not in HTML tag */
                                if (html_pos2 >= trailing_start_html) {
                                    /* We're in the trailing section */
                                    if (c == ' ' || c == '\t') {
                                        /* Replace with &nbsp; */
                                        const char *nbsp = "&nbsp;";
                                        size_t nbsp_len = strlen(nbsp);
                                        if (nbsp_len >= remaining) {
                                            size_t written = (size_t)(write - output);
                                            capacity = (written + nbsp_len + 100) * 2;
                                            char *new_output = realloc(output, capacity);
                                            if (!new_output) {
                                                free(plain_text);
                                                free(output);
                                                return NULL;
                                            }
                                            output = new_output;
                                            write = output + written;
                                            remaining = capacity - written;
                                        }
                                        memcpy(write, nbsp, nbsp_len);
                                        write += nbsp_len;
                                        remaining -= nbsp_len;
                                    } else {
                                        if (remaining < 10) {
                                            size_t written = (size_t)(write - output);
                                            capacity = (written + 100) * 2;
                                            char *new_output = realloc(output, capacity);
                                            if (!new_output) {
                                                free(plain_text);
                                                free(output);
                                                return NULL;
                                            }
                                            output = new_output;
                                            write = output + written;
                                            remaining = capacity - written;
                                        }
                                        *write++ = c;
                                        remaining--;
                                    }
                                } else {
                                    /* Not in trailing section, copy as-is */
                                    if (remaining < 10) {
                                        size_t written = (size_t)(write - output);
                                        capacity = (written + 100) * 2;
                                        char *new_output = realloc(output, capacity);
                                        if (!new_output) {
                                            free(plain_text);
                                            free(output);
                                            return NULL;
                                        }
                                        output = new_output;
                                        write = output + written;
                                        remaining = capacity - written;
                                    }
                                    *write++ = c;
                                    remaining--;
                                }
                                html_pos2++;
                            }
                            html_p2++;
                        }
                        free(plain_text);
                    } else {
                        /* No widont needed, copy heading text as-is */
                        if (heading_text_len >= remaining) {
                            size_t written = (size_t)(write - output);
                            capacity = (written + heading_text_len + 1) * 2;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(plain_text);
                                free(output);
                                return NULL;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        memcpy(write, read, heading_text_len);
                        write += heading_text_len;
                        remaining -= heading_text_len;
                        free(plain_text);
                    }
                } else {
                    /* Empty plain text, copy heading as-is */
                    if (heading_text_len >= remaining) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + heading_text_len + 1) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(plain_text);
                            free(output);
                            return NULL;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    memcpy(write, read, heading_text_len);
                    write += heading_text_len;
                    remaining -= heading_text_len;
                    free(plain_text);
                }
            } else {
                /* Empty heading, copy as-is */
                if (heading_text_len >= remaining) {
                    size_t written = (size_t)(write - output);
                    capacity = (written + heading_text_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + written;
                    remaining = capacity - written;
                }
                memcpy(write, read, heading_text_len);
                write += heading_text_len;
                remaining -= heading_text_len;
            }

            /* Copy the closing tag */
            size_t close_tag_len = strlen(close_tag);
            if (close_tag_len >= remaining) {
                size_t written = (size_t)(write - output);
                capacity = (written + close_tag_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
            }
            memcpy(write, close_tag, close_tag_len);
            write += close_tag_len;
            remaining -= close_tag_len;
            read = close_pos + close_tag_len;
        } else {
            /* Not a heading tag, copy character */
            if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                size_t written = (size_t)(write - output);
                capacity = (written + 100) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + written;
                remaining = capacity - written;
                *write++ = *read++;
                remaining--;
            }
        }
    }

    *write = '\0';
    return output;
}

static const char *apex_find_unquoted_gt(const char *p, const char *end) {
    int quote = 0;
    while (p < end) {
        if (quote) {
            if (*p == quote) quote = 0;
            p++;
            continue;
        }
        if (*p == '"' || *p == '\'') {
            quote = *p;
            p++;
            continue;
        }
        if (*p == '>') return p;
        p++;
    }
    return NULL;
}

static bool apex_html_void_element_name(const char *name, size_t nlen) {
    static const char *void_tags[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    for (size_t t = 0; t < sizeof(void_tags) / sizeof(void_tags[0]); t++) {
        const char *v = void_tags[t];
        size_t vl = strlen(v);
        if (vl != nlen) continue;
        size_t j;
        for (j = 0; j < nlen; j++) {
            if (tolower((unsigned char)name[j]) != tolower((unsigned char)v[j])) break;
        }
        if (j == nlen) return true;
    }
    return false;
}

static int apex_html_buf_append(char **outp, size_t *cap, size_t *olen, const char *s, size_t n) {
    while (*olen + n + 1 > *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8192;
        char *nbuf = realloc(*outp, new_cap);
        if (!nbuf) return -1;
        *outp = nbuf;
        *cap = new_cap;
    }
    memcpy(*outp + *olen, s, n);
    *olen += n;
    (*outp)[*olen] = '\0';
    return 0;
}

/**
 * Rewrite HTML void/empty elements to XML self-closing form (e.g. <br> -> <br />).
 * Skips contents of script, style, and HTML comments. Returns newly allocated string or NULL.
 */
static char *apex_html_apply_xhtml_void_tags(const char *html) {
    if (!html) return NULL;

    size_t cap = strlen(html) * 2 + 256;
    if (cap < 8192) cap = 8192;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t olen = 0;

    const char *r = html;
    const char *end = html + strlen(html);

    while (r < end) {
        if (*r != '<') {
            if (apex_html_buf_append(&out, &cap, &olen, r, 1) != 0) goto fail;
            r++;
            continue;
        }

        /* Comment */
        if (r + 4 <= end && strncmp(r, "<!--", 4) == 0) {
            const char *ce = strstr(r + 4, "-->");
            if (!ce) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(ce + 3 - r)) != 0) goto fail;
            r = ce + 3;
            continue;
        }

        /* CDATA */
        if (r + 9 <= end && strncmp(r, "<![CDATA[", 9) == 0) {
            const char *ce = strstr(r + 9, "]]>");
            if (!ce) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(ce + 3 - r)) != 0) goto fail;
            r = ce + 3;
            continue;
        }

        /* script */
        if (r + 7 <= end && strncasecmp(r, "<script", 7) == 0) {
            const char *close = strcasestr(r + 7, "</script>");
            if (!close) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(close + 9 - r)) != 0) goto fail;
            r = close + 9;
            continue;
        }

        /* style */
        if (r + 6 <= end && strncasecmp(r, "<style", 6) == 0) {
            const char *close = strcasestr(r + 6, "</style>");
            if (!close) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(close + 8 - r)) != 0) goto fail;
            r = close + 8;
            continue;
        }

        /* Declaration <!...> */
        if (r + 1 < end && r[1] == '!') {
            const char *gt = apex_find_unquoted_gt(r, end);
            if (!gt) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(gt + 1 - r)) != 0) goto fail;
            r = gt + 1;
            continue;
        }

        /* Closing tag */
        if (r + 1 < end && r[1] == '/') {
            const char *gt = apex_find_unquoted_gt(r, end);
            if (!gt) {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
                break;
            }
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(gt + 1 - r)) != 0) goto fail;
            r = gt + 1;
            continue;
        }

        /* Opening tag: extract name */
        const char *name_start = r + 1;
        while (name_start < end && isspace((unsigned char)*name_start)) name_start++;
        const char *name_end = name_start;
        while (name_end < end && (isalnum((unsigned char)*name_end) || *name_end == '-' || *name_end == '_' || *name_end == ':')) {
            name_end++;
        }
        size_t name_len = (size_t)(name_end - name_start);

        const char *gt = apex_find_unquoted_gt(r, end);
        if (!gt) {
            if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(end - r)) != 0) goto fail;
            break;
        }

        if (name_len > 0 && apex_html_void_element_name(name_start, name_len)) {
            const char *slash = gt - 1;
            while (slash > r && isspace((unsigned char)*slash)) slash--;
            if (slash >= r && *slash == '/') {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(gt + 1 - r)) != 0) goto fail;
            } else {
                if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(gt - r)) != 0) goto fail;
                if (apex_html_buf_append(&out, &cap, &olen, " />", 3) != 0) goto fail;
            }
            r = gt + 1;
            continue;
        }

        if (apex_html_buf_append(&out, &cap, &olen, r, (size_t)(gt + 1 - r)) != 0) goto fail;
        r = gt + 1;
    }

    return out;
fail:
    free(out);
    return NULL;
}

apex_toc_entry *apex_markdown_to_toc_entries(const char *markdown, size_t len,
                                             const apex_options *options,
                                             size_t *out_count) {
    if (out_count) *out_count = 0;
    if (!out_count) return NULL;
    if (!markdown || len == 0) return NULL;

    apex_options opts = options ? *options : apex_options_default();
    opts.output_format = APEX_OUTPUT_TOC;
    opts.toc_entries_out = NULL; /* set below after locals exist */
    opts.toc_entries_count_out = NULL;

    apex_toc_entry *entries = NULL;
    size_t count = 0;
    opts.toc_entries_out = &entries;
    opts.toc_entries_count_out = &count;

    char *discard = apex_markdown_to_html(markdown, len, &opts);
    apex_free_string(discard);

    *out_count = count;
    return entries;
}

char *apex_markdown_to_html(const char *markdown, size_t len, const apex_options *options) {
    if (!markdown || len == 0) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    PROFILE_START(total);

    /* Use default options if none provided, and create a mutable copy */
    apex_options local_opts;
    if (!options) {
        apex_options default_opts;
        default_opts = apex_options_default();
        local_opts = default_opts;
    } else {
        local_opts = *options;  /* Make a mutable copy */
    }
    /* Use local_opts for rest of function (mutable) - shadow the const parameter */
    #define options (&local_opts)

    if (local_opts.strict_xhtml) {
        local_opts.xhtml = true;
    }

    /* Man/man-html output: force disable smart typography so option names (e.g. --to) stay as literal -- */
    if (options->output_format == APEX_OUTPUT_MAN || options->output_format == APEX_OUTPUT_MAN_HTML) {
        local_opts.enable_smart_typography = false;
    }

    /* Extract metadata if enabled (preprocessing step) */
    /* Safety check: ensure len doesn't exceed actual string length */
    size_t actual_len = strlen(markdown);
    if (len > actual_len) len = actual_len;

    /* Create working copy of input text */
    char *working_text = malloc(len + 1);
    if (!working_text) return NULL;
    memcpy(working_text, markdown, len);
    working_text[len] = '\0';

    /* Discover plugins once per conversion. This currently supports
     * text-level pre-parse plugins described by simple YAML manifests
     * in project and global plugin directories.
     */
    apex_plugin_manager *plugin_manager = NULL;
    if (options->enable_plugins) {
        plugin_manager = apex_plugins_load(options);
    }

    char *quarto_shortcodes_processed = NULL;
    if (apex_quarto_feature(options, options->enable_quarto_shortcodes)) {
        bool warn_unknown = getenv("APEX_VERBOSE") != NULL;
        PROFILE_START(quarto_shortcodes_preprocess);
        quarto_shortcodes_processed = apex_preprocess_quarto_shortcodes(working_text, warn_unknown, options->unsafe);
        PROFILE_END(quarto_shortcodes_preprocess);
        if (quarto_shortcodes_processed) {
            free(working_text);
            working_text = quarto_shortcodes_processed;
            len = strlen(working_text);
        }
    }

    /* Optional pre-parse plugin hook: run all configured pre_parse plugins
     * over the raw markdown before any Apex-specific preprocessing.
     */
    if (plugin_manager) {
        char *plugin_text = apex_plugins_run_text_phase(plugin_manager,
                                                        APEX_PLUGIN_PHASE_PRE_PARSE,
                                                        working_text,
                                                        options);
        if (plugin_text) {
            free(working_text);
            working_text = plugin_text;
            len = strlen(working_text);
        }
    }

    apex_metadata_item *metadata = NULL;
    abbr_item *abbreviations = NULL;
    ald_entry *alds = NULL;
    char *text_ptr = working_text;
    /* Liquid tag placeholders (for {% ... %} tags) */
    char **liquid_tags = NULL;
    size_t liquid_tag_count = 0;
    char *liquid_protected = NULL;
    char *py_callouts_processed = NULL;
    /* Process Python-Markdown/markdown-callouts syntax early so metadata parsing
     * doesn't consume top-of-file TYPE: lines before they can be treated as callouts.
     */
    if (options->enable_py_callouts) {
        PROFILE_START(py_callouts_preprocess);
        py_callouts_processed = apex_preprocess_py_callouts(text_ptr);
        PROFILE_END(py_callouts_preprocess);
        if (py_callouts_processed) {
            text_ptr = py_callouts_processed;
        }
    }



    if (getenv("APEX_DEBUG_PIPELINE")) {
        size_t len = strlen(text_ptr);
        fprintf(stderr, "[APEX_DEBUG] pipeline start (len=%zu): %.200s%s\n",
                len, text_ptr, len > 200 ? "..." : "");
    }

    /* Create deflist debug log as soon as conversion starts (so it exists even if we exit early or deflists are disabled) */
    apex_deflist_debug_touch(options->enable_definition_lists);

    if (options->mode == APEX_MODE_MULTIMARKDOWN ||
        options->mode == APEX_MODE_KRAMDOWN ||
        apex_mode_is_unified_family(options->mode)) {
        /* Extract metadata FIRST */
        PROFILE_START(metadata);
        metadata = apex_extract_metadata_for_mode(&text_ptr, options->mode);
        PROFILE_END(metadata);
        if (getenv("APEX_DEBUG_PIPELINE")) {
            size_t len = strlen(text_ptr);
            fprintf(stderr, "[APEX_DEBUG] after extract_metadata (len=%zu): %.200s%s\n",
                    len, text_ptr, len > 200 ? "..." : "");
        }

        /* Extract ALDs for Kramdown */
        if (apex_mode_is_kramdown_or_unified_family(options->mode)) {
            alds = apex_extract_alds(&text_ptr);
            if (getenv("APEX_DEBUG_PIPELINE")) {
                size_t len = strlen(text_ptr);
                fprintf(stderr, "[APEX_DEBUG] after extract_alds (len=%zu): %.200s%s\n",
                        len, text_ptr, len > 200 ? "..." : "");
            }
        }

        /* Extract abbreviations */
        abbreviations = apex_extract_abbreviations(&text_ptr);
    }

    /* Check metadata for bibliography and enable citations if found */
    if (metadata && !local_opts.enable_citations) {
        const char *bib_value = apex_metadata_get(metadata, "bibliography");
        if (bib_value) {
            local_opts.enable_citations = true;
        }
        const char *csl_value = apex_metadata_get(metadata, "csl");
        if (csl_value) {
            local_opts.enable_citations = true;
        }
    }

    /* Apply metadata variable replacement BEFORE autolinking
     * This ensures replaced URLs get autolinked
     */
    char *metadata_replaced = NULL;
    if (metadata && options->enable_metadata_variables) {
        PROFILE_START(metadata_replace_pre);
        metadata_replaced = apex_metadata_replace_variables(text_ptr, metadata, options);
        PROFILE_END(metadata_replace_pre);
        if (metadata_replaced) {
            text_ptr = metadata_replaced;
        }
    }

    /* Load bibliography files if provided (before processing citations)
     * Check both CLI bibliography files and metadata bibliography
     * Only load bibliography if files are actually specified - this avoids
     * unnecessary file I/O and parsing when citations aren't being used
     */
    apex_bibliography_registry *bibliography = NULL;

    /* Load from CLI bibliography files if specified */
    if (options->bibliography_files) {
        PROFILE_START(bibliography_load);
        bibliography = apex_load_bibliography((const char **)options->bibliography_files, options->base_directory);
        PROFILE_END(bibliography_load);
    }

    /* Also check metadata for bibliography (merge with CLI bibliography if both exist) */
    if (metadata) {
        const char *bib_value = apex_metadata_get(metadata, "bibliography");
        if (bib_value) {
            PROFILE_START(bibliography_load_meta);
            /* Load bibliography from metadata */
            char *resolved_path = NULL;
            if (options->base_directory) {
                size_t base_len = strlen(options->base_directory);
                size_t bib_len = strlen(bib_value);
                resolved_path = malloc(base_len + bib_len + 2);
                if (resolved_path) {
                    strcpy(resolved_path, options->base_directory);
                    if (resolved_path[base_len - 1] != '/') {
                        resolved_path[base_len] = '/';
                        base_len++;
                    }
                    strcpy(resolved_path + base_len, bib_value);
                }
            } else {
                resolved_path = strdup(bib_value);
            }

            if (resolved_path) {
                apex_bibliography_registry *meta_bib = apex_load_bibliography_file(resolved_path);
                if (meta_bib) {
                    if (bibliography) {
                        /* Merge with existing bibliography */
                        apex_bibliography_entry *entry = meta_bib->entries;
                        while (entry) {
                            apex_bibliography_entry *next = entry->next;
                            if (!apex_find_bibliography_entry(bibliography, entry->id)) {
                                entry->next = bibliography->entries;
                                bibliography->entries = entry;
                                bibliography->count++;
                            } else {
                                apex_bibliography_entry_free(entry);
                            }
                            entry = next;
                        }
                        free(meta_bib);
                    } else {
                        /* Use metadata bibliography as the main bibliography */
                        bibliography = meta_bib;
                    }
                }
                free(resolved_path);
            }
            PROFILE_END(bibliography_load_meta);
        }
    }

    /* Process citations BEFORE autolinking to prevent @ symbols from being converted to mailto links
     * Citations like [@key] need to be processed before autolinking sees the @ symbol
     * Only process citations if bibliography is actually loaded or citations are explicitly enabled
     */
    apex_citation_registry citation_registry = {0};
    citation_registry.bibliography = bibliography;
    char *citations_processed = NULL;

    /* Check if we should process citations: bibliography loaded, CLI files specified, CSL specified, or metadata bibliography */
    bool should_process_citations = false;
    if (bibliography) {
        should_process_citations = true;
    } else if (options->bibliography_files) {
        should_process_citations = true;
    } else if (options->csl_file) {
        should_process_citations = true;
    } else if (metadata) {
        const char *bib_value = apex_metadata_get(metadata, "bibliography");
        const char *csl_value = apex_metadata_get(metadata, "csl");
        if (bib_value || csl_value) {
            should_process_citations = true;
        }
    }

    if (options->enable_citations && should_process_citations) {
        PROGRESS_REPORT("Processing citations", -1);
        PROFILE_START(citations);
        citations_processed = apex_process_citations(text_ptr, &citation_registry, options);
        PROFILE_END(citations);
        if (citations_processed) {
            text_ptr = citations_processed;
        }
    }

    /* Process index entries (preprocessing) */
    apex_index_registry index_registry = {0};
    char *concordance_processed = NULL;
    char *indices_processed = NULL;
    if (options->enable_indices && options->concordance_files && options->concordance_files[0]) {
        PROGRESS_REPORT("Applying concordance", -1);
        PROFILE_START(concordance);
        concordance_processed = apex_apply_concordance(text_ptr, options);
        PROFILE_END(concordance);
        if (concordance_processed) {
            text_ptr = concordance_processed;
        }
    }
    if (options->enable_indices) {
        PROGRESS_REPORT("Processing indices", -1);
        PROFILE_START(indices);
        indices_processed = apex_process_index_entries(text_ptr, &index_registry, options);
        PROFILE_END(indices);
        if (indices_processed) {
            if (concordance_processed) {
                free(concordance_processed);
                concordance_processed = NULL;
            }
            text_ptr = indices_processed;
        }
    }

    /* Preprocess autolinks to convert <https://...> to [https://...](https://...)
     * This must happen after citation processing so @ symbols in citations aren't autolinked
     * Note: Even with autolink extension enabled, preprocessing ensures compatibility
     */
    char *autolinks_processed = NULL;
    if (options->enable_autolink) {
        PROGRESS_REPORT("Processing autolinks", -1);
        PROFILE_START(autolinks);
        autolinks_processed = apex_preprocess_autolinks(text_ptr, options);
        PROFILE_END(autolinks);
        if (autolinks_processed) {
            text_ptr = autolinks_processed;
        }
    }

    /* Preprocess image attributes and URL-encode all link URLs */
    image_attr_entry *img_attrs = NULL;
    char *image_attrs_processed = NULL;
    if (apex_mode_is_unified_family(options->mode) ||
        options->mode == APEX_MODE_MULTIMARKDOWN ||
        options->mode == APEX_MODE_KRAMDOWN ||
        options->mode == APEX_MODE_GFM ||
        options->mode == APEX_MODE_COMMONMARK) {
        PROFILE_START(image_attrs_preprocess);
        image_attrs_processed = apex_preprocess_image_attributes(text_ptr, &img_attrs, options->mode);
        PROFILE_END(image_attrs_preprocess);
        if (image_attrs_processed) {
            text_ptr = image_attrs_processed;
            if (getenv("APEX_DEBUG_PIPELINE")) {
                size_t len = strlen(text_ptr);
                fprintf(stderr, "[APEX_DEBUG] after image_attrs (len=%zu): %.250s%s\n",
                        len, text_ptr, len > 250 ? "..." : "");
            }
        }
    }

    /* Preprocess IAL markers (insert blank lines before them so cmark parses correctly) */
    char *ial_preprocessed = NULL;
    char *escaped_toc_protected = NULL;
    if (options->mode == APEX_MODE_KRAMDOWN || apex_mode_is_unified_family(options->mode)) {
        PROFILE_START(ial_preprocess);
        ial_preprocessed = apex_preprocess_ial(text_ptr);
        PROFILE_END(ial_preprocess);
        if (ial_preprocessed) {
            text_ptr = ial_preprocessed;
            if (getenv("APEX_DEBUG_PIPELINE")) {
                size_t len = strlen(text_ptr);
                fprintf(stderr, "[APEX_DEBUG] after ial_preprocess (len=%zu): %.250s%s\n",
                        len, text_ptr, len > 250 ? "..." : "");
            }
        }
    }

    /* Preprocess grid tables BEFORE spans/special_markers.
     * Grid tables must process === separators before proofreader/highlight converts them.
     * Converts Pandoc +---+ syntax to pipe tables for the normal table pipeline.
     */
    char *grid_tables_processed = NULL;
    char *normalized_for_grid_tables = NULL;
    if (options->enable_grid_tables && options->enable_tables) {
        size_t pre_grid_len = strlen(text_ptr);
        bool needs_newline_for_grid = (pre_grid_len > 0 &&
                                       text_ptr[pre_grid_len - 1] != '\n' &&
                                       text_ptr[pre_grid_len - 1] != '\r');
        if (needs_newline_for_grid) {
            normalized_for_grid_tables = malloc(pre_grid_len + 2);
            if (normalized_for_grid_tables) {
                memcpy(normalized_for_grid_tables, text_ptr, pre_grid_len);
                normalized_for_grid_tables[pre_grid_len] = '\n';
                normalized_for_grid_tables[pre_grid_len + 1] = '\0';
            }
        }

        PROFILE_START(grid_tables_preprocess);
        grid_tables_processed = apex_preprocess_grid_tables(normalized_for_grid_tables ? normalized_for_grid_tables : text_ptr);
        PROFILE_END(grid_tables_preprocess);

        if (normalized_for_grid_tables) {
            free(normalized_for_grid_tables);
        }

        if (grid_tables_processed) {
            text_ptr = grid_tables_processed;
        }
    }

    /* Preprocess bracketed spans [text]{IAL} */
    char *spans_preprocessed = NULL;
    if (options->enable_spans && apex_mode_is_kramdown_or_unified_family(options->mode)) {
        PROFILE_START(spans_preprocess);
        spans_preprocessed = apex_preprocess_bracketed_spans(text_ptr);
        PROFILE_END(spans_preprocess);
        if (spans_preprocessed) {
            text_ptr = spans_preprocessed;
        }
    }

    /* Process file includes before parsing (preprocessing) */
    char *includes_processed = NULL;
    if (options->enable_file_includes) {
        PROFILE_START(includes);
        includes_processed = apex_process_includes(text_ptr,
                                                   options->base_directory,
                                                   metadata,
                                                   0,
                                                   options->wikilink_extension,
                                                   plugin_manager,
                                                   options);
        PROFILE_END(includes);
        if (includes_processed) {
            text_ptr = includes_processed;
        }
    }

    /* Process special markers (^ end-of-block marker) and inline tables BEFORE alpha lists */
    /* This ensures ^ markers and inline table markers are converted before alpha list processing */
    char *markers_processed_early = NULL;
    if (options->enable_marked_extensions) {
        PROFILE_START(special_markers);
        markers_processed_early = apex_process_special_markers(text_ptr);
        PROFILE_END(special_markers);
        if (markers_processed_early) {
            text_ptr = markers_processed_early;
        }
    }

    /* Process inline table fences and <!--TABLE--> markers before parsing */
    char *inline_tables_processed = NULL;
    PROFILE_START(inline_tables);
    inline_tables_processed = apex_process_inline_tables(text_ptr);
    PROFILE_END(inline_tables);
    if (inline_tables_processed) {
        text_ptr = inline_tables_processed;
    }

    /* Pandoc/Quarto list extensions before alpha/roman marker normalization */
    char *example_lists_processed = NULL;
    char *line_blocks_processed = NULL;
    char *roman_lists_processed = NULL;
    char *strict_lists_processed = NULL;
    if (apex_quarto_feature(options, options->enable_quarto_example_lists)) {
        PROFILE_START(example_lists_preprocess);
        example_lists_processed = apex_preprocess_example_lists(text_ptr);
        PROFILE_END(example_lists_preprocess);
        if (example_lists_processed) {
            text_ptr = example_lists_processed;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_line_blocks)) {
        PROFILE_START(line_blocks_preprocess);
        line_blocks_processed = apex_preprocess_line_blocks(text_ptr, options->unsafe);
        PROFILE_END(line_blocks_preprocess);
        if (line_blocks_processed) {
            text_ptr = line_blocks_processed;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_roman_lists)) {
        PROFILE_START(roman_lists_preprocess);
        roman_lists_processed = apex_preprocess_roman_lists(text_ptr);
        PROFILE_END(roman_lists_preprocess);
        if (roman_lists_processed) {
            text_ptr = roman_lists_processed;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_strict_lists)) {
        PROFILE_START(quarto_strict_lists_preprocess);
        strict_lists_processed = apex_preprocess_quarto_strict_lists(text_ptr);
        PROFILE_END(quarto_strict_lists_preprocess);
        if (strict_lists_processed) {
            text_ptr = strict_lists_processed;
        }
    }

    /* Process alpha lists before parsing (preprocessing) */
    char *alpha_lists_processed = NULL;
    bool inserted_synthetic_nested_alpha_break = false;
    bool saw_explicit_nested_alpha_break = false;
    if (options->allow_alpha_lists) {
        PROFILE_START(alpha_lists);
        alpha_lists_processed = apex_preprocess_alpha_lists(
            text_ptr,
            &inserted_synthetic_nested_alpha_break,
            &saw_explicit_nested_alpha_break
        );
        PROFILE_END(alpha_lists);
        if (alpha_lists_processed) {
            text_ptr = alpha_lists_processed;
        }
    }

    char *nested_ordered_sublists_processed = NULL;
    bool inserted_synthetic_nested_ordered_break = false;
    bool saw_explicit_nested_ordered_break = false;

    /* Process emoji autocorrect before parsing (preprocessing) */
    char *emoji_autocorrect_processed = NULL;
    if (options->enable_emoji_autocorrect && (options->mode == APEX_MODE_GFM || apex_mode_is_unified_family(options->mode))) {
        PROFILE_START(emoji_autocorrect);
        emoji_autocorrect_processed = apex_autocorrect_emoji_names(text_ptr);
        PROFILE_END(emoji_autocorrect);
        if (emoji_autocorrect_processed) {
            text_ptr = emoji_autocorrect_processed;
        }
    }

    /* Process inline footnotes before parsing (Kramdown ^[...] and MMD [^... ...]) */
    char *inline_footnotes_processed = NULL;
    if (options->enable_footnotes) {
        PROFILE_START(inline_footnotes);
        inline_footnotes_processed = apex_process_inline_footnotes(text_ptr);
        PROFILE_END(inline_footnotes);
        if (inline_footnotes_processed) {
            text_ptr = inline_footnotes_processed;
        }
    }

    /* Process ==highlight== syntax before parsing
     * Skip if proofreader mode is enabled (proofreader will handle it via CriticMarkup) */
    char *highlights_processed = NULL;
    if (!options->proofreader_mode) {
        PROFILE_START(highlights);
        highlights_processed = apex_process_highlights(text_ptr);
        PROFILE_END(highlights);
        if (highlights_processed) {
            text_ptr = highlights_processed;
        }
    }

    /* Process ++insert++ syntax before parsing */
    char *inserts_processed = NULL;
    PROFILE_START(inserts);
    inserts_processed = apex_process_inserts(text_ptr);
    PROFILE_END(inserts);
    if (inserts_processed) {
        text_ptr = inserts_processed;
    }

    /* Process superscript and subscript syntax before parsing */
    char *sup_sub_processed = NULL;
    if (options->enable_sup_sub) {
        PROFILE_START(sup_sub);
        sup_sub_processed = apex_process_sup_sub(text_ptr);
        PROFILE_END(sup_sub);
        if (sup_sub_processed) {
            text_ptr = sup_sub_processed;
        }
    }

    /* Process relaxed tables before parsing (preprocessing) */
    char *relaxed_tables_processed = NULL;
    char *normalized_for_relaxed = NULL;
    if (options->relaxed_tables && options->enable_tables) {
        /* Normalize text_ptr for relaxed tables processing if it doesn't end with newline */
        size_t pre_relaxed_len = strlen(text_ptr);
        bool needs_newline_for_relaxed = (pre_relaxed_len > 0 &&
                                          text_ptr[pre_relaxed_len - 1] != '\n' &&
                                          text_ptr[pre_relaxed_len - 1] != '\r');
        if (needs_newline_for_relaxed) {
            normalized_for_relaxed = malloc(pre_relaxed_len + 2);
            if (normalized_for_relaxed) {
                memcpy(normalized_for_relaxed, text_ptr, pre_relaxed_len);
                normalized_for_relaxed[pre_relaxed_len] = '\n';
                normalized_for_relaxed[pre_relaxed_len + 1] = '\0';
            }
        }

        PROGRESS_REPORT("Processing relaxed tables", -1);
        PROFILE_START(relaxed_tables);
        relaxed_tables_processed = apex_process_relaxed_tables(normalized_for_relaxed ? normalized_for_relaxed : text_ptr);
        PROFILE_END(relaxed_tables);
        /* Refresh progress after processing completes (in case it took a while) */
        PROGRESS_REPORT(NULL, -1);  /* NULL stage = refresh last known stage */

        /* Handle cleanup */
        if (normalized_for_relaxed) {
            if (relaxed_tables_processed) {
                /* Processing returned a new buffer - free our normalization buffer */
                free(normalized_for_relaxed);
            } else {
                /* Processing returned NULL - free normalization buffer and continue with original */
                free(normalized_for_relaxed);
            }
        }

        if (relaxed_tables_processed) {
            text_ptr = relaxed_tables_processed;
        }
    }

    /* Process headerless tables before parsing (preprocessing)
     * Detect separator rows without header rows and insert dummy headers
     * This must run after relaxed tables processing
     */
    char *headerless_tables_processed = NULL;
    char *normalized_for_headerless = NULL;
    if (options->enable_tables) {
        /* Normalize text_ptr for headerless tables processing if it doesn't end with newline */
        size_t pre_headerless_len = strlen(text_ptr);
        bool needs_newline_for_headerless = (pre_headerless_len > 0 &&
                                             text_ptr[pre_headerless_len - 1] != '\n' &&
                                             text_ptr[pre_headerless_len - 1] != '\r');
        if (needs_newline_for_headerless) {
            normalized_for_headerless = malloc(pre_headerless_len + 2);
            if (normalized_for_headerless) {
                memcpy(normalized_for_headerless, text_ptr, pre_headerless_len);
                normalized_for_headerless[pre_headerless_len] = '\n';
                normalized_for_headerless[pre_headerless_len + 1] = '\0';
            }
        }

        PROFILE_START(headerless_tables);
        headerless_tables_processed = apex_process_headerless_tables(normalized_for_headerless ? normalized_for_headerless : text_ptr);
        PROFILE_END(headerless_tables);

        /* Handle cleanup */
        if (normalized_for_headerless) {
            if (headerless_tables_processed) {
                /* Processing returned a new buffer - free our normalization buffer */
                free(normalized_for_headerless);
            } else {
                /* Processing returned NULL - free normalization buffer and continue with original */
                free(normalized_for_headerless);
            }
        }

        if (headerless_tables_processed) {
            text_ptr = headerless_tables_processed;
        }
    }

    /* Preprocess table rows to convert consecutive pipes (|||) to << markers for colspan
     * This must run after headerless table processing but before caption processing
     */
    char *table_colspans_processed = NULL;
    char *normalized_for_colspans = NULL;
    if (options->enable_tables) {
        /* Normalize text_ptr for table colspan preprocessing if it doesn't end with newline */
        size_t pre_colspans_len = strlen(text_ptr);
        bool needs_newline_for_colspans = (pre_colspans_len > 0 &&
                                           text_ptr[pre_colspans_len - 1] != '\n' &&
                                           text_ptr[pre_colspans_len - 1] != '\r');
        if (needs_newline_for_colspans) {
            normalized_for_colspans = malloc(pre_colspans_len + 2);
            if (normalized_for_colspans) {
                memcpy(normalized_for_colspans, text_ptr, pre_colspans_len);
                normalized_for_colspans[pre_colspans_len] = '\n';
                normalized_for_colspans[pre_colspans_len + 1] = '\0';
            }
        }

        PROFILE_START(table_colspans_preprocess);
        table_colspans_processed = apex_preprocess_table_colspans(normalized_for_colspans ? normalized_for_colspans : text_ptr);
        PROFILE_END(table_colspans_preprocess);

        /* Handle cleanup */
        if (normalized_for_colspans) {
            if (table_colspans_processed) {
                free(normalized_for_colspans);
            } else {
                free(normalized_for_colspans);
            }
        }

        if (table_colspans_processed) {
            text_ptr = table_colspans_processed;
        }
    }

    /* Normalize table captions before parsing (preprocessing)
     * - Ensure contiguous [Caption] lines become separate paragraphs
     * - Convert Pandoc-style 'Table: Caption' lines to [Caption]
     *
     * Note: apex_preprocess_table_captions now ensures output ends with newline, but we
     * normalize here too for safety, in case previous preprocessing removed it.
     */
    char *table_captions_processed = NULL;
    char *normalized_for_caption = NULL;
    if (options->enable_tables) {
        /* Normalize text_ptr for table caption preprocessing if it doesn't end with newline */
        size_t pre_caption_len = strlen(text_ptr);
        bool needs_newline_for_caption = (pre_caption_len > 0 &&
                                          text_ptr[pre_caption_len - 1] != '\n' &&
                                          text_ptr[pre_caption_len - 1] != '\r');
        if (needs_newline_for_caption) {
            normalized_for_caption = malloc(pre_caption_len + 2);
            if (normalized_for_caption) {
                memcpy(normalized_for_caption, text_ptr, pre_caption_len);
                normalized_for_caption[pre_caption_len] = '\n';
                normalized_for_caption[pre_caption_len + 1] = '\0';
            }
        }

        PROFILE_START(table_captions_preprocess);
        table_captions_processed = apex_preprocess_table_captions(normalized_for_caption ? normalized_for_caption : text_ptr);
        PROFILE_END(table_captions_preprocess);

        /* Handle cleanup: apex_preprocess_table_captions always returns a new allocated buffer (or NULL on malloc failure) */
        if (normalized_for_caption) {
            if (table_captions_processed) {
                /* Preprocessing returned a new buffer - free our normalization buffer */
                free(normalized_for_caption);
            } else {
                /* Preprocessing returned NULL (malloc failure) - free normalization buffer and continue with original text_ptr */
                free(normalized_for_caption);
            }
        }

        if (table_captions_processed) {
            text_ptr = table_captions_processed;
        }
    }

    /* Process definition lists before parsing (preprocessing) */
    char *deflist_processed = NULL;
    if (options->enable_definition_lists) {
        PROFILE_START(definition_lists);
        deflist_processed = apex_process_definition_lists(text_ptr, options->unsafe);
        PROFILE_END(definition_lists);
        if (deflist_processed) {
            text_ptr = deflist_processed;
        }
    }

    /* Pandoc/Quarto raw content ({=format}) before other fence preprocessors */
    char *raw_content_processed = NULL;
    char *code_fence_attrs_processed = NULL;
    char *quarto_diagrams_processed = NULL;
    if (apex_quarto_feature(options, options->enable_quarto_raw)) {
        PROFILE_START(raw_content_preprocess);
        raw_content_processed = apex_preprocess_raw_content(text_ptr, options->unsafe);
        PROFILE_END(raw_content_preprocess);
        if (raw_content_processed) {
            text_ptr = raw_content_processed;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_code_attrs)) {
        PROFILE_START(code_fence_attrs_preprocess);
        code_fence_attrs_processed = apex_preprocess_code_fence_attrs(text_ptr);
        PROFILE_END(code_fence_attrs_preprocess);
        if (code_fence_attrs_processed) {
            text_ptr = code_fence_attrs_processed;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_diagrams)) {
        PROFILE_START(quarto_diagrams_preprocess);
        quarto_diagrams_processed = apex_preprocess_quarto_diagrams(text_ptr, options->unsafe);
        PROFILE_END(quarto_diagrams_preprocess);
        if (quarto_diagrams_processed) {
            text_ptr = quarto_diagrams_processed;
        }
    }

    /* Process Quarto ::: callouts before fenced divs so recognized callouts bypass generic div conversion */
    char *quarto_callouts_processed = NULL;
    if (options->enable_quarto_callouts) {
        PROFILE_START(quarto_callouts_preprocess);
        quarto_callouts_processed = apex_preprocess_quarto_callouts(text_ptr);
        PROFILE_END(quarto_callouts_preprocess);
        if (quarto_callouts_processed) {
            text_ptr = quarto_callouts_processed;
        }
    }

    /* Process fenced divs before parsing (preprocessing) */
    /* Only enabled in Unified mode */
    char *fenced_divs_processed = NULL;
    if (options->enable_divs && apex_mode_is_unified_family(options->mode)) {
        PROFILE_START(fenced_divs);
        fenced_divs_processed = apex_process_fenced_divs(text_ptr);
        PROFILE_END(fenced_divs);
        if (fenced_divs_processed) {
            text_ptr = fenced_divs_processed;
        }
    }

    /* Process HTML markdown attributes before parsing (preprocessing) */
    char *html_markdown_processed = NULL;
    if (options->enable_markdown_in_html) {
        PROFILE_START(html_markdown);
        html_markdown_processed = apex_process_html_markdown(text_ptr, img_attrs, options);
        PROFILE_END(html_markdown);
        if (html_markdown_processed) {
            text_ptr = html_markdown_processed;
        }
    }

    /* Process hashtags: convert #tags to span-wrapped hashtags */
    char *hashtags_processed = NULL;
    if (options->enable_hashtags && text_ptr) {
        PROFILE_START(hashtags);
        size_t len = strlen(text_ptr);
        size_t capacity = len * 3 + 1;  /* Allow expansion with span tags */
        char *output = malloc(capacity);
        if (output) {
            const char *read = text_ptr;
            char *write = output;
            size_t remaining = capacity;
            bool in_code_block = false;
            int indent_count = 0;
            bool at_line_start = true;

            while (*read) {
                /* Track code blocks (4+ spaces or tab at line start) */
                if (at_line_start) {
                    if (*read == '\t') {
                        in_code_block = true;
                        indent_count = 0;
                    } else if (*read == ' ') {
                        indent_count++;
                        if (indent_count >= 4) {
                            in_code_block = true;
                        }
                    } else if (*read != '\n' && *read != '\r') {
                        at_line_start = false;
                        indent_count = 0;
                    }
                }

                if (*read == '\n') {
                    at_line_start = true;
                    indent_count = 0;
                    in_code_block = false;
                }

                /* Skip hashtag processing inside code blocks */
                if (in_code_block) {
                    if (remaining < 10) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + 100) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            output = NULL;
                            break;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    *write++ = *read++;
                    remaining--;
                    continue;
                }

                /* Check for hashtag pattern: # followed by alphanumeric, not preceded by non-whitespace */
                /* Pattern: (?<=\s|^)#[a-zA-Z0-9][^# \n,;.!\)\]]* */
                if (*read == '#' && (read == text_ptr || read[-1] == ' ' || read[-1] == '\t' || read[-1] == '\n')) {
                    const char *tag_start = read;
                    read++;  /* Skip # */

                    /* Check if it's a valid hashtag start (alphanumeric) */
                    if ((*read >= 'a' && *read <= 'z') || (*read >= 'A' && *read <= 'Z') || (*read >= '0' && *read <= '9')) {
                        /* Find the end of the hashtag */
                        const char *tag_end = read;
                        while (*tag_end && *tag_end != '#' && *tag_end != ' ' && *tag_end != '\n' &&
                               *tag_end != ',' && *tag_end != ';' && *tag_end != '.' && *tag_end != '!' &&
                               *tag_end != ')' && *tag_end != ']') {
                            tag_end++;
                        }

                        /* Check for special case: #tag# format (wrapped in #) */
                        if (*tag_end == '#') {
                            tag_end++;  /* Include the closing # */
                        }

                        if (tag_end > read) {
                            /* Valid hashtag found */
                            size_t tag_len = (size_t)(tag_end - tag_start);
                            const char *class_name = options->style_hashtags ? "mkstyledtag" : "mkhashtag";
                            size_t span_prefix_len = strlen("<span class=\"") + strlen(class_name) + strlen("\">");
                            size_t span_suffix_len = strlen("</span>");
                            size_t needed = span_prefix_len + tag_len + span_suffix_len;

                            if (needed >= remaining) {
                                size_t written = (size_t)(write - output);
                                capacity = (written + needed + 100) * 2;
                                char *new_output = realloc(output, capacity);
                                if (!new_output) {
                                    free(output);
                                    output = NULL;
                                    break;
                                }
                                output = new_output;
                                write = output + written;
                                remaining = capacity - written;
                            }

                            /* Write opening span */
                            memcpy(write, "<span class=\"", 13);
                            write += 13;
                            remaining -= 13;
                            memcpy(write, class_name, strlen(class_name));
                            write += strlen(class_name);
                            remaining -= strlen(class_name);
                            memcpy(write, "\">", 2);
                            write += 2;
                            remaining -= 2;

                            /* Write the hashtag */
                            memcpy(write, tag_start, tag_len);
                            write += tag_len;
                            remaining -= tag_len;

                            /* Write closing span */
                            memcpy(write, "</span>", 7);
                            write += 7;
                            remaining -= 7;

                            read = tag_end;
                            continue;
                        }
                    }
                    /* Not a valid hashtag, copy the # */
                    if (remaining < 10) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + 100) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            output = NULL;
                            break;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    *write++ = *tag_start;
                    remaining--;
                    read = tag_start + 1;
                } else {
                    /* Normal character, copy as-is */
                    if (remaining < 10) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + 100) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            output = NULL;
                            break;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    *write++ = *read++;
                    remaining--;
                }
            }
            if (output) {
                *write = '\0';
                hashtags_processed = output;
            }
        }
        PROFILE_END(hashtags);
        if (hashtags_processed) {
            text_ptr = hashtags_processed;
        }
    }

    /* Process proofreader mode: convert == and ~~ to CriticMarkup syntax */
    char *proofreader_processed = NULL;
    if (options->proofreader_mode && text_ptr) {
        PROFILE_START(proofreader);
        size_t len = strlen(text_ptr);
        size_t capacity = len * 2 + 1;  /* Allow expansion */
        char *output = malloc(capacity);
        if (output) {
            const char *read = text_ptr;
            char *write = output;
            size_t remaining = capacity;
            bool in_code_block = false;
            bool in_inline_code = false;
            int backtick_count = 0;

            while (*read) {
                /* Track code blocks and inline code to skip processing inside them */
                if (*read == '`') {
                    backtick_count++;
                    if (backtick_count >= 3) {
                        /* Code block fence */
                        in_code_block = !in_code_block;
                        backtick_count = 0;
                    } else if (!in_code_block) {
                        /* Check for inline code */
                        const char *next = read + 1;
                        if (*next != '`') {
                            in_inline_code = !in_inline_code;
                            backtick_count = 0;
                        }
                    }
                } else {
                    backtick_count = 0;
                }

                if (in_code_block || in_inline_code) {
                    /* Inside code, copy as-is */
                    if (remaining < 10) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + 100) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            output = NULL;
                            break;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    *write++ = *read++;
                    remaining--;
                } else if (read[0] == '=' && read[1] == '=') {
                    /* Found ==, convert to {== */
                    const char *start = read;
                    read += 2;
                    /* Find matching == */
                    const char *end = strstr(read, "==");
                    if (end) {
                        /* Found matching ==, wrap in {==...==} */
                        size_t content_len = (size_t)(end - read);
                        size_t needed = 2 + content_len + 2;  /* {== + content + ==} */
                        if (needed >= remaining) {
                            size_t written = (size_t)(write - output);
                            capacity = (written + needed + 100) * 2;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(output);
                                output = NULL;
                                break;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        *write++ = '{';
                        *write++ = '=';
                        *write++ = '=';
                        remaining -= 3;
                        memcpy(write, read, content_len);
                        write += content_len;
                        remaining -= content_len;
                        *write++ = '=';
                        *write++ = '=';
                        *write++ = '}';
                        remaining -= 3;
                        read = end + 2;
                    } else {
                        /* No matching ==, copy as-is */
                        if (remaining < 10) {
                            size_t written = (size_t)(write - output);
                            capacity = (written + 100) * 2;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(output);
                                output = NULL;
                                break;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        *write++ = *start++;
                        *write++ = *start;
                        remaining -= 2;
                        read = start + 1;
                    }
                } else if (read[0] == '~' && read[1] == '~') {
                    /* Found ~~, convert to {-- */
                    const char *start = read;
                    read += 2;
                    /* Find matching ~~ */
                    const char *end = strstr(read, "~~");
                    if (end) {
                        /* Found matching ~~, wrap in {--...--} */
                        size_t content_len = (size_t)(end - read);
                        size_t needed = 2 + content_len + 2;  /* {-- + content + --} */
                        if (needed >= remaining) {
                            size_t written = (size_t)(write - output);
                            capacity = (written + needed + 100) * 2;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(output);
                                output = NULL;
                                break;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        *write++ = '{';
                        *write++ = '-';
                        *write++ = '-';
                        remaining -= 3;
                        memcpy(write, read, content_len);
                        write += content_len;
                        remaining -= content_len;
                        *write++ = '-';
                        *write++ = '-';
                        *write++ = '}';
                        remaining -= 3;
                        read = end + 2;
                    } else {
                        /* No matching ~~, copy as-is */
                        if (remaining < 10) {
                            size_t written = (size_t)(write - output);
                            capacity = (written + 100) * 2;
                            char *new_output = realloc(output, capacity);
                            if (!new_output) {
                                free(output);
                                output = NULL;
                                break;
                            }
                            output = new_output;
                            write = output + written;
                            remaining = capacity - written;
                        }
                        *write++ = *start++;
                        *write++ = *start;
                        remaining -= 2;
                        read = start + 1;
                    }
                } else {
                    /* Normal character, copy as-is */
                    if (remaining < 10) {
                        size_t written = (size_t)(write - output);
                        capacity = (written + 100) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            output = NULL;
                            break;
                        }
                        output = new_output;
                        write = output + written;
                        remaining = capacity - written;
                    }
                    *write++ = *read++;
                    remaining--;
                }
            }
            if (output) {
                *write = '\0';
                proofreader_processed = output;
            }
        }
        PROFILE_END(proofreader);
        if (proofreader_processed) {
            text_ptr = proofreader_processed;
        }
    }

    /* Process Critic Markup before parsing (preprocessing) */
    char *critic_processed = NULL;
    if (options->enable_critic_markup) {
        PROFILE_START(critic);
        critic_mode_t critic_mode = (critic_mode_t)options->critic_mode;
        critic_processed = apex_process_critic_markup_text(text_ptr, critic_mode);
        PROFILE_END(critic);
        if (critic_processed) {
            text_ptr = critic_processed;
        }
    }

    /* Protect Liquid {% ... %} tags so they are not modified by later
     * processing (including parsing, math, and autolinks). We'll restore
     * them after rendering the final HTML.
     */
    liquid_protected = apex_protect_liquid_tags(text_ptr, &liquid_tags, &liquid_tag_count);
    if (liquid_protected) {
        text_ptr = liquid_protected;
    }

    /* Keep this late so later preprocessors do not collapse inserted separation. */
    if (apex_mode_is_unified_family(options->mode)) {
        PROFILE_START(nested_ordered_sublists);
        nested_ordered_sublists_processed = apex_preprocess_nested_ordered_sublists(
            text_ptr,
            &inserted_synthetic_nested_ordered_break,
            &saw_explicit_nested_ordered_break
        );
        PROFILE_END(nested_ordered_sublists);
        if (nested_ordered_sublists_processed) {
            text_ptr = nested_ordered_sublists_processed;
        }
    }

    /* Normalize input after ALL preprocessing: ensure it ends with a newline.
     * This is critical because various preprocessing steps (definition lists,
     * HTML markdown, critic markup, liquid protection) might remove the trailing
     * newline. cmark-gfm requires a trailing newline for proper table parsing,
     * especially for the last row of a table. */
    char *final_normalized = NULL;
    if (!text_ptr) {
        /* text_ptr should never be NULL, but be defensive */
        free(working_text);
        apex_free_metadata(metadata);
        return NULL;
    }
    size_t text_len = strlen(text_ptr);
    if (text_len == 0) {
        /* Empty text after preprocessing - use empty string with newline for consistency */
        final_normalized = malloc(2);
        if (final_normalized) {
            final_normalized[0] = '\n';
            final_normalized[1] = '\0';
            text_ptr = final_normalized;
            text_len = 1;
        }
    } else {
        /* Check if we need to add trailing newline - ensure text_len > 0 before accessing text_ptr[text_len - 1] */
        bool needs_newline = (text_len > 0 && text_ptr[text_len - 1] != '\n' && text_ptr[text_len - 1] != '\r');
        if (needs_newline) {
            /* Need to add a trailing line ending - use \n (LF) for consistency */
            final_normalized = malloc(text_len + 2);  /* +1 for newline, +1 for null term */
            if (final_normalized) {
                memcpy(final_normalized, text_ptr, text_len);
                final_normalized[text_len] = '\n';
                final_normalized[text_len + 1] = '\0';
                text_ptr = final_normalized;
                text_len = text_len + 1;
            } else {
                /* If malloc fails, we can't normalize - but this should never happen in practice */
                /* Continue with original text_ptr (will likely cause table parsing issue, but better than crashing) */
            }
        }
    }

    /* Convert options to cmark-gfm format */
    int cmark_opts = apex_to_cmark_options(options);

    /* Protect backslash-escaped {{TOC...}} markers before parsing */
    if (options->enable_marked_extensions || options->mode == APEX_MODE_MULTIMARKDOWN) {
        escaped_toc_protected = apex_protect_escaped_toc_markers(text_ptr);
        if (escaped_toc_protected) {
            text_ptr = escaped_toc_protected;
            text_len = strlen(text_ptr);
        }
    }

    /* Create parser */
    PROFILE_START(parsing);
    cmark_parser *parser = cmark_parser_new(cmark_opts);
    if (!parser) {
        if (final_normalized) free(final_normalized);
        free(working_text);
        apex_free_metadata(metadata);
        return NULL;
    }

    /* Register extensions based on mode and options */
    apex_register_extensions(parser, options);

    if (options->cmark_init) {
        options->cmark_init(parser, options, cmark_opts, options->cmark_user_data);
    }

    /* Feed normalized text to parser */
    if (getenv("APEX_DEBUG_PIPELINE")) {
        fprintf(stderr, "[APEX_DEBUG] markdown to parse (len=%zu): %.350s%s\n",
                text_len, text_ptr, text_len > 350 ? "..." : "");
    }
    cmark_parser_feed(parser, text_len ? text_ptr : "", text_len);
    cmark_node *document = cmark_parser_finish(parser);
    PROFILE_END(parsing);

    /* Free normalized buffer if we allocated it (after parser is finished) */
    if (final_normalized) {
        free(final_normalized);
    }

    if (!document) {
        if (options->cmark_done) {
            options->cmark_done(parser, options, cmark_opts, options->cmark_user_data);
        }
        cmark_parser_free(parser);
        free(working_text);
        apex_free_metadata(metadata);
        return NULL;
    }

    if (options->enable_task_lists) {
        apex_fix_blockquote_tasklists(document, cmark_find_syntax_extension("tasklist"), 0);
    }

    /* If output format is JSON, emit JSON right after parsing (before AST filters) */
    if (options->output_format == APEX_OUTPUT_JSON) {
        char *json = apex_cmark_to_pandoc_json(document, options);
        cmark_node_free(document);
        cmark_parser_free(parser);
        free(working_text);
        apex_free_metadata(metadata);
        /* Note: Preprocessing buffers are conditionally allocated and may not be in scope here.
         * This is acceptable as JSON output is typically used for debugging/inspection. */
        return json;
    }

    /* Run AST-level filters (Pandoc-style JSON filters) before any */
    /* AST post-processing or rendering. */
    if (options->ast_filter_commands && options->ast_filter_count > 0) {
        /* Determine target format string for filters based on output format */
        const char *target_format = "html";
        if (options->output_format == APEX_OUTPUT_JSON ||
            options->output_format == APEX_OUTPUT_JSON_FILTERED) {
            target_format = "json";
        } else if (options->output_format == APEX_OUTPUT_MARKDOWN ||
                   options->output_format == APEX_OUTPUT_MMD ||
                   options->output_format == APEX_OUTPUT_COMMONMARK ||
                   options->output_format == APEX_OUTPUT_KRAMDOWN ||
                   options->output_format == APEX_OUTPUT_GFM) {
            target_format = "markdown";
        } else if (options->output_format == APEX_OUTPUT_TERMINAL ||
                   options->output_format == APEX_OUTPUT_TERMINAL256) {
            target_format = "terminal";
        } else if (options->output_format == APEX_OUTPUT_RTF) {
            target_format = "rtf";
        }
        cmark_node *filtered = apex_run_ast_filters(document, options, target_format);
        if (!filtered && options->ast_filter_strict) {
            cmark_node_free(document);
            if (options->cmark_done) {
                options->cmark_done(parser, options, cmark_opts, options->cmark_user_data);
            }
            cmark_parser_free(parser);
            free(working_text);
            apex_free_metadata(metadata);
            return NULL;
        }
        if (filtered && filtered != document) {
            cmark_node_free(document);
            document = filtered;
        }
    }

    /* Postprocess wiki links if enabled */
    if (options->enable_wiki_links) {
        /* Fast path: skip AST walk if no wiki link markers present */
        if (strstr(text_ptr, "[[") != NULL) {
            PROGRESS_REPORT("Processing wiki links", -1);
            /* Create wiki link configuration from options */
            wiki_link_config wiki_config;
            wiki_config.base_path = "";
            wiki_config.extension = options->wikilink_extension ? options->wikilink_extension : "";
            wiki_config.space_mode = (wikilink_space_mode_t)options->wikilink_space;
            wiki_config.sanitize = options->wikilink_sanitize;
            apex_process_wiki_links_in_tree(document, &wiki_config);
        }
    }

    /* Postprocess callouts if enabled */
    if (options->enable_callouts) {
        apex_process_callouts_in_tree(document, options->enable_py_callouts, cmark_opts);
    }

    /* Process IAL (Inline Attribute Lists) BEFORE manual header IDs.
       IAL handles {: #id}, {#id}, and {.class} - running first ensures these
       are extracted and removed from heading text before manual header ID
       looks for MMD [id] or Kramdown {#id}. Avoids duplicate handling. */
    if (alds || apex_mode_is_kramdown_or_unified_family(options->mode)) {
        /* Fast path: skip AST walk if no IAL markers present */
        /* Check for both Kramdown-style ({:) and Pandoc-style ({# or {.) IALs */
        if (strstr(text_ptr, "{:") != NULL ||
            strstr(text_ptr, "{#") != NULL ||
            strstr(text_ptr, "{.") != NULL) {
            PROFILE_START(ial);
            apex_process_ial_in_tree(document, alds);
            PROFILE_END(ial);
        }
    }

    /* Process manual header IDs (MMD [id] and Kramdown {#id}) - after IAL
       so IAL's {#id} handling doesn't conflict; manual ID handles [id] and
       any {#id} IAL might have missed (e.g. in multi-child headings) */
    if (options->generate_header_ids) {
        cmark_iter *iter = cmark_iter_new(document);
        cmark_event_type event;
        while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
            cmark_node *node = cmark_iter_get_node(iter);
            if (event == CMARK_EVENT_ENTER && cmark_node_get_type(node) == CMARK_NODE_HEADING) {
                apex_process_manual_header_id(node);
            }
        }
        cmark_iter_free(iter);
    }

    /* Apply image attributes to image nodes */
    if (img_attrs) {
        PROFILE_START(image_attrs);
        apex_apply_image_attributes(document, img_attrs);
        PROFILE_END(image_attrs);
    }

    /* Merge lists with mixed markers if enabled */
    if (options->allow_mixed_list_markers) {
        apex_merge_mixed_list_markers(document);
    }
    if (apex_mode_is_unified_family(options->mode) &&
        ((inserted_synthetic_nested_ordered_break && !saw_explicit_nested_ordered_break) ||
         (inserted_synthetic_nested_alpha_break && !saw_explicit_nested_alpha_break))) {
        apex_tighten_nested_ordered_list_items(document);
    }

    /* Note: Critic Markup is now handled via preprocessing (before parsing) */

    /* If output format is JSON (after filters), serialize AST to JSON and return */
    if (options->output_format == APEX_OUTPUT_JSON_FILTERED) {
        char *json = apex_cmark_to_pandoc_json(document, options);
        /* Note: Cleanup happens at end of function - document and other resources
         * will be freed there. We return the JSON string here. */
        return json;
    }

    if (options->output_format == APEX_OUTPUT_TOC) {
        if (options->toc_entries_out && options->toc_entries_count_out) {
            *options->toc_entries_out = apex_generate_toc_entries(
                document, options->id_format, options->toc_min, options->toc_max,
                options->toc_entries_count_out);
            return strdup("");
        }
        char *toc_md = apex_generate_toc_markdown(document, options->id_format,
                                                  options->toc_min, options->toc_max);
        return toc_md ? toc_md : strdup("");
    }

    /* If output format is Markdown, serialize AST to Markdown and return */
    if (options->output_format == APEX_OUTPUT_MARKDOWN ||
        options->output_format == APEX_OUTPUT_MMD ||
        options->output_format == APEX_OUTPUT_COMMONMARK ||
        options->output_format == APEX_OUTPUT_KRAMDOWN ||
        options->output_format == APEX_OUTPUT_GFM) {
        apex_markdown_dialect_t dialect;
        if (options->output_format == APEX_OUTPUT_MARKDOWN) {
            dialect = APEX_MD_DIALECT_UNIFIED;
        } else if (options->output_format == APEX_OUTPUT_MMD) {
            dialect = APEX_MD_DIALECT_MMD;
        } else if (options->output_format == APEX_OUTPUT_COMMONMARK) {
            dialect = APEX_MD_DIALECT_COMMONMARK;
        } else if (options->output_format == APEX_OUTPUT_KRAMDOWN) {
            dialect = APEX_MD_DIALECT_KRAMDOWN;
        } else { /* APEX_OUTPUT_GFM */
            dialect = APEX_MD_DIALECT_GFM;
        }
        char *markdown = apex_cmark_to_markdown(document, options, dialect);
        /* Note: Cleanup happens at end of function - document and other resources
         * will be freed there. We return the markdown string here. */
        return markdown;
    }

    /* If output format is terminal/terminal256, serialize AST to ANSI terminal and return */
    if (options->output_format == APEX_OUTPUT_TERMINAL ||
        options->output_format == APEX_OUTPUT_TERMINAL256) {
        bool use_256 = (options->output_format == APEX_OUTPUT_TERMINAL256);
        char *tty = apex_cmark_to_terminal(document, options, use_256);
        return tty;
    }

    /* If output format is man (roff) or man-html, serialize AST and return */
    if (options->output_format == APEX_OUTPUT_MAN) {
        char *roff = apex_cmark_to_man_roff(document, options);
        return roff ? roff : strdup(".TH stub 1 \"\" \"\"\n");
    }
    if (options->output_format == APEX_OUTPUT_MAN_HTML) {
        char *man_html = apex_cmark_to_man_html(document, options);
        return man_html ? man_html : strdup("<!DOCTYPE html><html><body><p>stub</p></body></html>");
    }

    /* If output format is RTF, serialize AST and return */
    if (options->output_format == APEX_OUTPUT_RTF) {
        char *rtf = apex_cmark_to_rtf(document, options);
        return rtf ? rtf : strdup("{\\rtf1\\ansi\\deff0\\pard\\par}\n");
    }

    /* Render to HTML
     * Use custom renderer when we have attributes (IAL, ALDs, or image attributes)
     * Otherwise use standard renderer
     */
    PROFILE_START(rendering);
    char *html;
    if (img_attrs || alds || apex_mode_is_kramdown_or_unified_family(options->mode)) {
        /* Use custom renderer to inject attributes */
        html = apex_render_html_with_attributes(document, cmark_opts);
    } else {
        html = cmark_render_html(document, cmark_opts, NULL);
    }
    PROFILE_END(rendering);

    if (getenv("APEX_DEBUG_PIPELINE")) {
        size_t html_len = html ? strlen(html) : 0;
        fprintf(stderr, "[APEX_DEBUG] rendered html len=%zu\n", html_len);
        if (html_len > 0 && html_len < 600) {
            fprintf(stderr, "[APEX_DEBUG] html: %.500s%s\n", html, html_len > 500 ? "..." : "");
        }
    }

    /* Restore any protected Liquid tags in the rendered HTML */
    if (html && liquid_tags && liquid_tag_count > 0) {
        char *restored_html = apex_restore_liquid_tags(html, liquid_tags, liquid_tag_count);
        if (restored_html) {
            free(html);
            html = restored_html;
        }
    }

    /* Restore custom element tags from fenced-div wrappers (fixes out-of-order HTML) */
    if (html && options->enable_divs && apex_mode_is_unified_family(options->mode)) {
        char *fenced_divs_html = apex_postprocess_fenced_divs_html(html);
        if (fenced_divs_html) {
            free(html);
            html = fenced_divs_html;
        }
    }

    /* Post-process HTML for advanced table attributes (rowspan/colspan) */
    if (options->enable_tables && html) {
        PROFILE_START(inject_table_attributes);
        extern char *apex_inject_table_attributes(const char *html, cmark_node *document, int caption_position);
        char *processed_html = apex_inject_table_attributes(html, document, options->caption_position);
        PROFILE_END(inject_table_attributes);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Replace <hr> elements with Marked-style page breaks if requested */
    if (options->hr_page_break && html) {
        PROFILE_START(hr_page_break);
        char *processed_html = apex_replace_hr_with_pagebreak(html);
        PROFILE_END(hr_page_break);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Apply widont to headings if requested */
    if (options->enable_widont && html) {
        PROFILE_START(widont);
        char *processed_html = apex_apply_widont_to_headings(html);
        PROFILE_END(widont);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Add poetry class to code blocks without language if requested */
    if (options->code_is_poetry && html) {
        PROFILE_START(code_is_poetry);
        char *processed_html = apex_add_poetry_class_to_code_blocks(html);
        PROFILE_END(code_is_poetry);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Add hash prefix to footnote IDs if requested */
    if (options->random_footnote_ids && html && working_text) {
        PROFILE_START(footnote_hash_ids);
        /* Compute hash from original markdown content */
        char *hash_prefix = apex_compute_document_hash(working_text, strlen(working_text));
        if (hash_prefix) {
            char *processed_html = apex_add_hash_to_footnote_ids(html, hash_prefix);
            if (processed_html && processed_html != html) {
                free(html);
                html = processed_html;
            }
            free(hash_prefix);
        }
        PROFILE_END(footnote_hash_ids);
    }

    /* Insert page break before footnotes section if requested */
    if (options->page_break_before_footnotes && html) {
        PROFILE_START(page_break_before_footnotes);
        const char *marker = "<section class=\"footnotes\"";
        char *pos = strstr(html, marker);
        if (pos) {
            const char *replacement =
                "<div class=\"mkpagebreak manualbreak\" "
                "title=\"Page break created before footnotes\" "
                "data-description=\"PAGE (Footnotes)\" "
                "style=\"page-break-after:always\">"
                "<span style=\"display:none\">&nbsp;</span></div>";
            size_t html_len = strlen(html);
            size_t prefix_len = (size_t)(pos - html);
            size_t repl_len = strlen(replacement);
            size_t new_len = html_len + repl_len;
            char *with_break = malloc(new_len + 1);
            if (with_break) {
                memcpy(with_break, html, prefix_len);
                memcpy(with_break + prefix_len, replacement, repl_len);
                memcpy(with_break + prefix_len + repl_len, pos, html_len - prefix_len);
                with_break[new_len] = '\0';
                free(html);
                html = with_break;
            }
        }
        PROFILE_END(page_break_before_footnotes);
    }

    /* Extract metadata values needed for standalone HTML and post-processing BEFORE freeing metadata */
    /* We need to duplicate strings because metadata will be freed */
    char *css_metadata = NULL;
    char *html_header_metadata = NULL;
    char *html_footer_metadata = NULL;
    char *language_metadata = NULL;
    char *quotes_lang_metadata = NULL;
    char *generic_meta_tags = NULL;
    int base_header_level = 1;  /* Default is 1 */

    if (metadata) {
        /* Extract values we'll need later (before metadata is freed) and duplicate them */
        const char *css_val = apex_metadata_get(metadata, "css");
        if (css_val) css_metadata = strdup(css_val);

        const char *html_header_val = apex_metadata_get(metadata, "HTML Header");
        if (!html_header_val) {
            html_header_val = apex_metadata_get(metadata, "html header");
        }
        if (html_header_val) html_header_metadata = strdup(html_header_val);

        const char *html_footer_val = apex_metadata_get(metadata, "HTML Footer");
        if (!html_footer_val) {
            html_footer_val = apex_metadata_get(metadata, "html footer");
        }
        if (html_footer_val) html_footer_metadata = strdup(html_footer_val);

        const char *lang_val = apex_metadata_get(metadata, "language");
        if (lang_val) language_metadata = strdup(lang_val);

        /* Get quotes language */
        const char *quotes_lang_val = apex_metadata_get(metadata, "Quotes Language");
        if (!quotes_lang_val) {
            quotes_lang_val = apex_metadata_get(metadata, "quotes language");
        }
        if (!quotes_lang_val) {
            quotes_lang_val = apex_metadata_get(metadata, "quoteslanguage");
        }
        /* If language is set but quotes language is not, use language for quotes */
        if (!quotes_lang_val && lang_val) {
            quotes_lang_val = lang_val;
        }
        if (quotes_lang_val) quotes_lang_metadata = strdup(quotes_lang_val);

        /* Get header level */
        const char *header_level_str = apex_metadata_get(metadata, "HTML Header Level");
        if (!header_level_str) {
            header_level_str = apex_metadata_get(metadata, "Base Header Level");
        }
        if (header_level_str) {
            char *endptr;
            long level = strtol(header_level_str, &endptr, 10);
            if (endptr != header_level_str && level >= 1 && level <= 6) {
                base_header_level = (int)level;
            }
        }

        /* Collect remaining metadata as generic head meta tags. */
        generic_meta_tags = apex_render_generic_meta_tags(metadata);
    }

    /* Adjust header levels and quote language based on metadata */
    if (html) {
        if (base_header_level > 1) {
            PROFILE_START(adjust_header_levels);
            char *adjusted_html = apex_adjust_header_levels(html, base_header_level);
            PROFILE_END(adjust_header_levels);
            if (adjusted_html) {
                free(html);
                html = adjusted_html;
            }
        }

        if (quotes_lang_metadata) {
            PROFILE_START(adjust_quotes);
            char *adjusted_quotes = apex_adjust_quote_language(html, quotes_lang_metadata);
            PROFILE_END(adjust_quotes);
            if (adjusted_quotes) {
                free(html);
                html = adjusted_quotes;
            }
        }
    }

    /* Expand auto media (discover formats from filesystem for img with auto attribute).
     * Use base_directory when set (e.g. from file path or metadata); otherwise use "."
     * so auto expansion runs when piping stdin (images resolved relative to cwd). */
    if (html && strstr(html, "data-apex-replace-auto=1")) {
        PROFILE_START(expand_auto_media);
        const char *base = options->base_directory && options->base_directory[0]
            ? options->base_directory : ".";
        char *expanded = apex_expand_auto_media(html, base);
        PROFILE_END(expand_auto_media);
        if (expanded) {
            free(html);
            html = expanded;
        }
    }

    /* Convert images to figures with captions (caption="..." always wraps; otherwise when enable_image_captions) */
    if (html) {
        PROFILE_START(image_captions);
        char *with_captions = apex_convert_image_captions(html, options->enable_image_captions, options->title_captions_only);
        PROFILE_END(image_captions);
        if (with_captions) {
            free(html);
            html = with_captions;
        }
    }

    /* Strip redundant <p> around single <img> inside <figure> (e.g. from ::: >figure with "< ![Image](...)") */
    if (html) {
        char *stripped = apex_strip_figure_paragraph_wrapper(html);
        if (stripped) {
            free(html);
            html = stripped;
        }
    }

    /* Strip <p> that wraps only a single block element (figure, video, picture) - invalid HTML5 */
    if (html) {
        char *stripped = apex_strip_block_paragraph_wrapper(html);
        if (stripped) {
            free(html);
            html = stripped;
        }
    }

    /* Inject header IDs if enabled */
    if (options->generate_header_ids && html) {
        PROFILE_START(header_ids);
        char *processed_html = apex_inject_header_ids(html, document, true, options->header_anchors, options->id_format);
        PROFILE_END(header_ids);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    /* Obfuscate email links if requested */
    if (options->obfuscate_emails && html) {
        PROFILE_START(obfuscate_emails);
        char *obfuscated = apex_obfuscate_email_links(html);
        PROFILE_END(obfuscate_emails);
        if (obfuscated) {
            free(html);
            html = obfuscated;
        }
    }

    /* Embed images as base64 data URLs if requested (local images only) */
    if (options->embed_images && html) {
        PROFILE_START(embed_images);
        char *embedded = apex_embed_images(html, options, options->base_directory);
        PROFILE_END(embed_images);
        if (embedded) {
            free(html);
            html = embedded;
        }
    }

    /* Apply metadata variable replacement if enabled (post-processing for HTML attributes, etc.)
     * Note: Most replacements happen in preprocessing, but this handles edge cases in HTML
     */
    if (metadata && options->enable_metadata_variables && html) {
        PROFILE_START(metadata_replace);
        char *replaced = apex_metadata_replace_variables(html, metadata, options);
        PROFILE_END(metadata_replace);
        if (replaced && replaced != html) {
            free(html);
            html = replaced;
        } else if (replaced == html) {
            /* No replacements found, free the duplicate */
            free(replaced);
        }
    }

    /* Process TOC markers if enabled (Marked extensions) or in MultiMarkdown mode.
     * MultiMarkdown uses {{TOC}} syntax even when Marked extensions are disabled.
     */
    if ((options->enable_marked_extensions || options->mode == APEX_MODE_MULTIMARKDOWN) && html) {
        PROFILE_START(toc);
        char *with_toc = apex_process_toc(html, document, options->id_format,
                                          options->toc_min, options->toc_max);
        PROFILE_END(toc);
        if (with_toc) {
            free(html);
            html = with_toc;
        }
        char *restored = apex_restore_escaped_toc_markers(html);
        if (restored) {
            free(html);
            html = restored;
        }
    }

    /* Apply ARIA labels if enabled */
    if (options->enable_aria && html) {
        PROFILE_START(aria_labels);
        char *aria_html = apex_apply_aria_labels(html, document);
        PROFILE_END(aria_labels);
        if (aria_html && aria_html != html) {
            free(html);
            html = aria_html;
        }
    }

    /* Apply external syntax highlighting if requested */
    if (options->code_highlighter && html) {
        PROFILE_START(syntax_highlight);
        bool ansi_out = (options->output_format == APEX_OUTPUT_TERMINAL || options->output_format == APEX_OUTPUT_TERMINAL256);
        char *highlighted = apex_apply_syntax_highlighting(html,
                                                           options->code_highlighter,
                                                           options->code_line_numbers,
                                                           options->highlight_language_only,
                                                           ansi_out,
                                                           options->code_highlight_theme);
        PROFILE_END(syntax_highlight);
        if (highlighted && highlighted != html) {
            free(html);
            html = highlighted;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_code_attrs) && html) {
        PROFILE_START(code_fence_attrs_postprocess);
        char *fence_html = apex_postprocess_code_fence_attrs_html(html);
        PROFILE_END(code_fence_attrs_postprocess);
        if (fence_html) {
            free(html);
            html = fence_html;
        }
    }

    /* Replace abbreviations if any were found */
    if (abbreviations && html) {
        PROFILE_START(abbreviations);
        char *with_abbrs = apex_replace_abbreviations(html, abbreviations);
        PROFILE_END(abbreviations);
        if (with_abbrs) {
            free(html);
            html = with_abbrs;
        }
    }

    /* Replace GitHub emoji if enabled (default on in GFM / unified family) */
    if (options->enable_emoji && html) {
        PROFILE_START(emoji);
        char *with_emoji = apex_replace_emoji(html);
        PROFILE_END(emoji);
        if (with_emoji) {
            free(html);
            html = with_emoji;
        }
    }

    /* Render citations in HTML if enabled and bibliography is available */
    bool should_render_citations = false;
    if (citation_registry.bibliography) {
        should_render_citations = true;
    } else if (options->bibliography_files) {
        should_render_citations = true;
    } else if (options->csl_file) {
        should_render_citations = true;
    } else if (metadata) {
        const char *bib_value = apex_metadata_get(metadata, "bibliography");
        const char *csl_value = apex_metadata_get(metadata, "csl");
        if (bib_value || csl_value) {
            should_render_citations = true;
        }
    }

    if (options->enable_citations && html && should_render_citations) {
        if (citation_registry.count > 0) {
            PROFILE_START(citations_render);
            char *with_citations = apex_render_citations(html, &citation_registry, options);
            PROFILE_END(citations_render);
            if (with_citations) {
                free(html);
                html = with_citations;
            }
        }

        /* Insert bibliography at marker or end of document (even if no citations, if bibliography loaded) */
        if (html && !options->suppress_bibliography && citation_registry.bibliography) {
            PROFILE_START(bibliography);
            char *with_bibliography = apex_insert_bibliography(html, &citation_registry, options);
            PROFILE_END(bibliography);
            if (with_bibliography) {
                free(html);
                html = with_bibliography;
            }
        }
    }

    /* Render index markers and insert index */
    if (options->enable_indices && html && index_registry.count > 0) {
        PROFILE_START(index_render);
        char *with_index_markers = apex_render_index_markers(html, &index_registry, options);
        PROFILE_END(index_render);
        if (with_index_markers) {
            free(html);
            html = with_index_markers;
        }

        /* Insert index at marker or end of document */
        if (html) {
            PROFILE_START(index_insert);
            char *with_index = apex_insert_index(html, &index_registry, options);
            PROFILE_END(index_insert);
            if (with_index) {
                free(html);
                html = with_index;
            }
        }
    }

    /* Clean up index registry */
    apex_free_index_registry(&index_registry);

    /* Clean up HTML tag spacing (compress multiple spaces, remove spaces before >) */
    if (html) {
        PROFILE_START(html_clean);
        char *cleaned = apex_clean_html_tag_spacing(html);
        PROFILE_END(html_clean);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* In non-pretty mode, collapse extra newlines between adjacent tags so that
     * sequences like </table>\n\n<figure> become </table><figure>. This keeps
     * compact HTML output while still letting pretty mode control layout.
     */
    if (html && !local_opts.pretty) {
        PROFILE_START(collapse_intertag_newlines);
        char *collapsed = apex_collapse_intertag_newlines(html);
        PROFILE_END(collapse_intertag_newlines);
        if (collapsed) {
            free(html);
            html = collapsed;
        }
    }

    /* Convert thead to tbody for relaxed tables and remove empty thead from headerless tables */
    /* Only run this when relaxed_tables is enabled, otherwise keep thead as-is */
    if (html && options->enable_tables && options->relaxed_tables) {
        PROFILE_START(relaxed_tables_convert);
        char *converted = apex_convert_relaxed_table_headers(html);
        PROFILE_END(relaxed_tables_convert);
        if (converted) {
            free(html);
            html = converted;
        }
    }

    /* Post-process HTML to add style attributes to alpha lists */
    if (options->allow_alpha_lists && html) {
        PROFILE_START(alpha_lists_postprocess);
        char *processed_html = apex_postprocess_alpha_lists_html(html);
        PROFILE_END(alpha_lists_postprocess);
        if (processed_html && processed_html != html) {
            free(html);
            html = processed_html;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_roman_lists) && html) {
        PROFILE_START(roman_lists_postprocess);
        char *roman_html = apex_postprocess_roman_lists_html(html);
        PROFILE_END(roman_lists_postprocess);
        if (roman_html) {
            free(html);
            html = roman_html;
        }
    }

    if (apex_quarto_feature(options, options->enable_quarto_xrefs) && html) {
        PROFILE_START(quarto_xrefs_postprocess);
        char *xref_html = apex_postprocess_quarto_xrefs_html(html);
        PROFILE_END(quarto_xrefs_postprocess);
        if (xref_html) {
            free(html);
            html = xref_html;
        }
    }

    /* Remove empty paragraphs created by ^ marker (zero-width space only) */
    if (html && options->enable_marked_extensions) {
        PROFILE_START(remove_empty_paragraphs);
        char *cleaned = apex_remove_empty_paragraphs(html);
        PROFILE_END(remove_empty_paragraphs);
        if (cleaned && cleaned != html) {
            free(html);
            html = cleaned;
        }
    }

    /* Clean up */
    cmark_node_free(document);
    if (options->cmark_done) {
        options->cmark_done(parser, options, cmark_opts, options->cmark_user_data);
    }
    cmark_parser_free(parser);
    free(working_text);
    if (ial_preprocessed) free(ial_preprocessed);
    if (escaped_toc_protected) free(escaped_toc_protected);
    if (spans_preprocessed) free(spans_preprocessed);
    if (grid_tables_processed) free(grid_tables_processed);
    if (raw_content_processed) free(raw_content_processed);
    if (code_fence_attrs_processed) free(code_fence_attrs_processed);
    if (quarto_diagrams_processed) free(quarto_diagrams_processed);
    if (example_lists_processed) free(example_lists_processed);
    if (line_blocks_processed) free(line_blocks_processed);
    if (roman_lists_processed) free(roman_lists_processed);
    if (strict_lists_processed) free(strict_lists_processed);
    if (quarto_callouts_processed) free(quarto_callouts_processed);
    if (py_callouts_processed) free(py_callouts_processed);
    if (includes_processed) free(includes_processed);
    if (emoji_autocorrect_processed) free(emoji_autocorrect_processed);
    if (markers_processed_early) {
        /* Only free if alpha_lists_processed didn't use it */
        if (!alpha_lists_processed || alpha_lists_processed != markers_processed_early) {
            free(markers_processed_early);
        }
    }
    if (inline_footnotes_processed) free(inline_footnotes_processed);
    if (highlights_processed) free(highlights_processed);
    if (inserts_processed) free(inserts_processed);
    if (alpha_lists_processed) free(alpha_lists_processed);
    if (nested_ordered_sublists_processed) free(nested_ordered_sublists_processed);
    if (relaxed_tables_processed) free(relaxed_tables_processed);
    if (headerless_tables_processed) free(headerless_tables_processed);
    if (table_captions_processed) free(table_captions_processed);
    if (deflist_processed) free(deflist_processed);
    if (fenced_divs_processed) free(fenced_divs_processed);
    if (metadata_replaced) free(metadata_replaced);
    if (concordance_processed) free(concordance_processed);
    if (autolinks_processed) free(autolinks_processed);
    if (html_markdown_processed) free(html_markdown_processed);
    if (hashtags_processed) free(hashtags_processed);
    if (critic_processed) free(critic_processed);
    if (proofreader_processed) free(proofreader_processed);
    if (liquid_protected) free(liquid_protected);
    if (liquid_tags) {
        for (size_t i = 0; i < liquid_tag_count; i++) {
            free(liquid_tags[i]);
        }
        free(liquid_tags);
    }
    apex_free_metadata(metadata);
    apex_free_abbreviations(abbreviations);
    apex_free_alds(alds);
    apex_free_image_attributes(img_attrs);
    apex_free_citation_registry(&citation_registry);

    /* Post-render plugin phase: allow plugins to transform the final HTML
     * fragment before standalone wrapping and pretty-printing.
     */
    if (plugin_manager && html) {
        char *plugin_html = apex_plugins_run_text_phase(plugin_manager,
                                                        APEX_PLUGIN_PHASE_POST_RENDER,
                                                        html,
                                                        &local_opts);
        if (plugin_html) {
            free(html);
            html = plugin_html;
        }
    }

    /* Build script HTML (if any) from script_tags before wrapping or appending */
    char *scripts_html = NULL;
    const char *auto_mermaid_script = NULL;
    if (local_opts.standalone && html && local_opts.enable_quarto_diagrams &&
        apex_html_has_mermaid_diagram(html)) {
        bool has_mermaid_script = false;
        if (local_opts.script_tags) {
            for (char **p = local_opts.script_tags; *p; ++p) {
                if (*p && strstr(*p, "mermaid")) {
                    has_mermaid_script = true;
                    break;
                }
            }
        }
        if (!has_mermaid_script) {
            auto_mermaid_script = apex_mermaid_script_tag();
        }
    }

    if (local_opts.script_tags || auto_mermaid_script) {
        /* Join script tag snippets with newlines */
        size_t total_len = 0;
        size_t count = 0;
        for (char **p = local_opts.script_tags; p && *p; ++p) {
            size_t len = strlen(*p);
            if (len == 0) continue;
            total_len += len + 1; /* +1 for newline */
            count++;
        }
        if (auto_mermaid_script) {
            total_len += strlen(auto_mermaid_script) + 1;
            count++;
        }

        if (count > 0 && total_len > 0) {
            scripts_html = malloc(total_len + 1); /* +1 for null terminator */
            if (scripts_html) {
                size_t pos = 0;
                for (char **p = local_opts.script_tags; p && *p; ++p) {
                    size_t len = strlen(*p);
                    if (len == 0) continue;
                    memcpy(scripts_html + pos, *p, len);
                    pos += len;
                    scripts_html[pos++] = '\n';
                }
                if (auto_mermaid_script) {
                    size_t len = strlen(auto_mermaid_script);
                    memcpy(scripts_html + pos, auto_mermaid_script, len);
                    pos += len;
                    scripts_html[pos++] = '\n';
                }
                scripts_html[pos] = '\0';
            }
        }
    }

    /* Undefine the macro */
    #undef options

    /* Free plugin manager after all phases complete */
    if (plugin_manager) {
        apex_plugins_free(plugin_manager);
    }

    /* Extract title from first H1 if requested and no title is set */
    char *h1_title = NULL;
    if (local_opts.title_from_h1 && local_opts.standalone && html &&
        (!local_opts.document_title || local_opts.document_title[0] == '\0')) {
        h1_title = apex_extract_first_h1_text(html);
        if (h1_title) {
            local_opts.document_title = h1_title;
        }
    }

    /* Wrap in complete HTML document if requested */
    if (local_opts.standalone && html) {
        /* CSS precedence: CLI flag (--css/--style) overrides metadata */
        const char **css_paths = local_opts.stylesheet_paths;
        size_t css_count = local_opts.stylesheet_count;

        /* If no CLI stylesheets, check metadata for single CSS path */
        if (!css_paths || css_count == 0) {
            if (css_metadata) {
                /* Allocate array for single metadata stylesheet */
                css_paths = malloc(2 * sizeof(const char*));
                if (css_paths) {
                    css_paths[0] = css_metadata;
                    css_paths[1] = NULL;
                    css_count = 1;
                }
            }
        }

        /* Combine any existing HTML footer metadata with scripts (footer first, then scripts) */
        char *footer_with_scripts = NULL;
        if (html_footer_metadata || scripts_html) {
            size_t footer_len = html_footer_metadata ? strlen(html_footer_metadata) : 0;
            size_t scripts_len = scripts_html ? strlen(scripts_html) : 0;
            size_t extra_newline = (footer_len > 0 && scripts_len > 0) ? 1 : 0;

            footer_with_scripts = malloc(footer_len + extra_newline + scripts_len + 1);
            if (footer_with_scripts) {
                size_t pos = 0;
                if (footer_len > 0) {
                    memcpy(footer_with_scripts + pos, html_footer_metadata, footer_len);
                    pos += footer_len;
                }
                if (extra_newline) {
                    footer_with_scripts[pos++] = '\n';
                }
                if (scripts_len > 0) {
                    memcpy(footer_with_scripts + pos, scripts_html, scripts_len);
                    pos += scripts_len;
                }
                footer_with_scripts[pos] = '\0';
            }
        }

        const char *footer_to_use = footer_with_scripts ? footer_with_scripts : html_footer_metadata;

        /* Combine generated generic meta tags with any explicit HTML Header metadata. */
        char *combined_head_metadata = NULL;
        const char *head_to_use = html_header_metadata;
        if (generic_meta_tags || html_header_metadata) {
            size_t generic_len = generic_meta_tags ? strlen(generic_meta_tags) : 0;
            size_t header_len = html_header_metadata ? strlen(html_header_metadata) : 0;
            size_t newline_len = (generic_len > 0 && header_len > 0) ? 1 : 0;
            combined_head_metadata = malloc(generic_len + newline_len + header_len + 1);
            if (combined_head_metadata) {
                size_t pos = 0;
                if (generic_len > 0) {
                    memcpy(combined_head_metadata + pos, generic_meta_tags, generic_len);
                    pos += generic_len;
                }
                if (newline_len) {
                    combined_head_metadata[pos++] = '\n';
                }
                if (header_len > 0) {
                    memcpy(combined_head_metadata + pos, html_header_metadata, header_len);
                    pos += header_len;
                }
                combined_head_metadata[pos] = '\0';
                head_to_use = combined_head_metadata;
            }
        }

        PROFILE_START(standalone_wrap);
        char *document = apex_wrap_html_document(html, local_opts.document_title, css_paths, css_count,
                                                 local_opts.code_highlighter, head_to_use, footer_to_use,
                                                 language_metadata, local_opts.strict_xhtml);
        PROFILE_END(standalone_wrap);

        /* Free temporary metadata stylesheet array if we allocated it */
        if (css_paths && css_paths[0] == css_metadata) {
            free((void*)css_paths);
        }
        if (document) {
            free(html);
            html = document;
        }

        if (footer_with_scripts) {
            free(footer_with_scripts);
        }
        if (combined_head_metadata) {
            free(combined_head_metadata);
        }

        /* If requested, replace stylesheet links with embedded CSS contents */
        if (html && css_paths && css_count > 0 && local_opts.embed_stylesheet) {
            /* Process each stylesheet in reverse order to maintain correct positions */
            for (int i = (int)css_count - 1; i >= 0; i--) {
                if (!css_paths[i]) continue;

                const char *css_path = css_paths[i];
                char *css_content = NULL;
                size_t css_len = 0;

                /* Helper lambda-like block to read file into memory */
                {
                    FILE *css_fp = fopen(css_path, "rb");
                    if (!css_fp && local_opts.base_directory && local_opts.base_directory[0] != '\0') {
                        /* Try base_directory + "/" + css_path */
                        size_t base_len = strlen(local_opts.base_directory);
                        size_t path_len = strlen(css_path);
                        size_t full_len = base_len + 1 + path_len + 1;
                        char *full_path = malloc(full_len);
                        if (full_path) {
                            snprintf(full_path, full_len, "%s/%s", local_opts.base_directory, css_path);
                            css_fp = fopen(full_path, "rb");
                            free(full_path);
                        }
                    }

                    if (css_fp) {
                        if (fseek(css_fp, 0, SEEK_END) == 0) {
                            long fsize = ftell(css_fp);
                            if (fsize >= 0 && fsize < 10 * 1024 * 1024) { /* 10MB safety limit */
                                rewind(css_fp);
                                css_content = malloc((size_t)fsize + 1);
                                if (css_content) {
                                    css_len = fread(css_content, 1, (size_t)fsize, css_fp);
                                    css_content[css_len] = '\0';
                                }
                            }
                        }
                        fclose(css_fp);
                    }
                }

                if (css_content && css_len > 0) {
                    /* Build the exact link line we expect from apex_wrap_html_document */
                    char link_line[2048];
                    int link_n = snprintf(link_line, sizeof(link_line),
                                          "  <link rel=\"stylesheet\" href=\"%s\">\n",
                                          css_path);
                    if (link_n > 0 && (size_t)link_n < sizeof(link_line)) {
                        char *pos = strstr(html, link_line);
                        if (pos) {
                            size_t html_len = strlen(html);
                            size_t link_len = (size_t)link_n;
                            const char *style_prefix = "  <style>\n";
                            const char *style_suffix = "\n  </style>\n";
                            size_t prefix_len = strlen(style_prefix);
                            size_t suffix_len = strlen(style_suffix);

                            size_t new_len = html_len - link_len + prefix_len + css_len + suffix_len;
                            char *embedded = malloc(new_len + 1);
                            if (embedded) {
                                size_t before_len = (size_t)(pos - html);
                                memcpy(embedded, html, before_len);
                                size_t offset = before_len;

                                memcpy(embedded + offset, style_prefix, prefix_len);
                                offset += prefix_len;

                                memcpy(embedded + offset, css_content, css_len);
                                offset += css_len;

                                memcpy(embedded + offset, style_suffix, suffix_len);
                                offset += suffix_len;

                                size_t after_len = html_len - before_len - link_len;
                                if (after_len > 0) {
                                    memcpy(embedded + offset, pos + link_len, after_len);
                                    offset += after_len;
                                }

                                embedded[offset] = '\0';
                                free(html);
                                html = embedded;
                            }
                        }
                    }
                    free(css_content);
                }
            }
        }
    } else if (html && scripts_html) {
        /* Snippet mode: append scripts to the end of the HTML fragment */
        size_t html_len = strlen(html);
        size_t scripts_len = strlen(scripts_html);
        size_t extra_newline = (html_len > 0 && scripts_len > 0 && html[html_len - 1] != '\n') ? 1 : 0;

        char *combined = malloc(html_len + extra_newline + scripts_len + 1);
        if (combined) {
            size_t pos = 0;
            if (html_len > 0) {
                memcpy(combined + pos, html, html_len);
                pos += html_len;
            }
            if (extra_newline) {
                combined[pos++] = '\n';
            }
            if (scripts_len > 0) {
                memcpy(combined + pos, scripts_html, scripts_len);
                pos += scripts_len;
            }
            combined[pos] = '\0';

            free(html);
            html = combined;
        }
    }

    if (scripts_html) {
        free(scripts_html);
    }

    /* Free duplicated metadata strings */
    if (css_metadata) free(css_metadata);
    if (html_header_metadata) free(html_header_metadata);
    if (html_footer_metadata) free(html_footer_metadata);
    if (language_metadata) free(language_metadata);
    if (quotes_lang_metadata) free(quotes_lang_metadata);
    if (generic_meta_tags) free(generic_meta_tags);
    if (h1_title) free(h1_title);

    /* Remove blank lines within tables (applies to both pretty and non-pretty) */
    if (html) {
        PROFILE_START(remove_table_blank_lines);
        char *cleaned = apex_remove_table_blank_lines(html);
        PROFILE_END(remove_table_blank_lines);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* Remove table separator rows that were incorrectly rendered as data rows */
    /* This happens when smart typography converts --- to — in separator rows */
    if (html && local_opts.enable_tables) {
        PROFILE_START(remove_table_separator_rows);
        extern char *apex_remove_table_separator_rows(const char *html);
        char *cleaned = apex_remove_table_separator_rows(html);
        PROFILE_END(remove_table_separator_rows);
        if (cleaned) {
            free(html);
            html = cleaned;
        }
    }

    /* Pretty-print HTML if requested */
    if (local_opts.pretty && html) {
        PROFILE_START(pretty_print);
        char *pretty = apex_pretty_print_html(html);
        PROFILE_END(pretty_print);
        if (pretty) {
            free(html);
            html = pretty;
        }
    }

    /* XHTML-style void elements (--xhtml / --strict-xhtml); run after pretty-print (HTML only) */
    if (local_opts.xhtml && html && options->output_format == APEX_OUTPUT_HTML) {
        PROFILE_START(xhtml_void_tags);
        char *xhtml_out = apex_html_apply_xhtml_void_tags(html);
        PROFILE_END(xhtml_void_tags);
        if (xhtml_out) {
            free(html);
            html = xhtml_out;
        }
    }

    PROFILE_END(total);

    if (profiling_enabled()) {
        fprintf(stderr, "[PROFILE] %-30s: %8s\n", "---", "---");
    }

    return html;
}

/**
 * Wrap HTML content in complete HTML5 document structure
 */
char *apex_wrap_html_document(const char *content, const char *title, const char **stylesheet_paths, size_t stylesheet_count, const char *code_highlighter, const char *html_header, const char *html_footer, const char *language, bool strict_xhtml) {
    if (!content) return NULL;

    const char *doc_title = title ? title : "Document";
    const char *lang = language ? language : "en";

    /* Strip any existing </body></html> tags from end of content to avoid duplicates */
    size_t content_len = strlen(content);
    const char *html_end = NULL;
    const char *body_end = NULL;

    /* Find last occurrence of </html> near the end */
    const char *search = content + content_len;
    while (search > content) {
        search--;
        if (strncmp(search, "</html>", 7) == 0) {
            html_end = search;
            break;
        }
    }

    /* If we found </html>, look for </body> before it */
    if (html_end) {
        search = html_end;
        while (search > content) {
            search--;
            if (strncmp(search, "</body>", 7) == 0) {
                body_end = search;
                break;
            }
            /* Stop if we hit a non-whitespace character before finding </body> */
            if (!isspace((unsigned char)*search) && *search != '>') {
                break;
            }
        }

        /* If we found both tags at the end, strip them */
        if (body_end) {
            /* Check that there's only whitespace/newlines between </body> and </html> */
            const char *between = body_end + 7;
            bool only_whitespace = true;
            while (between < html_end) {
                if (!isspace((unsigned char)*between)) {
                    only_whitespace = false;
                    break;
                }
                between++;
            }

            if (only_whitespace) {
                /* Strip everything from </body> to end */
                content_len = body_end - content;
            }
        }
    }
    size_t title_len = strlen(doc_title);
    /* Calculate total length for all stylesheet links */
    size_t style_len = 0;
    if (stylesheet_paths && stylesheet_count > 0) {
        for (size_t i = 0; i < stylesheet_count && stylesheet_paths[i]; i++) {
            style_len += strlen(stylesheet_paths[i]) + 50; /* +50 for link tag overhead */
        }
    }
    /* Add space for syntax highlighting CSS if needed */
    size_t syntax_css_len = 0;
    if (code_highlighter) {
        syntax_css_len = 8000; /* Approximate size for syntax CSS */
    }
    size_t header_len = html_header ? strlen(html_header) : 0;
    size_t footer_len = html_footer ? strlen(html_footer) : 0;
    size_t lang_len = strlen(lang);
    /* Need generous space for styles (1.5KB) + structure + header + footer + syntax CSS */
    size_t capacity = content_len + title_len + style_len + syntax_css_len + header_len + footer_len + lang_len + 4096;

    char *output = malloc(capacity + 1);  /* +1 for null terminator */
    if (!output) return strdup(content);

    char *write = output;
    size_t remaining = capacity + 1;  /* Include null terminator in remaining count */

    /* Ensure we have a valid version string */
    const char *version_str = APEX_VERSION_STRING;
    if (!version_str) version_str = "unknown";

    /* HTML5 doctype and opening (polyglot XHTML when strict_xhtml) */
    /* Add body class if code highlighting is enabled */
    const char *body_class = code_highlighter ? " class=\"code-highlighted\"" : "";
    int n;
    if (strict_xhtml) {
        n = snprintf(write, remaining,
                     "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<!DOCTYPE html>\n"
                     "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"%s\" lang=\"%s\">\n<head>\n",
                     lang, lang);
    } else {
        n = snprintf(write, remaining, "<!DOCTYPE html>\n<html lang=\"%s\">\n<head>\n", lang);
    }
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    /* Meta tags */
    if (strict_xhtml) {
        n = snprintf(write, remaining,
                     "  <meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=UTF-8\" />\n");
    } else {
        n = snprintf(write, remaining, "  <meta charset=\"UTF-8\">\n");
    }
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    n = snprintf(write, remaining, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    n = snprintf(write, remaining, "  <meta name=\"generator\" content=\"Apex %s\">\n", version_str);
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    /* Title */
    n = snprintf(write, remaining, "  <title>%s</title>\n", doc_title);
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    /* Syntax highlighting CSS if code highlighter is enabled */
    if (code_highlighter) {
        /* Write CSS directly to buffer to avoid string literal length warning */
        n = snprintf(write, remaining, "  <style>\n"
            "    /* GitHub-style syntax highlighting for Pygments and Skylighting */\n"
            "    /* Don't apply background to wrapper when code highlighting is enabled - let default styles handle it */\n"
            "    .code-highlighted .highlight, .code-highlighted .sourceCode { background: inherit; border-radius: 6px; padding: 16px; overflow-x: auto; }\n"
            "    .highlight pre, .sourceCode pre { margin: 0; padding: 0; background: transparent; }\n"
            "    .highlight code, .sourceCode code { font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace; font-size: 12px; line-height: 1.45; }\n");
        if (n < 0 || (size_t)n >= remaining) {
            /* Need to expand buffer */
            size_t written = write - output;
            size_t new_cap = (written + 6000) * 2;
            char *new_output = realloc(output, new_cap);
            if (!new_output) {
                free(output);
                return strdup(content);
            }
            output = new_output;
            write = output + written;
            remaining = new_cap - written;
            capacity = new_cap;
            /* Retry */
            n = snprintf(write, remaining, "  <style>\n"
                "    /* GitHub-style syntax highlighting for Pygments and Skylighting */\n"
                "    /* Don't apply background to wrapper when code highlighting is enabled - let default styles handle it */\n"
                "    .code-highlighted .highlight, .code-highlighted .sourceCode { background: inherit; border-radius: 6px; padding: 16px; overflow-x: auto; }\n"
                "    .highlight pre, .sourceCode pre { margin: 0; padding: 0; background: transparent; }\n"
                "    .highlight code, .sourceCode code { font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace; font-size: 12px; line-height: 1.45; }\n");
            if (n < 0 || (size_t)n >= remaining) {
                free(output);
                return strdup(content);
            }
        }
        write += n;
        remaining -= n;

        /* Write Pygments classes comment */
        n = snprintf(write, remaining, "    /* Pygments classes */\n");
        if (n < 0 || (size_t)n >= remaining) {
            free(output);
            return strdup(content);
        }
        write += n;
        remaining -= n;

        /* Write Pygments classes in chunks */
        const char *pygments_css =
            "    .highlight .k { color: #d73a49; font-weight: 600; } /* Keyword */\n"
            "    .highlight .kt { color: #d73a49; } /* Keyword.Type */\n"
            "    .highlight .kd { color: #d73a49; font-weight: 600; } /* Keyword.Declaration */\n"
            "    .highlight .kn { color: #d73a49; } /* Keyword.Namespace */\n"
            "    .highlight .kp { color: #d73a49; } /* Keyword.Pseudo */\n"
            "    .highlight .kr { color: #d73a49; font-weight: 600; } /* Keyword.Reserved */\n"
            "    .highlight .n { color: #6f42c1; } /* Name */\n"
            "    .highlight .na { color: #6f42c1; } /* Name.Attribute */\n"
            "    .highlight .nc { color: #6f42c1; font-weight: 600; } /* Name.Class */\n"
            "    .highlight .no { color: #005cc5; } /* Name.Constant */\n"
            "    .highlight .nd { color: #6f42c1; font-weight: 600; } /* Name.Decorator */\n"
            "    .highlight .ni { color: #800080; } /* Name.Entity */\n"
            "    .highlight .ne { color: #990000; font-weight: 600; } /* Name.Exception */\n"
            "    .highlight .nf { color: #6f42c1; font-weight: 600; } /* Name.Function */\n"
            "    .highlight .nl { color: #6f42c1; } /* Name.Label */\n"
            "    .highlight .nn { color: #555; } /* Name.Namespace */\n"
            "    .highlight .nt { color: #22863a; } /* Name.Tag */\n"
            "    .highlight .nv { color: #e36209; } /* Name.Variable */\n"
            "    .highlight .s { color: #032f62; } /* String */\n"
            "    .highlight .sb { color: #032f62; } /* String.Backtick */\n"
            "    .highlight .sc { color: #032f62; } /* String.Char */\n"
            "    .highlight .sd { color: #032f62; } /* String.Doc */\n"
            "    .highlight .s2 { color: #032f62; } /* String.Double */\n"
            "    .highlight .se { color: #032f62; } /* String.Escape */\n"
            "    .highlight .sh { color: #032f62; } /* String.Heredoc */\n"
            "    .highlight .si { color: #032f62; } /* String.Interpol */\n"
            "    .highlight .sx { color: #032f62; } /* String.Other */\n"
            "    .highlight .sr { color: #032f62; } /* String.Regex */\n"
            "    .highlight .s1 { color: #032f62; } /* String.Single */\n"
            "    .highlight .ss { color: #032f62; } /* String.Symbol */\n"
            "    .highlight .c { color: #6a737d; font-style: italic; } /* Comment */\n"
            "    .highlight .c1 { color: #6a737d; font-style: italic; } /* Comment.Single */\n"
            "    .highlight .cm { color: #6a737d; font-style: italic; } /* Comment.Multiline */\n"
            "    .highlight .cp { color: #6a737d; font-weight: 600; } /* Comment.Preproc */\n"
            "    .highlight .cs { color: #6a737d; font-weight: 600; font-style: italic; } /* Comment.Special */\n"
            "    .highlight .m { color: #005cc5; } /* Literal.Number */\n"
            "    .highlight .mb { color: #005cc5; } /* Literal.Number.Bin */\n"
            "    .highlight .mf { color: #005cc5; } /* Literal.Number.Float */\n"
            "    .highlight .mh { color: #005cc5; } /* Literal.Number.Hex */\n"
            "    .highlight .mi { color: #005cc5; } /* Literal.Number.Integer */\n"
            "    .highlight .il { color: #005cc5; } /* Literal.Number.Integer.Long */\n"
            "    .highlight .mo { color: #005cc5; } /* Literal.Number.Oct */\n"
            "    .highlight .o { color: #d73a49; } /* Operator */\n"
            "    .highlight .ow { color: #d73a49; } /* Operator.Word */\n"
            "    .highlight .p { color: #24292e; } /* Punctuation */\n"
            "    .highlight .w { color: #e1e4e8; } /* Text.Whitespace */\n";
        size_t pygments_len = strlen(pygments_css);
        if (pygments_len >= remaining) {
            size_t written = write - output;
            size_t new_cap = (written + pygments_len + 1) * 2;
            char *new_output = realloc(output, new_cap);
            if (!new_output) {
                free(output);
                return strdup(content);
            }
            output = new_output;
            write = output + written;
            remaining = new_cap - written;
            capacity = new_cap;
        }
        memcpy(write, pygments_css, pygments_len);
        write += pygments_len;
        remaining -= pygments_len;

        /* Write Skylighting classes */
        n = snprintf(write, remaining, "    /* Skylighting classes */\n");
        if (n < 0 || (size_t)n >= remaining) {
            free(output);
            return strdup(content);
        }
        write += n;
        remaining -= n;

        const char *skylighting_css =
            "    .sourceCode .kw { color: #d73a49; font-weight: 600; } /* Keyword */\n"
            "    .sourceCode .dt { color: #6f42c1; } /* DataType */\n"
            "    .sourceCode .dv { color: #005cc5; } /* DecVal */\n"
            "    .sourceCode .bn { color: #005cc5; } /* BaseN */\n"
            "    .sourceCode .fl { color: #005cc5; } /* Float */\n"
            "    .sourceCode .ch { color: #032f62; } /* Char */\n"
            "    .sourceCode .st { color: #032f62; } /* String */\n"
            "    .sourceCode .co { color: #6a737d; font-style: italic; } /* Comment */\n"
            "    .sourceCode .ot { color: #22863a; } /* Other */\n"
            "    .sourceCode .al { color: #e36209; font-weight: 600; } /* Alert */\n"
            "    .sourceCode .fu { color: #6f42c1; font-weight: 600; } /* Function */\n"
            "    .sourceCode .re { color: #032f62; } /* RegionMarker */\n"
            "    .sourceCode .er { color: #d73a49; font-weight: 600; } /* Error */\n"
            "    .sourceCode .cf { color: #d73a49; font-weight: 600; } /* ControlFlow */\n"
            "    .sourceCode .op { color: #d73a49; } /* Operator */\n"
            "    .sourceCode .pp { color: #6a737d; } /* Preprocessor */\n"
            "    .sourceCode .at { color: #005cc5; } /* Attribute */\n"
            "    .sourceCode .do { color: #6a737d; font-style: italic; } /* Documentation */\n"
            "    .sourceCode .an { color: #6a737d; font-weight: 600; } /* Annotation */\n"
            "    .sourceCode .cv { color: #6a737d; font-weight: 600; font-style: italic; } /* CommentVar */\n"
            "    .sourceCode .in { color: #6a737d; } /* Information */\n"
            "    .sourceCode .wa { color: #e36209; font-weight: 600; } /* Warning */\n"
            "    .sourceCode .im { color: #d73a49; } /* Import */\n"
            "    .sourceCode .bu { color: #005cc5; } /* BuiltIn */\n"
            "    .sourceCode .ex { color: #6f42c1; } /* Extension */\n"
            "    .sourceCode .va { color: #e36209; } /* Variable */\n"
            "    .sourceCode .ss { color: #032f62; } /* SpecialString */\n"
            "    .sourceCode .sc { color: #032f62; } /* SpecialChar */\n"
            "    .sourceCode .vs { color: #032f62; } /* VerbatimString */\n"
            "    .sourceCode .il { color: #005cc5; } /* Special */\n";
        size_t skylighting_len = strlen(skylighting_css);
        if (skylighting_len >= remaining) {
            size_t written = write - output;
            size_t new_cap = (written + skylighting_len + 1) * 2;
            char *new_output = realloc(output, new_cap);
            if (!new_output) {
                free(output);
                return strdup(content);
            }
            output = new_output;
            write = output + written;
            remaining = new_cap - written;
            capacity = new_cap;
        }
        memcpy(write, skylighting_css, skylighting_len);
        write += skylighting_len;
        remaining -= skylighting_len;

        /* Write line numbers CSS and close style tag */
        n = snprintf(write, remaining,
            "    /* Line numbers (Skylighting) */\n"
            "    .sourceCode.numberSource .sourceCode { counter-reset: line; }\n"
            "    .sourceCode.numberSource .sourceCode > span { position: relative; left: -4em; counter-increment: line; }\n"
            "    .sourceCode.numberSource .sourceCode > span > a:first-child::before { content: counter(line); position: relative; left: -1em; text-align: right; vertical-align: baseline; border: none; display: inline-block; min-width: 1em; padding-right: 0.5em; color: #aaa; }\n"
            "  </style>\n");
        if (n < 0 || (size_t)n >= remaining) {
            size_t written = write - output;
            size_t new_cap = (written + 500) * 2;
            char *new_output = realloc(output, new_cap);
            if (!new_output) {
                free(output);
                return strdup(content);
            }
            output = new_output;
            write = output + written;
            remaining = new_cap - written;
            capacity = new_cap;
            /* Retry */
            n = snprintf(write, remaining,
                "    /* Line numbers (Skylighting) */\n"
                "    .sourceCode.numberSource .sourceCode { counter-reset: line; }\n"
                "    .sourceCode.numberSource .sourceCode > span { position: relative; left: -4em; counter-increment: line; }\n"
                "    .sourceCode.numberSource .sourceCode > span > a:first-child::before { content: counter(line); position: relative; left: -1em; text-align: right; vertical-align: baseline; border: none; display: inline-block; min-width: 1em; padding-right: 0.5em; color: #aaa; }\n"
                "  </style>\n");
            if (n < 0 || (size_t)n >= remaining) {
                free(output);
                return strdup(content);
            }
        }
        write += n;
        remaining -= n;
    }

    /* Stylesheet links if provided */
    if (stylesheet_paths && stylesheet_count > 0) {
        for (size_t i = 0; i < stylesheet_count && stylesheet_paths[i]; i++) {
            n = snprintf(write, remaining, "  <link rel=\"stylesheet\" href=\"%s\">\n", stylesheet_paths[i]);
            if (n < 0 || (size_t)n >= remaining) {
                /* Need to expand buffer */
                size_t written = write - output;
                size_t new_cap = (written + strlen(stylesheet_paths[i]) + 100) * 2;
                char *new_output = realloc(output, new_cap);
                if (!new_output) {
                    free(output);
                    return strdup(content);
                }
                output = new_output;
                write = output + written;
                remaining = new_cap - written;
                capacity = new_cap;
                /* Retry */
                n = snprintf(write, remaining, "  <link rel=\"stylesheet\" href=\"%s\">\n", stylesheet_paths[i]);
                if (n < 0 || (size_t)n >= remaining) {
                    free(output);
                    return strdup(content);
                }
            }
            write += n;
            remaining -= n;
        }
    } else {
        /* Include minimal default styles */
        const char *styles = "  <style>\n"
            "    body {\n"
            "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;\n"
            "      line-height: 1.6;\n"
            "      max-width: 800px;\n"
            "      margin: 2rem auto;\n"
            "      padding: 0 1rem;\n"
            "      color: #333;\n"
            "    }\n"
            "    pre { background: #f5f5f5; padding: 1rem; overflow-x: auto; }\n"
            "    code { background: #f0f0f0; padding: 0.2em 0.4em; border-radius: 3px; }\n"
            "    blockquote { border-left: 4px solid #ddd; margin: 0; padding-left: 1rem; color: #666; }\n"
            "    table { border-collapse: collapse; width: 100%%; }\n"
            "    th, td { border: 1px solid #ddd; padding: 0.5rem; }\n"
            "    th { background: #f5f5f5; }\n"
            "    tfoot td { background: #e8e8e8; }\n"
            "    figure.table-figure { width: fit-content; margin: 1em 0; }\n"
            "    figure.table-figure table { width: auto; }\n"
            "    figcaption { text-align: center; font-weight: bold; font-size: 0.8em; }\n"
            "    .page-break { page-break-after: always; }\n"
            "    .callout { padding: 1rem; margin: 1rem 0; border-left: 4px solid; }\n"
            "    .callout-note { border-color: #3b82f6; background: #eff6ff; }\n"
            "    .callout-warning { border-color: #f59e0b; background: #fffbeb; }\n"
            "    .callout-tip { border-color: #10b981; background: #f0fdf4; }\n"
            "    .callout-danger { border-color: #ef4444; background: #fef2f2; }\n"
            "    ins { background: #d4fcbc; text-decoration: none; }\n"
            "    del { background: #fbb6c2; text-decoration: line-through; }\n"
            "    mark { background: #fff3cd; }\n"
            "    .smallcaps { font-variant: small-caps; }\n"
            "    .underline { text-decoration: underline; }\n"
            "    span.mark { background: #fff3cd; }\n"
            "    .hidden { display: none; }\n"
            "    .quarto-xref { font-variant: normal; }\n"
            "    .critic.comment { background: #e7e7e7; color: #666; font-style: italic; }\n"
            "    .mkhashtag { color: #666; }\n"
            "    .mkstyledtag {\n"
            "      display: inline-block;\n"
            "      background: #e0e0e0;\n"
            "      padding: 3px 9px;\n"
            "      border-radius: 20px;\n"
            "      font-size: 0.9em;\n"
            "      line-height: 1.4;\n"
            "      color: #333;\n"
            "      margin: 0 2px;\n"
            "    }\n"
            "  </style>\n";
        size_t styles_len = strlen(styles);
        if (styles_len >= remaining) {
            free(output);
            return strdup(content);
        }
        memcpy(write, styles, styles_len);
        write += styles_len;
        remaining -= styles_len;
    }

    /* HTML Header metadata - raw HTML inserted in <head> */
    if (html_header) {
        n = snprintf(write, remaining, "  %s\n", html_header);
        if (n < 0 || (size_t)n >= remaining) {
            free(output);
            return strdup(content);
        }
        write += n;
        remaining -= n;
    }

    /* Close head, open body */
    n = snprintf(write, remaining, "</head>\n<body%s>\n\n", body_class);
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    /* Content */
    if (content_len + 1 > remaining) {  /* +1 for null terminator at end */
        free(output);
        return strdup(content);
    }
    memcpy(write, content, content_len);
    write += content_len;
    remaining -= content_len;

    /* HTML Footer metadata - raw HTML appended before </body> */
    if (html_footer) {
        n = snprintf(write, remaining, "\n%s", html_footer);
        if (n < 0 || (size_t)n >= remaining) {
            free(output);
            return strdup(content);
        }
        write += n;
        remaining -= n;
    }

    /* Close body and html */
    n = snprintf(write, remaining, "\n</body>\n</html>\n");
    if (n < 0 || (size_t)n >= remaining) {
        free(output);
        return strdup(content);
    }
    write += n;
    remaining -= n;

    /* Null terminate - ensure we have space */
    if (remaining > 0) {
        *write = '\0';
    } else {
        /* Should never happen, but be safe */
        free(output);
        return strdup(content);
    }

    return output;
}

/**
 * Free a string allocated by Apex
 */
void apex_free_string(char *str) {
    if (str) {
        free(str);
    }
}

/**
 * Version information
 */
const char *apex_version_string(void) {
    return APEX_VERSION_STRING;
}

int apex_version_major(void) {
    return APEX_VERSION_MAJOR;
}

int apex_version_minor(void) {
    return APEX_VERSION_MINOR;
}

int apex_version_patch(void) {
    return APEX_VERSION_PATCH;
}
