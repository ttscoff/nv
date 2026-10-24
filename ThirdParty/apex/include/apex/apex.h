/**
 * Apex - Unified Markdown Processor
 *
 * A comprehensive Markdown processor with support for CommonMark, GFM,
 * MultiMarkdown, Kramdown, and Marked's special syntax extensions.
 */

#ifndef APEX_H
#define APEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "ast_terminal.h"
#ifndef CMARK_GFM_H
typedef struct cmark_node cmark_node;
#endif

#define APEX_VERSION_MAJOR 1
#define APEX_VERSION_MINOR 1
#define APEX_VERSION_PATCH 47
#define APEX_VERSION_STRING "1.1.47"

/**
 * Processor compatibility modes
 */
#ifndef APEX_MODE_DEFINED
#define APEX_MODE_DEFINED
typedef enum {
    APEX_MODE_COMMONMARK = 0,      /* Pure CommonMark spec */
    APEX_MODE_GFM = 1,              /* GitHub Flavored Markdown */
    APEX_MODE_MULTIMARKDOWN = 2,    /* MultiMarkdown compatibility */
    APEX_MODE_KRAMDOWN = 3,         /* Kramdown compatibility */
    APEX_MODE_UNIFIED = 4,          /* All features enabled */
    APEX_MODE_QUARTO = 5            /* Pandoc/Quarto markdown (HTML-oriented) */
} apex_mode_t;
#endif

/** True for unified and quarto processor modes. */
static inline bool apex_mode_is_unified_family(apex_mode_t mode) {
    return mode == APEX_MODE_UNIFIED || mode == APEX_MODE_QUARTO;
}

/** True for kramdown, unified, or quarto processor modes. */
static inline bool apex_mode_is_kramdown_or_unified_family(apex_mode_t mode) {
    return mode == APEX_MODE_KRAMDOWN || apex_mode_is_unified_family(mode);
}

/**
 * Output format options
 */
typedef enum {
    APEX_OUTPUT_HTML = 0,          /* HTML output (default) */
    APEX_OUTPUT_JSON = 1,           /* Pandoc-style JSON AST (before filters) */
    APEX_OUTPUT_JSON_FILTERED = 2, /* Pandoc-style JSON AST (after filters/postprocessing) */
    APEX_OUTPUT_MARKDOWN = 3,       /* Unified-mode compatible Markdown */
    APEX_OUTPUT_MMD = 4,            /* MultiMarkdown-compatible Markdown */
    APEX_OUTPUT_COMMONMARK = 5,     /* CommonMark-compatible Markdown */
    APEX_OUTPUT_KRAMDOWN = 6,       /* Kramdown-compatible Markdown */
    APEX_OUTPUT_GFM = 7,            /* GitHub Flavored Markdown */
    APEX_OUTPUT_TERMINAL = 8,       /* ANSI terminal output (8/16-color) */
    APEX_OUTPUT_TERMINAL256 = 9,    /* ANSI terminal output (256-color) */
    APEX_OUTPUT_MAN = 10,           /* roff (man page source) */
    APEX_OUTPUT_MAN_HTML = 11,      /* styled HTML man page */
    APEX_OUTPUT_TOC = 12,           /* Markdown TOC list only */
    APEX_OUTPUT_RTF = 13            /* Rich Text Format */
} apex_output_format_t;

/**
 * Flat TOC entry for structured outlines (Swift/ObjC table views, etc.).
 * level is 1-6; text and id are heap-owned when returned from Apex APIs.
 */
typedef struct apex_toc_entry {
    int level;
    char *text;
    char *id;
} apex_toc_entry;

/* Markdown dialects for AST->Markdown serialization. */
#ifndef APEX_MARKDOWN_DIALECT_DEFINED
#define APEX_MARKDOWN_DIALECT_DEFINED
typedef enum {
    APEX_MD_DIALECT_UNIFIED = 0,
    APEX_MD_DIALECT_MMD = 1,
    APEX_MD_DIALECT_COMMONMARK = 2,
    APEX_MD_DIALECT_KRAMDOWN = 3,
    APEX_MD_DIALECT_GFM = 4
} apex_markdown_dialect_t;
#endif



/* Plugin phases */
typedef enum {
    APEX_PLUGIN_PHASE_PRE_PARSE  = 1 << 0,
    APEX_PLUGIN_PHASE_BLOCK      = 1 << 1,
    APEX_PLUGIN_PHASE_INLINE     = 1 << 2,
    APEX_PLUGIN_PHASE_POST_RENDER= 1 << 3
} apex_plugin_phase_mask;

typedef struct apex_plugin_manager apex_plugin_manager;

/**
 * Register a new callback plugin.
 * @return Returns true if the plugin was registered successfully.
 */
bool apex_plugin_register(apex_plugin_manager *manager, const char *id, apex_plugin_phase_mask phase, char *(*callback)(const char *text, const char *id_plugin, apex_plugin_phase_mask phase, const struct apex_options *options));

struct cmark_parser;  /* Opaque; for cmark_init callback. Include cmark-gfm when implementing. */

/**
 * Configuration options for the parser and renderer
 */
typedef struct apex_options {
    apex_mode_t mode;

    /* Feature flags */
    bool enable_plugins;  /* Enable external/plugin processing */
    bool allow_external_plugin_detection; /* Enable detection of external plugins */
    void (*plugin_register)(apex_plugin_manager *manager, const struct apex_options *options); /* Function to register callback plugins */

    bool enable_tables;
    bool enable_footnotes;
    bool enable_definition_lists;
    bool enable_smart_typography;
    bool enable_math;
    bool enable_critic_markup;
    bool enable_wiki_links;
    bool enable_task_lists;
    bool enable_attributes;
    bool enable_callouts;
    bool enable_py_callouts;      /* Enable Python-Markdown !!! callout preprocessing */
    bool enable_quarto_callouts;  /* Enable Quarto ::: callout preprocessing */
    bool enable_quarto_extensions; /* Enable Pandoc/Quarto-specific preprocessors ({=raw}, example lists, etc.) */
    bool enable_quarto_raw;        /* Enable {=format} raw content preprocessing */
    bool enable_quarto_example_lists; /* Enable (@) example list markers (quarto-list-continuation) */
    bool enable_quarto_line_blocks;  /* Enable | line blocks */
    bool enable_quarto_roman_lists;  /* Enable i) ii) roman list markers */
    bool enable_quarto_code_attrs;   /* Enable ```{.lang attr="val"} fence attributes */
    bool enable_quarto_diagrams;   /* Enable Quarto diagram fences ({mermaid}, {dot}, {graphviz}) */
    bool enable_quarto_shortcodes; /* Enable Quarto shortcode shim ({{< ... >}}) */
    bool enable_quarto_strict_lists; /* Require blank line before list blocks (Pandoc strict) */
    bool enable_quarto_xrefs;      /* Wrap @fig-/@sec- cross-refs in span.quarto-xref */
    bool enable_marked_extensions;
    bool enable_divs;  /* Enable Pandoc fenced divs (unified/quarto modes) */
    bool enable_spans;  /* Enable bracketed spans [text]{IAL} (Pandoc-style) */
    bool enable_grid_tables;  /* Enable Pandoc grid table syntax (preprocess to pipe tables) */

    /* Critic markup mode */
    int critic_mode;  /* 0=accept, 1=reject, 2=markup (default) */

    /* Metadata handling */
    bool strip_metadata;
    bool enable_metadata_variables;  /* [%key] replacement */
    bool enable_metadata_transforms; /* [%key:transform] transforms */

    /* File inclusion */
    bool enable_file_includes;
    int max_include_depth;
    const char *base_directory;

    /* Output options */
    apex_output_format_t output_format;  /* Output format (HTML, JSON, Markdown variants) */
    bool unsafe;  /* Allow raw HTML */
    bool validate_utf8;
    bool github_pre_lang;  /* Use GitHub code block language format */
    bool standalone;  /* Generate complete HTML document */
    bool pretty;      /* Pretty-print HTML with indentation */
    bool xhtml;       /* HTML5 output with self-closing void/empty tags (<br />, <meta ... />) */
    bool strict_xhtml; /* Polyglot XHTML/XML: xmlns, strict Content-Type meta with --standalone; void-tag serialization always (implies xhtml). Fragments are not fully validated as XML if raw HTML is ill-formed */
    const char **stylesheet_paths;  /* NULL-terminated array of CSS file paths to link in head */
    size_t stylesheet_count;        /* Number of stylesheets */
    const char *document_title;   /* Title for HTML document */

    /* Terminal / CLI rendering options */
    const char *theme_name;      /* Optional terminal theme name (for -t terminal/terminal256) */
    int terminal_width;          /* Optional fixed wrapping width for terminal output (0 = auto / none) */
    bool paginate;               /* When true and output_format is terminal/terminal256, page output via pager */
    bool paginate_symbols;       /* When paginating, render images as chafa ANSI symbols (less -R compatible) */
    bool terminal_inline_images; /* When true, render local images via imgcat/chafa/viu/catimg on a TTY */
    int terminal_image_width;    /* Max width/cells for terminal image tools (default 50; 0 = use 50) */

    /* Line break handling */
    bool hardbreaks;  /* Treat newlines as hard breaks (GFM style) */
    bool nobreaks;    /* Render soft breaks as spaces */

    /* Header ID generation */
    bool generate_header_ids;  /* Generate IDs for headers */
    bool header_anchors;  /* Generate <a> anchor tags instead of header IDs */
    int id_format;  /* 0=GFM (with dashes), 1=MMD (no dashes) */

    /* TOC depth defaults (used by -t toc and by HTML TOC markers without an explicit range) */
    int toc_min;  /* Inclusive min heading level (default 1) */
    int toc_max;  /* Inclusive max heading level (default 3) */

    /* Optional structured TOC capture (normally NULL). Prefer apex_markdown_to_toc_entries().
     * When both are non-NULL and output_format is APEX_OUTPUT_TOC, apex_markdown_to_html
     * fills *toc_entries_out / *toc_entries_count_out (free with apex_toc_entries_free)
     * and returns an empty string. */
    apex_toc_entry **toc_entries_out;
    size_t *toc_entries_count_out;

    /* Table options */
    bool relaxed_tables;  /* Support tables without separator rows (kramdown/unified only) */
    int caption_position;  /* 0=above, 1=below (default: 1=below) */
    bool per_cell_alignment;  /* Enable per-cell alignment markers (colons at start/end of cells) */

    /* List options */
    bool allow_mixed_list_markers;  /* Allow mixed list markers at same level (inherit type from first item) */
    bool allow_alpha_lists;  /* Support alpha list markers (a., b., c. and A., B., C.) */

    /* Superscript and subscript */
    bool enable_sup_sub;  /* Support MultiMarkdown-style ^text^ and ~text~ syntax */

    /* Strikethrough (GFM-style ~~text~~) */
    bool enable_strikethrough;

    /* Autolink options */
    bool enable_autolink;  /* Enable autolinking of URLs and email addresses */
    bool obfuscate_emails; /* Obfuscate email links/text using HTML entities */

    /* Image options */
    bool embed_images;           /* Embed local images as base64 data URLs */
    bool enable_image_captions;  /* Wrap images in <figure> with <figcaption> when alt/title present */
    bool title_captions_only;    /* When enable_image_captions is true, only add captions for images with a title attribute (ignore alt) */

    /* Citation options */
    bool enable_citations;  /* Enable citation processing */
    char **bibliography_files;  /* NULL-terminated array of bibliography file paths */
    const char *csl_file;  /* CSL style file path */
    bool suppress_bibliography;  /* Suppress bibliography output */
    bool link_citations;  /* Link citations to bibliography entries */
    bool show_tooltips;  /* Show tooltips on citations */
    const char *nocite;  /* Comma-separated citation keys to include without citing, or "*" for all */

    /* Index options */
    bool enable_indices;  /* Enable index processing */
    bool enable_mmark_index_syntax;  /* Enable mmark (!item) syntax */
    bool enable_textindex_syntax;  /* Enable TextIndex {^} syntax */
    bool enable_leanpub_index_syntax;  /* Enable Leanpub {i: "term"} syntax */
    bool suppress_index;  /* Suppress index output */
    bool group_index_by_letter;  /* Group index entries by first letter */
    char **concordance_files;  /* NULL-terminated TextIndex concordance TSV paths */

    /* Wiki link options */
    int wikilink_space;  /* Space replacement: 0=dash, 1=none, 2=underscore, 3=space */
    const char *wikilink_extension;  /* File extension to append (e.g., "html") */
    bool wikilink_sanitize;  /* Sanitize URLs: lowercase, remove apostrophes, replace non-alnum */

    /* Script injection options */
    /* Raw <script>...</script> HTML snippets to inject either:
     * - Before </body> when generating standalone HTML
     * - At the end of the HTML fragment in snippet mode
     *
     * This is typically populated by the CLI --script flag.
     */
    char **script_tags;  /* NULL-terminated array of script tag strings (may be NULL for none) */

    /* Stylesheet embedding options */
    /* When true and a stylesheet path is provided, Apex will attempt to
     * read the CSS file and embed it directly into a <style> block in the
     * document head instead of emitting a <link rel="stylesheet"> tag.
     * This is typically enabled via the CLI --embed-css flag.
     */
    bool embed_stylesheet;

    /* ARIA accessibility options */
    bool enable_aria;  /* Add ARIA labels and accessibility attributes to HTML output */

    /* Emoji options */
    bool enable_emoji;  /* Replace :name: with Unicode/image emoji (GFM/unified family by default) */
    bool enable_emoji_autocorrect;  /* Enable emoji name autocorrect (enabled by default in unified mode) */

    /* Syntax highlighting options */
    const char *code_highlighter;   /* External tool: "pygments", "skylighting", "shiki", or NULL for no highlighting */
    bool code_line_numbers;         /* Enable line numbers in syntax-highlighted code blocks */
    bool highlight_language_only;   /* Only highlight code blocks that have a language specified */
    const char *code_highlight_theme; /* Theme/style name for external syntax highlighters (tool-specific) */

    /* Marked / integration-specific options */
    bool enable_widont;                 /* Apply widont to headings (prevent short widows) */
    bool code_is_poetry;                /* Treat unlanguaged code blocks as poetry */
    bool enable_markdown_in_html;       /* Process markdown inside HTML when enabled */
    bool random_footnote_ids;           /* Use hash-based/randomized footnote IDs */
    bool enable_hashtags;               /* Convert #tags to span-marked hashtags */
    bool style_hashtags;                /* Use styled hashtag class instead of basic */
    bool proofreader_mode;              /* Convert == / ~~ to CriticMarkup syntax */
    bool hr_page_break;                 /* Replace <hr> with page break divs */
    bool title_from_h1;                 /* Use first H1 as document title fallback */
    bool page_break_before_footnotes;   /* Insert page break before footnotes section */

    /* Source file information for plugins */
    /* When Apex is invoked on a file, this is the full path to that file. */
    /* When reading from stdin, this is either the base directory (if set) or empty. */
    const char *input_file_path;

    /* AST filter options (Pandoc-style JSON filters) */
    /* When non-NULL and ast_filter_count > 0, Apex will serialize the */
    /* cmark AST to a Pandoc-compatible JSON AST, pipe it through each  */
    /* configured filter command, and then parse the transformed JSON    */
    /* back into a cmark AST before rendering.                           */
    const char **ast_filter_commands;  /* Array of command strings */
    size_t ast_filter_count;           /* Number of filter commands */
    bool ast_filter_strict;            /* If true, abort on filter error/invalid JSON */

    /* Progress reporting callback */
    /* Called during processing to report progress. Parameters:
     * - stage: Description of current processing stage (e.g., "Processing tables", "Running plugin: kbd")
     * - percent: Progress percentage (0-100), or -1 if unknown
     * - user_data: User-provided context (can be NULL)
     * If NULL, no progress reporting is performed.
     */
    void (*progress_callback)(const char *stage, int percent, void *user_data);
    void *progress_user_data;  /* User data passed to progress callback */

    /* Custom cmark extension registration callback */
    /* Called after Apex registers its built-in extensions, before parsing.
     * Use this to attach custom cmark-gfm syntax extensions via
     * cmark_parser_attach_syntax_extension(). When implementing, include
     * cmark-gfm.h and cmark-gfm-extension_api.h.
     * If NULL, no custom extensions are registered.
     *
     * The user_data parameter receives options->cmark_user_data.
     */
    void (*cmark_init)(struct cmark_parser *parser, const struct apex_options *options, int cmark_opts, void *user_data);
    /**
     * Custom cmark finalize callback, called before release the parser.
     *
     * The user_data parameter receives options->cmark_user_data.
     */
    void (*cmark_done)(struct cmark_parser *parser, const struct apex_options *options, int cmark_opts, void *user_data);
    void *cmark_user_data; /* User data passed to cmark init/done callback */
} apex_options;

/* AST serializers */
char *apex_cmark_to_markdown(cmark_node *document,
                             const struct apex_options *options,
                             apex_markdown_dialect_t dialect);
char *apex_cmark_to_man_roff(cmark_node *document, const struct apex_options *options);
char *apex_cmark_to_man_html(cmark_node *document, const struct apex_options *options);
char *apex_cmark_to_rtf(cmark_node *document, const struct apex_options *options);

/**
 * Get default options for a specific mode
 */
apex_options apex_options_default(void);
apex_options apex_options_for_mode(apex_mode_t mode);

/**
 * Load metadata from an external file and apply it to options.
 * Supports the same YAML, Pandoc, and MultiMarkdown formats as the CLI.
 * String fields populated by this function are heap-allocated copies. The
 * caller may leave them allocated for the process lifetime or free each field
 * individually after the final conversion that uses these options.
 * @return true on success; false if options or path is NULL, path is empty,
 *         the file cannot be loaded, or ownership copies cannot be allocated.
 */
bool apex_options_apply_meta_file(apex_options *options, const char *path);

/**
 * Register cmark-gfm's core extensions once.
 * cmark_gfm_core_extensions_ensure_registered() is not thread-safe: a second
 * overlapping call aborts in cmark_register_node_flag. Marked converts on a
 * concurrent queue, so every cmark entry point must go through this.
 */
void apex_ensure_cmark_extensions(void);

/**
 * Main conversion function: Markdown to HTML
 *
 * @param markdown Input markdown text
 * @param len Length of input text
 * @param options Processing options (NULL for defaults)
 * @return Newly allocated HTML string (must be freed with apex_free_string)
 */
char *apex_markdown_to_html(const char *markdown, size_t len, const apex_options *options);

/**
 * Collect document headings as a flat array of TOC entries.
 * Honors id_format, min/max levels, and .no_toc the same as -t toc.
 * @param out_count Receives entry count (may be 0); required
 * @return Heap array of count entries, or NULL if out_count is NULL / allocation fails.
 *         Free with apex_toc_entries_free().
 */
apex_toc_entry *apex_generate_toc_entries(cmark_node *document, int id_format,
                                          int min_level, int max_level,
                                          size_t *out_count);

/**
 * Parse markdown and return structured TOC entries (same pipeline as -t toc).
 * Uses options->toc_min / toc_max / id_format (defaults apply when options is NULL).
 * Free the result with apex_toc_entries_free().
 */
apex_toc_entry *apex_markdown_to_toc_entries(const char *markdown, size_t len,
                                             const apex_options *options,
                                             size_t *out_count);

/**
 * Free an array returned by apex_generate_toc_entries / apex_markdown_to_toc_entries.
 */
void apex_toc_entries_free(apex_toc_entry *entries, size_t count);

/**
 * Resolve a local image path against base_directory (same rules as HTML embedding).
 * Returns a newly allocated path, or NULL on allocation failure.
 * Caller must free with free() or apex_free_string.
 */
char *apex_resolve_local_image_path(const char *filepath, const char *base_dir);

/**
 * Wrap HTML content in complete HTML5 document structure
 *
 * @param content HTML content to wrap
 * @param title Document title (NULL for default)
 * @param stylesheet_paths NULL-terminated array of CSS file paths to link in head
 * @param stylesheet_count Number of CSS files on the stylesheet_paths array.
 * @param code_highlighter Highlighter engine.
 * @param html_header Raw HTML to insert in <head> section (NULL for none)
 * @param html_footer Raw HTML to append before </body> (NULL for none)
 * @param language Language code for <html lang> attribute (NULL for "en")
 * @param strict_xhtml When true, emit polyglot XHTML document (XML declaration, xmlns, Content-Type meta; not combined with legacy HTML5 head)
 * @return Newly allocated HTML document string (must be freed with apex_free_string)
 */
char *apex_wrap_html_document(const char *content, const char *title, const char **stylesheet_paths, size_t stylesheet_count, const char *code_highlighter, const char *html_header, const char *html_footer, const char *language, bool strict_xhtml);

/**
 * Pretty-print HTML with proper indentation
 *
 * @param html HTML to format
 * @return Newly allocated formatted HTML string (must be freed with apex_free_string)
 */
char *apex_pretty_print_html(const char *html);

/**
 * Free a string allocated by Apex
 */
void apex_free_string(char *str);

/**
 * Get version information
 */
const char *apex_version_string(void);
int apex_version_major(void);
int apex_version_minor(void);
int apex_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif /* APEX_H */
