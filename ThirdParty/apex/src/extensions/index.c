/**
 * Index Extension for Apex
 * Implementation
 */

#include "index.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

static int apex_idx_ptrdiff_to_int(ptrdiff_t v) {
    if (v <= 0) return 0;
    if (v > INT_MAX) return INT_MAX;
    return (int)v;
}

static int apex_idx_size_to_int(size_t v) {
    if (v > (size_t)INT_MAX) return INT_MAX;
    return (int)v;
}

/* Index placeholder prefix - we'll use a unique marker */
#define INDEX_PLACEHOLDER_PREFIX "<!--IDX:"
#define INDEX_PLACEHOLDER_SUFFIX "-->"
/* Protects literal {^...} marks in {^-} regions from later superscript */
#define TEXTINDEX_CARET_PLACEHOLDER "APEXTICARET"

/**
 * Check if character is valid in index term
 * Index terms can contain letters, digits, spaces, and common punctuation
 */
static bool is_valid_index_char(char c) {
    return isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '/' ||
           c == '.' || c == ',' || c == ':' || c == ';' || c == '\'' || c == '"';
}

/**
 * Trim whitespace from string (in-place)
 */
static char *trim_string(char *str) {
    if (!str) return NULL;

    /* Trim leading whitespace */
    while (*str && isspace((unsigned char)*str)) str++;

    /* Trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * Strip Markdown single-underscore emphasis markers, keeping the inner text.
 * TextIndex uses _…_ in headings; wildcards insert the stripped form.
 */
static char *strip_underscore_emphasis(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;

    size_t wi = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '_') {
            /* Look for a matching closing _ */
            size_t j = i + 1;
            while (j < len && s[j] != '_') j++;
            if (j < len && j > i + 1) {
                /* Copy inner text without the underscores */
                for (size_t k = i + 1; k < j; k++) {
                    out[wi++] = s[k];
                }
                i = j;
                continue;
            }
        }
        out[wi++] = s[i];
    }
    out[wi] = '\0';
    return out;
}

/**
 * Convert TextIndex Markdown underscore emphasis to HTML <em> tags.
 */
static char *underscore_emphasis_to_html(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s);
    /* Worst case: every char becomes part of <em></em> expansion */
    char *out = malloc(len * 6 + 1);
    if (!out) return NULL;

    size_t wi = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '_') {
            size_t j = i + 1;
            while (j < len && s[j] != '_') j++;
            if (j < len && j > i + 1) {
                memcpy(out + wi, "<em>", 4);
                wi += 4;
                for (size_t k = i + 1; k < j; k++) {
                    out[wi++] = s[k];
                }
                memcpy(out + wi, "</em>", 5);
                wi += 5;
                i = j;
                continue;
            }
        }
        out[wi++] = s[i];
    }
    out[wi] = '\0';
    return out;
}

/**
 * Expand TextIndex * / ** wildcards using the preceding heading.
 * *  → preceding with emphasis stripped
 * ** → same, lowercased
 * Returns a new string; template is unchanged.
 */
static char *expand_textindex_wildcards(const char *template, const char *preceding) {
    if (!template) return NULL;

    char *stripped = preceding ? strip_underscore_emphasis(preceding) : strdup("");
    if (!stripped) return NULL;

    char *lowered = strdup(stripped);
    if (!lowered) {
        free(stripped);
        return NULL;
    }
    for (char *p = lowered; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }

    size_t tlen = strlen(template);
    size_t slen = strlen(stripped);
    size_t llen = strlen(lowered);
    /* Generous: every char could expand to lowered preceding */
    size_t cap = tlen * (llen > slen ? llen : slen) + tlen + 1;
    if (cap < tlen + 1) cap = tlen + 1;
    char *out = malloc(cap);
    if (!out) {
        free(stripped);
        free(lowered);
        return NULL;
    }

    size_t wi = 0;
    for (size_t i = 0; i < tlen; i++) {
        if (template[i] == '*') {
            const char *ins = stripped;
            size_t ilen = slen;
            if (i + 1 < tlen && template[i + 1] == '*') {
                ins = lowered;
                ilen = llen;
                i++;  /* consume second * */
            }
            if (wi + ilen + 1 > cap) {
                cap = (wi + ilen + 1) * 2;
                char *n = realloc(out, cap);
                if (!n) {
                    free(out);
                    free(stripped);
                    free(lowered);
                    return NULL;
                }
                out = n;
            }
            memcpy(out + wi, ins, ilen);
            wi += ilen;
            continue;
        }
        if (wi + 2 > cap) {
            cap = (wi + 2) * 2;
            char *n = realloc(out, cap);
            if (!n) {
                free(out);
                free(stripped);
                free(lowered);
                return NULL;
            }
            out = n;
        }
        out[wi++] = template[i];
    }
    out[wi] = '\0';
    free(stripped);
    free(lowered);
    return out;
}

/**
 * True if template uses a TextIndex * / ** wildcard.
 */
static bool textindex_has_wildcard(const char *s) {
    return s && strchr(s, '*') != NULL;
}

/**
 * Strip HTML tags for sorting / letter grouping of index terms.
 */
static char *strip_html_tags(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t wi = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '<') {
            while (i < len && s[i] != '>') i++;
            continue;
        }
        out[wi++] = s[i];
    }
    out[wi] = '\0';
    return out;
}

/**
 * Detect TextIndex prefix wildcards: *^ (full path) or *^- (label only).
 * Returns 1 for *^, 2 for *^-, 0 otherwise. Ignores trailing ! or /.
 */
static int textindex_prefix_wildcard_kind(const char *s) {
    if (!s || s[0] != '*') return 0;
    char buf[16];
    size_t n = 0;
    for (const char *p = s; *p && n + 1 < sizeof(buf); p++) {
        if (*p == '!' || *p == '/') break;
        buf[n++] = *p;
    }
    buf[n] = '\0';
    while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
    if (strcmp(buf, "*^-") == 0) return 2;
    if (strcmp(buf, "*^") == 0) return 1;
    return 0;
}

/**
 * Case-sensitive prefix match of label against an index heading (HTML stripped).
 */
static bool textindex_heading_has_prefix(const char *heading, const char *label) {
    if (!heading || !label || !label[0]) return false;
    char *plain = strip_html_tags(heading);
    if (!plain) return false;
    size_t llen = strlen(label);
    bool match = strncmp(plain, label, llen) == 0;
    free(plain);
    return match;
}

/**
 * Find the earliest registry entry whose item or subitem starts with label.
 * label should already have underscore emphasis stripped.
 */
static const apex_index_entry *textindex_find_prefix_entry(const apex_index_registry *registry,
                                                           const char *label) {
    if (!registry || !label || !label[0]) return NULL;

    const apex_index_entry *best = NULL;
    for (const apex_index_entry *e = registry->entries; e; e = e->next) {
        bool match = textindex_heading_has_prefix(e->item, label);
        if (!match && e->subitem) {
            match = textindex_heading_has_prefix(e->subitem, label);
        }
        if (match && (!best || e->position < best->position)) {
            best = e;
        }
    }
    return best;
}

/**
 * Parse one TextIndex path segment (quoted or bare) into newly allocated text.
 * Advances *pp past the segment. Returns NULL on empty/failure.
 */
static char *textindex_parse_path_segment(const char **pp) {
    const char *p = *pp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p || *p == '>' || *p == ';' || *p == '|' || *p == '~' || *p == '[') {
        *pp = p;
        return NULL;
    }

    char *seg = NULL;
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        const char *start = p;
        while (*p) {
            if (*p == '\\' && p[1]) {
                p += 2;
                continue;
            }
            if (*p == quote) break;
            p++;
        }
        size_t len = (size_t)(p - start);
        seg = malloc(len + 1);
        if (seg) {
            size_t wi = 0;
            for (size_t i = 0; i < len; i++) {
                if (start[i] == '\\' && i + 1 < len) {
                    seg[wi++] = start[++i];
                } else {
                    seg[wi++] = start[i];
                }
            }
            seg[wi] = '\0';
        }
        if (*p == quote) p++;
    } else {
        const char *start = p;
        while (*p && *p != '>' && *p != ';' && *p != '|' && *p != '~' &&
               *p != '[' && !isspace((unsigned char)*p)) {
            p++;
        }
        size_t len = (size_t)(p - start);
        if (len == 0) {
            *pp = p;
            return NULL;
        }
        seg = malloc(len + 1);
        if (seg) {
            memcpy(seg, start, len);
            seg[len] = '\0';
        }
    }

    while (*p && isspace((unsigned char)*p)) p++;
    *pp = p;
    return seg;
}

/**
 * Convert a TextIndex heading path (a>b>"c d") to display form "a: b: c d"
 * with underscore emphasis converted to <em>.
 */
static char *textindex_path_to_display(const char *path) {
    if (!path || !path[0]) return NULL;

    const char *p = path;
    char *out = NULL;
    size_t out_len = 0;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == '>') {
            p++;
            continue;
        }

        char *seg = textindex_parse_path_segment(&p);
        if (!seg) break;

        char *html_seg = underscore_emphasis_to_html(seg);
        free(seg);
        if (!html_seg) {
            free(out);
            return NULL;
        }

        size_t add = strlen(html_seg);
        size_t need = out_len + add + (out ? 2 : 0) + 1;
        char *n = realloc(out, need);
        if (!n) {
            free(html_seg);
            free(out);
            return NULL;
        }
        out = n;
        if (out_len > 0) {
            out[out_len++] = ':';
            out[out_len++] = ' ';
        }
        memcpy(out + out_len, html_seg, add);
        out_len += add;
        out[out_len] = '\0';
        free(html_seg);

        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '>') p++;
    }

    return out;
}

/**
 * Free a cross-reference list.
 */
static void apex_index_xref_free_all(apex_index_xref *xref) {
    while (xref) {
        apex_index_xref *next = xref->next;
        free(xref->target);
        free(xref);
        xref = next;
    }
}

static char *textindex_expand_alias_in_path(const apex_index_registry *registry, const char *path);
static apex_index_alias *textindex_find_alias(const apex_index_registry *registry, const char *name);

/**
 * Parse TextIndex cross-references after '|': ergonomics;+safety;@foo;@+bar;#alias
 * Sets *has_conventional_see if any non-inbound see-type is present.
 * Expands #alias targets when registry is provided.
 */
static apex_index_xref *textindex_parse_xrefs(const char *spec, bool *has_conventional_see,
                                              const apex_index_registry *registry) {
    if (has_conventional_see) *has_conventional_see = false;
    if (!spec) return NULL;

    apex_index_xref *head = NULL;
    apex_index_xref *tail = NULL;
    const char *p = spec;

    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ';')) p++;
        if (!*p) break;

        bool inbound = false;
        bool also = false;
        if (*p == '@') {
            inbound = true;
            p++;
        }
        if (*p == '+') {
            also = true;
            p++;
        }

        const char *path_start = p;
        while (*p && *p != ';') p++;
        size_t path_len = (size_t)(p - path_start);
        while (path_len > 0 && isspace((unsigned char)path_start[path_len - 1])) path_len--;

        if (path_len == 0) continue;

        char *path_buf = malloc(path_len + 1);
        if (!path_buf) continue;
        memcpy(path_buf, path_start, path_len);
        path_buf[path_len] = '\0';

        char *expanded = textindex_expand_alias_in_path(registry, path_buf);
        free(path_buf);
        if (!expanded) continue;

        char *display = NULL;
        apex_index_alias *a = NULL;
        if (expanded[0] == '#' ) {
            /* Unexpanded alias — leave literal (will show in tests as failure) */
            display = textindex_path_to_display(expanded);
        } else {
            /* If expansion produced a raw path, prefer alias display when exact match */
            display = textindex_path_to_display(expanded);
            /* Prefer stored display for pure alias expansion */
            if (registry) {
                for (a = registry->aliases; a; a = a->next) {
                    if (a->path && strcmp(a->path, expanded) == 0 && a->display) {
                        free(display);
                        display = strdup(a->display);
                        break;
                    }
                }
            }
        }
        free(expanded);
        if (!display || !display[0]) {
            free(display);
            continue;
        }

        apex_index_xref *xref = calloc(1, sizeof(apex_index_xref));
        if (!xref) {
            free(display);
            continue;
        }
        xref->target = display;
        xref->also = also;
        xref->inbound = inbound;
        xref->next = NULL;

        if (!also && !inbound && has_conventional_see) {
            *has_conventional_see = true;
        }

        if (!head) head = xref;
        else tail->next = xref;
        tail = xref;
    }

    return head;
}

/**
 * True if character is valid in a TextIndex alias name.
 */
static bool textindex_alias_name_char(unsigned char c) {
    return isalnum(c) || c == '-' || c == '_';
}

/**
 * Lookup an alias by exact name.
 */
static apex_index_alias *textindex_find_alias(const apex_index_registry *registry, const char *name) {
    if (!registry || !name) return NULL;
    for (apex_index_alias *a = registry->aliases; a; a = a->next) {
        if (a->name && strcmp(a->name, name) == 0) return a;
    }
    return NULL;
}

/**
 * Define or replace a TextIndex alias.
 */
static void textindex_define_alias(apex_index_registry *registry,
                                   const char *name,
                                   const char *path,
                                   const char *item,
                                   const char *subitem) {
    if (!registry || !name || !name[0] || !item) return;

    apex_index_alias *existing = textindex_find_alias(registry, name);
    if (existing) {
        free(existing->path);
        free(existing->item);
        free(existing->subitem);
        free(existing->display);
        existing->path = path ? strdup(path) : NULL;
        existing->item = strdup(item);
        existing->subitem = subitem ? strdup(subitem) : NULL;
    } else {
        existing = calloc(1, sizeof(apex_index_alias));
        if (!existing) return;
        existing->name = strdup(name);
        existing->path = path ? strdup(path) : NULL;
        existing->item = strdup(item);
        existing->subitem = subitem ? strdup(subitem) : NULL;
        existing->next = registry->aliases;
        registry->aliases = existing;
    }

    if (existing->subitem && existing->subitem[0]) {
        size_t n = strlen(existing->item) + strlen(existing->subitem) + 3;
        existing->display = malloc(n);
        if (existing->display) {
            snprintf(existing->display, n, "%s: %s", existing->item, existing->subitem);
        }
    } else {
        existing->display = strdup(existing->item);
    }
}

static void textindex_free_aliases(apex_index_alias *alias) {
    while (alias) {
        apex_index_alias *next = alias->next;
        free(alias->name);
        free(alias->path);
        free(alias->item);
        free(alias->subitem);
        free(alias->display);
        free(alias);
        alias = next;
    }
}

/**
 * Expand a leading #alias in a path string. Returns newly allocated expanded
 * path (raw, with > separators) or strdup of input if no expansion.
 */
static char *textindex_expand_alias_in_path(const apex_index_registry *registry, const char *path) {
    if (!path) return NULL;
    if (path[0] != '#') return strdup(path);

    const char *name = path + 1;
    size_t nlen = 0;
    while (name[nlen] && textindex_alias_name_char((unsigned char)name[nlen])) nlen++;
    if (nlen == 0) return strdup(path);

    char *name_buf = malloc(nlen + 1);
    if (!name_buf) return strdup(path);
    memcpy(name_buf, name, nlen);
    name_buf[nlen] = '\0';

    apex_index_alias *a = textindex_find_alias(registry, name_buf);
    free(name_buf);
    if (!a) return strdup(path);

    /* Remaining path after #alias (e.g. #apple>extra) */
    const char *rest = path + 1 + nlen;
    while (*rest && isspace((unsigned char)*rest)) rest++;

    if (a->path && a->path[0]) {
        if (*rest == '>') {
            size_t n = strlen(a->path) + strlen(rest) + 1;
            char *out = malloc(n);
            if (!out) return strdup(a->path);
            snprintf(out, n, "%s%s", a->path, rest);
            return out;
        }
        if (*rest) {
            /* Alias followed by other text — treat alias as first segment */
            size_t n = strlen(a->path) + strlen(rest) + 2;
            char *out = malloc(n);
            if (!out) return strdup(a->path);
            snprintf(out, n, "%s>%s", a->path, rest);
            return out;
        }
        return strdup(a->path);
    }

    /* Fall back to item/subitem */
    if (a->subitem && a->subitem[0]) {
        if (*rest == '>' || *rest) {
            size_t n = strlen(a->item) + strlen(a->subitem) + strlen(rest) + 4;
            char *out = malloc(n);
            if (!out) return NULL;
            if (*rest == '>') {
                snprintf(out, n, "%s>%s%s", a->item, a->subitem, rest);
            } else if (*rest) {
                snprintf(out, n, "%s>%s>%s", a->item, a->subitem, rest);
            } else {
                snprintf(out, n, "%s>%s", a->item, a->subitem);
            }
            return out;
        }
        size_t n = strlen(a->item) + strlen(a->subitem) + 2;
        char *out = malloc(n);
        if (!out) return NULL;
        snprintf(out, n, "%s>%s", a->item, a->subitem);
        return out;
    }
    return strdup(a->item);
}

/**
 * Strip a trailing #name or ##name from heading_part (modified in place).
 * Sets *alias_name and *unref. Returns true if an alias definition was found.
 */
static bool textindex_strip_trailing_alias(char *heading_part, char **alias_name, bool *unref) {
    if (alias_name) *alias_name = NULL;
    if (unref) *unref = false;
    if (!heading_part) return false;

    /* Walk backwards outside of quotes to find a # alias definition */
    size_t len = strlen(heading_part);
    bool in_quote = false;
    char quote = 0;
    int hash_at = -1;
    for (size_t i = 0; i < len; i++) {
        if (in_quote) {
            if (heading_part[i] == '\\' && i + 1 < len) {
                i++;
                continue;
            }
            if (heading_part[i] == quote) in_quote = false;
            continue;
        }
        if (heading_part[i] == '"' || heading_part[i] == '\'') {
            in_quote = true;
            quote = heading_part[i];
            continue;
        }
        if (heading_part[i] == '#') {
            hash_at = (int)i;
            /* ##alias — keep hash_at at the first '#', skip the second */
            if (i + 1 < len && heading_part[i + 1] == '#') {
                i++;
            }
            continue;
        }
    }
    if (hash_at < 0) return false;

    const char *p = heading_part + hash_at;
    bool is_unref = false;
    if (p[0] == '#' && p[1] == '#') {
        is_unref = true;
        p += 2;
    } else {
        p += 1;
    }

    if (!*p || !textindex_alias_name_char((unsigned char)*p)) return false;
    const char *name_start = p;
    while (*p && textindex_alias_name_char((unsigned char)*p)) p++;
    /* Only trailing alias: rest must be whitespace */
    const char *rest = p;
    while (*rest && isspace((unsigned char)*rest)) rest++;
    if (*rest) return false;

    size_t nlen = (size_t)(p - name_start);
    char *name = malloc(nlen + 1);
    if (!name) return false;
    memcpy(name, name_start, nlen);
    name[nlen] = '\0';

    /* Truncate heading_part before the # */
    size_t cut = (size_t)hash_at;
    while (cut > 0 && isspace((unsigned char)heading_part[cut - 1])) cut--;
    heading_part[cut] = '\0';

    if (alias_name) *alias_name = name;
    else free(name);
    if (unref) *unref = is_unref;
    return true;
}

/**
 * Split a raw TextIndex path into item + optional subitem (HTML-converted).
 * For A>B>C: item=A, subitem="B: C".
 */
static void textindex_path_to_item_subitem(const char *path, char **item_out, char **subitem_out) {
    if (item_out) *item_out = NULL;
    if (subitem_out) *subitem_out = NULL;
    if (!path || !path[0]) return;

    const char *p = path;
    char *first = textindex_parse_path_segment(&p);
    if (!first) return;

    char *item_html = underscore_emphasis_to_html(first);
    free(first);
    if (!item_html) return;

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '>') p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (!*p) {
        if (item_out) *item_out = item_html;
        else free(item_html);
        return;
    }

    char *rest_display = textindex_path_to_display(p);
    if (item_out) *item_out = item_html;
    else free(item_html);
    if (subitem_out) *subitem_out = rest_display;
    else free(rest_display);
}

/**
 * Parse mmark index syntax: (!item), (!item, subitem), (!!item, subitem)
 * Returns length consumed, or 0 if not a match
 */
static int parse_mmark_index(const char *text, int pos, int len,
                             apex_index_entry **entry_out) {
    if (pos + 3 >= len) return 0;

    const char *p = text + pos;

    /* Must start with (! */
    if (*p != '(' || p[1] != '!') return 0;

    p += 2;  /* Skip (! */

    /* Check for primary entry (!!) */
    bool primary = false;
    if (*p == '!') {
        primary = true;
        p++;
    }

    /* Extract item */
    const char *item_start = p;
    while (p < text + len && *p != ',' && *p != ')') {
        if (!is_valid_index_char(*p) && *p != '!') {
            return 0;  /* Invalid character */
        }
        p++;
    }

    if (p == item_start) return 0;  /* No item found */

    size_t item_len = p - item_start;
    char *item = malloc(item_len + 1);
    if (!item) return 0;
    memcpy(item, item_start, item_len);
    item[item_len] = '\0';
    trim_string(item);

    if (strlen(item) == 0) {
        free(item);
        return 0;
    }

    char *subitem = NULL;

    /* Check for subitem */
    if (*p == ',') {
        p++;  /* Skip comma */
        while (p < text + len && isspace((unsigned char)*p)) p++;  /* Skip whitespace */

        const char *subitem_start = p;
        while (p < text + len && *p != ')') {
            if (!is_valid_index_char(*p)) {
                free(item);
                return 0;  /* Invalid character */
            }
            p++;
        }

        if (p > subitem_start) {
            size_t subitem_len = p - subitem_start;
            subitem = malloc(subitem_len + 1);
            if (subitem) {
                memcpy(subitem, subitem_start, subitem_len);
                subitem[subitem_len] = '\0';
                trim_string(subitem);
            }
        }
    }

    /* Must end with ) */
    if (p >= text + len || *p != ')') {
        free(item);
        free(subitem);
        return 0;
    }
    p++;  /* Skip ) */

    /* Create index entry */
    apex_index_entry *entry = apex_index_entry_new(item, APEX_INDEX_MMARK);
    if (entry) {
        entry->subitem = subitem;
        entry->primary = primary;
        *entry_out = entry;
    } else {
        free(item);
        free(subitem);
    }

    return apex_idx_ptrdiff_to_int(p - (text + pos));
}

/**
 * Parse TextIndex syntax: {^}, [term]{^}, {^params}, {^"quoted"}, [vis]{^"override"}
 * Supports _underscore_ emphasis in headings and * / ** wildcards (TextIndex Fig. 4-5).
 * Returns length consumed from {^...}, or 0 if not a match.
 *
 * When the term is an explicit [term] before {^}, *bracketed_prefix_out is set to
 * the length of that "[term]" span (including brackets) so the caller can strip
 * the brackets from already-copied output. Otherwise *bracketed_prefix_out is 0.
 *
 * Bare word{^} indexes only the immediate preceding word (not a multi-word phrase);
 * use [phrase]{^} for multi-word terms. A quoted heading inside the braces
 * ({^"foo bar"} or [visible]{^"index term"}) overrides the index entry text.
 */
static int parse_textindex(const char *text, int pos, int len,
                           apex_index_entry **entry_out,
                           int *bracketed_prefix_out,
                           apex_index_registry *registry) {
    if (bracketed_prefix_out) *bracketed_prefix_out = 0;
    if (pos + 2 >= len) return 0;

    const char *p = text + pos;

    /* Look for {^ pattern */
    if (*p != '{' || p[1] != '^') return 0;

    const char *brace_start = p;
    p += 2;  /* Skip {^ */

    /* Extract parameters from {^params} */
    const char *params_start = p;
    while (p < text + len && *p != '}' && *p != '\n') {
        p++;
    }

    if (p >= text + len || *p != '}') return 0;

    size_t params_len = p - params_start;
    char *params = NULL;
    if (params_len > 0) {
        params = malloc(params_len + 1);
        if (params) {
            memcpy(params, params_start, params_len);
            params[params_len] = '\0';
        }
    }

    p++;  /* Skip } */
    int consumed = apex_idx_ptrdiff_to_int(p - (text + pos));

    /* Split params into heading path and optional |xrefs (pipe outside quotes) */
    char *heading_part = NULL;
    const char *xref_spec = NULL;
    if (params && params[0]) {
        const char *pipe = NULL;
        bool in_q = false;
        char qch = 0;
        for (const char *s = params; *s; s++) {
            if (in_q) {
                if (*s == '\\' && s[1]) {
                    s++;
                    continue;
                }
                if (*s == qch) in_q = false;
                continue;
            }
            if (*s == '"' || *s == '\'') {
                in_q = true;
                qch = *s;
                continue;
            }
            if (*s == '|') {
                pipe = s;
                break;
            }
        }
        if (pipe) {
            size_t hlen = (size_t)(pipe - params);
            heading_part = malloc(hlen + 1);
            if (heading_part) {
                memcpy(heading_part, params, hlen);
                heading_part[hlen] = '\0';
                trim_string(heading_part);
            }
            xref_spec = pipe + 1;
        } else {
            heading_part = strdup(params);
            if (heading_part) {
                size_t rlen = strlen(heading_part);
                while (rlen > 0 && (heading_part[rlen - 1] == '!' || heading_part[rlen - 1] == '/')) {
                    heading_part[--rlen] = '\0';
                }
                trim_string(heading_part);
            }
        }
    }

    /* Check for explicit visible term before {^: [term]{^...} */
    char *visible_term = NULL;
    if (brace_start > text && brace_start[-1] == ']') {
        const char *bracket_start = brace_start - 1;
        int lookback = 0;
        while (bracket_start > text && *bracket_start != '[' && lookback < 200) {
            bracket_start--;
            lookback++;
        }

        if (*bracket_start == '[') {
            size_t term_len = (brace_start - 1) - (bracket_start + 1);
            if (term_len > 0 && term_len < 200) {
                visible_term = malloc(term_len + 1);
                if (visible_term) {
                    memcpy(visible_term, bracket_start + 1, term_len);
                    visible_term[term_len] = '\0';
                    trim_string(visible_term);
                }
                if (bracketed_prefix_out) {
                    *bracketed_prefix_out = apex_idx_ptrdiff_to_int(brace_start - bracket_start);
                }
            }
        }
    }

    /* Implicit preceding word when immediately adjacent (no whitespace). */
    char *implicit_word = NULL;
    if ((!visible_term || !visible_term[0]) &&
        brace_start > text && !isspace((unsigned char)brace_start[-1])) {
        const char *word_end = brace_start;
        const char *word_start = word_end;
        int word_chars = 0;
        while (word_start > text && word_chars < 50) {
            unsigned char c = (unsigned char)word_start[-1];
            if (isalnum(c) || c == '-' || c == '_') {
                word_start--;
                word_chars++;
            } else {
                break;
            }
        }

        if (word_chars > 0) {
            size_t term_len = word_end - word_start;
            implicit_word = malloc(term_len + 1);
            if (implicit_word) {
                memcpy(implicit_word, word_start, term_len);
                implicit_word[term_len] = '\0';
                trim_string(implicit_word);
            }
        }
    }

    char *preceding = NULL;
    if (visible_term && visible_term[0]) {
        preceding = visible_term;
        visible_term = NULL;
    } else if (implicit_word && implicit_word[0]) {
        preceding = implicit_word;
        implicit_word = NULL;
    }
    free(visible_term);
    free(implicit_word);

    /* Trailing #alias / ##alias on the heading path */
    char *alias_def_name = NULL;
    bool unref_alias = false;
    if (heading_part) {
        textindex_strip_trailing_alias(heading_part, &alias_def_name, &unref_alias);
    }

    char *path_raw = NULL;
    char *item_html = NULL;
    char *subitem_html = NULL;
    bool heading_already_html = false;

    if (heading_part && heading_part[0]) {
        /* Expand leading #alias, then handle wildcards / paths */
        char *expanded = textindex_expand_alias_in_path(registry, heading_part);
        free(heading_part);
        heading_part = NULL;
        path_raw = expanded;

        /* Bare prefix / simple wildcards without hierarchy */
        int pkind = textindex_prefix_wildcard_kind(path_raw);
        if (pkind) {
            char *label = preceding ? strip_underscore_emphasis(preceding) : NULL;
            const apex_index_entry *match =
                (label && registry) ? textindex_find_prefix_entry(registry, label) : NULL;
            free(label);
            if (!match) {
                free(path_raw);
                free(alias_def_name);
                free(preceding);
                free(params);
                if (bracketed_prefix_out) *bracketed_prefix_out = 0;
                return 0;
            }
            if (pkind == 2) {
                char *lab2 = strip_underscore_emphasis(preceding);
                if (match->subitem && textindex_heading_has_prefix(match->subitem, lab2)) {
                    item_html = strdup(match->subitem);
                } else {
                    item_html = strdup(match->item);
                }
                free(lab2);
            } else {
                item_html = strdup(match->item);
                if (match->subitem) subitem_html = strdup(match->subitem);
            }
            heading_already_html = true;
            free(path_raw);
            path_raw = NULL;
        } else if (textindex_has_wildcard(path_raw) && !strchr(path_raw, '>')) {
            /* Single-segment wildcard heading: strip quotes if present */
            char *tmpl = path_raw;
            char *owned = path_raw;
            if ((tmpl[0] == '"' || tmpl[0] == '\'') && strlen(tmpl) >= 2) {
                char quote = tmpl[0];
                size_t tlen = strlen(tmpl);
                if (tmpl[tlen - 1] == quote) {
                    tmpl[tlen - 1] = '\0';
                    tmpl++;
                }
            }
            if (!preceding) {
                free(owned);
                free(alias_def_name);
                free(preceding);
                free(params);
                if (bracketed_prefix_out) *bracketed_prefix_out = 0;
                return 0;
            }
            char *expanded_w = expand_textindex_wildcards(tmpl, preceding);
            free(owned);
            path_raw = expanded_w;
            /* Expanded wildcard text is a single heading, not a > path */
            if (path_raw) {
                item_html = underscore_emphasis_to_html(path_raw);
            }
        } else {
            textindex_path_to_item_subitem(path_raw, &item_html, &subitem_html);
        }
    } else {
        free(heading_part);
        heading_part = NULL;
    }

    if (!item_html && preceding) {
        item_html = underscore_emphasis_to_html(preceding);
        if (!path_raw && preceding) path_raw = strdup(preceding);
    }

    /* Define alias for this path when #name / ##name was present */
    if (alias_def_name && item_html) {
        textindex_define_alias(registry, alias_def_name, path_raw, item_html, subitem_html);
    }
    free(alias_def_name);
    free(preceding);

    /* ##alias: define only — consume mark, no index entry */
    if (unref_alias) {
        free(item_html);
        free(subitem_html);
        free(path_raw);
        free(params);
        *entry_out = NULL;
        return consumed;
    }

    if (!item_html || !item_html[0]) {
        free(item_html);
        free(subitem_html);
        free(path_raw);
        free(params);
        if (bracketed_prefix_out) *bracketed_prefix_out = 0;
        return 0;
    }

    /* Cross-references after '|' (with #alias expansion) */
    apex_index_xref *xrefs = NULL;
    bool has_conventional_see = false;
    if (xref_spec) {
        xrefs = textindex_parse_xrefs(xref_spec, &has_conventional_see, registry);
    }

    (void)heading_already_html; /* item_html already final */

    apex_index_entry *entry = apex_index_entry_new(item_html, APEX_INDEX_TEXTINDEX);
    free(item_html);
    free(path_raw);
    if (entry) {
        entry->subitem = subitem_html;
        entry->xrefs = xrefs;
        entry->suppress_locator = has_conventional_see;
        *entry_out = entry;
    } else {
        free(subitem_html);
        apex_index_xref_free_all(xrefs);
        if (bracketed_prefix_out) *bracketed_prefix_out = 0;
    }

    free(params);
    return consumed;
}

/**
 * Strip Leanpub formatting (*italics*, **bold*) from index term for display
 */
static void strip_leanpub_formatting(char *str) {
    if (!str) return;

    char *w = str;
    const char *r = str;

    while (*r) {
        if (*r == '*') {
            /* Skip * or ** */
            if (r[1] == '*') {
                r += 2;
            } else {
                r++;
            }
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

/**
 * Parse Leanpub index syntax: {i: term}, {i: "term"}, {i: "Main!sub"}
 * See https://help.leanpub.com/en/articles/6961502-how-to-create-an-index-in-a-leanpub-book
 * Returns length consumed, or 0 if not a match
 */
static int parse_leanpub_index(const char *text, int pos, int len,
                               apex_index_entry **entry_out) {
    if (pos + 4 >= len) return 0;

    const char *p = text + pos;

    /* Must start with {i: */
    if (p[0] != '{' || p[1] != 'i' || p[2] != ':') return 0;
    p += 3;

    /* Skip space after colon */
    while (p < text + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= text + len) return 0;

    char *item = NULL;
    char *subitem = NULL;

    if (*p == '"') {
        /* Quoted: {i: "term"} or {i: "Main!sub"} */
        p++;  /* Skip opening quote */
        const char *start = p;

        while (p < text + len && *p != '"') {
            if (*p == '\\' && p + 1 < text + len) {
                p += 2;  /* Skip escaped char */
            } else {
                p++;
            }
        }
        if (p >= text + len || *p != '"') return 0;

        size_t term_len = p - start;
        char *term = malloc(term_len + 1);
        if (!term) return 0;
        memcpy(term, start, term_len);
        term[term_len] = '\0';

        /* Parse Main!sub hierarchy */
        char *excl = strchr(term, '!');
        if (excl) {
            *excl = '\0';
            item = strdup(term);
            subitem = strdup(excl + 1);
            trim_string(item);
            trim_string(subitem);
            strip_leanpub_formatting(item);
            strip_leanpub_formatting(subitem);
            free(term);
        } else {
            strip_leanpub_formatting(term);
            trim_string(term);
            item = term;
        }

        p++;  /* Skip closing quote */
    } else {
        /* Unquoted: {i: Ishmael} */
        const char *start = p;
        while (p < text + len && *p != '}' && *p != '\n') {
            if (is_valid_index_char(*p)) {
                p++;
            } else {
                return 0;  /* Invalid char in unquoted term */
            }
        }
        if (p >= text + len || *p != '}') {
            return 0;
        }

        size_t term_len = p - start;
        if (term_len == 0) return 0;

        item = malloc(term_len + 1);
        if (!item) return 0;
        memcpy(item, start, term_len);
        item[term_len] = '\0';
        trim_string(item);
        if (strlen(item) == 0) {
            free(item);
            return 0;
        }
    }

    /* Must end with } */
    while (p < text + len && (*p == ' ' || *p == '\t')) p++;
    if (p >= text + len || *p != '}') {
        free(item);
        free(subitem);
        return 0;
    }
    p++;  /* Skip } */

    apex_index_entry *entry = apex_index_entry_new(item, APEX_INDEX_LEANPUB);
    if (entry) {
        entry->subitem = subitem;
        *entry_out = entry;
    } else {
        free(item);
        free(subitem);
    }

    return apex_idx_ptrdiff_to_int(p - (text + pos));
}

/**
 * Create a new index entry
 */
apex_index_entry *apex_index_entry_new(const char *item, apex_index_syntax_t syntax_type) {
    if (!item) return NULL;

    apex_index_entry *entry = malloc(sizeof(apex_index_entry));
    if (!entry) return NULL;

    entry->item = strdup(item);
    entry->subitem = NULL;
    entry->primary = false;
    entry->suppress_locator = false;
    entry->position = 0;
    entry->anchor_id = NULL;
    entry->syntax_type = syntax_type;
    entry->xrefs = NULL;
    entry->next = NULL;

    return entry;
}

/**
 * Free an index entry
 */
void apex_index_entry_free(apex_index_entry *entry) {
    if (!entry) return;

    free(entry->item);
    free(entry->subitem);
    free(entry->anchor_id);
    apex_index_xref_free_all(entry->xrefs);
    free(entry);
}

/**
 * Free index registry
 */
void apex_free_index_registry(apex_index_registry *registry) {
    if (!registry) return;

    apex_index_entry *entry = registry->entries;
    while (entry) {
        apex_index_entry *next = entry->next;
        apex_index_entry_free(entry);
        entry = next;
    }

    registry->entries = NULL;
    textindex_free_aliases(registry->aliases);
    registry->aliases = NULL;
    registry->count = 0;
    registry->next_ref_id = 0;
}

/**
 * Process index entries in text via preprocessing
 */
char *apex_process_index_entries(const char *text, apex_index_registry *registry, const apex_options *options) {
    if (!text || !registry || !options->enable_indices) {
        return NULL;
    }

    size_t text_len = strlen(text);

    /* Quick scan: check if any index patterns exist before processing */
    bool has_mmark_pattern = false;
    bool has_textindex_pattern = false;
    bool has_leanpub_pattern = false;

    if (options->enable_mmark_index_syntax) {
        /* Look for (! or (!! patterns */
        const char *p = text;
        while (*p && p < text + text_len - 2) {
            if (*p == '(' && p[1] == '!') {
                has_mmark_pattern = true;
                break;
            }
            p++;
        }
    }

    if (options->enable_textindex_syntax && !has_mmark_pattern) {
        /* Look for {^ pattern */
        const char *p = text;
        while (*p && p < text + text_len - 1) {
            if (*p == '{' && p[1] == '^') {
                has_textindex_pattern = true;
                break;
            }
            p++;
        }
    }

    if (options->enable_leanpub_index_syntax && !has_mmark_pattern) {
        /* Look for {i: pattern */
        const char *p = text;
        while (*p && p < text + text_len - 4) {
            if (p[0] == '{' && p[1] == 'i' && p[2] == ':') {
                has_leanpub_pattern = true;
                break;
            }
            p++;
        }
    }

    /* Early exit if no patterns found */
    if (!has_mmark_pattern && !has_textindex_pattern && !has_leanpub_pattern) {
        return NULL;
    }

    size_t capacity = text_len * 2;  /* Generous buffer */
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;
    bool textindex_processing = true;  /* {^-} / {^+} toggles */

    while (*read) {
        apex_index_entry *entry = NULL;
        int consumed = 0;
        int bracketed_prefix = 0;

        /* TextIndex processing toggles: {^-} disables, {^+} enables.
         * Effective toggles are removed; redundant ones are left untouched. */
        if (options->enable_textindex_syntax &&
            read[0] == '{' && read[1] == '^' &&
            (read[2] == '-' || read[2] == '+') && read[3] == '}') {
            if (read[2] == '-' && textindex_processing) {
                textindex_processing = false;
                read += 4;
                continue;
            }
            if (read[2] == '+' && !textindex_processing) {
                textindex_processing = true;
                read += 4;
                continue;
            }
            /* Redundant toggle: emit with caret placeholder (survives superscript) */
            const char *lit = (read[2] == '-') ? "{APEXTICARET-}" : "{APEXTICARET+}";
            size_t lit_len = strlen(lit);
            if (remaining < lit_len + 1) {
                size_t used = write - output;
                capacity = (used + lit_len + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            memcpy(write, lit, lit_len);
            write += lit_len;
            remaining -= lit_len;
            read += 4;
            continue;
        }

        /* While disabled, leave TextIndex marks literal (protect ^ from superscript) */
        if (options->enable_textindex_syntax && !textindex_processing &&
            read[0] == '{' && read[1] == '^') {
            const char *end = read + 2;
            while (*end && *end != '}' && *end != '\n') end++;
            if (*end == '}') {
                size_t inner_len = (size_t)(end - (read + 2));
                size_t ph_len = strlen(TEXTINDEX_CARET_PLACEHOLDER);
                size_t lit_len = 1 + ph_len + inner_len + 1; /* { PLACEHOLDER inner } */
                if (remaining < lit_len + 1) {
                    size_t used = write - output;
                    capacity = (used + lit_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        return NULL;
                    }
                    output = new_output;
                    write = output + used;
                    remaining = capacity - used;
                }
                *write++ = '{';
                memcpy(write, TEXTINDEX_CARET_PLACEHOLDER, ph_len);
                write += ph_len;
                if (inner_len > 0) {
                    memcpy(write, read + 2, inner_len);
                    write += inner_len;
                }
                *write++ = '}';
                remaining -= lit_len;
                read = end + 1;
                continue;
            }
        }

        /* Try mmark syntax first if enabled */
        if (options->enable_mmark_index_syntax) {
            consumed = parse_mmark_index(text, apex_idx_ptrdiff_to_int(read - text), apex_idx_size_to_int(text_len), &entry);
        }

        /* Try TextIndex syntax if mmark didn't match and TextIndex is enabled */
        if (!entry && options->enable_textindex_syntax && textindex_processing &&
            *read == '{' && read + 1 < text + text_len && read[1] == '^') {
            consumed = parse_textindex(text, apex_idx_ptrdiff_to_int(read - text), apex_idx_size_to_int(text_len), &entry, &bracketed_prefix, registry);
        }

        /* Try Leanpub syntax if no match yet and Leanpub is enabled */
        if (!entry && options->enable_leanpub_index_syntax && *read == '{' && read + 3 < text + text_len &&
            read[1] == 'i' && read[2] == ':') {
            consumed = parse_leanpub_index(text, apex_idx_ptrdiff_to_int(read - text), apex_idx_size_to_int(text_len), &entry);
        }

        if (consumed > 0 && !entry) {
            /* Unreferenced ##alias definition: strip brackets if any, drop mark */
            if (bracketed_prefix >= 2 && (write - output) >= bracketed_prefix) {
                char *span = write - bracketed_prefix;
                if (span[0] == '[' && span[bracketed_prefix - 1] == ']') {
                    size_t inner_len = (size_t)bracketed_prefix - 2;
                    if (inner_len > 0) {
                        memmove(span, span + 1, inner_len);
                    }
                    write = span + inner_len;
                    remaining += 2;
                }
            }
            read += consumed;
            continue;
        }

        if (entry && consumed > 0) {
            /* [term]{^}: [term] was already copied into output; strip the brackets,
             * keep the inner text, then append the index placeholder. */
            if (bracketed_prefix >= 2 && (write - output) >= bracketed_prefix) {
                char *span = write - bracketed_prefix;
                if (span[0] == '[' && span[bracketed_prefix - 1] == ']') {
                    size_t inner_len = (size_t)bracketed_prefix - 2;
                    if (inner_len > 0) {
                        memmove(span, span + 1, inner_len);
                    }
                    write = span + inner_len;
                    remaining += 2;  /* freed [ and ] */
                }
            }

            /* Add entry to registry */
            entry->position = apex_idx_ptrdiff_to_int(read - text);

            /* Apply inbound (@ / @+) cross-refs onto their target entries */
            apex_index_xref **xptr = &entry->xrefs;
            while (*xptr) {
                apex_index_xref *x = *xptr;
                if (!x->inbound) {
                    xptr = &(*xptr)->next;
                    continue;
                }

                /* Find or create the external target entry */
                apex_index_entry *target = NULL;
                for (apex_index_entry *e = registry->entries; e; e = e->next) {
                    char *ka = strip_html_tags(e->item);
                    char *kb = strip_html_tags(x->target);
                    bool same = ka && kb && strcasecmp(ka, kb) == 0 && !e->subitem;
                    free(ka);
                    free(kb);
                    if (same) {
                        target = e;
                        break;
                    }
                }
                if (!target) {
                    target = apex_index_entry_new(x->target, APEX_INDEX_TEXTINDEX);
                    if (target) {
                        target->suppress_locator = true;
                        target->position = entry->position;
                        target->next = registry->entries;
                        registry->entries = target;
                        registry->count++;
                    }
                }

                if (target) {
                    apex_index_xref *rev = calloc(1, sizeof(apex_index_xref));
                    if (rev) {
                        rev->target = strdup(entry->item);
                        rev->also = x->also;
                        rev->inbound = false;
                        rev->next = target->xrefs;
                        target->xrefs = rev;
                    }
                }

                *xptr = x->next;
                free(x->target);
                free(x);
            }

            if (!entry->suppress_locator) {
                char anchor_id[64];
                snprintf(anchor_id, sizeof(anchor_id), "idxref-%d", registry->next_ref_id);
                entry->anchor_id = strdup(anchor_id);
                registry->next_ref_id++;

                /* Replace mark with placeholder */
                size_t placeholder_len = strlen(INDEX_PLACEHOLDER_PREFIX) +
                                       strlen(anchor_id) +
                                       strlen(INDEX_PLACEHOLDER_SUFFIX);

                if (remaining < placeholder_len + 1) {
                    size_t used = write - output;
                    capacity = (used + placeholder_len + 1) * 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) {
                        free(output);
                        apex_index_entry_free(entry);
                        return NULL;
                    }
                    output = new_output;
                    write = output + used;
                    remaining = capacity - used;
                }

                snprintf(write, remaining, "%s%s%s",
                        INDEX_PLACEHOLDER_PREFIX, anchor_id, INDEX_PLACEHOLDER_SUFFIX);
                write += placeholder_len;
                remaining -= placeholder_len;
            }
            /* see-type marks: consume without inserting a locator span */

            entry->next = registry->entries;
            registry->entries = entry;
            registry->count++;

            read += consumed;
        } else {
            /* Copy character as-is */
            if (remaining < 2) {
                size_t used = write - output;
                capacity = (used + 1) * 2;
                char *new_output = realloc(output, capacity);
                if (!new_output) {
                    free(output);
                    return NULL;
                }
                output = new_output;
                write = output + used;
                remaining = capacity - used;
            }
            *write++ = *read++;
            remaining--;
        }
    }

    *write = '\0';
    return output;
}

/**
 * Restore TextIndex caret placeholders left for literal marks in {^-} regions.
 */
static char *apex_restore_textindex_carets(const char *html) {
    if (!html || !strstr(html, TEXTINDEX_CARET_PLACEHOLDER)) {
        return NULL;
    }

    size_t html_len = strlen(html);
    size_t ph_len = strlen(TEXTINDEX_CARET_PLACEHOLDER);
    size_t capacity = html_len + 1;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = html;
    char *write = output;
    while (*read) {
        if (strncmp(read, TEXTINDEX_CARET_PLACEHOLDER, ph_len) == 0) {
            *write++ = '^';
            read += ph_len;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
    return output;
}

/**
 * Render index markers in HTML output
 */
char *apex_render_index_markers(const char *html, apex_index_registry *registry, const apex_options *options) {
    if (!html || !options->enable_indices) {
        return NULL;
    }

    /* Always restore caret placeholders from {^-} protected literal marks */
    char *caret_restored = apex_restore_textindex_carets(html);
    const char *work = caret_restored ? caret_restored : html;

    if (!registry || registry->count == 0) {
        /* No markers to expand; return caret restore if any */
        return caret_restored;
    }

    size_t html_len = strlen(work);
    size_t capacity = html_len * 2;
    char *output = malloc(capacity);
    if (!output) {
        free(caret_restored);
        return NULL;
    }

    const char *read = work;
    char *write = output;
    size_t remaining = capacity;

    while (*read) {
        /* Look for placeholder */
        if (strncmp(read, INDEX_PLACEHOLDER_PREFIX, strlen(INDEX_PLACEHOLDER_PREFIX)) == 0) {
            read += strlen(INDEX_PLACEHOLDER_PREFIX);

            /* Extract anchor ID */
            const char *id_start = read;
            while (*read && *read != '>' && strncmp(read, INDEX_PLACEHOLDER_SUFFIX, strlen(INDEX_PLACEHOLDER_SUFFIX)) != 0) {
                read++;
            }

            if (strncmp(read, INDEX_PLACEHOLDER_SUFFIX, strlen(INDEX_PLACEHOLDER_SUFFIX)) == 0) {
                size_t id_len = read - id_start;
                char anchor_id[64];
                if (id_len < sizeof(anchor_id)) {
                    memcpy(anchor_id, id_start, id_len);
                    anchor_id[id_len] = '\0';

                    /* Replace with HTML span */
                    size_t span_len = snprintf(NULL, 0, "<span class=\"index\" id=\"%s\"></span>", anchor_id);
                    if (remaining < span_len + 1) {
                        size_t used = write - output;
                        capacity = (used + span_len + 1) * 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) {
                            free(output);
                            free(caret_restored);
                            return NULL;
                        }
                        output = new_output;
                        write = output + used;
                        remaining = capacity - used;
                    }

                    snprintf(write, remaining, "<span class=\"index\" id=\"%s\"></span>", anchor_id);
                    write += span_len;
                    remaining -= span_len;

                    read += strlen(INDEX_PLACEHOLDER_SUFFIX);
                    continue;
                }
            }
        }

        /* Copy character as-is */
        if (remaining < 2) {
            size_t used = write - output;
            capacity = (used + 1) * 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) {
                free(output);
                free(caret_restored);
                return NULL;
            }
            output = new_output;
            write = output + used;
            remaining = capacity - used;
        }
        *write++ = *read++;
        remaining--;
    }

    *write = '\0';
    free(caret_restored);
    return output;
}

/**
 * Compare function for sorting index entries
 */
static int compare_index_entries(const void *a, const void *b) {
    const apex_index_entry *entry_a = *(const apex_index_entry **)a;
    const apex_index_entry *entry_b = *(const apex_index_entry **)b;

    char *key_a = strip_html_tags(entry_a->item);
    char *key_b = strip_html_tags(entry_b->item);
    int item_cmp = strcasecmp(key_a ? key_a : "", key_b ? key_b : "");
    free(key_a);
    free(key_b);
    if (item_cmp != 0) return item_cmp;

    /* If items are equal, compare subitems */
    if (entry_a->subitem && entry_b->subitem) {
        char *sa = strip_html_tags(entry_a->subitem);
        char *sb = strip_html_tags(entry_b->subitem);
        int sub_cmp = strcasecmp(sa ? sa : "", sb ? sb : "");
        free(sa);
        free(sb);
        if (sub_cmp != 0) return sub_cmp;
    } else if (entry_a->subitem) {
        return 1;  /* Entry with subitem comes after entry without */
    } else if (entry_b->subitem) {
        return -1;
    }

    /* Same heading: document order for locator merging */
    if (entry_a->position < entry_b->position) return -1;
    if (entry_a->position > entry_b->position) return 1;
    return 0;
}

/**
 * True if two entries share the same main heading (for merging locators).
 */
static bool same_index_item(const apex_index_entry *a, const apex_index_entry *b) {
    if (!a || !b || !a->item || !b->item) return false;
    char *ka = strip_html_tags(a->item);
    char *kb = strip_html_tags(b->item);
    bool same = ka && kb && strcasecmp(ka, kb) == 0;
    free(ka);
    free(kb);
    return same;
}

/**
 * True if two entries share the same subitem (both NULL counts as same).
 */
static bool same_index_subitem(const apex_index_entry *a, const apex_index_entry *b) {
    if (!a || !b) return false;
    if (!a->subitem && !b->subitem) return true;
    if (!a->subitem || !b->subitem) return false;
    char *sa = strip_html_tags(a->subitem);
    char *sb = strip_html_tags(b->subitem);
    bool same = sa && sb && strcasecmp(sa, sb) == 0;
    free(sa);
    free(sb);
    return same;
}

/**
 * Get first letter of index term (for grouping)
 */
static char get_first_letter(const char *term) {
    if (!term || *term == '\0') return '?';

    /* Skip HTML tags, whitespace, and punctuation */
    while (*term) {
        if (*term == '<') {
            while (*term && *term != '>') term++;
            if (*term == '>') term++;
            continue;
        }
        if (isalnum((unsigned char)*term)) {
            return (char)toupper((unsigned char)*term);
        }
        term++;
    }

    return '?';
}

/**
 * Append TextIndex see / see-also run-in text for entries[start..end).
 * Deduplicates by (also, target). Uses APPEND macro from caller.
 */
#define APPEND_INDEX_XREFS(start, end) do { \
    bool _have_see = false, _have_also = false; \
    for (size_t _xi = (start); _xi < (end); _xi++) { \
        for (apex_index_xref *_x = entries[_xi]->xrefs; _x; _x = _x->next) { \
            if (_x->inbound || !_x->target) continue; \
            if (_x->also) _have_also = true; else _have_see = true; \
        } \
    } \
    if (_have_see) { \
        APPEND(". <em>See</em> "); \
        bool _first = true; \
        for (size_t _xi = (start); _xi < (end); _xi++) { \
            for (apex_index_xref *_x = entries[_xi]->xrefs; _x; _x = _x->next) { \
                if (_x->inbound || _x->also || !_x->target) continue; \
                bool _dup = false; \
                for (size_t _yj = (start); _yj <= _xi && !_dup; _yj++) { \
                    for (apex_index_xref *_y = entries[_yj]->xrefs; _y; _y = _y->next) { \
                        if (_y == _x) break; \
                        if (!_y->inbound && !_y->also && _y->target && \
                            strcasecmp(_y->target, _x->target) == 0) { _dup = true; break; } \
                    } \
                } \
                if (_dup) continue; \
                if (!_first) APPEND("; "); \
                APPEND(_x->target); \
                _first = false; \
            } \
        } \
    } \
    if (_have_also) { \
        APPEND(_have_see ? ". <em>See also</em> " : ". <em>See also</em> "); \
        bool _first = true; \
        for (size_t _xi = (start); _xi < (end); _xi++) { \
            for (apex_index_xref *_x = entries[_xi]->xrefs; _x; _x = _x->next) { \
                if (_x->inbound || !_x->also || !_x->target) continue; \
                bool _dup = false; \
                for (size_t _yj = (start); _yj <= _xi && !_dup; _yj++) { \
                    for (apex_index_xref *_y = entries[_yj]->xrefs; _y; _y = _y->next) { \
                        if (_y == _x) break; \
                        if (!_y->inbound && _y->also && _y->target && \
                            strcasecmp(_y->target, _x->target) == 0) { _dup = true; break; } \
                    } \
                } \
                if (_dup) continue; \
                if (!_first) APPEND("; "); \
                APPEND(_x->target); \
                _first = false; \
            } \
        } \
    } \
} while (0)

/**
 * Generate index HTML from collected entries
 */
char *apex_generate_index_html(apex_index_registry *registry, const apex_options *options) {
    if (!registry || registry->count == 0) {
        return strdup("");
    }

    /* Collect entries into array for sorting */
    apex_index_entry **entries = malloc(registry->count * sizeof(apex_index_entry *));
    if (!entries) return strdup("");

    size_t idx = 0;
    for (apex_index_entry *entry = registry->entries; entry; entry = entry->next) {
        entries[idx++] = entry;
    }

    /* Sort entries */
    qsort(entries, registry->count, sizeof(apex_index_entry *), compare_index_entries);

    /* Generate HTML */
    size_t capacity = 8192;
    char *html = malloc(capacity);
    if (!html) {
        free(entries);
        return strdup("");
    }

    char *write = html;
    size_t remaining = capacity;

    #define APPEND(str) do { \
        size_t len = strlen(str); \
        if (len < remaining) { \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } else { \
            size_t used = write - html; \
            capacity = (used + len + 1) * 2; \
            char *new_html = realloc(html, capacity); \
            if (!new_html) { \
                free(html); \
                free(entries); \
                return strdup(""); \
            } \
            html = new_html; \
            write = html + used; \
            remaining = capacity - used; \
            memcpy(write, str, len); \
            write += len; \
            remaining -= len; \
        } \
    } while(0)

    APPEND("<h1 id=\"index-section\">Index</h1>\n");
    APPEND("<div class=\"index\">\n");

    if (options->group_index_by_letter) {
        /* Group by first letter */
        char current_letter = '\0';
        bool in_group = false;

        for (size_t i = 0; i < registry->count; ) {
            apex_index_entry *entry = entries[i];
            char letter = get_first_letter(entry->item);

            if (letter != current_letter) {
                if (in_group) {
                    APPEND("</ul>\n</dd>\n</dl>\n");
                }

                current_letter = letter;
                char letter_str[2] = {letter, '\0'};
                char group_html[256];
                snprintf(group_html, sizeof(group_html), "<dl>\n<dt>%s</dt>\n<dd>\n<ul>\n", letter_str);
                APPEND(group_html);
                in_group = true;
            }

            /* Merge consecutive entries with the same main heading */
            size_t start = i;
            i++;
            while (i < registry->count && same_index_item(entries[start], entries[i])) {
                i++;
            }

            char item_html[2048];
            snprintf(item_html, sizeof(item_html), "<li>\n%s", entries[start]->item);
            APPEND(item_html);

            for (size_t li = start; li < i; li++) {
                if (entries[li]->subitem) continue;
                if (!entries[li]->anchor_id) continue;
                char link_html[256];
                if (entries[li]->primary) {
                    snprintf(link_html, sizeof(link_html),
                             " <strong><a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a></strong>",
                             entries[li]->anchor_id);
                } else {
                    snprintf(link_html, sizeof(link_html),
                             " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                             entries[li]->anchor_id);
                }
                APPEND(link_html);
            }

            size_t j = start;
            bool opened_sub = false;
            while (j < i) {
                if (!entries[j]->subitem) {
                    j++;
                    continue;
                }
                if (!opened_sub) {
                    APPEND("<ul>\n");
                    opened_sub = true;
                }
                size_t sub_start = j;
                j++;
                while (j < i && entries[j]->subitem &&
                       same_index_subitem(entries[sub_start], entries[j])) {
                    j++;
                }
                APPEND("<li>\n");
                APPEND(entries[sub_start]->subitem);
                for (size_t k = sub_start; k < j; k++) {
                    if (!entries[k]->anchor_id) continue;
                    char link_html[256];
                    snprintf(link_html, sizeof(link_html),
                             " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                             entries[k]->anchor_id);
                    APPEND(link_html);
                }
                APPEND("</li>\n");
            }
            if (opened_sub) {
                /* Also-refs with sub-entries: append as subordinate list items */
                for (size_t xi = start; xi < i; xi++) {
                    for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                        if (x->inbound || !x->also || !x->target) continue;
                        APPEND("<li>\n<em>See also</em> ");
                        APPEND(x->target);
                        APPEND("</li>\n");
                    }
                }
                APPEND("</ul>\n");
                /* See-refs still run-in after the list */
                bool have_see = false;
                for (size_t xi = start; xi < i; xi++) {
                    for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                        if (!x->inbound && !x->also && x->target) have_see = true;
                    }
                }
                if (have_see) {
                    APPEND(". <em>See</em> ");
                    bool first = true;
                    for (size_t xi = start; xi < i; xi++) {
                        for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                            if (x->inbound || x->also || !x->target) continue;
                            if (!first) APPEND("; ");
                            APPEND(x->target);
                            first = false;
                        }
                    }
                }
            } else {
                APPEND_INDEX_XREFS(start, i);
            }
            APPEND("</li>\n");
        }

        if (in_group) {
            APPEND("</ul>\n</dd>\n</dl>\n");
        }
    } else {
        /* Simple list without grouping */
        APPEND("<ul>\n");

        for (size_t i = 0; i < registry->count; ) {
            size_t start = i;
            i++;
            while (i < registry->count && same_index_item(entries[start], entries[i])) {
                i++;
            }

            char item_html[2048];
            snprintf(item_html, sizeof(item_html), "<li>\n%s", entries[start]->item);
            APPEND(item_html);

            for (size_t li = start; li < i; li++) {
                if (entries[li]->subitem) continue;
                if (!entries[li]->anchor_id) continue;
                char link_html[256];
                if (entries[li]->primary) {
                    snprintf(link_html, sizeof(link_html),
                             " <strong><a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a></strong>",
                             entries[li]->anchor_id);
                } else {
                    snprintf(link_html, sizeof(link_html),
                             " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                             entries[li]->anchor_id);
                }
                APPEND(link_html);
            }

            size_t j = start;
            bool opened_sub = false;
            while (j < i) {
                if (!entries[j]->subitem) {
                    j++;
                    continue;
                }
                if (!opened_sub) {
                    APPEND("<ul>\n");
                    opened_sub = true;
                }
                size_t sub_start = j;
                j++;
                while (j < i && entries[j]->subitem &&
                       same_index_subitem(entries[sub_start], entries[j])) {
                    j++;
                }
                APPEND("<li>\n");
                APPEND(entries[sub_start]->subitem);
                for (size_t k = sub_start; k < j; k++) {
                    if (!entries[k]->anchor_id) continue;
                    char link_html[256];
                    snprintf(link_html, sizeof(link_html),
                             " <a class=\"index-return\" href=\"#%s\"><sup>[go]</sup></a>",
                             entries[k]->anchor_id);
                    APPEND(link_html);
                }
                APPEND("</li>\n");
            }
            if (opened_sub) {
                for (size_t xi = start; xi < i; xi++) {
                    for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                        if (x->inbound || !x->also || !x->target) continue;
                        APPEND("<li>\n<em>See also</em> ");
                        APPEND(x->target);
                        APPEND("</li>\n");
                    }
                }
                APPEND("</ul>\n");
                bool have_see = false;
                for (size_t xi = start; xi < i; xi++) {
                    for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                        if (!x->inbound && !x->also && x->target) have_see = true;
                    }
                }
                if (have_see) {
                    APPEND(". <em>See</em> ");
                    bool first = true;
                    for (size_t xi = start; xi < i; xi++) {
                        for (apex_index_xref *x = entries[xi]->xrefs; x; x = x->next) {
                            if (x->inbound || x->also || !x->target) continue;
                            if (!first) APPEND("; ");
                            APPEND(x->target);
                            first = false;
                        }
                    }
                }
            } else {
                APPEND_INDEX_XREFS(start, i);
            }
            APPEND("</li>\n");
        }

        APPEND("</ul>\n");
    }

    APPEND("</div>\n");

    #undef APPEND
    #undef APPEND_INDEX_XREFS

    *write = '\0';
    free(entries);
    return html;
}

/**
 * Insert index at <!--INDEX--> marker or end of document
 */
char *apex_insert_index(const char *html, apex_index_registry *registry, const apex_options *options) {
    if (!html || !registry || registry->count == 0 || !options->enable_indices || options->suppress_index) {
        return NULL;
    }

    char *index_html = apex_generate_index_html(registry, options);
    if (!index_html || strlen(index_html) == 0) {
        return NULL;
    }

    /* Look for <!--INDEX--> marker */
    const char *marker = "<!--INDEX-->";
    const char *marker_pos = strstr(html, marker);

    if (marker_pos) {
        /* Insert at marker */
        size_t before_len = marker_pos - html;
        size_t after_len = strlen(marker_pos + strlen(marker));
        size_t index_len = strlen(index_html);
        size_t total_len = before_len + index_len + after_len + 1;

        char *output = malloc(total_len);
        if (!output) {
            free(index_html);
            return NULL;
        }

        memcpy(output, html, before_len);
        memcpy(output + before_len, index_html, index_len);
        memcpy(output + before_len + index_len, marker_pos + strlen(marker), after_len);
        output[total_len - 1] = '\0';

        free(index_html);
        return output;
    } else {
        /* Insert at end, before </body> if present, otherwise at very end */
        const char *body_end = strstr(html, "</body>");
        if (body_end) {
            size_t before_len = body_end - html;
            size_t index_len = strlen(index_html);
            size_t after_len = strlen(body_end);
            size_t total_len = before_len + index_len + after_len + 1;

            char *output = malloc(total_len);
            if (!output) {
                free(index_html);
                return NULL;
            }

            memcpy(output, html, before_len);
            memcpy(output + before_len, index_html, index_len);
            memcpy(output + before_len + index_len, body_end, after_len);
            output[total_len - 1] = '\0';

            free(index_html);
            return output;
        } else {
            /* Append at end */
            size_t html_len = strlen(html);
            size_t index_len = strlen(index_html);
            size_t total_len = html_len + index_len + 1;

            char *output = malloc(total_len);
            if (!output) {
                free(index_html);
                return NULL;
            }

            memcpy(output, html, html_len);
            memcpy(output + html_len, index_html, index_len);
            output[total_len - 1] = '\0';

            free(index_html);
            return output;
        }
    }
}
