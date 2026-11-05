---
title: More Markdown editing features for nvALT, again
layout: post
tags:
- markdown
- linking
- nvalt
categories:
- Blog
- Code
post_class: code
comments: true
project_tag: nvalt
date: 2026-11-05 08:12
keywords: [shortcuts, lists, editing, editor]
---
Another pass at the editor. The [nvALT project page](https://brettterpstra.com/projects/nvalt/) has the full writeup. This is the short version of the four things I recently released.

### Markers

Editing settings now has a choice for bold, italic, and unordered lists. Bold is `**` or `__`. Italic is `*` or `_`. A new bullet is `-`, `*`, or `+`. The defaults are still asterisks, which is what Command-B, Command-I, and Option-Command-L have been inserting.

```markdown
**asterisks** or __underscores__
*asterisk* or _underscore_
```

Pick the ones you want. The shortcuts use that choice from then on. Existing notes are left alone.

### Lists, more than one line

Option-Command-L on one line is unchanged. Plain text becomes a bullet, a bullet becomes a number, a number goes back to plain text.

Select more than one line and the same cycle applies to the whole selection. Indents stay where they are. Blank lines stay blank.

```markdown
milk
eggs
    sourdough
```

The first press uses whatever bullet you chose:

```markdown
- milk
- eggs
    - sourdough
```

If every item is already a bullet, the next press numbers them. Nested lines count on their own.

```markdown
1. milk
2. eggs
    1. sourdough
```

One more press removes the markers and leaves the indent behind. If the selection is mixed, some lines marked and some not, that press clears the markers instead of numbering. The selection stays put, so you can walk the cycle again.

### Backticks

Auto-pair now knows about backticks. The first one inserts a pair and leaves the cursor between them. The next one types over the closer, which is the inline-code case:

```markdown
`code`
```

A third backtick does not wrap another pair. It stays a single character, so three in a row is a fence, and more backticks lengthen that run. A backtick typed right against a letter is left alone, and a selected word gets wrapped.

### Reference links

Typing the id in `[text][id]`, or in an image link `![alt][id]`, offers the definition ids already in the note. This note already has `spec` and `diagram` defined:

```markdown
See [the spec][spec] and ![diagram][diagram].

[spec]: https://example.com/spec
[diagram]: https://example.com/diagram.png
```

Put the cursor after `[more][` and `spec` is in the list. Keep typing and the list narrows. Return, Tab, or a click accepts one. If the closing bracket is already there, it stays and the cursor moves past it.

A `[` at the start of a line is a new definition. That list is the other direction: ids the note already uses that do not have a definition yet.

```markdown
[CommonMark][]
```

Start the next line with `[` and `CommonMark` is offered. `[[wiki links]]` still complete note titles, and none of this runs inside a fenced code block.

### Addendum: theme colors

Two color bugs that showed up once a theme was on.

Autocomplete was drawing the suggestion in black. The letters you had already typed went black too, and so did the brackets around a link. That text now stays the theme's foreground color for as long as the completion is open. Accept it or dismiss it and the normal syntax colors come back.

The word count in the search field was a light cyan token on dark themes, Svelte in particular, and it was hard to read. That color was the system token tint, not anything in the theme file. The count is plain text now, in a color that contrasts with the white search field.

### Addendum: note links in the preview

`[[wiki links]]` already open a note from the editor. In the preview they were just text. They are links now.

```markdown
See [[Meeting notes]] for the rest.
```

The brackets stay visible. Clicking one opens the note with that title, including a title with spaces. The same text inside a code span, a fenced block, math, or a diagram stays text.
