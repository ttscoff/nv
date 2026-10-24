# IAL (Inline Attribute Lists) Demo

This document demonstrates all supported IAL features in
Apex.

## Block-Level IALs (Next-Line)

Block-level IALs appear on a separate line after the element
they apply to.

### Headings with IALs

# Heading with ID
{: #custom-heading-id}

## Heading with Class
{: .highlight}

### Heading with Both
{: #heading-id .class1 .class2}

#### Multiple Classes
{: .primary .large .bold}

### Paragraphs with IALs

This paragraph has a class applied to it.
{: .tip}

This paragraph has an ID and multiple classes.
{: #paragraph-id .important .note}

### Lists with IALs

- First item
- Second item
- Third item

{: .unordered-list}

1. Numbered item one
2. Numbered item two
3. Numbered item three

{: #ordered-list .numbered}

### Blockquotes with IALs

> This is a blockquote with attributes.
{: .quote .inspirational}

## Inline Span-Level IALs

Inline IALs appear immediately after inline elements within
paragraphs.

### Links with Inline IALs

This is a [regular link](https://example.com) without an
IAL.

Here's a paragraph with a [styled
link](https://example.com){:.button} that has a button
class.

You can have [multiple
links](https://example.com){:.link-primary} with [different
classes](https://example.com){:.link-secondary} in the same
paragraph.

Links with the [same URL](https://example.com) should still
work correctly when [one has an
IAL](https://example.com){:.special-link}.

### Images with Inline IALs

![Regular image](image.png)

![Styled image](logo.png){:.logo .centered}

![Image with ID](icon.png){:#site-icon .icon}

### Emphasis with Inline IALs

This paragraph has **bold text**{:.bold-style} with a custom
class and *italic text*{:.italic-style} with another class.

You can combine **bold**{:.bold} and *italic*{:.italic} in
the same paragraph with different styles.

Nested **bold with *italic*{:.nested-italic}
inside**{:.bold-wrapper} works too.

### Code with Inline IALs

Use `inline code`{:.code-inline} for code snippets with
styling.

You can have multiple `code spans`{:.code-1} and `more
code`{:.code-2} with different classes.

### Mixed Inline Elements

This paragraph combines a
[link](https://example.com){:.link-class}, **bold
text**{:.bold-class}, *italic text*{:.italic-class}, and
`code`{:.code-class} all with different IALs.

## Complex Examples

### Multiple IALs in Sequence

[First link](url1){:.first} and [second
link](url2){:.second} and [third link](url3){:.third} all in
one paragraph.

**First bold**{:.b1}, **second bold**{:.b2}, and **third
bold**{:.b3} text elements.

### IALs with Text After

[Click here](https://example.com){:.button} to visit our
website and learn more.

**Important**: This text has an IAL{:.highlight} that isn't
associated with a span or block level element (bold,
italics, link, etc.) and is therefore ignored (left in the
text).

### IALs with Duplicate URLs

Here's a [regular link](https://example.com) to demonstrate
link styling.

[Click This Button](https://example.com){:.button}

Both links point to the same URL, but only the second one
has the button class.

### IALs with Multiple Attributes

[Link with multiple classes](url){:.primary .large .button}

**Bold with ID and classes**{:#bold-id .highlight
.important}

## Edge Cases

### IAL at End of Paragraph

This paragraph ends with a
[link](https://example.com){:.end-link}.

### IAL with Whitespace

[Link](url){: .spaced-class } works with spaces.

### IALs in Lists

- List item with [link](url){:.list-link}
- Another item with **bold**{:.list-bold} text
- Final item with `code`{:.list-code}

## Summary

IALs provide a powerful way to add HTML attributes to
markdown elements:

- Block-level IALs use next-line syntax: `{: .class #id}`

Inline IALs appear immediately after elements: `[link](url){:.class}`

- Support for IDs, classes, and custom attributes
- Works with headings, paragraphs, lists, blockquotes,

  links, images, emphasis, and code

