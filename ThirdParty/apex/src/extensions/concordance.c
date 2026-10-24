/**
 * TextIndex concordance file support for Apex
 *
 * Loads tab-separated concordance rules and inserts [match]{^col2} marks
 * before the normal TextIndex / index preprocessing pass.
 *
 * Format and semantics follow Matt Gemmell's TextIndex:
 * https://mattgemmell.scot/textindex/#concordance-files
 */

#include "index.h"
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t start;
    size_t end;
} apex_conc_range;

typedef struct {
    size_t start;
    size_t end;
    char *captured; /* owned */
    char *payload;  /* owned; content after {^ ; may be empty */
} apex_conc_term;

typedef struct {
    char *pattern; /* owned; POSIX ERE, no (?i) prefix */
    char *payload; /* owned; may be empty */
    bool icase;
} apex_conc_rule;

static char *apex_conc_read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0 || size > 10 * 1024 * 1024) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    char *content = malloc((size_t)size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    size_t n = fread(content, 1, (size_t)size, fp);
    content[n] = '\0';
    fclose(fp);
    return content;
}

static char *apex_conc_resolve_path(const char *filepath, const char *base_directory) {
    if (!filepath) return NULL;
    if (filepath[0] == '/' || !base_directory || base_directory[0] == '\0') {
        return strdup(filepath);
    }

    size_t base_len = strlen(base_directory);
    size_t path_len = strlen(filepath);
    int need_slash = (base_len > 0 && base_directory[base_len - 1] != '/');
    char *resolved = malloc(base_len + (size_t)need_slash + path_len + 1);
    if (!resolved) return NULL;

    memcpy(resolved, base_directory, base_len);
    size_t pos = base_len;
    if (need_slash) resolved[pos++] = '/';
    memcpy(resolved + pos, filepath, path_len + 1);
    return resolved;
}

/* Convert Python-ish regex bits TextIndex examples use into POSIX ERE. */
static char *apex_conc_posixify_pattern(const char *pattern) {
    if (!pattern) return NULL;

    size_t len = strlen(pattern);
    /* Worst case: each \b -> [[:<:]] (7 chars) */
    size_t cap = len * 4 + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    size_t wi = 0;
    for (size_t i = 0; i < len; i++) {
        if (pattern[i] == '\\' && i + 1 < len) {
            char n = pattern[i + 1];
            const char *rep = NULL;
            if (n == 'b') {
                /* Prefer end-bound when previous char looks like a word atom. */
                bool after_atom = (wi > 0 && (isalnum((unsigned char)out[wi - 1]) ||
                                              out[wi - 1] == '_' || out[wi - 1] == ')' ||
                                              out[wi - 1] == ']' || out[wi - 1] == '?' ||
                                              out[wi - 1] == '+' || out[wi - 1] == '*'));
                rep = after_atom ? "[[:>:]]" : "[[:<:]]";
            } else if (n == 's') {
                rep = "[[:space:]]";
            } else if (n == 'd') {
                rep = "[[:digit:]]";
            } else if (n == 'w') {
                rep = "[[:alnum:]_]";
            }

            if (rep) {
                size_t rl = strlen(rep);
                if (wi + rl + 1 > cap) {
                    cap = (wi + rl + 1) * 2;
                    char *grown = realloc(out, cap);
                    if (!grown) {
                        free(out);
                        return NULL;
                    }
                    out = grown;
                }
                memcpy(out + wi, rep, rl);
                wi += rl;
                i++;
                continue;
            }
        }

        if (wi + 2 > cap) {
            cap *= 2;
            char *grown = realloc(out, cap);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        out[wi++] = pattern[i];
    }
    out[wi] = '\0';
    return out;
}

static bool apex_conc_has_uppercase(const char *s) {
    if (!s) return false;
    for (; *s; s++) {
        if (isupper((unsigned char)*s)) return true;
    }
    return false;
}

static void apex_conc_collapse_tabs(char *line) {
    char *r = line;
    char *w = line;
    while (*r) {
        if (*r == '\t') {
            *w++ = '\t';
            while (*r == '\t') r++;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static int apex_conc_range_cmp(const void *a, const void *b) {
    const apex_conc_range *ra = a;
    const apex_conc_range *rb = b;
    if (ra->start < rb->start) return -1;
    if (ra->start > rb->start) return 1;
    if (ra->end < rb->end) return -1;
    if (ra->end > rb->end) return 1;
    return 0;
}

static int apex_conc_term_cmp(const void *a, const void *b) {
    const apex_conc_term *ta = a;
    const apex_conc_term *tb = b;
    if (ta->start < tb->start) return -1;
    if (ta->start > tb->start) return 1;
    return 0;
}

static bool apex_conc_ranges_overlap(size_t a0, size_t a1, size_t b0, size_t b1) {
    return a0 < b1 && a1 > b0;
}

static bool apex_conc_is_excluded(const apex_conc_range *ranges, size_t count,
                                  size_t start, size_t end, size_t *hint) {
    size_t i = hint ? *hint : 0;
    if (i > count) i = 0;

    for (; i < count; i++) {
        if (ranges[i].end <= start) {
            if (hint) *hint = i;
            continue;
        }
        if (ranges[i].start >= end) break;
        if (apex_conc_ranges_overlap(start, end, ranges[i].start, ranges[i].end)) {
            return true;
        }
    }
    return false;
}

static size_t apex_conc_mark_extent_start(const char *text, size_t brace_at) {
    if (!text || brace_at == 0) return brace_at;

    if (text[brace_at - 1] == ']') {
        size_t j = brace_at - 1;
        while (j > 0) {
            j--;
            if (text[j] == '[') return j;
        }
        /* Fall through to word walk if brackets were unmatched. */
    }

    size_t i = brace_at;
    while (i > 0) {
        unsigned char c = (unsigned char)text[i - 1];
        if (isspace(c) || c == '[' || c == ']' || c == '{' || c == '}' || c == '<' || c == '>') {
            break;
        }
        i--;
    }
    return i;
}

static bool apex_conc_push_range(apex_conc_range **ranges, size_t *count, size_t *cap,
                                 size_t start, size_t end) {
    if (end <= start) return true;
    if (*count >= *cap) {
        size_t ncap = *cap ? *cap * 2 : 32;
        apex_conc_range *grown = realloc(*ranges, ncap * sizeof(apex_conc_range));
        if (!grown) return false;
        *ranges = grown;
        *cap = ncap;
    }
    (*ranges)[*count].start = start;
    (*ranges)[*count].end = end;
    (*count)++;
    return true;
}

static bool apex_conc_collect_exclusions(const char *text, apex_conc_range **out_ranges,
                                         size_t *out_count) {
    apex_conc_range *ranges = NULL;
    size_t count = 0;
    size_t cap = 0;
    size_t len = strlen(text);

    for (size_t i = 0; i < len; i++) {
        if (text[i] == '<' ) {
            size_t j = i + 1;
            while (j < len && text[j] != '>') j++;
            if (j < len) {
                if (!apex_conc_push_range(&ranges, &count, &cap, i, j + 1)) {
                    free(ranges);
                    return false;
                }
                i = j;
            }
            continue;
        }

        if (text[i] == '{' && i + 1 < len) {
            if (text[i + 1] == '^') {
                size_t j = i + 2;
                while (j < len && text[j] != '}' && text[j] != '<' && text[j] != '\n') j++;
                if (j < len && text[j] == '}') {
                    size_t start = apex_conc_mark_extent_start(text, i);
                    if (!apex_conc_push_range(&ranges, &count, &cap, start, j + 1)) {
                        free(ranges);
                        return false;
                    }
                    i = j;
                }
                continue;
            }

            if (strncasecmp(text + i, "{index", 6) == 0 &&
                (text[i + 6] == '}' || isspace((unsigned char)text[i + 6]))) {
                size_t j = i + 6;
                while (j < len && text[j] != '}') j++;
                if (j < len) {
                    if (!apex_conc_push_range(&ranges, &count, &cap, i, j + 1)) {
                        free(ranges);
                        return false;
                    }
                    i = j;
                }
            }
        }
    }

    if (count > 1) {
        qsort(ranges, count, sizeof(apex_conc_range), apex_conc_range_cmp);
    }
    *out_ranges = ranges;
    *out_count = count;
    return true;
}

static void apex_conc_free_rules(apex_conc_rule *rules, size_t count) {
    if (!rules) return;
    for (size_t i = 0; i < count; i++) {
        free(rules[i].pattern);
        free(rules[i].payload);
    }
    free(rules);
}

static void apex_conc_free_terms(apex_conc_term *terms, size_t count) {
    if (!terms) return;
    for (size_t i = 0; i < count; i++) {
        free(terms[i].captured);
        free(terms[i].payload);
    }
    free(terms);
}

static bool apex_conc_parse_file(const char *contents, apex_conc_rule **out_rules,
                                  size_t *out_count) {
    apex_conc_rule *rules = NULL;
    size_t count = 0;
    size_t cap = 0;

    const char *p = contents;
    while (*p) {
        const char *line_end = p;
        while (*line_end && *line_end != '\n') line_end++;

        size_t line_len = (size_t)(line_end - p);
        char *line = malloc(line_len + 1);
        if (!line) {
            apex_conc_free_rules(rules, count);
            return false;
        }
        memcpy(line, p, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') line[line_len - 1] = '\0';

        p = *line_end ? line_end + 1 : line_end;

        char *trim = line;
        while (*trim && isspace((unsigned char)*trim)) trim++;
        if (*trim == '\0' || *trim == '#') {
            free(line);
            continue;
        }

        apex_conc_collapse_tabs(trim);

        char *col1 = trim;
        char *col2 = NULL;
        char *tab = strchr(trim, '\t');
        if (tab) {
            *tab = '\0';
            col2 = tab + 1;
            char *tab2 = strchr(col2, '\t');
            if (tab2) *tab2 = '\0';
            while (*col2 && isspace((unsigned char)*col2)) col2++;
            char *end2 = col2 + strlen(col2);
            while (end2 > col2 && isspace((unsigned char)end2[-1])) {
                end2--;
                *end2 = '\0';
            }
        }

        bool case_sensitive = false;
        if (col1[0] == '\\' && col1[1] == '=') {
            memmove(col1, col1 + 1, strlen(col1 + 1) + 1);
        } else if (col1[0] == '=') {
            memmove(col1, col1 + 1, strlen(col1 + 1) + 1);
            case_sensitive = true;
        } else if (apex_conc_has_uppercase(col1)) {
            case_sensitive = true;
        }

        char *posix_pat = apex_conc_posixify_pattern(col1);
        if (!posix_pat) {
            free(line);
            apex_conc_free_rules(rules, count);
            return false;
        }

        if (count >= cap) {
            size_t ncap = cap ? cap * 2 : 8;
            apex_conc_rule *grown = realloc(rules, ncap * sizeof(apex_conc_rule));
            if (!grown) {
                free(posix_pat);
                free(line);
                apex_conc_free_rules(rules, count);
                return false;
            }
            rules = grown;
            cap = ncap;
        }

        rules[count].pattern = posix_pat;
        rules[count].payload = strdup(col2 ? col2 : "");
        rules[count].icase = !case_sensitive;
        if (!rules[count].payload) {
            free(posix_pat);
            free(line);
            apex_conc_free_rules(rules, count);
            return false;
        }
        count++;
        free(line);
    }

    *out_rules = rules;
    *out_count = count;
    return true;
}

static bool apex_conc_push_term(apex_conc_term **terms, size_t *count, size_t *cap,
                                 size_t start, size_t end, const char *captured,
                                 const char *payload) {
    if (*count >= *cap) {
        size_t ncap = *cap ? *cap * 2 : 32;
        apex_conc_term *grown = realloc(*terms, ncap * sizeof(apex_conc_term));
        if (!grown) return false;
        *terms = grown;
        *cap = ncap;
    }

    char *cap_copy = malloc(end - start + 1);
    char *pay_copy = strdup(payload ? payload : "");
    if (!cap_copy || !pay_copy) {
        free(cap_copy);
        free(pay_copy);
        return false;
    }
    memcpy(cap_copy, captured, end - start);
    cap_copy[end - start] = '\0';

    (*terms)[*count].start = start;
    (*terms)[*count].end = end;
    (*terms)[*count].captured = cap_copy;
    (*terms)[*count].payload = pay_copy;
    (*count)++;
    return true;
}

static char *apex_conc_apply_rules(const char *text, const apex_conc_rule *rules,
                                    size_t rule_count) {
    if (!text || rule_count == 0) return NULL;

    apex_conc_range *excluded = NULL;
    size_t excl_count = 0;
    if (!apex_conc_collect_exclusions(text, &excluded, &excl_count)) {
        return NULL;
    }

    apex_conc_term *terms = NULL;
    size_t term_count = 0;
    size_t term_cap = 0;
    size_t text_len = strlen(text);

    for (size_t ri = 0; ri < rule_count; ri++) {
        regex_t re;
        int cflags = REG_EXTENDED;
        if (rules[ri].icase) cflags |= REG_ICASE;

        if (regcomp(&re, rules[ri].pattern, cflags) != 0) {
            continue; /* skip invalid patterns */
        }

        apex_conc_range *new_excl = NULL;
        size_t new_excl_count = 0;
        size_t new_excl_cap = 0;
        size_t hint = 0;
        size_t off = 0;

        while (off <= text_len) {
            regmatch_t m;
            int flags = (off > 0) ? REG_NOTBOL : 0;
            if (regexec(&re, text + off, 1, &m, flags) != 0) break;

            if (m.rm_so < 0 || m.rm_eo < m.rm_so) break;

            size_t start = off + (size_t)m.rm_so;
            size_t end = off + (size_t)m.rm_eo;
            if (end == start) {
                off = end + 1;
                continue;
            }

            if (!apex_conc_is_excluded(excluded, excl_count, start, end, &hint)) {
                if (!apex_conc_push_term(&terms, &term_count, &term_cap, start, end,
                                         text + start, rules[ri].payload)) {
                    regfree(&re);
                    free(excluded);
                    free(new_excl);
                    apex_conc_free_terms(terms, term_count);
                    return NULL;
                }
                if (!apex_conc_push_range(&new_excl, &new_excl_count, &new_excl_cap, start, end)) {
                    regfree(&re);
                    free(excluded);
                    free(new_excl);
                    apex_conc_free_terms(terms, term_count);
                    return NULL;
                }
            }

            off = end;
        }

        regfree(&re);

        if (new_excl_count > 0) {
            size_t merged_count = excl_count + new_excl_count;
            apex_conc_range *merged = realloc(excluded, merged_count * sizeof(apex_conc_range));
            if (!merged) {
                free(excluded);
                free(new_excl);
                apex_conc_free_terms(terms, term_count);
                return NULL;
            }
            memcpy(merged + excl_count, new_excl, new_excl_count * sizeof(apex_conc_range));
            excluded = merged;
            excl_count = merged_count;
            qsort(excluded, excl_count, sizeof(apex_conc_range), apex_conc_range_cmp);
        }
        free(new_excl);
    }

    free(excluded);

    if (term_count == 0) {
        apex_conc_free_terms(terms, term_count);
        return NULL;
    }

    qsort(terms, term_count, sizeof(apex_conc_term), apex_conc_term_cmp);

    /* Estimate output size */
    size_t extra = 0;
    for (size_t i = 0; i < term_count; i++) {
        /* [captured]{^payload} replaces captured */
        extra += 2 + 3 + strlen(terms[i].payload); /* [] + {^} + payload */
    }

    char *out = malloc(text_len + extra + 1);
    if (!out) {
        apex_conc_free_terms(terms, term_count);
        return NULL;
    }

    size_t wi = 0;
    size_t ri = 0;
    size_t ti = 0;
    while (ri < text_len) {
        if (ti < term_count && terms[ti].start == ri) {
            size_t cap_len = terms[ti].end - terms[ti].start;
            size_t pay_len = strlen(terms[ti].payload);
            out[wi++] = '[';
            memcpy(out + wi, terms[ti].captured, cap_len);
            wi += cap_len;
            out[wi++] = ']';
            out[wi++] = '{';
            out[wi++] = '^';
            memcpy(out + wi, terms[ti].payload, pay_len);
            wi += pay_len;
            out[wi++] = '}';
            ri = terms[ti].end;
            ti++;
            continue;
        }
        out[wi++] = text[ri++];
    }
    out[wi] = '\0';

    apex_conc_free_terms(terms, term_count);
    return out;
}

char *apex_apply_concordance(const char *text, const apex_options *options) {
    if (!text || !options || !options->concordance_files || !options->concordance_files[0]) {
        return NULL;
    }

    char *current = NULL;
    const char *src = text;
    bool any = false;

    for (size_t fi = 0; options->concordance_files[fi] != NULL; fi++) {
        char *resolved = apex_conc_resolve_path(options->concordance_files[fi],
                                                options->base_directory);
        if (!resolved) continue;

        char *contents = apex_conc_read_file(resolved);
        free(resolved);
        if (!contents) {
            fprintf(stderr, "Warning: could not read concordance file: %s\n",
                    options->concordance_files[fi]);
            continue;
        }

        apex_conc_rule *rules = NULL;
        size_t rule_count = 0;
        if (!apex_conc_parse_file(contents, &rules, &rule_count)) {
            free(contents);
            free(current);
            return NULL;
        }
        free(contents);

        if (rule_count == 0) {
            apex_conc_free_rules(rules, rule_count);
            continue;
        }

        char *updated = apex_conc_apply_rules(src, rules, rule_count);
        apex_conc_free_rules(rules, rule_count);

        if (updated) {
            free(current);
            current = updated;
            src = current;
            any = true;
        }
    }

    return any ? current : NULL;
}
