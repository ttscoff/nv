/**
 * @file syntax_highlight.h
 * @brief External syntax highlighting support for code blocks
 *
 * Provides integration with external syntax highlighting tools like
 * Pygments and Skylighting to produce colorized HTML output for
 * fenced code blocks.
 */

#ifndef APEX_SYNTAX_HIGHLIGHT_H
#define APEX_SYNTAX_HIGHLIGHT_H

#include <stdbool.h>

/**
 * Apply syntax highlighting to code blocks using an external tool.
 *
 * Scans the HTML for <pre><code class="language-XXX">...</code></pre> blocks,
 * extracts the code content, runs it through the specified external tool,
 * and replaces the original block with the highlighted HTML output.
 *
 * Supported tools:
 * - "pygments": Uses pygmentize command (Python)
 * - "skylighting": Uses skylighting command (Haskell)
 * - "shiki": Uses shiki CLI (@shikijs/cli); uses --format html or ansi based on ansi_output
 *
 * @param html The HTML output containing code blocks to highlight
 * @param tool The highlighting tool name ("pygments", "skylighting", or "shiki")
 * @param line_numbers Whether to include line numbers in output
 * @param language_only When true, only highlight blocks that have a language specified
 * @param ansi_output When true, request ANSI output (e.g. for terminal); only affects Shiki (--format ansi vs html)
 * @param theme Optional theme/style name to pass to the external tool (e.g. Pygments style, Shiki theme)
 * @return Newly allocated HTML with highlighted code blocks, or NULL on error.
 *         If the tool is not found or fails, returns a copy of the original HTML
 *         with a warning printed to stderr. For Shiki, non-zero exit (e.g. missing --lang) yields plain text.
 */
char *apex_apply_syntax_highlighting(const char *html, const char *tool, bool line_numbers, bool language_only, bool ansi_output, const char *theme);

/**
 * Check if a syntax highlighting tool is available in PATH.
 *
 * @param tool The tool name ("pygments", "skylighting", or "shiki")
 * @return true if the tool's binary is found and executable, false otherwise
 */
bool apex_syntax_highlighter_available(const char *tool);

#endif /* APEX_SYNTAX_HIGHLIGHT_H */
