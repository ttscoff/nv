/**
 * Table HTML Postprocessing
 * Implementation
 *
 * This is a pragmatic solution: we walk the AST to collect cells with
 * rowspan/colspan attributes, then do pattern matching on the HTML to inject them.
 */

#include "table_html_postprocess.h"
#include "cmark-gfm-core-extensions.h"
#include "table.h"  /* For CMARK_NODE_TABLE_ROW, CMARK_NODE_TABLE_CELL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

/* Placeholder for escaped \<< (literal <<). Must match table.c. No underscore so inline parser doesn't treat as emphasis. */
static const unsigned char ESCAPED_LTLT_PLACEHOLDER[] = "APEXLTLT";
#define ESCAPED_LTLT_PLACEHOLDER_LEN 8
#define ESCAPED_LTLT_REPLACEMENT "&lt;&lt;"
#define ESCAPED_LTLT_REPLACEMENT_LEN 8

/* Structure to track cells with attributes */
typedef struct cell_attr {
    int table_index;   /* Which table (0, 1, 2, ...) */
    int row_index;
    int col_index;
    char *attributes;  /* e.g. " rowspan=\"2\"" or " data-remove=\"true\"" */
    char *cell_text;  /* Store cell content for content-based matching */
    struct cell_attr *next;
} cell_attr;

/* Structure to track rows that should be in tfoot */
typedef struct tfoot_row {
    int table_index;
    int row_index;
    struct tfoot_row *next;
} tfoot_row;

/* Structure to track table captions */
typedef struct table_caption {
    int table_index;   /* Which table (0, 1, 2, ...) */
    char *caption;     /* Caption text */
    struct table_caption *next;
} table_caption;

/* Structure to track paragraphs to remove (caption paragraphs) */
typedef struct para_to_remove {
    int para_index;    /* Which paragraph (0, 1, 2, ...) */
    char *text_fingerprint;  /* First 50 chars for matching */
    struct para_to_remove *next;
} para_to_remove;

/* Structure to track all cells (for mapping calculation) */
typedef struct all_cell {
    int table_index;
    int row_index;
    int col_index;
    bool is_removed;  /* true if marked with data-remove */
    struct all_cell *next;
} all_cell;

/**
 * Walk AST and collect all cells (for mapping calculation)
 */
static all_cell *collect_all_cells(cmark_node *document) {
    all_cell *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int row_index = -1;  /* Start at -1, will increment to 0 on first row */
    int col_index = 0;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */
                row_index = -1;  /* Reset row index for new table */
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;  /* Increment for each row */
                col_index = 0;
            } else if (type == CMARK_NODE_TABLE_CELL) {
                char *attrs = (char *)cmark_node_get_user_data(node);
                bool is_removed = (attrs && strstr(attrs, "data-remove"));

                /* Store ALL cells for mapping calculation */
                all_cell *cell = malloc(sizeof(all_cell));
                if (cell) {
                    cell->table_index = table_index;
                    cell->row_index = row_index;
                    cell->col_index = col_index;
                    cell->is_removed = is_removed;
                    cell->next = list;
                    list = cell;
                }

                col_index++;
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Walk AST and collect cells with attributes
 */
/* True when the cell whose opening tag starts at `cell` contains only `marker`
 * (or `alt`), ignoring surrounding whitespace. */
static bool cell_is_only_marker(const char *cell, const char *marker, const char *alt) {
    bool is_th = strncmp(cell, "<th", 3) == 0;
    const char *content = strchr(cell, '>');
    if (!content) return false;
    content++;
    const char *end = strstr(content, is_th ? "</th>" : "</td>");
    if (!end) return false;
    while (content < end && isspace((unsigned char)*content)) content++;
    while (end > content && isspace((unsigned char)end[-1])) end--;
    size_t len = (size_t)(end - content);
    if (len == strlen(marker) && strncmp(content, marker, len) == 0) return true;
    return alt && len == strlen(alt) && strncmp(content, alt, len) == 0;
}

static cell_attr *collect_table_cell_attributes(cmark_node *document) {
    cell_attr *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int row_index = -1;  /* Start at -1, will increment to 0 on first row */
    int col_index = 0;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */
                row_index = -1;  /* Reset row index for new table */
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;  /* Increment for each row */
                col_index = 0;

                /* Check if this row is marked as tfoot */
                char *row_attrs = (char *)cmark_node_get_user_data(node);
                if (row_attrs && strstr(row_attrs, "data-tfoot")) {
                    /* Store this row as a tfoot row */
                    tfoot_row *tfoot = malloc(sizeof(tfoot_row));
                    if (tfoot) {
                        tfoot->table_index = table_index;
                        tfoot->row_index = row_index;
                        /* We'll add this to a list later - for now just mark it on the row */
                        /* We'll check for it during HTML processing */
                    }
                }
            } else if (type == CMARK_NODE_TABLE_CELL) {
                char *attrs = (char *)cmark_node_get_user_data(node);
                if (attrs) {
                    /* Get cell content - check recursively for text nodes */
                    const char *cell_text = NULL;
                    cmark_node *child = cmark_node_first_child(node);
                    while (child) {
                        if (cmark_node_get_type(child) == CMARK_NODE_TEXT) {
                            cell_text = cmark_node_get_literal(child);
                            break;
                        }
                        /* Check nested nodes (paragraphs, etc.) */
                        cmark_node *nested = cmark_node_first_child(child);
                        while (nested) {
                            if (cmark_node_get_type(nested) == CMARK_NODE_TEXT) {
                                cell_text = cmark_node_get_literal(nested);
                                break;
                            }
                            nested = cmark_node_next(nested);
                        }
                        if (cell_text) break;
                        child = cmark_node_next(child);
                    }
                    if (!cell_text) cell_text = "?";

                    /* Store this cell's attributes */
                    cell_attr *attr = malloc(sizeof(cell_attr));
                    if (attr) {
                        attr->table_index = table_index;
                        attr->row_index = row_index;
                        attr->col_index = col_index;
                        attr->attributes = strdup(attrs);
                        attr->cell_text = cell_text ? strdup(cell_text) : NULL;
                        attr->next = list;
                        list = attr;                    }
                }
                /* Count all cells (including removed ones) to match the column indices
                 * used in advanced_tables.c when finding target cells for rowspan.
                 * The HTML renderer will remove cells with data-remove, but we need to
                 * match based on the original column positions. */
                col_index++;
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Process cell alignment colons and return alignment style
 * Detects leading/trailing colons (respecting escaped colons) and returns
 * the appropriate text-align style string, or NULL if no alignment detected.
 * Modifies content_start and content_end to remove the colons.
 *
 * Based on Jekyll Spaceship's handle_text_align function.
 *
 * @param content_start Pointer to start of cell content (after >)
 * @param content_end Pointer to end of cell content (before </td> or </th>)
 * @param align_out Output parameter: receives alignment string (must be freed) or NULL
 * @return true if alignment was detected and processed, false otherwise
 */
static bool process_cell_alignment(const char **content_start, const char **content_end, char **align_out) {
    if (!content_start || !content_end || !align_out) return false;

    const char *start = *content_start;
    const char *end = *content_end;

    if (start >= end) return false;

    /* Fast early exit: check if there's any colon in the content at all */
    /* This avoids expensive scanning for cells that clearly don't have alignment */
    bool has_colon = false;
    for (const char *check = start; check < end; check++) {
        if (*check == ':') {
            has_colon = true;
            break;
        }
    }
    if (!has_colon) {
        *align_out = NULL;
        return false;
    }

    /* Check for leading colon (left or center align)
     * Must be at start (after whitespace), not escaped, and not followed by another colon */
    bool has_leading_colon = false;
    const char *p = start;
    while (p < end && isspace((unsigned char)*p)) p++;  /* Skip leading whitespace */

    if (p < end && *p == ':') {
        /* Check if it's escaped (backslash before colon) */
        bool is_escaped = (p > start && *(p - 1) == '\\');
        /* Check if it's followed by another colon (:: means something else, not alignment) */
        bool is_double_colon = (p + 1 < end && *(p + 1) == ':');
        if (!is_escaped && !is_double_colon) {
            has_leading_colon = true;
        }
    }

    /* Check for trailing colon (right or center align)
     * Must be at end (before whitespace) and not escaped */
    bool has_trailing_colon = false;
    p = end - 1;
    while (p >= start && isspace((unsigned char)*p)) p--;  /* Skip trailing whitespace */

    if (p >= start && *p == ':') {
        /* Check if it's escaped (backslash before colon) */
        if (p == start || *(p - 1) != '\\') {
            has_trailing_colon = true;
        }
    }

    if (!has_leading_colon && !has_trailing_colon) {
        *align_out = NULL;
        return false;
    }

    /* Determine alignment */
    const char *align_str = NULL;
    if (has_leading_colon && has_trailing_colon) {
        align_str = "text-align: center";
    } else if (has_leading_colon) {
        align_str = "text-align: left";
    } else if (has_trailing_colon) {
        align_str = "text-align: right";
    }

    if (align_str) {
        *align_out = strdup(align_str);

        /* Update content_start and content_end to remove the colons */
        if (has_leading_colon) {
            /* Skip leading whitespace and colon */
            p = start;
            while (p < end && isspace((unsigned char)*p)) p++;
            if (p < end && *p == ':') {
                *content_start = p + 1;
            }
        }

        if (has_trailing_colon) {
            /* Skip trailing whitespace and colon */
            p = end - 1;
            while (p >= *content_start && isspace((unsigned char)*p)) p--;
            if (p >= *content_start && *p == ':') {
                *content_end = p;
            }
        }

        return true;
    }

    *align_out = NULL;
    return false;
}

/**
 * Extract [Caption] text from a paragraph node.
 * Handles both: (1) TEXT node with literal "[Caption]", (2) LINK node where [text] was parsed as link.
 * Returns allocated string or NULL. Caller must free.
 */
static char *get_caption_from_paragraph(cmark_node *para) {
    if (!para || cmark_node_get_type(para) != CMARK_NODE_PARAGRAPH) return NULL;
    cmark_node *child = cmark_node_first_child(para);
    if (!child) return NULL;

    cmark_node_type t = cmark_node_get_type(child);
    if (t == CMARK_NODE_TEXT) {
        const char *text = cmark_node_get_literal(child);
        if (!text || text[0] != '[') return NULL;
        const char *end = strchr(text + 1, ']');
        if (!end) return NULL;
        const char *after = end + 1;
        while (*after && isspace((unsigned char)*after)) after++;
        if (*after != '\0') return NULL;
        size_t len = (size_t)(end - text - 1);
        if (len == 0 || len >= 512) return NULL;
        char *caption = malloc(len + 1);
        if (caption) {
            memcpy(caption, text + 1, len);
            caption[len] = '\0';
        }
        return caption;
    }
    if (t == CMARK_NODE_LINK) {
        /* [Caption] parsed as link - caption is the link text */
        cmark_node *link_text = cmark_node_first_child(child);
        if (!link_text || cmark_node_get_type(link_text) != CMARK_NODE_TEXT) return NULL;
        const char *text = cmark_node_get_literal(link_text);
        if (!text) return NULL;
        /* Only treat as caption if paragraph contains exactly one link node.
         * Footnote definitions like "[^1]: ..." can parse as LINK + TEXT;
         * those must never be treated as table captions. */
        if (cmark_node_next(link_text) != NULL) return NULL; /* extra content inside link */
        if (cmark_node_next(child) != NULL) return NULL;     /* extra paragraph content after link */
        return strdup(text);
    }
    return NULL;
}

/**
 * Get text fingerprint from paragraph node (first 50 chars for matching)
 */
static char *get_para_text_fingerprint(cmark_node *node) {
    if (!node || cmark_node_get_type(node) != CMARK_NODE_PARAGRAPH) return NULL;

    cmark_node *child = cmark_node_first_child(node);
    if (child && cmark_node_get_type(child) == CMARK_NODE_TEXT) {
        const char *text = cmark_node_get_literal(child);
        if (text) {
            size_t len = strlen(text);
            if (len > 50) len = 50;
            char *fingerprint = malloc(len + 1);
            if (fingerprint) {
                memcpy(fingerprint, text, len);
                fingerprint[len] = '\0';
                return fingerprint;
            }
        }
    }
    return NULL;
}

/**
 * Walk AST and collect table captions and paragraphs to remove
 */
static table_caption *collect_table_captions(cmark_node *document, para_to_remove **paras_to_remove) {
    table_caption *list = NULL;
    *paras_to_remove = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1; /* Track which table we're in */
    int para_index = -1;  /* Track paragraph index */

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;  /* New table */

                /* Check for caption in user_data */
                char *user_data = (char *)cmark_node_get_user_data(node);
                bool caption_found = false;
                if (user_data && strstr(user_data, "data-caption=")) {
                    caption_found = true;
                    /* Extract caption text - handle both " data-caption=" and just "data-caption=" */
                    char caption[512];
                    const char *caption_start = strstr(user_data, "data-caption=");
                    if (caption_start) {
                        caption_start += strlen("data-caption=");
                        /* Skip the opening quote */
                        if (*caption_start == '"') {
                            caption_start++;
                            /* Extract until closing quote */
                            int i = 0;
                            while (*caption_start && *caption_start != '"' && i < 511) {
                                caption[i++] = *caption_start++;
                            }
                            caption[i] = '\0';
                            if (i > 0) {
                                table_caption *cap = malloc(sizeof(table_caption));
                                if (cap) {
                                    cap->table_index = table_index;
                                    cap->caption = strdup(caption);
                                    cap->next = list;
                                    list = cap;
                                }
                            }
                        }
                    }
                }
                /* If caption not found in user_data, check for caption paragraph before/after table */
                /* This handles cases where IAL processing replaced user_data */
                if (!caption_found) {
                    /* Check previous node for caption */
                    cmark_node *prev = cmark_node_previous(node);
                    if (prev && cmark_node_get_type(prev) == CMARK_NODE_PARAGRAPH) {
                        char *caption = get_caption_from_paragraph(prev);
                        if (caption) {
                            table_caption *cap = malloc(sizeof(table_caption));
                            if (cap) {
                                cap->table_index = table_index;
                                cap->caption = caption;
                                cap->next = list;
                                list = cap;
                                caption_found = true;
                                /* Mark caption paragraph for removal (para_index is prev's index) */
                                char *fingerprint = get_para_text_fingerprint(prev);
                                if (fingerprint) {
                                    para_to_remove *para = malloc(sizeof(para_to_remove));
                                    if (para) {
                                        para->para_index = para_index;
                                        para->text_fingerprint = fingerprint;
                                        para->next = *paras_to_remove;
                                        *paras_to_remove = para;
                                    } else {
                                        free(fingerprint);
                                    }
                                }
                            } else {
                                free(caption);
                            }
                        }
                    }
                    /* Also check next node for caption */
                    if (!caption_found) {
                        cmark_node *next = cmark_node_next(node);
                        if (next && cmark_node_get_type(next) == CMARK_NODE_PARAGRAPH) {
                            char *caption = get_caption_from_paragraph(next);
                            if (caption) {
                                table_caption *cap = malloc(sizeof(table_caption));
                                if (cap) {
                                    cap->table_index = table_index;
                                    cap->caption = caption;
                                    cap->next = list;
                                    list = cap;
                                    /* Mark caption paragraph for removal (next's index = para_index + 1) */
                                    char *fingerprint = get_para_text_fingerprint(next);
                                    if (fingerprint) {
                                        para_to_remove *para = malloc(sizeof(para_to_remove));
                                        if (para) {
                                            para->para_index = para_index + 1;
                                            para->text_fingerprint = fingerprint;
                                            para->next = *paras_to_remove;
                                            *paras_to_remove = para;
                                        } else {
                                            free(fingerprint);
                                        }
                                    }
                                } else {
                                    free(caption);
                                }
                            }
                        }
                    }
                }
            } else if (type == CMARK_NODE_PARAGRAPH) {
                para_index++;

                /* Check if this paragraph is marked for removal */
                char *user_data = (char *)cmark_node_get_user_data(node);
                if (user_data && strstr(user_data, "data-remove")) {
                    char *fingerprint = get_para_text_fingerprint(node);
                    if (fingerprint) {
                        para_to_remove *para = malloc(sizeof(para_to_remove));
                        if (para) {
                            para->para_index = para_index;
                            para->text_fingerprint = fingerprint;
                            para->next = *paras_to_remove;
                            *paras_to_remove = para;
                        } else {
                            free(fingerprint);
                        }
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

/**
 * Inject attributes into HTML or remove cells
 * Also wraps tables with captions in <figure> tags
 */
/**
 * Collect rows that should be in tfoot sections
 */
static tfoot_row *collect_tfoot_rows(cmark_node *document) {
    tfoot_row *list = NULL;

    cmark_iter *iter = cmark_iter_new(document);
    cmark_event_type ev_type;

    int table_index = -1;
    int row_index = -1;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        if (ev_type == CMARK_EVENT_ENTER) {
            if (type == CMARK_NODE_TABLE) {
                table_index++;
                row_index = -1;
            } else if (type == CMARK_NODE_TABLE_ROW) {
                row_index++;
                char *row_attrs = (char *)cmark_node_get_user_data(node);
                if (row_attrs && strstr(row_attrs, "data-tfoot")) {
                    tfoot_row *tfoot = malloc(sizeof(tfoot_row));
                    if (tfoot) {
                        tfoot->table_index = table_index;
                        tfoot->row_index = row_index;
                        tfoot->next = list;
                        list = tfoot;
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
    return list;
}

char *apex_inject_table_attributes(const char *html, cmark_node *document, int caption_position) {
    if (!html || !document) return (char *)html;

    /* Collect all cells with attributes */
    cell_attr *attrs = collect_table_cell_attributes(document);

    /* Collect tfoot rows */
    tfoot_row *tfoot_rows = collect_tfoot_rows(document);

    /* Collect all table captions and paragraphs to remove */
    para_to_remove *paras_to_remove = NULL;
    table_caption *captions = collect_table_captions(document, &paras_to_remove);

    /* Early exit: if there are no attributes, captions, or tfoot rows to process,
     * and no alignment colons, return early to avoid expensive processing.
     * This avoids the expensive collect_all_cells() call and HTML processing for simple tables. */
    bool needs_all_cells = (attrs != NULL || captions != NULL || paras_to_remove != NULL || tfoot_rows != NULL);
    bool has_alignment_colons = false;

    /* For very large HTML (>50KB), check if we can skip processing entirely */
    size_t html_len = strlen(html);
    if (html_len > 50000 && !needs_all_cells) {
        /* Quick check for alignment colons - if none found, skip everything */
        has_alignment_colons = (strstr(html, ":</td>") != NULL || strstr(html, ":</th>") != NULL);
        /* Also check for rowspan markers (^^) - these need processing */
        bool has_rowspan_markers = (strstr(html, "^^") != NULL);
        if (!has_alignment_colons && !has_rowspan_markers) {
            return (char *)html;
        }
    }

    /* DEBUG: If there are no attributes at all, we can skip most processing */
    if (attrs == NULL && captions == NULL && paras_to_remove == NULL && tfoot_rows == NULL) {
        /* Check for alignment colons */
        has_alignment_colons = (strstr(html, ":</td>") != NULL || strstr(html, ":</th>") != NULL);
        /* Also check for rowspan markers (^^) - these need processing even without attributes */
        bool has_rowspan_markers = (strstr(html, "^^") != NULL);
        /* Check for empty first header cell (<th></th> in thead) - this indicates row-header detection is needed */
        /* Simple check: if we have <thead> with a <th></th> pattern, we need to process for row-header detection */
        /* Note: We always process tables with <thead> to check for row-header detection, since this happens
         * during HTML processing and we can't detect it from AST attributes alone */
        bool needs_row_header_detection = (strstr(html, "<thead>") != NULL);
        if (!has_alignment_colons && !has_rowspan_markers && !needs_row_header_detection) {
            /* Absolutely nothing to process - return immediately */
            return (char *)html;
        }
    }

    if (!needs_all_cells) {
        /* Quick check for alignment colons - look for :</td> or :</th> patterns in rendered HTML */
        /* Check for trailing colon alignment (most common) */
        has_alignment_colons = (strstr(html, ":</td>") != NULL || strstr(html, ":</th>") != NULL);
        if (!has_alignment_colons) {
            /* Also check for leading colon alignment - but be more specific to avoid false positives */
            /* Look for pattern like ": text</td>" or ": text</th>" (colon, space, text, closing tag) */
            const char *colon_pos = strstr(html, ":<");
            if (colon_pos) {
                /* Check if it's followed by a closing tag within reasonable distance */
                const char *check = colon_pos + 2;
                int distance = 0;
                while (*check && distance < 200) {
                    if (strncmp(check, "</td>", 5) == 0 || strncmp(check, "</th>", 5) == 0) {
                        has_alignment_colons = true;
                        break;
                    }
                    check++;
                    distance++;
                }
            }
        }
        /* Also check for rowspan markers (^^) - these need processing even without other attributes */
        bool has_rowspan_markers = (strstr(html, "^^") != NULL);
        /* Also check for empty first header cell - this indicates row-header detection is needed */
        bool needs_row_header_detection = (strstr(html, "<thead>") != NULL);
        if (!has_alignment_colons && !has_rowspan_markers && !needs_row_header_detection) {
            /* No alignment colons, rowspan markers, or row-header detection needed - return early */
            return (char *)html;
        }
    } else {
        /* If we need all cells, check for alignment colons anyway to know if we should process them */
        /* But first check if most cells already have align attributes (cmark-gfm already processed them) */
        /* If so, we can skip alignment processing entirely */
        const char *align_attr_count = html;
        int cells_with_align = 0;
        int total_cells_checked = 0;
        while ((align_attr_count = strstr(align_attr_count, "align=")) != NULL && total_cells_checked < 100) {
            cells_with_align++;
            total_cells_checked++;
            align_attr_count += 6; /* Skip past "align=" */
        }
        /* If most cells (>=80%) already have align attributes, skip alignment processing */
        if (total_cells_checked >= 20 && cells_with_align * 100 / total_cells_checked >= 80) {
            has_alignment_colons = false; /* Skip alignment processing - already handled by cmark-gfm */
        } else {
            has_alignment_colons = (strstr(html, ":</td>") != NULL || strstr(html, ":</th>") != NULL);
        }
    }

    /* Collect all cells (for mapping calculation) - only needed for attribute processing, not alignment */
    all_cell *all_cells = NULL;
    if (needs_all_cells) {
        /* IMPORTANT: Always process if we have ANY attributes, even if they seem "simple".
         * This ensures rowspan/colspan processing isn't skipped. Only skip if we truly
         * have nothing to process (no attributes, no captions, no tfoot, no alignment). */
        if (attrs == NULL && captions == NULL && tfoot_rows == NULL && !has_alignment_colons) {
            return (char *)html;
        }

        all_cells = collect_all_cells(document);
    }

    /* Allocate output buffer (same size as input, we'll realloc if needed) */
    size_t capacity = strlen(html) * 2;
    char *output = malloc(capacity);
    if (!output) {
        /* Clean up */
        while (all_cells) {
            all_cell *next = all_cells->next;
            free(all_cells);
            all_cells = next;
        }
        while (attrs) {
            cell_attr *next = attrs->next;
            free(attrs->attributes);
            if (attrs->cell_text) free(attrs->cell_text);
            free(attrs);
            attrs = next;
        }
        while (captions) {
            table_caption *next = captions->next;
            free(captions->caption);
            free(captions);
            captions = next;
        }
        while (paras_to_remove) {
            para_to_remove *next = paras_to_remove->next;
            free(paras_to_remove->text_fingerprint);
            free(paras_to_remove);
            paras_to_remove = next;
        }
        return (char *)html;
    }

    const char *read = html;
    char *write = output;
    size_t written = 0;
    int table_idx = -1; /* Track which table we're in */
    int row_idx = -1;
    int ast_row_idx = -1;  /* AST row index corresponding to current HTML row */
    int col_idx = 0;
    int para_idx = -1;  /* Track paragraph index */
    bool in_table = false;
    bool in_row = false;
    bool in_tbody = false;  /* Track if we're currently in tbody */
    bool in_tfoot = false;  /* Track if we're currently in tfoot */
    bool in_thead = false;  /* Track if we're currently in thead */
    bool current_row_is_tfoot = false;  /* Track if current row is a tfoot row */

    /* Pre-calculated mapping for current row: HTML position -> original column index */
    int row_col_mapping[50];  /* row_col_mapping[html_pos] = original_col_index */
    int row_col_mapping_size = 0;

    /* Track if we should process alignment (only if alignment colons were detected) */
    bool should_process_alignment = has_alignment_colons;

    /* Track active rowspan cells per column (inspired by Jekyll Spaceship approach).
     * active_rowspan_cells[col] points to the cell_attr for the cell that's currently
     * being rowspanned in that column. When we see a ^^ cell, we increment its rowspan. */
    cell_attr *active_rowspan_cells[50];  /* One per column */
    for (int i = 0; i < 50; i++) active_rowspan_cells[i] = NULL;

    /* Track tables that have an explicit row-header first column.
     * Detected when the first header row's first cell is empty (|   | Header ...). */
    bool table_has_row_header_first_col[50];
    for (int i = 0; i < 50; i++) table_has_row_header_first_col[i] = false;

    /* Track the previous cell's matching status to check for colspan */
    cell_attr *prev_cell_matching = NULL;

    /* Timeout check: if processing takes more than 10 seconds, skip the rest */
    time_t start_time = time(NULL);
    const time_t timeout_seconds = 10;
    size_t timeout_check_counter = 0;

    while (*read) {
        timeout_check_counter++;

        /* Check timeout every 1000 characters */
        if (timeout_check_counter % 1000 == 0) {
            time_t current_time = time(NULL);
            if (current_time - start_time >= timeout_seconds) {
                /* Copy remaining HTML as-is to avoid corruption */
                while (*read) {
                    *write++ = *read++;
                }
                *write = '\0';
                goto done;
            }
        }
        /* Ensure we have space (realloc if needed) */
        if (written + 100 > capacity) {
            capacity *= 2;
            char *new_output = realloc(output, capacity);
            if (!new_output) break;
            output = new_output;
            write = output + written;
        }
        /* Track table structure (BEFORE cell processing so indices are correct) */
        /* Also fix missing space in table tag (e.g., <tableid= -> <table id=) */
        if (strncmp(read, "<table", 6) == 0 && (read[6] == '>' || read[6] == ' ' || (read[6] == 'i' && strncmp(read + 6, "id=", 3) == 0) || isalnum((unsigned char)read[6]))) {
            in_table = true;
            table_idx++; /* New table */
            row_idx = -1; /* Reset for each new table */
            in_thead = false;

            /* Check if this table has a caption (before fixing spacing so we can add figure tag) */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            /* Fix missing space before id or class attributes */
            if (read[6] == 'i' && strncmp(read + 6, "id=", 3) == 0) {
                /* If we have a caption and it should be above, write figure tag and caption first */
                if (cap && caption_position == 0) {
                    /* Write <figure><figcaption>caption</figcaption> */
                    const char *fig_open = "<figure class=\"table-figure\">\n<figcaption>";
                    size_t fig_open_len = strlen(fig_open);
                    const char *fig_close_cap = "</figcaption>\n";
                    size_t fig_close_cap_len = strlen(fig_close_cap);

                    /* Ensure we have space */
                    while (written + fig_open_len + strlen(cap->caption) * 6 + fig_close_cap_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_close_cap, fig_close_cap_len);
                    write += fig_close_cap_len;
                    written += fig_close_cap_len;
                } else if (cap && caption_position == 1) {
                    /* Caption below - just open figure tag */
                    const char *fig_open = "<figure class=\"table-figure\">\n";
                    size_t fig_open_len = strlen(fig_open);
                    while (written + fig_open_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }
                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;
                }

                /* Write "<table " then copy the rest of the tag */
                memcpy(write, read, 6);
                write += 6;
                *write++ = ' ';
                written += 7;
                read += 6;
                /* Copy the rest of the tag until closing > */
                while (*read && *read != '>') {
                    *write++ = *read++;
                    written++;
                }
                if (*read == '>') {
                    *write++ = *read++;
                    written++;
                }
                continue; /* Skip the normal copy below, we've handled it */
            }

            /* Normal table tag (no spacing fix needed) - check for caption */
            if (cap) {
                if (caption_position == 0) {
                    /* Caption above - write <figure><figcaption>caption</figcaption> */
                    const char *fig_open = "<figure class=\"table-figure\">\n<figcaption>";
                    size_t fig_open_len = strlen(fig_open);
                    const char *fig_close_cap = "</figcaption>\n";
                    size_t fig_close_cap_len = strlen(fig_close_cap);

                    /* Ensure we have space */
                    while (written + fig_open_len + strlen(cap->caption) * 6 + fig_close_cap_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_close_cap, fig_close_cap_len);
                    write += fig_close_cap_len;
                    written += fig_close_cap_len;
                } else {
                    /* Caption below - just open figure tag */
                    const char *fig_open = "<figure class=\"table-figure\">\n";
                    size_t fig_open_len = strlen(fig_open);
                    while (written + fig_open_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }
                    memcpy(write, fig_open, fig_open_len);
                    write += fig_open_len;
                    written += fig_open_len;
                }
            }
        } else if (strncmp(read, "</table>", 8) == 0) {
            /* Close tfoot or tbody if still open */
            if (in_tfoot) {
                const char *tfoot_close = "</tfoot>\n";
                size_t tfoot_close_len = strlen(tfoot_close);
                while (written + tfoot_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tfoot_close, tfoot_close_len);
                write += tfoot_close_len;
                written += tfoot_close_len;
                in_tfoot = false;
            } else if (in_tbody) {
                const char *tbody_close = "</tbody>\n";
                size_t tbody_close_len = strlen(tbody_close);
                while (written + tbody_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tbody_close, tbody_close_len);
                write += tbody_close_len;
                written += tbody_close_len;
                in_tbody = false;
            }

            /* Check if this table had a caption */
            table_caption *cap = NULL;
            for (table_caption *c = captions; c; c = c->next) {
                if (c->table_index == table_idx) {
                    cap = c;
                    break;
                }
            }

            if (cap) {
                /* Write </table> first */
                memcpy(write, read, 8);
                write += 8;
                read += 8;
                written += 8;

                if (caption_position == 1) {
                    /* Caption below - write <figcaption>caption</figcaption> before </figure> */
                    const char *fig_cap_open = "<figcaption>";
                    const char *fig_cap_close = "</figcaption>\n";
                    size_t fig_cap_open_len = strlen(fig_cap_open);
                    size_t fig_cap_close_len = strlen(fig_cap_close);

                    /* Ensure we have space */
                    while (written + fig_cap_open_len + strlen(cap->caption) * 6 + fig_cap_close_len + 100 > capacity) {
                        capacity *= 2;
                        char *new_output = realloc(output, capacity);
                        if (!new_output) break;
                        output = new_output;
                        write = output + written;
                    }

                    memcpy(write, fig_cap_open, fig_cap_open_len);
                    write += fig_cap_open_len;
                    written += fig_cap_open_len;

                    /* Write caption text (escape HTML entities if needed) */
                    const char *cap_text = cap->caption;
                    while (*cap_text) {
                        if (*cap_text == '&') {
                            const char *amp = "&amp;";
                            memcpy(write, amp, 5);
                            write += 5;
                            written += 5;
                        } else if (*cap_text == '<') {
                            const char *lt = "&lt;";
                            memcpy(write, lt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '>') {
                            const char *gt = "&gt;";
                            memcpy(write, gt, 4);
                            write += 4;
                            written += 4;
                        } else if (*cap_text == '"') {
                            const char *quot = "&quot;";
                            memcpy(write, quot, 6);
                            write += 6;
                            written += 6;
                        } else {
                            *write++ = *cap_text;
                            written++;
                        }
                        cap_text++;
                    }

                    memcpy(write, fig_cap_close, fig_cap_close_len);
                    write += fig_cap_close_len;
                    written += fig_cap_close_len;
                }

                /* Close </figure> */
                const char *fig_close = "</figure>\n";
                size_t fig_close_len = strlen(fig_close);

                /* Ensure we have space */
                while (written + fig_close_len + 100 > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }

                memcpy(write, fig_close, fig_close_len);
                write += fig_close_len;
                written += fig_close_len;

                in_table = false;
                continue; /* Skip the normal copy below */
            }

            in_table = false;
        } else if (in_table && strncmp(read, "<thead>", 7) == 0) {
            in_thead = true;
        } else if (in_table && strncmp(read, "</thead>", 8) == 0) {
            in_thead = false;
        } else if (in_table && strncmp(read, "<tbody>", 7) == 0) {
            /* We're entering tbody - mark it */
            in_tbody = true;
            in_tfoot = false;
        } else if (in_table && strncmp(read, "</tbody>", 8) == 0) {
            /* We're leaving tbody - but only if we haven't already closed it for tfoot */
            if (!in_tfoot) {
                in_tbody = false;
            } else {
                /* We're already in tfoot, so skip this </tbody> tag (we already closed it) */
                read += 8;
                continue;
            }
        } else if (in_table && strncmp(read, "<tr>", 4) == 0) {
            row_idx++;
            col_idx = 0;
            prev_cell_matching = NULL;  /* Reset previous cell matching for new row */

            /* Map HTML row index to AST row index 1:1.
             *
             * cmark still emits rows that the AST marks data-remove (dummy headerless
             * thead, em-dash separator leftovers, === markers). Those <tr> tags are
             * present in the HTML we walk here and are stripped only when we decide
             * should_skip_row. Counting only non-removed AST rows desyncs indices and
             * can apply a later row's colspan onto earlier blank cells (| |). */
            ast_row_idx = row_idx;

            /* Pre-calculate which original columns will be visible in this row's HTML.
             * This creates a mapping: HTML position -> original column index.
             * IMPORTANT: Only include columns that render NEW <td> tags in this HTML row.
             * Columns occupied by rowspans from previous rows should NOT be included,
             * because they don't generate new <td> tags in this row. */
            row_col_mapping_size = 0;

            /* For each original column, check if it renders a NEW cell in this HTML row */
            for (int orig_col = 0; orig_col < 50 && row_col_mapping_size < 50; orig_col++) {
                bool renders_new_cell = false;

                /* Check if this column has a non-removed cell in current AST row */
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx &&
                        c->row_index == ast_row_idx &&
                        c->col_index == orig_col &&
                        !c->is_removed) {
                        /* This column has a new cell in the current row */
                        renders_new_cell = true;
                        break;
                    }
                }

                /* Check if this column is covered by an active rowspan from a previous row.
                 * If a previous row has a cell with rowspan that's still active (spans to this row),
                 * then this column won't render a new <td> tag in this HTML row. */
                bool covered_by_rowspan = false;
                if (renders_new_cell && row_idx > 0) {
                    /* Check all previous HTML rows to see if any have a rowspan covering this column */
                    for (int prev_html_row = 0; prev_html_row < row_idx; prev_html_row++) {
                        /* Map previous HTML row to AST row */
                        int prev_ast_row = -1;
                        int prev_html_row_count = -1;
                        for (int r = 0; r < 100; r++) {
                            bool has_non_removed = false;
                            for (all_cell *c = all_cells; c; c = c->next) {
                                if (c->table_index == table_idx &&
                                    c->row_index == r &&
                                    !c->is_removed) {
                                    has_non_removed = true;
                                    break;
                                }
                            }
                            if (has_non_removed) {
                                prev_html_row_count++;
                                if (prev_html_row_count == prev_html_row) {
                                    prev_ast_row = r;
                                    break;
                                }
                            }
                        }
                        if (prev_ast_row >= 0) {
                            /* Check if this AST row has a cell at orig_col with rowspan that spans to current row */
                            for (cell_attr *a = attrs; a; a = a->next) {
                                if (a->table_index == table_idx &&
                                    a->row_index == prev_ast_row &&
                                    a->col_index == orig_col &&
                                    strstr(a->attributes, "rowspan=")) {
                                    int rowspan_val = 1;
                                    sscanf(strstr(a->attributes, "rowspan="), "rowspan=\"%d\"", &rowspan_val);
                                    /* Check if this rowspan spans to the current HTML row */
                                    int rows_spanned = row_idx - prev_html_row;
                                    if (rows_spanned < rowspan_val) {
                                        covered_by_rowspan = true;
                                        break;
                                    }
                                }
                            }
                            if (covered_by_rowspan) break;
                        }
                    }
                }

                /* Do NOT include columns occupied by rowspans from previous rows,
                 * because they don't generate new <td> tags in this HTML row */

                if (renders_new_cell && !covered_by_rowspan) {
                    row_col_mapping[row_col_mapping_size++] = orig_col;
                }
            }

            /* Store the mapping in a way we can access it during cell processing */
            /* We'll use a simple approach: store it in a static array keyed by table_idx and row_idx */
            /* Actually, let's use a simpler approach: process cells immediately using this mapping */

            /* Note: Captions immediately following tables (with no blank line) are not supported.
             * When a caption like [Caption] appears on the line immediately after a table, cmark-gfm
             * parses it as a table row rather than a paragraph, making it difficult to detect and extract
             * reliably. Captions work correctly when:
             * - They appear before the table (with or without blank line)
             * - They appear after the table with a blank line between (parsed as a paragraph)
             * To use a caption after a table, include a blank line between the table and caption.
             */

            /* Check if this row should be in tfoot.
             * tfoot_rows are stored with AST row indices, so we need to check ast_row_idx.
             *
             * A row is in tfoot only when the AST marked it and it comes AFTER the ===
             * marker row. Do not use HTML-position fudge factors — those incorrectly
             * forced the first footer row into tbody for normal GFM tables. */
            current_row_is_tfoot = false;

            /* First, find the === row's AST index */
            int min_equals_row_idx = -1;
            for (int r = 0; r < 100; r++) {
                int eq_total = 0;
                int eq_removed = 0;
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx && c->row_index == r) {
                        eq_total++;
                        if (c->is_removed) eq_removed++;
                    }
                }
                if (eq_total > 0 && eq_total == eq_removed) {
                    /* Prefer rows that look like === markers when attrs are available */
                    bool looks_like_equals = false;
                    if (attrs) {
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index != table_idx || a->row_index != r || !a->cell_text)
                                continue;
                            const char *text = a->cell_text;
                            while (*text && isspace((unsigned char)*text)) text++;
                            if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                looks_like_equals = true;
                                break;
                            }
                        }
                    }
                    if (looks_like_equals || !attrs) {
                        min_equals_row_idx = r;
                        break;
                    }
                    /* Fully-removed non-equals row (e.g. em-dash helper): keep looking */
                    if (min_equals_row_idx < 0) {
                        /* fall through and continue search for a real === row */
                    }
                    continue;
                }
            }

            /* Check if this row is marked as tfoot in the AST */
            bool is_marked_tfoot = false;
            for (tfoot_row *t = tfoot_rows; t; t = t->next) {
                if (t->table_index == table_idx && t->row_index == ast_row_idx) {
                    is_marked_tfoot = true;
                    break;
                }
            }

            if (is_marked_tfoot) {
                if (min_equals_row_idx >= 0) {
                    /* Footer content is strictly after the === marker row */
                    current_row_is_tfoot = (ast_row_idx > min_equals_row_idx);
                } else {
                    /* === row not found as fully-removed; trust AST except header */
                    current_row_is_tfoot = (ast_row_idx > 0);
                }
            }

            /* If we're in tbody and this is a tfoot row, close tbody and open tfoot */
            if (current_row_is_tfoot && in_tbody && !in_tfoot) {
                /* Close tbody */
                const char *tbody_close = "</tbody>\n";
                size_t tbody_close_len = strlen(tbody_close);
                while (written + tbody_close_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tbody_close, tbody_close_len);
                write += tbody_close_len;
                written += tbody_close_len;
                in_tbody = false;

                /* Open tfoot */
                const char *tfoot_open = "<tfoot>\n";
                size_t tfoot_open_len = strlen(tfoot_open);
                while (written + tfoot_open_len > capacity) {
                    capacity *= 2;
                    char *new_output = realloc(output, capacity);
                    if (!new_output) break;
                    output = new_output;
                    write = output + written;
                }
                memcpy(write, tfoot_open, tfoot_open_len);
                write += tfoot_open_len;
                written += tfoot_open_len;
                in_tfoot = true;
            }
            /* Note: Once we're in tfoot, we stay in tfoot - don't reopen tbody */
            /* tfoot rows should be at the end of the table */

            /* Check if this row should be completely removed (all cells marked for removal).
             * Always skip fully-removed rows (dummy headerless thead, em-dash separators, ===),
             * even among the first HTML rows — force_keep only protects real data rows. */
            bool should_skip_row = false;
            bool row_fully_removed = false;

            if (all_cells) {
                int total_cells_in_row = 0;
                int removed_cells_in_row = 0;
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx && c->row_index == ast_row_idx) {
                        total_cells_in_row++;
                        if (c->is_removed) {
                            removed_cells_in_row++;
                        }
                    }
                }
                if (total_cells_in_row > 0 && total_cells_in_row == removed_cells_in_row) {
                    /* Fully-removed === marker rows: drop the whole <tr>.
                     * Em-dash separators and empty dummy headers: keep <tr>, remove
                     * cells individually so an empty <tr></tr> remains (needed for
                     * relaxed-header detection and empty-thead cleanup). */
                    bool is_equals_row = false;
                    if (attrs) {
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index != table_idx || a->row_index != ast_row_idx)
                                continue;
                            if (!a->cell_text) continue;
                            const char *text = a->cell_text;
                            while (*text && isspace((unsigned char)*text)) text++;
                            if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                is_equals_row = true;
                                break;
                            }
                        }
                    }
                    if (is_equals_row) {
                        row_fully_removed = true;
                        should_skip_row = true;
                    } else {
                        row_fully_removed = false;
                        should_skip_row = false;
                    }
                }
            }

            /* Keep early data rows from being skipped by removal heuristics.
             * Do not override tfoot classification here — a short table can have
             * its footer at a low HTML row index once the === marker is omitted. */
            bool force_keep_row = false;
            if (!row_fully_removed && row_idx <= 3 && !current_row_is_tfoot) {
                force_keep_row = true;
                should_skip_row = false;
            }

            /* CRITICAL: Only check if row should be skipped if we haven't already determined
             * it should be in tbody. If current_row_is_tfoot is false, this row was either
             * never marked as tfoot OR was forced to tbody because it appears before === in HTML.
             * Fully-removed rows are already handled via row_fully_removed.
             *
             * IMPORTANT: We must check this AFTER setting current_row_is_tfoot, so we know
             * if the row was forced to tbody. */
            if (!row_fully_removed && !force_keep_row && current_row_is_tfoot) {
                /* Count ALL cells in this row (using all_cells) to see if any are non-removed */
                int total_cells_in_row = 0;
                int removed_cells_in_row = 0;
                for (all_cell *c = all_cells; c; c = c->next) {
                    if (c->table_index == table_idx && c->row_index == ast_row_idx) {
                        total_cells_in_row++;
                        if (c->is_removed) {
                            removed_cells_in_row++;
                        }
                    }
                }
                /* If all cells in this row are marked for removal, skip the entire row */
                /* This applies to tfoot rows that are pure === markers */
                if (total_cells_in_row > 0 && total_cells_in_row == removed_cells_in_row) {
                    should_skip_row = true;
                }
            }
            /* If current_row_is_tfoot is false, this is a data row that should be rendered,
             * regardless of whether cells are marked for removal in the AST */
            /* Also check if this row contains === markers by checking the AST row directly.
             * This handles cases where the row mapping might have issues.
             * BUT: Only check for === markers if this row is actually in tfoot.
             * If current_row_is_tfoot is false (forced to tbody), don't skip it even if it has === markers.
             *
             * CRITICAL: Don't run this check if force_keep_row is true. */
            if (!force_keep_row && !should_skip_row && current_row_is_tfoot) {
                /* Check if any cells in current AST row contain === */
                bool has_equals = false;
                for (cell_attr *a = attrs; a; a = a->next) {
                    if (a->table_index == table_idx && a->row_index == ast_row_idx &&
                        strstr(a->attributes, "data-remove")) {
                        if (a->cell_text) {
                            const char *text = a->cell_text;
                            while (*text && isspace((unsigned char)*text)) text++;
                            if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                has_equals = true;
                                break;
                            }
                        }
                    }
                }
                /* If this row contains === markers, skip it */
                /* BUT: Don't skip if force_keep_row is true (first few rows are protected) */
                if (has_equals && !force_keep_row) {
                    should_skip_row = true;
                } else {
                    /* Also check previous AST row in case row mapping is off */
                    /* BUT: Only do this check if we're actually in tfoot.
                     * If current_row_is_tfoot is false, this row was forced to tbody,
                     * so don't skip it even if the previous row has === markers. */
                    if (ast_row_idx > 0 && current_row_is_tfoot) {
                        int prev_ast_row = ast_row_idx - 1;
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index == table_idx && a->row_index == prev_ast_row &&
                                strstr(a->attributes, "data-remove") && a->cell_text) {
                                const char *text = a->cell_text;
                                while (*text && isspace((unsigned char)*text)) text++;
                                    if (text[0] == '=' && text[1] == '=' && text[2] == '=') {
                                        has_equals = true;
                                        /* If previous row has === and this is first tfoot row, this HTML row is the === row */
                                        /* BUT: Don't skip if this row was forced to tbody (row_idx <= 3) or force_keep_row is true */
                                        if (row_idx <= 4 && !force_keep_row && !(min_equals_row_idx >= 0 && row_idx <= 3)) {
                                            should_skip_row = true;
                                        }
                                        break;
                                    }
                            }
                        }
                    }
                }
            }
            /* FINAL CHECK: If this row was forced to tbody (because it appears before === in HTML),
             * NEVER skip it, regardless of any other conditions. This check happens after all
             * skip logic has been evaluated, to ensure rows forced to tbody are always rendered.
             *
             * IMPORTANT: This check must recalculate min_equals_row_idx to ensure it's available,
             * in case it wasn't calculated earlier or was calculated incorrectly. */
            bool was_forced_to_tbody = false;

            /* First, find the === row's AST index (recalculate if needed) */
            int final_min_equals_row_idx = min_equals_row_idx;
            if (final_min_equals_row_idx < 0) {
                for (int r = 0; r < 100; r++) {
                    int eq_total = 0;
                    int eq_removed = 0;
                    for (all_cell *c = all_cells; c; c = c->next) {
                        if (c->table_index == table_idx && c->row_index == r) {
                            eq_total++;
                            if (c->is_removed) eq_removed++;
                        }
                    }
                    if (eq_total > 0 && eq_total == eq_removed) {
                        final_min_equals_row_idx = r;
                        break;
                    }
                }
            }

            if (final_min_equals_row_idx >= 0) {
                int html_rows_before_equals = -1;
                for (int r = 0; r < 100 && r <= final_min_equals_row_idx; r++) {
                    bool has_non_removed = false;
                    for (all_cell *c = all_cells; c; c = c->next) {
                        if (c->table_index == table_idx &&
                            c->row_index == r &&
                            !c->is_removed) {
                            has_non_removed = true;
                            break;
                        }
                    }
                    if (has_non_removed) {
                        html_rows_before_equals++;
                    }
                }
                /* Check HTML position: if row_idx is <= html_rows_before_equals + 1,
                 * it appears before the === row in HTML, so it must be in tbody */
                if (html_rows_before_equals >= 0 && row_idx <= html_rows_before_equals + 1) {
                    was_forced_to_tbody = true;
                }
                /* Also check AST position as a fallback: if ast_row_idx <= final_min_equals_row_idx,
                 * it's before or at the === row in AST, so it must be in tbody */
                if (!was_forced_to_tbody && ast_row_idx >= 0 && ast_row_idx <= final_min_equals_row_idx) {
                    was_forced_to_tbody = true;
                }
            }

            /* CRITICAL: If this row was forced to tbody, NEVER skip it and ensure it's not in tfoot.
             * This check runs AFTER all skip logic, to ensure rows forced to tbody are always rendered.
             *
             * ADDITIONAL SAFEGUARD: If there's a === row and this is one of the first 3 HTML rows
             * (row_idx <= 3), it's almost certainly in tbody, not tfoot. This handles edge cases
             * where the calculation might be off.
             *
             * FINAL OVERRIDE: This is the last check before skipping, so it must override any
             * previous skip decisions. Also check force_keep_row flag. */
            if (!row_fully_removed && (force_keep_row || was_forced_to_tbody || (final_min_equals_row_idx >= 0 && row_idx <= 3) ||
                (min_equals_row_idx >= 0 && row_idx <= 3))) {
                should_skip_row = false;
                current_row_is_tfoot = false;
            }

            /* FINAL CHECK: If force_keep_row is true, NEVER skip this row, regardless of should_skip_row.
             * Also, if row_idx <= 3, protect it as an extra safeguard.
             * Fully-removed rows (dummy/separator/===) stay skipped. */
            if (!row_fully_removed && (force_keep_row || row_idx <= 3)) {
                should_skip_row = false;
                /* If this is one of the first few rows and there's a === row, it's definitely in tbody */
                if (row_idx <= 3 && min_equals_row_idx >= 0) {
                    current_row_is_tfoot = false;
                }
            }

            if (should_skip_row) {
                /* Skip the opening <tr> tag and everything until </tr> */
                read += 4;
                const char *tr_end = strstr(read, "</tr>");
                if (tr_end) {
                    read = tr_end + 5;
                } else {
                    /* Fallback: skip to next tag */
                    while (*read && strncmp(read, "</tr>", 5) != 0) read++;
                    if (*read) read += 5;
                }
                continue; /* Don't set in_row, skip to next iteration (won't copy <tr>) */
            }
            /* Otherwise, this is a normal row (or tfoot row) - copy <tr> and process cells */
            /* Set in_row BEFORE processing cells so detection code can run */
            in_row = true;
            /* Copy the <tr> tag */
            while (*read && *read != '>') {
                *write++ = *read++;
                written++;
            }
            if (*read == '>') {
                *write++ = *read++;
                written++;
            }
        } else if (in_row && strncmp(read, "</tr>", 5) == 0) {
            in_row = false;
        } else if (strncmp(read, "<p>", 3) == 0) {
            /* Check if this paragraph should be removed */
            para_idx++;
            para_to_remove *para_remove = NULL;
            for (para_to_remove *p = paras_to_remove; p; p = p->next) {
                if (p->para_index == para_idx) {
                    para_remove = p;
                    break;
                }
            }

            /* Extract paragraph text to check */
            const char *para_start = read + 3;
            const char *para_end = strstr(para_start, "</p>");
            if (para_end) {
                /* Check if paragraph starts with [ (caption format) or : (colon caption format) */
                const char *text_start = para_start;
                /* Skip any leading whitespace */
                while (*text_start && text_start < para_end && isspace((unsigned char)*text_start)) text_start++;

                bool is_caption_para = false;
                if (*text_start == '[' ||
                    (text_start < para_end - 4 && strncmp(text_start, "&lt;", 4) == 0)) {
                    /* This looks like a bracket caption paragraph */
                    is_caption_para = true;
                } else if (*text_start == ':' && (text_start + 1) < para_end &&
                           (text_start[1] == ' ' || text_start[1] == '\t')) {
                    /* This looks like a : Caption format paragraph */
                    is_caption_para = true;
                }

                if (is_caption_para) {
                    /* This is a caption paragraph */

                    /* First check: if we have a fingerprint match from AST */
                    if (para_remove && para_remove->text_fingerprint) {
                        const char *fingerprint_text = para_remove->text_fingerprint;
                        if (fingerprint_text && strlen(fingerprint_text) > 0) {
                            /* Simple substring match - check if fingerprint appears anywhere in paragraph content */
                            if (strstr(para_start, fingerprint_text) != NULL) {
                                /* Skip this entire paragraph */
                                read = para_end + 4; /* Skip past </p> */
                                continue; /* Skip normal copy */
                            }
                        }
                    }

                    /* Second check: if we're near a table/figure that has a caption, remove this paragraph */
                    /* This is a fallback for cases where fingerprint matching fails */
                    /* Check if any table has a caption, and if this paragraph's caption text matches */
                    const char *caption_start = NULL;
                    const char *caption_end = NULL;

                    if (*text_start == '[') {
                        /* Bracket format: [Caption] */
                        caption_start = text_start + 1;
                        caption_end = strchr(caption_start, ']');
                        if (!caption_end || caption_end >= para_end) {
                            /* Try HTML entity version */
                            const char *gt_entity = strstr(caption_start, "&gt;");
                            if (gt_entity && gt_entity < para_end) caption_end = gt_entity;
                        }
                    } else if (text_start < para_end - 4 && strncmp(text_start, "&lt;", 4) == 0) {
                        /* HTML entity bracket format: &lt;Caption&gt; */
                        caption_start = text_start + 4;
                        caption_end = strstr(caption_start, "&gt;");
                    } else if (*text_start == ':' && (text_start + 1) < para_end &&
                               (text_start[1] == ' ' || text_start[1] == '\t')) {
                        /* Colon format: : Caption */
                        caption_start = text_start + 2; /* Skip : and space */
                        /* Find IAL at the end (if any) - look for { */
                        caption_end = para_end;
                        const char *ial_search = para_end - 1;
                        while (ial_search >= caption_start) {
                            if (*ial_search == '}') {
                                /* Found closing brace, look backwards for opening brace */
                                const char *open = ial_search;
                                while (open >= caption_start && *open != '{') {
                                    open--;
                                }
                                if (open >= caption_start && *open == '{') {
                                    /* Check if it's a valid IAL pattern */
                                    if ((open[1] == ':' || open[1] == '#' || open[1] == '.') &&
                                        ial_search > open) {
                                        caption_end = open; /* Caption ends before IAL */
                                        break;
                                    }
                                }
                            }
                            ial_search--;
                        }
                        /* Trim whitespace from caption */
                        while (caption_start < caption_end && isspace((unsigned char)*caption_start)) {
                            caption_start++;
                        }
                        while (caption_end > caption_start && isspace((unsigned char)*(caption_end - 1))) {
                            caption_end--;
                        }
                    }

                    if (caption_start && caption_end && caption_end > caption_start && caption_end < para_end) {
                        size_t caption_len = caption_end - caption_start;
                        if (caption_len > 0 && caption_len < 512) {
                            char para_caption[512];
                            memcpy(para_caption, caption_start, caption_len);
                            para_caption[caption_len] = '\0';
                            /* Compare with all table captions - if any match, remove this paragraph */
                            for (table_caption *cap = captions; cap; cap = cap->next) {
                                if (cap->caption && strcmp(para_caption, cap->caption) == 0) {
                                    /* Match! Remove this paragraph */
                                    read = para_end + 4;
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
        }

        /* Check for cell opening tags - process both header and body cells
         * IMPORTANT: Check for <thead> and <tbody> first to avoid matching them as <th> or <td> cells */
        if ((in_row || in_thead) &&
            (strncmp(read, "<td", 3) == 0 || strncmp(read, "<th", 3) == 0) &&
            strncmp(read, "<thead>", 7) != 0 && strncmp(read, "<tbody>", 7) != 0 &&
            strncmp(read, "<tfoot>", 7) != 0) {
            bool is_th = strncmp(read, "<th", 3) == 0 && strncmp(read, "<thead>", 7) != 0;
            /* Extract cell content for debugging and matching - only if we need it */
            char cell_preview[100] = {0};

            /* Extract cell content if we need it for:
             * - Header row detection (first header cell only)
             * - Attribute matching (only if we need content verification)
             * Note: Alignment processing extracts content separately and only when needed */
            bool need_cell_content_for_header = (in_table && !in_tbody && !in_tfoot && in_thead &&
                                                 table_idx >= 0 && table_idx < 50 &&
                                                 row_idx == 0 && col_idx == 0 && is_th);
            bool need_cell_content = need_cell_content_for_header;

            /* For attribute matching, we'll extract content only if we find a potential match
             * that requires content verification (e.g., when there are multiple candidates) */
            if (need_cell_content) {
                const char *close_tag = is_th ? "</th>" : "</td>";
                const char *content_start = strchr(read, '>');
                if (content_start) {
                    const char *content_end = strstr(content_start + 1, close_tag);
                    if (content_end) {
                        /* Extract just the content between > and </td>/</th> */
                        /* For empty cells like <th></th>, content_end is right after content_start */
                        size_t len = content_end - content_start - 1;
                        if (len >= 0 && len < 99) {
                            strncpy(cell_preview, content_start + 1, len);
                            cell_preview[len] = '\0';
                            /* Trim trailing whitespace and newlines */
                            while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                                cell_preview[--len] = '\0';
                            }
                        } else if (len == 0) {
                            /* Empty cell like <th></th> - set cell_preview to empty string */
                            cell_preview[0] = '\0';
                        }
                    }
                }
            }

            /* Detect header row with empty first cell to enable row-header column.
             * We only consider the first header row's first cell (<thead>, first row, col_idx == 0).
             * row_idx starts at -1 and is incremented to 0 for the first <tr> (header row).
             * After the first <tr> increment, row_idx is 0 for the header row.
             * Note: Comments suggest row_idx is 1-based, but code shows it's 0-based (starts at -1, becomes 0). */
            if (in_table && !in_tbody && !in_tfoot && in_thead &&
                table_idx >= 0 && table_idx < 50 &&
                row_idx == 0 && col_idx == 0 && is_th) {
                /* Check if this cell is empty - extract directly from HTML for reliability.
                 * read points to the start of the <th tag, so we need to find the > immediately after it. */
                bool header_first_cell_empty = false;
                /* Find the > that closes this <th tag - it should be right after <th or <th with attributes */
                const char *tag_end = strchr(read, '>');
                if (tag_end && tag_end > read && tag_end < read + 20) {
                    /* Found the > for this <th tag - now find the matching </th> immediately following */
                    const char *content_start = tag_end + 1;
                    const char *content_end = strstr(content_start, "</th>");
                    if (content_end && content_end < read + 100) {
                        /* Check if content between > and </th> is empty or whitespace only */
                        /* For empty cells like <th></th>, content_end should be right after content_start */
                        header_first_cell_empty = true;
                        for (const char *p = content_start; p < content_end; p++) {
                            if (!isspace((unsigned char)*p)) {
                                header_first_cell_empty = false;
                                break;
                            }
                        }
                    } else {
                        /* No closing tag found in reasonable distance - treat as empty */
                        header_first_cell_empty = true;
                    }
                } else {
                    /* No > found in reasonable distance - treat as empty (shouldn't happen) */
                    header_first_cell_empty = true;
                }
                if (header_first_cell_empty) {
                    table_has_row_header_first_col[table_idx] = true;
                }
            }

            /* Use pre-calculated mapping: HTML position -> original column index */
            int target_original_col = -1;
            if (col_idx < row_col_mapping_size) {
                target_original_col = row_col_mapping[col_idx];
            }

            /* Find matching attribute using the mapped original column index and AST row index.
             * Also verify that the cell content matches to avoid matching cells that are covered
             * by rowspans from previous rows.
             *
             * If no match found in current row, also check the previous AST row in case of row
             * detection issues (e.g., when a cell with rowspan is processed in the wrong HTML row).
             *
             * For tfoot rows with === markers, also check the previous AST row since the row mapping
             * might skip the === row if all its cells are marked for removal. */
            cell_attr *matching = NULL;

            if (target_original_col >= 0 && attrs != NULL) {
                /* First, try to match in the current AST row by position (fast) */
                for (cell_attr *a = attrs; a; a = a->next) {
                    if (a->table_index == table_idx &&
                        a->row_index == ast_row_idx &&
                        a->col_index == target_original_col) {
                        /* Found a position match - only verify content if we have cell_text to compare */
                        bool content_matches = true;
                        if (a->cell_text && a->cell_text[0] != '\0') {
                            /* Need to extract cell content for verification (lazy extraction) */
                            if (cell_preview[0] == '\0') {
                                bool is_th = strncmp(read, "<th", 3) == 0;
                                const char *close_tag = is_th ? "</th>" : "</td>";
                                const char *content_start = strchr(read, '>');
                                if (content_start) {
                                    const char *content_end = strstr(content_start + 1, close_tag);
                                    if (content_end && content_end - content_start - 1 < 99) {
                                        size_t len = content_end - content_start - 1;
                                        strncpy(cell_preview, content_start + 1, len);
                                        cell_preview[len] = '\0';
                                        while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                                            cell_preview[--len] = '\0';
                                        }
                                    }
                                }
                            }
                            if (cell_preview[0] != '\0') {
                                /* Compare cell content - trim whitespace for comparison */
                                const char *attr_text = a->cell_text;
                                const char *html_text = cell_preview;
                                /* Skip leading whitespace */
                                while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                                while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                                /* Compare (case-sensitive, but we can make it more lenient if needed) */
                                content_matches = (strncmp(attr_text, html_text, strlen(attr_text)) == 0 &&
                                                 (html_text[strlen(attr_text)] == '\0' || isspace((unsigned char)html_text[strlen(attr_text)])));
                            } else {
                                /* Non-empty AST text must not match an empty HTML cell.
                                 * Otherwise colspan from |full||||| attaches to | | blanks. */
                                content_matches = false;
                            }
                        }
                        if (content_matches) {
                            matching = a;
                            break;
                        }
                    }
                }

                /* If no match found in current row, also check the previous AST row —
                 * but ONLY for === tfoot marker cells. Em-dash separator rows are also
                 * data-remove; applying those to the next row would delete legitimate
                 * blank cells (| |). */
                if (!matching && ast_row_idx > 0) {
                    for (cell_attr *a = attrs; a; a = a->next) {
                        if (a->table_index == table_idx &&
                            a->row_index == ast_row_idx - 1 &&
                            a->col_index == target_original_col &&
                            strstr(a->attributes, "data-remove") &&
                            a->cell_text) {
                            const char *marker = a->cell_text;
                            while (*marker && isspace((unsigned char)*marker)) marker++;
                            if (!(marker[0] == '=' && marker[1] == '=' && marker[2] == '=')) {
                                continue;
                            }
                            /* === marker from previous row — treat as match for removal */
                            matching = a;
                            break;
                        }
                    }
                }

                /* Don't use fallback matching to previous row - it causes incorrect attribute application.
                 * If no match is found in the current row, that's correct - the cell doesn't have any attributes. */
            }

            /* Content-based fallback matching for header/footer rows (or when column-based matching fails).
             * This is important because column mapping can be wrong for rows with colspans.
             * Match cells by content and prioritize cells with colspan/rowspan attributes.
             * Skip this expensive operation for very large tables to avoid timeout. */
            /* Extract cell content if we need it for content-based or rowspan matching */
            if (attrs != NULL && !matching && cell_preview[0] == '\0' && ast_row_idx >= 0) {
                const char *close_tag = is_th ? "</th>" : "</td>";
                const char *content_start = strchr(read, '>');
                if (content_start) {
                    const char *content_end = strstr(content_start + 1, close_tag);
                    if (content_end && content_end - content_start - 1 < 99) {
                        size_t len = content_end - content_start - 1;
                        strncpy(cell_preview, content_start + 1, len);
                        cell_preview[len] = '\0';
                        while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                            cell_preview[--len] = '\0';
                        }
                    }
                }
            }
            if (attrs != NULL && !matching && ast_row_idx >= 0) {
                /* Extract cell content if we haven't already */
                if (cell_preview[0] == '\0') {
                    bool is_th_tag = strncmp(read, "<th", 3) == 0;
                    const char *close_tag_str = is_th_tag ? "</th>" : "</td>";
                    const char *content_start = strchr(read, '>');
                    if (content_start) {
                        const char *content_end = strstr(content_start + 1, close_tag_str);
                        if (content_end && content_end - content_start - 1 < 99) {
                            size_t len = content_end - content_start - 1;
                            strncpy(cell_preview, content_start + 1, len);
                            cell_preview[len] = '\0';
                            while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                                cell_preview[--len] = '\0';
                            }
                        }
                    }
                }

                if (cell_preview[0] != '\0') {
                    /* For very large tables, skip content-based matching to avoid timeout */
                    /* Position-based matching should be sufficient for most cases */
                    int attr_count = 0;
                    for (cell_attr *check = attrs; check && attr_count < 1000; check = check->next) attr_count++;
                    if (attr_count <= 500) {
                        /* Only do expensive content-based matching if we don't have too many attributes */
                        cell_attr *content_match = NULL;
                        cell_attr *span_match = NULL;

                    /* Try to find a cell in the same AST row by matching content */
                    for (cell_attr *a = attrs; a; a = a->next) {
                        if (a->table_index == table_idx &&
                            a->row_index == ast_row_idx &&
                            a->cell_text) {
                            const char *attr_text = a->cell_text;
                            const char *html_text = cell_preview;
                        /* Skip leading whitespace */
                        while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                        while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                        /* Compare - use lenient comparison */
                        size_t attr_len = strlen(attr_text);
                        size_t html_len = strlen(html_text);
                        /* Skip trailing whitespace for comparison */
                        while (attr_len > 0 && isspace((unsigned char)attr_text[attr_len - 1])) attr_len--;
                        while (html_len > 0 && isspace((unsigned char)html_text[html_len - 1])) html_len--;
                        if (attr_len > 0 && html_len > 0 &&
                            attr_len == html_len &&
                            strncmp(attr_text, html_text, attr_len) == 0) {
                            /* Found a match by content */
                            if (!content_match) {
                                content_match = a;  /* Remember first content match */
                            }
                            /* Prefer matches with colspan/rowspan attributes */
                            if (strstr(a->attributes, "colspan") || strstr(a->attributes, "rowspan")) {
                                span_match = a;
                                break;  /* Found the best match, stop searching */
                            }
                        }
                    }
                    }

                    /* Use span_match if available, otherwise use content_match */
                    if (span_match) {
                        matching = span_match;
                    } else if (content_match) {
                        matching = content_match;
                    }
                    }
                }
            }

            /* Additional fallback: If we still don't have a match, try to find any cell in the same
             * table/row with matching content that has colspan. This is important when cells with <<
             * are removed and column indices shift. Extract cell content if we haven't already. */
            if (!matching && ast_row_idx >= 0 && attrs != NULL) {
                /* Extract cell content if we haven't already */
                if (cell_preview[0] == '\0') {
                    bool is_th_tag = strncmp(read, "<th", 3) == 0;
                    const char *close_tag_str = is_th_tag ? "</th>" : "</td>";
                    const char *content_start = strchr(read, '>');
                    if (content_start) {
                        const char *content_end = strstr(content_start + 1, close_tag_str);
                        if (content_end && content_end - content_start - 1 < 99) {
                            size_t len = content_end - content_start - 1;
                            strncpy(cell_preview, content_start + 1, len);
                            cell_preview[len] = '\0';
                            while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                                cell_preview[--len] = '\0';
                            }
                        }
                    }
                }

                if (cell_preview[0] != '\0') {
                    /* Try matching within the same AST row first */
                    for (cell_attr *a = attrs; a; a = a->next) {
                        if (a->table_index == table_idx &&
                            a->row_index == ast_row_idx &&
                            strstr(a->attributes, "colspan") &&
                            a->cell_text) {
                            const char *attr_text = a->cell_text;
                            const char *html_text = cell_preview;
                            /* Trim whitespace */
                            while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                            while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                            size_t attr_len = strlen(attr_text);
                            size_t html_len = strlen(html_text);
                            while (attr_len > 0 && isspace((unsigned char)attr_text[attr_len - 1])) attr_len--;
                            while (html_len > 0 && isspace((unsigned char)html_text[html_len - 1])) html_len--;
                            if (attr_len > 0 && html_len > 0 &&
                                attr_len == html_len &&
                                strncmp(attr_text, html_text, attr_len) == 0) {
                                matching = a;
                                break;
                            }
                        }
                    }

                    /* If no match found, try nearby rows (row index might be off by 1 due to removed rows) */
                    if (!matching) {
                        for (cell_attr *a = attrs; a; a = a->next) {
                            if (a->table_index == table_idx &&
                                (a->row_index == ast_row_idx || a->row_index == ast_row_idx - 1 || a->row_index == ast_row_idx + 1) &&
                                strstr(a->attributes, "colspan") &&
                                a->cell_text) {
                                const char *attr_text = a->cell_text;
                                const char *html_text = cell_preview;
                                /* Trim whitespace */
                                while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                                while (*html_text && isspace((unsigned char)*html_text)) html_text++;
                                size_t attr_len = strlen(attr_text);
                                size_t html_len = strlen(html_text);
                                while (attr_len > 0 && isspace((unsigned char)attr_text[attr_len - 1])) attr_len--;
                                while (html_len > 0 && isspace((unsigned char)html_text[html_len - 1])) html_len--;
                                if (attr_len > 0 && html_len > 0 &&
                                    attr_len == html_len &&
                                    strncmp(attr_text, html_text, attr_len) == 0) {
                                    matching = a;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            /* Final safety fallback for rowspan cells:
             * If we still don't have a match, but this HTML cell's text matches exactly one AST cell
             * in the same table that has a rowspan attribute, use that. This ensures that rowspans
             * computed in the AST (e.g., for the last ^^ in a block) always get injected, even if
             * row/column mapping was slightly off.
             *
             * To avoid mis-applying attributes, we:
             *  - Require an exact trimmed-text match
             *  - Restrict to cells in the same table
             *  - Restrict to cells that already have a \"rowspan\" attribute
             *  - Require that the match be unique (only one candidate) */
            /* Extract cell content if we need it for rowspan matching */
            if (attrs != NULL && !matching && cell_preview[0] == '\0' && ast_row_idx >= 0) {
                const char *close_tag = is_th ? "</th>" : "</td>";
                const char *content_start = strchr(read, '>');
                if (content_start) {
                    const char *content_end = strstr(content_start + 1, close_tag);
                    if (content_end && content_end - content_start - 1 < 99) {
                        size_t len = content_end - content_start - 1;
                        strncpy(cell_preview, content_start + 1, len);
                        cell_preview[len] = '\0';
                        while (len > 0 && (cell_preview[len-1] == '\n' || cell_preview[len-1] == '\r' || isspace((unsigned char)cell_preview[len-1]))) {
                            cell_preview[--len] = '\0';
                        }
                    }
                }
            }
            if (attrs != NULL && !matching && cell_preview[0] != '\0' && ast_row_idx >= 0) {
                cell_attr *rowspan_candidate = NULL;
                bool multiple_candidates = false;

                for (cell_attr *a = attrs; a; a = a->next) {
                    if (a->table_index != table_idx) continue;
                    /* Only consider cells in the same AST row, or at most one row above.
                     * This allows us to recover from small row-mapping off-by-one errors
                     * (e.g., when a header/body boundary shifts indices by 1), while
                     * still preventing rowspans from leaking far down into unrelated
                     * rows (like a later \"Active\" block). */
                    int row_diff = ast_row_idx - a->row_index;
                    if (row_diff < 0 || row_diff > 1) continue;
                    if (!a->cell_text) continue;
                    if (!strstr(a->attributes, "rowspan")) continue;

                    const char *attr_text = a->cell_text;
                    const char *html_text = cell_preview;

                    /* Trim leading whitespace */
                    while (*attr_text && isspace((unsigned char)*attr_text)) attr_text++;
                    while (*html_text && isspace((unsigned char)*html_text)) html_text++;

                    /* Compute trimmed lengths */
                    size_t attr_len = strlen(attr_text);
                    size_t html_len = strlen(html_text);
                    while (attr_len > 0 && isspace((unsigned char)attr_text[attr_len - 1])) attr_len--;
                    while (html_len > 0 && isspace((unsigned char)html_text[html_len - 1])) html_len--;

                    if (attr_len == 0 || html_len == 0) continue;
                    if (attr_len != html_len) continue;
                    if (strncmp(attr_text, html_text, attr_len) != 0) continue;

                    /* We have an exact trimmed-text match for a rowspan cell in this table. */
                    if (!rowspan_candidate) {
                        rowspan_candidate = a;
                    } else {
                        /* More than one candidate with same text - ambiguous, bail out. */
                        multiple_candidates = true;
                        break;
                    }
                }

                if (rowspan_candidate && !multiple_candidates) {
                    matching = rowspan_candidate;
                }
            }

            /* Cells holding only a span marker (^^ or <<) are removed */
            bool is_rowspan_marker = cell_is_only_marker(read, "^^", NULL);
            bool is_colspan_marker = cell_is_only_marker(read, "&lt;&lt;", "<<");

            /* Check if this cell should be removed:
             * 1. If it's matched and marked for removal
             * 2. If it contains "^^" (rowspan marker)
             * 3. If it contains "<< " or "&lt;&lt;" (colspan marker)
             * 4. If it's empty and the previous cell in the same row has colspan (empty cells after colspan should be removed) */
            bool should_remove_cell = false;
            if (matching && strstr(matching->attributes, "data-remove")) {
                should_remove_cell = true;
            } else if (is_rowspan_marker) {
                should_remove_cell = true;
            } else if (is_colspan_marker) {
                should_remove_cell = true;
            }

            /* Check if this empty cell should be removed.
             * Only remove empty cells that are explicitly marked for removal in the AST (part of a colspan).
             * We need to check both:
             * 1. Cells in the mapping (target_original_col >= 0) - check if marked for removal
             * 2. Cells not in the mapping (target_original_col < 0) - these might be part of colspan,
             *    but we should only remove if the previous cell in the same row has colspan.
             *
             * IMPORTANT: Be conservative - only remove empty cells if we're certain they're part of a colspan.
             * Don't remove legitimate empty cells. */
            if (!should_remove_cell && cell_preview[0] == '\0' && ast_row_idx >= 0) {
                if (target_original_col >= 0) {
                    /* Cell is in the mapping - check if explicitly marked for removal in AST */
                    for (cell_attr *a = attrs; a; a = a->next) {
                        if (a->table_index == table_idx &&
                            a->row_index == ast_row_idx &&
                            a->col_index == target_original_col &&
                            strstr(a->attributes, "data-remove")) {
                            should_remove_cell = true;
                            break;
                        }
                    }
                } else if (target_original_col < 0 &&
                           prev_cell_matching &&
                           prev_cell_matching->row_index == ast_row_idx &&
                           strstr(prev_cell_matching->attributes, "colspan")) {
                    /* Cell not in mapping - check if previous cell in same row has colspan > 1.
                     * This is a strong indicator that the empty cell is part of the colspan. */
                    int colspan_val = 1;
                    if (strstr(prev_cell_matching->attributes, "colspan=")) {
                        sscanf(strstr(prev_cell_matching->attributes, "colspan="), "colspan=\"%d\"", &colspan_val);
                    }
                    if (colspan_val > 1) {
                        should_remove_cell = true;
                    }
                }
            }

            if (should_remove_cell) {
                /* Skip this entire cell (including for tfoot rows - === rows are skipped entirely) */
                /* Note: Removed cells are not rendered by the HTML renderer, so we shouldn't see them here.
                 * But if we do (e.g., from cmark-gfm before our processing), skip them.
                 * Also remove cells containing "^^" (rowspan markers) even if they're not matched.
                 * We still increment col_idx to match the column index used when collecting attributes
                 * (which counts all cells including removed ones). */
                bool is_th = strncmp(read, "<th", 3) == 0;
                const char *close_tag = is_th ? "</th>" : "</td>";

                /* Skip opening tag */
                while (*read && *read != '>') read++;
                if (*read == '>') read++;

                /* Skip content until closing tag */
                while (*read && strncmp(read, close_tag, 5) != 0) read++;
                if (strncmp(read, close_tag, 5) == 0) read += 5;

                col_idx++;  /* Increment to match column index from collection (counts all cells) */
                /* Don't reset prev_cell_matching for removed cells - keep it so we can remove
                 * subsequent empty cells that are part of the same colspan range */
                continue;
            } else if (matching && (strstr(matching->attributes, "rowspan") || strstr(matching->attributes, "colspan"))) {
                /* Copy only the tag name (<td or <th). Skip everything until '>' - do not copy, so we never
                 * include erroneously placed cell content (e.g. <td A > or <tdA>). Then inject our attributes
                 * and write '>'. Existing attributes like align are dropped for this cell to avoid copying
                 * malformed content; alignment can be re-applied by postprocess if needed. */
                memcpy(write, read, 3);
                write += 3;
                read += 3;
                while (*read && *read != '>') read++;
                /* Always add a space between tag name and first attribute */
                *write++ = ' ';
                /* Inject our attributes (skip leading space in attrs if present) */
                const char *attr_str = matching->attributes;
                while (*attr_str == ' ' || *attr_str == '\t') attr_str++;
                while (*attr_str) {
                    *write++ = *attr_str++;
                }
                /* Copy or write the '>' so cell content follows correctly */
                if (*read == '>') {
                    *write++ = *read++;
                } else {
                    *write++ = '>';
                }
                col_idx++;
                prev_cell_matching = matching;  /* Track this cell for next cell's colspan check */
                continue;
            }

            /* Convert first-column body cells to row headers when the header
             * row's first cell was empty (|   | Header ...).
             * We emit <th scope="row"> for those cells before any alignment processing. */
            bool make_row_header = false;
            if (!is_th &&
                in_tbody &&
                in_row &&
                table_idx >= 0 && table_idx < 50 &&
                table_has_row_header_first_col[table_idx] &&
                col_idx == 0) {
                make_row_header = true;
            }

            /* Also detect empty first header cell here as a fallback, in case the earlier detection didn't run.
             * This runs for header cells (is_th) when processing the first cell (col_idx == 0) in the first row (row_idx == 0).
             * We need to check BEFORE col_idx is incremented, so this check happens early in the cell processing. */
            if (!make_row_header &&
                in_table && !in_tbody && !in_tfoot && in_thead &&
                table_idx >= 0 && table_idx < 50 &&
                row_idx == 0 && col_idx == 0 && is_th) {
                /* Check if this cell is empty - extract directly from HTML for reliability */
                bool is_empty = false;
                const char *close_tag = "</th>";
                const char *tag_end = strchr(read, '>');
                if (tag_end && tag_end > read && tag_end < read + 20) {
                    const char *content_start = tag_end + 1;
                    const char *content_end = strstr(content_start, close_tag);
                    if (content_end && content_end < read + 100) {
                        /* Check if content between > and </th> is empty or whitespace only */
                        is_empty = true;
                        for (const char *p = content_start; p < content_end; p++) {
                            if (!isspace((unsigned char)*p)) {
                                is_empty = false;
                                break;
                            }
                        }
                    } else {
                        /* No closing tag found in reasonable distance - treat as empty */
                        is_empty = true;
                    }
                } else {
                    /* No > found in reasonable distance - treat as empty (shouldn't happen) */
                    is_empty = true;
                }
                if (is_empty && !table_has_row_header_first_col[table_idx]) {
                    table_has_row_header_first_col[table_idx] = true;
                }
            }

            if (make_row_header) {
                const char *tag_end = strchr(read, '>');
                if (tag_end) {
                    const char *cell_content_start = tag_end + 1;
                    const char *close_tag_td = strstr(cell_content_start, "</td>");
                    const char *close_tag_th = strstr(cell_content_start, "</th>");
                    const char *cell_content_end = NULL;
                    const char *orig_close = NULL;

                    if (close_tag_td && (!close_tag_th || close_tag_td < close_tag_th)) {
                        cell_content_end = close_tag_td;
                        orig_close = close_tag_td;
                    } else if (close_tag_th) {
                        cell_content_end = close_tag_th;
                        orig_close = close_tag_th;
                    }

                    if (cell_content_end) {
                        /* Write <th scope="row"> */
                        const char *th_open = "<th scope=\"row\">";
                        size_t th_open_len = strlen(th_open);
                        memcpy(write, th_open, th_open_len);
                        write += th_open_len;
                        written += th_open_len;

                        /* Copy original cell content */
                        const char *p = cell_content_start;
                        while (p < cell_content_end) {
                            *write++ = *p++;
                            written++;
                        }

                        /* Write closing </th> */
                        const char *th_close = "</th>";
                        size_t th_close_len = strlen(th_close);
                        memcpy(write, th_close, th_close_len);
                        write += th_close_len;
                        written += th_close_len;

                        /* Advance read pointer past original closing tag */
                        read = orig_close;
                        if (strncmp(read, "</td>", 5) == 0 || strncmp(read, "</th>", 5) == 0) {
                            read += 5;
                        }

                        col_idx++;
                        prev_cell_matching = matching;
                        continue;
                    }
                }
            }

            /* Process cell alignment (check for leading/trailing colons) for cells without spans or row-header conversion */
            /* Only process if alignment colons were detected in the early exit check */
            if (should_process_alignment) {
                /* Fast inline check: look for '>' in opening tag (avoid strchr if possible) */
                const char *tag_end = read;
                int tag_len = 0;
                while (tag_len < 100 && *tag_end && *tag_end != '>') {
                    tag_end++;
                    tag_len++;
                }
                if (*tag_end != '>') {
                    /* Tag too long or malformed, skip alignment processing */
                    col_idx++;
                    prev_cell_matching = matching;
                    /* Continue with normal character-by-character processing */
                } else {
                    /* Check if cell already has align attribute (from column alignment) */
                    /* We still need to check for per-cell alignment colons to override column alignment */
                    bool has_align_attr = false;
                    const char *align_attr_start = NULL;
                    const char *align_attr_end = NULL;
                    const char *tag_check = read;

                    /* Scan up to tag_end for "align=" */
                    for (int i = 0; i < tag_len && tag_check < tag_end; i++, tag_check++) {
                        if (strncmp(tag_check, "align=", 6) == 0) {
                            has_align_attr = true;
                            align_attr_start = tag_check;
                            /* Find the end of the align attribute value */
                            const char *quote = strchr(tag_check + 6, '"');
                            if (quote) {
                                align_attr_end = strchr(quote + 1, '"');
                            }
                            break;
                        }
                    }

                    /* Fast check: look for colon in cell content before extracting full content */
                    /* This avoids expensive strchr/strstr for cells that clearly don't have alignment */
                    bool is_th = strncmp(read, "<th", 3) == 0;
                    const char *close_tag = is_th ? "</th>" : "</td>";
                    /* Use inline search for close tag (faster than strstr for short distances) */
                    const char *close_tag_pos = tag_end + 1;
                    bool found_close = false;
                    /* Limit search to first 500 chars to avoid scanning huge cells */
                    for (int i = 0; i < 500 && *close_tag_pos; i++) {
                        if (strncmp(close_tag_pos, close_tag, 5) == 0) {
                            found_close = true;
                            break;
                        }
                        close_tag_pos++;
                    }

                    if (!found_close) {
                        /* Close tag not found nearby, skip alignment processing */
                        col_idx++;
                        prev_cell_matching = matching;
                        /* Continue with normal character-by-character processing */
                    } else if (close_tag_pos && close_tag_pos > tag_end + 1) {
                        /* Quick check for colon - after whitespace, alignment colons are only at start or end */
                        /* This is much faster than scanning 50+ characters */
                        bool has_colon = false;
                        const char *content_start = tag_end + 1;
                        const char *content_end = close_tag_pos;

                        /* Skip leading whitespace and check first non-whitespace character */
                        const char *first_char = content_start;
                        while (first_char < content_end && isspace((unsigned char)*first_char)) {
                            first_char++;
                        }
                        if (first_char < content_end && *first_char == ':') {
                            has_colon = true;
                        }

                        /* If not found, skip trailing whitespace and check last non-whitespace character */
                        if (!has_colon) {
                            const char *last_char = content_end - 1;
                            while (last_char > content_start && isspace((unsigned char)*last_char)) {
                                last_char--;
                            }
                            if (last_char >= content_start && *last_char == ':') {
                                has_colon = true;
                            }
                        }

                        if (!has_colon) {
                            /* No colon found, skip alignment processing for this cell */
                            col_idx++;
                            prev_cell_matching = matching;
                            /* Continue with normal character-by-character processing */
                        } else {
                            /* Colon found, extract full content and process alignment */
                            const char *cell_content_start = tag_end + 1;
                            const char *cell_content_end = close_tag_pos;

                            /* Quick check: if content is too long, skip alignment processing to avoid timeout */
                            size_t content_len = cell_content_end - cell_content_start;
                            if (content_len > 10000) {
                                /* Content too long, skip alignment processing for this cell */
                                col_idx++;
                                prev_cell_matching = matching;
                                /* Continue with normal character-by-character processing */
                            } else {
                                /* Check for alignment colons */
                                const char *content_start = cell_content_start;
                                const char *content_end = cell_content_end;
                                char *align_style = NULL;

                                if (process_cell_alignment(&content_start, &content_end, &align_style)) {
                                    /* Per-cell alignment detected - override column alignment */
                                    /* Copy the opening tag, but remove existing align attribute if present */
                                    if (has_align_attr && align_attr_start && align_attr_end) {
                                        /* Copy up to align attribute */
                                        while (read < align_attr_start) {
                                            *write++ = *read++;
                                        }
                                        /* Skip the align attribute (including quotes) */
                                        read = align_attr_end + 1;
                                        /* Remove any trailing space before '>' */
                                        while (read < tag_end && (*read == ' ' || *read == '\t')) {
                                            read++;
                                        }
                                    } else {
                                        /* Copy the opening tag up to '>' */
                                        while (*read && *read != '>') {
                                            *write++ = *read++;
                                        }
                                    }

                                    /* Add style attribute before closing '>' */
                                    if (*read == '>') {
                                        /* Add style attribute (overrides column alignment) */
                                        *write++ = ' ';
                                        *write++ = 's';
                                        *write++ = 't';
                                        *write++ = 'y';
                                        *write++ = 'l';
                                        *write++ = 'e';
                                        *write++ = '=';
                                        *write++ = '"';
                                        const char *style_str = align_style;
                                        while (*style_str) {
                                            *write++ = *style_str++;
                                        }
                                        *write++ = '"';
                                        free(align_style);
                                    }

                                    /* Copy the '>' */
                                    if (*read == '>') {
                                        *write++ = *read++;
                                    }

                                    /* Copy modified content (with colons removed) */
                                    while (content_start < content_end) {
                                        *write++ = *content_start++;
                                    }

                                    /* Skip original content and write closing tag */
                                    read = cell_content_end;
                                    memcpy(write, close_tag, 5);
                                    write += 5;
                                    read += 5;

                                    col_idx++;
                                    prev_cell_matching = matching;
                                    continue;
                                }
                                /* If alignment processing failed, free align_style if it was allocated */
                                if (align_style) {
                                    free(align_style);
                                }
                            }
                        }
                    }
                }
            }  /* End of should_process_alignment check */

            /* For normal cells (no special processing), copy the entire cell tag and content.
             * This ensures cells are properly output while still advancing read correctly. */
            if (!should_remove_cell && !matching && !make_row_header) {
                const char *cell_close_tag = is_th ? "</th>" : "</td>";
                /* Copy opening tag */
                while (*read && *read != '>') {
                    *write++ = *read++;
                    written++;
                }
                if (*read == '>') {
                    *write++ = *read++;
                    written++;
                }
                /* Copy content until closing tag */
                while (*read && strncmp(read, cell_close_tag, 5) != 0) {
                    *write++ = *read++;
                    written++;
                }
                /* Copy closing tag */
                if (strncmp(read, cell_close_tag, 5) == 0) {
                    for (int i = 0; i < 5; i++) {
                        *write++ = *read++;
                        written++;
                    }
                }
                col_idx++;
                prev_cell_matching = matching;
                continue;
            }

            col_idx++;
            prev_cell_matching = matching;  /* Track this cell for next cell's colspan check */
        }

        /* Copy character */
        *write++ = *read++;
        written++;
    }

    *write = '\0';

    /* Clean up all_cells list */
    while (all_cells) {
        all_cell *next = all_cells->next;
        free(all_cells);
        all_cells = next;
    }

    /* Clean up attributes list */
    while (attrs) {
        cell_attr *next = attrs->next;
        free(attrs->attributes);
        if (attrs->cell_text) free(attrs->cell_text);
        free(attrs);
        attrs = next;
    }

    /* Clean up captions list */
    while (captions) {
        table_caption *next = captions->next;
        free(captions->caption);
        free(captions);
        captions = next;
    }

    /* Clean up paragraphs to remove list */
    while (paras_to_remove) {
        para_to_remove *next = paras_to_remove->next;
        free(paras_to_remove->text_fingerprint);
        free(paras_to_remove);
        paras_to_remove = next;
    }

done:
    /* Replace placeholder for escaped \<< with &lt;&lt; so literal << displays correctly. */
    if (output) {
        const char *replacement = ESCAPED_LTLT_REPLACEMENT;
        const size_t replacement_len = ESCAPED_LTLT_REPLACEMENT_LEN;

        size_t out_len = strlen(output);
        size_t n = 0;
        const char *p = output;
        while (p + ESCAPED_LTLT_PLACEHOLDER_LEN <= output + out_len) {
            if (memcmp(p, ESCAPED_LTLT_PLACEHOLDER, ESCAPED_LTLT_PLACEHOLDER_LEN) == 0) {
                n++;
                p += ESCAPED_LTLT_PLACEHOLDER_LEN;
            } else {
                p++;
            }
        }
        if (n > 0) {
            size_t new_len = out_len - n * ESCAPED_LTLT_PLACEHOLDER_LEN + n * replacement_len;
            char *new_out = malloc(new_len + 1);
            if (new_out) {
                char *w = new_out;
                const char *r = output;
                while (r < output + out_len) {
                    if (r + ESCAPED_LTLT_PLACEHOLDER_LEN <= output + out_len &&
                        memcmp(r, ESCAPED_LTLT_PLACEHOLDER, ESCAPED_LTLT_PLACEHOLDER_LEN) == 0) {
                        memcpy(w, replacement, replacement_len);
                        w += replacement_len;
                        r += ESCAPED_LTLT_PLACEHOLDER_LEN;
                    } else {
                        *w++ = *r++;
                    }
                }
                *w = '\0';
                free(output);
                output = new_out;
                out_len = new_len;
            }
        }

    }
    return output;
}

