/**
 * Citations Extension for Apex
 *
 * Supports multiple citation syntaxes:
 * - Pandoc: [@key], @key, [see @key, pp. 33-35]
 * - MultiMarkdown: [#key], [p. 23][#key]
 * - mmark: [@RFC2535], [@!RFC1034], [@RFC1034;@RFC1035]
 */

#ifndef APEX_CITATIONS_H
#define APEX_CITATIONS_H

#include <stdbool.h>
#include <stddef.h>
#include "cmark-gfm.h"
#include "cmark-gfm-extension_api.h"
#include "../../include/apex/apex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Citation syntax types */
typedef enum {
    APEX_CITATION_PANDOC = 0,
    APEX_CITATION_MMD = 1,
    APEX_CITATION_MMARK = 2
} apex_citation_syntax_t;

/* Citation structure */
typedef struct apex_citation {
    char *key;                    /* Citation key (e.g., "doe99") */
    char *prefix;                 /* Prefix text (e.g., "see") */
    char *locator;                /* Locator (e.g., "pp. 33-35") */
    char *suffix;                 /* Suffix text (e.g., "and passim") */
    bool author_suppressed;       /* -@key syntax */
    bool author_in_text;          /* @key syntax (no brackets) */
    apex_citation_syntax_t syntax_type;  /* PANDOC, MMD, MMARK */
    int position;                 /* Position in document */
    struct apex_citation *next;   /* Linked list */
} apex_citation;

/* Bibliography entry structure (simplified CSL JSON) */
typedef struct apex_bibliography_entry {
    char *id;                     /* Citation key (e.g., "doe99") */
    char *type;                   /* Entry type (article-journal, book, etc.) */
    char *title;                  /* Title */
    char *author;                 /* Author (formatted string) */
    char *year;                   /* Year */
    char *container_title;        /* Journal/container title */
    char *publisher;              /* Publisher */
    char *volume;                /* Volume */
    char *page;                  /* Pages */
    char *raw_data;              /* Raw JSON/BibTeX data for future use */
    struct apex_bibliography_entry *next;  /* Linked list */
} apex_bibliography_entry;

/* Bibliography registry */
typedef struct {
    apex_bibliography_entry *entries;  /* Linked list of bibliography entries */
    size_t count;                      /* Number of entries */
} apex_bibliography_registry;

/* Citation registry */
typedef struct {
    apex_citation *citations;     /* Linked list of citations */
    size_t count;                 /* Number of citations */
    apex_bibliography_registry *bibliography;  /* Bibliography entries */
} apex_citation_registry;

/**
 * Create and return the citations extension
 */
cmark_syntax_extension *create_citations_extension(void);

/**
 * Process citations in text via preprocessing
 * Extracts citations and stores them in registry
 * Returns modified text with citations marked
 */
char *apex_process_citations(const char *text, apex_citation_registry *registry, const apex_options *options);

/**
 * Render citations in HTML output
 * Replaces citation markers with formatted HTML
 */
char *apex_render_citations(const char *html, apex_citation_registry *registry, const apex_options *options);

/**
 * Generate bibliography HTML from cited entries
 * Returns formatted bibliography HTML
 */
char *apex_generate_bibliography(apex_citation_registry *registry, const apex_options *options);

/**
 * Insert bibliography at <!-- REFERENCES --> marker or end of document
 * Returns HTML with bibliography inserted
 */
char *apex_insert_bibliography(const char *html, apex_citation_registry *registry, const apex_options *options);

/**
 * Free citation registry
 */
void apex_free_citation_registry(apex_citation_registry *registry);

/**
 * Create a new citation
 */
apex_citation *apex_citation_new(const char *key, apex_citation_syntax_t syntax_type);

/**
 * Free a citation
 */
void apex_citation_free(apex_citation *citation);

/**
 * Load bibliography from file(s)
 * Auto-detects format from extension (.bib, .json, .yaml, .yml)
 * Returns bibliography registry, or NULL on error
 */
apex_bibliography_registry *apex_load_bibliography(const char **files, const char *base_directory);

/**
 * Load bibliography from a single file
 * Auto-detects format from extension
 */
apex_bibliography_registry *apex_load_bibliography_file(const char *filepath);

/**
 * Parse BibTeX file
 */
apex_bibliography_registry *apex_parse_bibtex(const char *content);

/**
 * Parse CSL JSON file
 */
apex_bibliography_registry *apex_parse_csl_json(const char *content);

/**
 * Parse CSL YAML file
 */
apex_bibliography_registry *apex_parse_csl_yaml(const char *content);

/**
 * Find bibliography entry by ID
 */
apex_bibliography_entry *apex_find_bibliography_entry(apex_bibliography_registry *registry, const char *id);

/**
 * Free bibliography registry
 */
void apex_free_bibliography_registry(apex_bibliography_registry *registry);

/**
 * Free bibliography entry
 */
void apex_bibliography_entry_free(apex_bibliography_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* APEX_CITATIONS_H */
