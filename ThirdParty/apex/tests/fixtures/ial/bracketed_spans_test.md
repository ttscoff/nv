# Bracketed Spans Test

This test verifies that bracketed spans `[text]{IAL}` work
correctly and that reference links take precedence.

## Reference Links (Should NOT be spans)

This is a [link] that works as a reference.

[link]: https://example.com

This is [another reference][ref2] with a different ID.

[ref2]: https://example.org "Example Site"

This is a [shortcut reference][] that uses the text as the
ID.

[shortcut reference]: https://example.net

## Bracketed Spans (Should be spans)

This is [some *text*]{.class key="val"} with a span.

This is [another span]{#id .class1 .class2 title="Title"}
with multiple attributes.

This is [a span with **bold** text]{.highlight} that should
process markdown.

This is [a span with `code`]{.code-style} inside.

This is [a span with [nested brackets]]{.nested} that should
work.

## Nested Brackets

This is [Text with [nested brackets]]{.nested} that should
preserve the nested brackets.

This is [Text with [multiple [levels] of
nesting]]{.multi-level} that should work.

This is [Text with [inner] and [more inner]
brackets]{.multiple-nested} that should work.

This is [Simple text]{.simple} without nesting.

## Edge Cases

This is [text]{.class} followed by regular text.

This is [text with *markdown*]{.class} that should be
processed.

This is [text]{#id} with just an ID.

This is [text]{.class1 .class2} with multiple classes.

This is [text]{key="value" key2='value2'} with multiple
attributes.

## Mixed Content

This paragraph has both a [reference link][ref3] and a
[bracketed span]{.span-class}.

[ref3]: https://example.com

This paragraph has [a reference][ref4] and [another
reference][ref4] using the same definition.

[ref4]: https://example.com

