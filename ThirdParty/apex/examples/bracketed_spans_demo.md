# Bracketed Spans Demo

Bracketed spans are a Pandoc-inspired feature that allows you to create HTML `<span>` elements with attributes using the syntax `[text]{IAL}`. The text inside the brackets is processed as markdown, and the IAL (Inline Attribute List) provides HTML attributes.

## Basic Usage

This is [some *text*]{.highlight} with a class.

This is [another example]{#custom-id .important} with an ID
and class.

This is [text with **bold**]{.bold-style} that processes
markdown inside.

## Attributes

You can use all standard IAL attribute types:

### IDs

This is [text]{#my-id} with just an ID.

### Classes

This is [text]{.class1 .class2 .class3} with multiple
classes.

### Key-Value Attributes

This is [text]{key="value" title="Tooltip"} with custom
attributes.

This is [text]{data-id="123" aria-label="Description"} with
data and ARIA attributes.

### Combined Attributes

This is [text]{#my-id .highlight .important key="value"}
combining all attribute types.

## Markdown Processing

The text inside brackets is fully processed as markdown:

- [**Bold text**]{.bold}
- [*Italic text*]{.italic}
- [`Code spans`]{.code}
- [Text with [nested brackets]]{.nested}

## Nested Brackets

Bracketed spans support nested brackets. The parser
correctly matches the outer brackets:

This is [Text with [nested brackets]]{.nested} that
preserves the inner brackets.

This is [Text with [multiple [levels] of
nesting]]{.multi-level} that handles deep nesting.

This is [Text with [inner] and [more inner]
brackets]{.multiple-nested} that handles multiple nested
pairs.

## Reference Links Take Precedence

If the bracketed text matches a reference link definition,
it will be treated as a link instead of a span:

This is [a link][example] that should be a link, not a span.

[example]: https://example.com

This is [another link][example2] with a title.

[example2]: https://example.org "Example Site"

## Use Cases

### Styling Inline Text

You can [highlight important text]{.highlight .important}
inline.

You can [mark text as new]{.new} or
[deprecated]{.deprecated}.

### Adding Metadata

You can [add data attributes]{data-type="note" data-id="42"}
for JavaScript.

You can [add ARIA labels]{aria-label="Important notice"} for
accessibility.

### Semantic Markup

You can [mark text as a term]{.term} or
[citation]{.citation}.

## Examples in Context

Here's a paragraph with [highlighted text]{.highlight},
[important text]{.important}, and [a term]{.term}.

Here's a list with styled items:

- [First item]{.first}
- [Second item]{.second}
- [Third item]{.third}

## Notes

- Bracketed spans are enabled by default in unified mode
- Use `--spans` to enable in other modes
- Use `--no-spans` to disable in unified mode
- Reference links always take precedence over spans
- Markdown inside spans is fully processed

