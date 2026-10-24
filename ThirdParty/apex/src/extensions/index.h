/**
 * Index Extension for Apex
 *
 * Supports three index syntaxes:
 * - mmark/MultiMarkdown: (!item), (!item, subitem), (!!item, subitem)
 * - TextIndex: {^}, [term]{^}, {^params}
 * - Leanpub: {i: term}, {i: "term"}, {i: "Main!sub"}
 */

#ifndef APEX_INDEX_H
#define APEX_INDEX_H

#include <stdbool.h>
#include <stddef.h>
#include "../../include/apex/apex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Index syntax types */
typedef enum {
    APEX_INDEX_MMARK = 0,
    APEX_INDEX_TEXTINDEX = 1,
    APEX_INDEX_LEANPUB = 2
} apex_index_syntax_t;

/* TextIndex cross-reference (see / see also) */
typedef struct apex_index_xref {
    char *target;                  /* Display path, e.g. "safety: of functions" */
    bool also;                     /* true = see also; false = see */
    bool inbound;                  /* @ / @+ inbound (applied to target entry) */
    struct apex_index_xref *next;
} apex_index_xref;

/* TextIndex alias (#name / ##name) */
typedef struct apex_index_alias {
    char *name;                    /* alias id without '#' */
    char *path;                    /* raw path, e.g. Apple (company)>"OS platforms" */
    char *item;                    /* top-level heading (HTML) */
    char *subitem;                 /* nested heading (HTML), optional */
    char *display;                 /* "item: subitem" display form */
    struct apex_index_alias *next;
} apex_index_alias;

/* Index entry structure */
typedef struct apex_index_entry {
    char *item;                    /* Main index term */
    char *subitem;                 /* Sub-item (optional) */
    bool primary;                  /* Primary entry flag (mmark) */
    bool suppress_locator;         /* TextIndex see-type mark: no locator */
    int position;                  /* Position in document */
    char *anchor_id;               /* Generated anchor ID (e.g., "idxref-0") */
    apex_index_syntax_t syntax_type;  /* MMARK or TEXTINDEX */
    apex_index_xref *xrefs;        /* TextIndex see / see-also refs */
    struct apex_index_entry *next;  /* Linked list */
} apex_index_entry;

/* Index registry */
typedef struct {
    apex_index_entry *entries;     /* Linked list of index entries */
    apex_index_alias *aliases;     /* TextIndex aliases */
    size_t count;                  /* Number of entries */
    int next_ref_id;               /* Next reference ID for anchors */
} apex_index_registry;

/**
 * Process index entries in text via preprocessing
 * Extracts index entries and stores them in registry
 * Returns modified text with index markers
 */
char *apex_process_index_entries(const char *text, apex_index_registry *registry, const apex_options *options);

/**
 * Render index markers in HTML output
 * Replaces index markers with formatted HTML spans
 */
char *apex_render_index_markers(const char *html, apex_index_registry *registry, const apex_options *options);

/**
 * Generate index HTML from collected entries
 * Returns formatted index HTML
 */
char *apex_generate_index_html(apex_index_registry *registry, const apex_options *options);

/**
 * Insert index at <!--INDEX--> marker or end of document
 * Returns HTML with index inserted
 */
char *apex_insert_index(const char *html, apex_index_registry *registry, const apex_options *options);

/**
 * Apply TextIndex concordance file(s): scan the document for column-1 patterns
 * and insert [match]{^col2} marks. Returns a new string if any marks were added,
 * or NULL if unchanged / no concordance files.
 */
char *apex_apply_concordance(const char *text, const apex_options *options);

/**
 * Free index registry
 */
void apex_free_index_registry(apex_index_registry *registry);

/**
 * Create a new index entry
 */
apex_index_entry *apex_index_entry_new(const char *item, apex_index_syntax_t syntax_type);

/**
 * Free an index entry
 */
void apex_index_entry_free(apex_index_entry *entry);

#ifdef __cplusplus
}
#endif

#endif /* APEX_INDEX_H */
