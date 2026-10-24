/**
 * Superscript and Subscript Extension
 * Converts ^text^ to <sup>text</sup> and ~text~ to <sub>text</sub>
 * MultiMarkdown-style syntax
 */

#include "sup_sub.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/** True if content at p looks like a list marker (- , * , + , or digit+. ) */
static bool looks_like_list_marker(const char *p) {
    if (*p == '-' || *p == '*' || *p == '+')
        return (p[1] == ' ' || p[1] == '\t');
    if (isdigit((unsigned char)*p)) {
        while (isdigit((unsigned char)*p)) p++;
        return (*p == '.' && (p[1] == ' ' || p[1] == '\t'));
    }
    return false;
}

/** True if we're at the start of a line that is an indented code block (4+ spaces or tab)
 * and not a list line. List lines (nested or continuation) should still get sup/sub. */
static bool line_is_indented_code_block(const char *read) {
    if (*read == '\t') {
        return !looks_like_list_marker(read + 1);
    }
    if (read[0] != ' ' || read[1] != ' ' || read[2] != ' ' || read[3] != ' ')
        return false;
    const char *content = read + 4;
    while (*content == ' ')
        content++;
    return !looks_like_list_marker(content);
}

/**
 * Process superscript and subscript syntax as preprocessing
 * Converts to <sup>text</sup> and <sub>text</sub> before parsing
 */
char *apex_process_sup_sub(const char *text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    /* Allocate enough space: original text + potential tag expansions
     * Each ^ or ~ can expand to ~20 chars (<sup>content</sup> or <sub>content</sub>)
     * Use len * 5 to be safe, with a minimum of 64 bytes */
    size_t capacity = (len * 5 > 64) ? len * 5 : 64;
    char *output = malloc(capacity);
    if (!output) return NULL;

    const char *read = text;
    char *write = output;
    size_t remaining = capacity;

    bool in_code_block = false;
    bool in_inline_code = false;
    bool in_indented_code_block = false;
    bool in_math_inline = false;
    bool in_math_display = false;
    bool in_liquid = false;

    while (*read) {
        /* At line start: indented code block only if 4+ spaces/tab and not a list line */
        if (read == text || read[-1] == '\n') {
            in_indented_code_block = line_is_indented_code_block(read);
        }

        /* Track Liquid tags (skip processing inside them) */
        if (!in_liquid && *read == '{' && read[1] == '%') {
            in_liquid = true;
            if (remaining > 1) {
                *write++ = *read++;
                *write++ = *read++;
                remaining -= 2;
            } else {
                read += 2;
            }
            continue;
        }
        if (in_liquid) {
            if (*read == '%' && read[1] == '}') {
                if (remaining > 1) {
                    *write++ = *read++;
                    *write++ = *read++;
                    remaining -= 2;
                } else {
                    read += 2;
                }
                in_liquid = false;
            } else if (remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else {
                read++;
            }
            continue;
        }
        /* Track fenced code blocks (skip processing inside them) */
        if (*read == '`') {
            if (read[1] == '`' && read[2] == '`') {
                in_code_block = !in_code_block;
            } else if (!in_code_block) {
                in_inline_code = !in_inline_code;
            }
        }

        /* Track math spans (skip processing inside them) */
        bool handled_math = false;
        if (!in_code_block && !in_inline_code && !in_indented_code_block) {
            /* Check for display math: $$...$$ */
            if (*read == '$' && read[1] == '$') {
                in_math_display = !in_math_display;
                if (remaining > 1) {
                    *write++ = *read++;
                    *write++ = *read++;
                    remaining -= 2;
                } else {
                    read += 2;
                }
                handled_math = true;
            }
            /* Check for inline math: $...$ */
            else if (*read == '$' && !in_math_display) {
                /* Check if next char is not $ (to avoid matching $$) */
                if (read[1] != '$') {
                    /* If we're already in inline math, this is the closing delimiter */
                    if (in_math_inline) {
                        in_math_inline = false;
                        /* Write the closing $ */
                        if (remaining > 0) {
                            *write++ = *read++;
                            remaining--;
                        } else {
                            read++;
                        }
                        handled_math = true;
                    }
                    /* Otherwise, check if it's a valid opening delimiter (not whitespace) */
                    else if (read[1] != '\0' && read[1] != ' ' && read[1] != '\t' && read[1] != '\n') {
                        in_math_inline = true;
                        /* Write the opening $ */
                        if (remaining > 0) {
                            *write++ = *read++;
                            remaining--;
                        } else {
                            read++;
                        }
                        handled_math = true;
                    }
                }
            }
        }

        /* Skip processing inside code or math */
        if (handled_math || in_code_block || in_inline_code || in_indented_code_block || in_math_inline || in_math_display) {
            if (!handled_math && remaining > 0) {
                *write++ = *read++;
                remaining--;
            } else if (!handled_math) {
                read++;
            }
            continue;
        }

        /* Check for superscript: ^word (only first word, stops at space, ^, or end) */
        /* Skip if it's part of a footnote reference pattern [^ */
        if (*read == '^' && read[1] != '\0' && read[1] != ' ' && read[1] != '\t' && read[1] != '\n' && read[1] != '^') {
            /* Skip if previous character is '[' (footnote reference) */
            if (read > text && read[-1] == '[') {
                /* Copy the ^ character and continue */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            const char *content_start = read + 1;
            const char *content_end = content_start;

            /* Find end of word (stops at space, punctuation, ^, newline, or end of string) */
            /* Don't include sentence terminators in the superscript */
            while (*content_end && *content_end != ' ' && *content_end != '\t' && *content_end != '\n' && *content_end != '^') {
                /* Stop at sentence terminators: . , ; : ! ? */
                if (*content_end == '.' || *content_end == ',' || *content_end == ';' ||
                    *content_end == ':' || *content_end == '!' || *content_end == '?') {
                    break;
                }
                content_end++;
            }

            size_t content_len = content_end - content_start;

            /* Only process if we have content */
            if (content_len > 0) {
                const char *open_tag = "<sup>";
                const char *close_tag = "</sup>";
                size_t open_tag_len = strlen(open_tag);
                size_t close_tag_len = strlen(close_tag);
                size_t total_needed = open_tag_len + content_len + close_tag_len;

                /* Only write if we have enough space for all parts */
                if (remaining >= total_needed) {
                    /* Write <sup> */
                    memcpy(write, open_tag, open_tag_len);
                    write += open_tag_len;
                    remaining -= open_tag_len;

                    /* Copy superscript content */
                    memcpy(write, content_start, content_len);
                    write += content_len;
                    remaining -= content_len;

                    /* Write </sup> - we know we have space because we checked total_needed */
                    memcpy(write, close_tag, close_tag_len);
                    write += close_tag_len;
                    remaining -= close_tag_len;

                    /* Skip past the content (and the marker if we stopped at it) */
                    read = content_end;
                    /* If we stopped at ^ or ~, skip past it so it's not reprocessed */
                    if (*read == '^' || *read == '~') {
                        read++;
                    }
                    continue;
                }
            }
        }

        /* Check for subscript: ~word (only first word, stops at space, ~, or end) */
        /* First, check for critic markup patterns that use ~ */
        if (*read == '~') {
            /* If this ~ is part of a double-tilde sequence (~~), leave it alone
             * so the strikethrough extension can handle it.
             * Also skip if the previous character was ~ (already part of ~~).
             */
            if ((read > text && read[-1] == '~') || read[1] == '~') {
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }

            /* Check for {~~ (opening critic substitution) - previous char is { and next is ~ */
            if (read > text && read[-1] == '{' && read[1] == '~') {
                /* Copy both ~ characters and continue */
                if (remaining > 1) {
                    *write++ = *read++;
                    *write++ = *read++;
                    remaining -= 2;
                } else {
                    read += 2;
                }
                continue;
            }
            /* Check for ~~} (closing critic substitution) - we're at second ~, previous is ~, next is } */
            else if (read > text && read[-1] == '~' && read[1] == '}') {
                /* Copy the ~ character and continue */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }
            /* Check for ~> (critic substitution separator) */
            else if (read[1] == '>') {
                /* Copy the ~ character and continue */
                if (remaining > 0) {
                    *write++ = *read++;
                    remaining--;
                } else {
                    read++;
                }
                continue;
            }
        }

        /* Check for tilde-based syntax: ~text~ (underline), ~word (subscript), or ~~text~~ (strikethrough, already skipped) */
        if (*read == '~' && read[1] != '\0' && read[1] != ' ' && read[1] != '\t' && read[1] != '\n' && read[1] != '~') {
            const char *content_start = read + 1;
            const char *content_end = content_start;
            const char *closing_tilde = NULL;
            bool is_underline = false;

            /* First, check if the next character is a sentence terminator - if so, it's definitely subscript */
            bool is_likely_subscript = false;
            const char *check = content_start;
            while (*check && *check != ' ' && *check != '\t' && *check != '\n' && *check != '~') {
                if (*check == '.' || *check == ',' || *check == ';' || *check == ':' || *check == '!' || *check == '?') {
                    is_likely_subscript = true;
                    break;
                }
                check++;
            }

            if (!is_likely_subscript) {
                /* No sentence terminator found, check for underline pattern: scan forward to find a closing ~ */
                /* Underline requires tildes at word boundaries, subscript requires tildes within a word */
                const char *scan = content_start;
                while (*scan && *scan != '\n') {
                    if (*scan == '~') {
                        /* Found a potential closing ~ */
                        /* First check: if this is part of a double-tilde (~~), skip it - strikethrough handles this */
                        if (scan[1] == '~') {
                            /* This is part of ~~, skip both tildes and continue looking */
                            scan += 2;
                            continue;
                        }
                        /* Check if there's a space before it - if so, it's not a closing ~ for underline */
                        if (scan > content_start) {
                            unsigned char prev_char = (unsigned char)scan[-1];
                            if (isspace(prev_char)) {
                                /* Space before ~, so this is not a closing ~ for underline - continue looking */
                                scan++;
                                continue;
                            }
                        }
                        /* Check if tildes are within a word (subscript) or at word boundaries (underline) */
                        /* For subscript: char before opening ~ must be alphanumeric, and content between must be alphanumeric */
                        /* For underline: char before opening ~ must be non-alphanumeric (word boundary) */
                        bool char_before_is_word = (read > text) && isalnum((unsigned char)read[-1]);
                        bool char_after_is_word = scan[1] != '\0' && isalnum((unsigned char)scan[1]);
                        bool char_after_is_space_or_end = scan[1] == '\0' || isspace((unsigned char)scan[1]) || ispunct((unsigned char)scan[1]);

                        /* Check if content between tildes is alphanumeric */
                        bool content_is_word = true;
                        const char *check_content = content_start;
                        while (check_content < scan) {
                            if (!isalnum((unsigned char)*check_content)) {
                                content_is_word = false;
                                break;
                            }
                            check_content++;
                        }

                        if (char_before_is_word && content_is_word && (char_after_is_word || char_after_is_space_or_end)) {
                            /* Opening ~ is after alnum, content is alnum, closing ~ is before alnum or end/punct - this is subscript within a word */
                            /* Store the closing tilde for subscript, but don't set is_underline */
                            closing_tilde = scan;
                            break;
                        }

                        /* No space before ~, not part of ~~, and at word boundary - this is underline */
                        closing_tilde = scan;
                        is_underline = true;
                        break;
                    }
                    scan++;
                }
            }

            if (is_underline && closing_tilde) {
                /* Underline: content is between start and closing ~ */
                content_end = closing_tilde;
            } else if (closing_tilde && !is_underline) {
                /* Subscript with closing ~ within a word: content is between start and closing ~ */
                content_end = closing_tilde;
            } else {
                /* Subscript: find end of word (stops at space, punctuation, newline, or ~) */
                /* Don't include sentence terminators in the subscript */
                while (*content_end && *content_end != ' ' && *content_end != '\t' && *content_end != '\n' && *content_end != '~') {
                    /* Stop at sentence terminators: . , ; : ! ? */
                    if (*content_end == '.' || *content_end == ',' || *content_end == ';' ||
                        *content_end == ':' || *content_end == '!' || *content_end == '?') {
                        break;
                    }
                    content_end++;
                }
            }

            size_t content_len = content_end - content_start;

            /* Only process if we have content */
            if (content_len > 0) {

                const char *open_tag = is_underline ? "<u>" : "<sub>";
                const char *close_tag = is_underline ? "</u>" : "</sub>";
                size_t open_tag_len = strlen(open_tag);
                size_t close_tag_len = strlen(close_tag);
                size_t total_needed = open_tag_len + content_len + close_tag_len;

                /* Only write if we have enough space for all parts */
                if (remaining >= total_needed) {
                    /* Write opening tag */
                    memcpy(write, open_tag, open_tag_len);
                    write += open_tag_len;
                    remaining -= open_tag_len;

                    /* Copy content */
                    memcpy(write, content_start, content_len);
                    write += content_len;
                    remaining -= content_len;

                    /* Write closing tag */
                    memcpy(write, close_tag, close_tag_len);
                    write += close_tag_len;
                    remaining -= close_tag_len;

                    /* Skip past the content and closing marker */
                    if (is_underline && closing_tilde) {
                        /* For underline, skip past the closing ~ */
                        read = closing_tilde + 1;
                    } else {
                        /* For subscript, skip past the content */
                        read = content_end;
                        /* If we stopped at ~, skip past it so it's not reprocessed */
                        if (*read == '~') {
                            read++;
                        }
                    }
                    continue;
                }
            }
        }

        /* Copy character */
        if (remaining > 0) {
            *write++ = *read++;
            remaining--;
        } else {
            read++;
        }
    }

    *write = '\0';
    return output;
}
