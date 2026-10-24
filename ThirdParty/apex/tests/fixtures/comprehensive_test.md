---
title: Apex Markdown Processor - Comprehensive Feature Test
author: Test Suite
date: 2025-12-05
version: 1.0
keywords: markdown, processor, test, benchmark
description: A comprehensive test document exercising all Apex features
---

# Apex Markdown Processor: Complete Feature Demonstration

This document exercises **every feature** of the Apex Markdown processor, providing both a comprehensive test and a real-world performance benchmark.

{{TOC:2-4}}

## Introduction and Metadata

The document begins with YAML metadata (extracted above). We can reference metadata variables like this:

- Title: [%title]
- Author: [%author]
- Version: [%version]

This demonstrates **metadata extraction** and **variable replacement**.

## Basic Markdown Syntax

### Text Formatting

This paragraph demonstrates *italic text*, **bold text**, ***bold italic***, ~~strikethrough text~~, and `inline code`. We can also use ==highlighted text== and {++inserted text++}.

### Typography Enhancements

Smart typography converts:
- Three dashes --- into em-dash
- Two dashes -- into en-dash
- Three dots... into ellipsis
- Quotes "like this" into smart quotes
- Guillemets << and >> for French quotes

### Lists and Tasks

Unordered list:

- First item
- Second item
  - Nested item
  - Another nested item
- Third item

Ordered list:

1. First step
2. Second step
   1. Sub-step A
   2. Sub-step B
3. Third step

Task list:

- [x] Completed task
- [ ] Pending task
- [x] Another completed task
- [ ] Future task

## Links and References

### Standard Links

Here's a [standard link](https://example.com "Example Site") and an autolink: https://github.com/apex/markdown

### Wiki Links

Navigate to [[Home]] or [[Documentation|Docs]] or [[API#Methods]].

### Footnotes

Here's a simple footnote[^1] and another one[^2]. We also support inline footnotes^[This is an inline footnote] and MMD inline footnotes[^This is an MMD style inline footnote with spaces].

[^1]: This is the first footnote with **formatted** content.

[^2]: This footnote has multiple paragraphs.

    It can contain code blocks:

    ```python
    def example():
        return "footnote code"
    ```

    And other block-level content!

## Code and Syntax

### Inline Code

Use `const variable = "value";` for inline code.

### Fenced Code Blocks

```javascript
// JavaScript example with syntax highlighting
function processMarkdown(input) {
    const parser = new ApexParser();
    return parser.convert(input);
}

class MarkdownProcessor {
    constructor(options) {
        this.options = options;
    }

    process(text) {
        return this.transform(text);
    }
}
```

```python
# Python example
def fibonacci(n):
    """Calculate fibonacci number recursively"""
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

# List comprehension
squares = [x**2 for x in range(10)]
print(f"Squares: {squares}")
```

```bash
#!/bin/bash
# Shell script example
echo "Testing Apex processor"
for file in *.md; do
    ./apex "$file" > "${file%.md}.html"
done
```

## Mathematics

### Inline Math

The quadratic formula is $x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}$ and Einstein's famous equation is $E = mc^2$.

### Display Math

$$
\int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi}
$$

$$
\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6}
$$

## Tables

### Basic Table

| Feature        | CommonMark | GFM | MMD | Kramdown | Apex |
| -------------- | ---------- | --- | --- | -------- | ---- |
| Basic Markdown | ✅         | ✅  | ✅  | ✅       | ✅   |
| Tables         | ❌         | ✅  | ✅  | ✅       | ✅   |
| Footnotes      | ❌         | ❌  | ✅  | ✅       | ✅   |
| Math           | ❌         | ❌  | ✅  | ✅       | ✅   |
| Metadata       | ❌         | ❌  | ✅  | ✅       | ✅   |
| Attributes     | ❌         | ❌  | ❌  | ✅       | ✅   |

### Advanced Table with Spans

#### Column Span (Empty Cells)

Empty cells merge with the previous cell (colspan):

| Header 1       | Header 2    | Header 3 | Header 4 |
| -------------- | ----------- | -------- | -------- |
| Regular        | Regular     | Regular  | Regular  |
| Span 3 columns ||| Regular  |
| Regular        | Span 2 cols || Regular  |

#### Row Span (^^ Marker)

Use `^^` to merge cells vertically (rowspan):

| Name    | Department  | Project  | Status |
| ------- | ----------- | -------- | ------ |
| Alice   | Engineering | Alpha    | Active |
| ^^      | ^^          | Beta     | ^^     |
| ^^      | ^^          | Gamma    | ^^     |
| Bob     | Marketing   | Campaign | Active |
| Charlie | Sales       | Q4       | Active |

#### Combined Spans Example

[Employee Performance Q4 2025]

| Department  | Employee | Q1-Q2 Average | Q3  | Q4    | Overall |
| ----------: | -------- | ------------- | --- | ----- | ------- |
| Engineering | Alice    | 93.5          | 94  | 96    | 94.25   |
| ^^          | Bob      | 89.0          | 87  | 91    | 89.00   |
| Marketing   | Charlie  | Absent        |     |       | 92.00   |
| Sales       | Diana    | 87.5          | 88  | 90    | 88.50   |
| ^^          | Eve      | 93.0          | 95  | 93    | 93.50   |
{: .performance-table #q4-results}

## Blockquotes and Callouts

### Standard Blockquote

> This is a standard blockquote.
> It can span multiple lines.
>
> And contain multiple paragraphs.

### Callouts

> [!NOTE]
> This is an informational note callout. It provides helpful context.

> [!WARNING]
> This is a warning callout. Pay attention to this important information!

> [!TIP]
> This is a tip callout with useful advice for optimal results.

> [!IMPORTANT]
> This is an important callout that requires your attention.

> [!CAUTION]
> This is a caution callout indicating potential issues.

> [!NOTE]+ Collapsible Callout
> This callout starts expanded but can be collapsed.
> It contains multiple lines of content.

### Python-Markdown Callouts (requires `--py-callouts`)

!!! note "Python Note"
    This callout uses Python-Markdown syntax.
    The body is indented.

!!! warning
    This warning has no explicit title.

### Quarto Callouts (requires `--quarto-callouts`)

::: {.callout-tip}
## Quarto Tip

This callout uses Quarto fenced syntax and heading title extraction.
:::

::: {.callout-warning title="Quarto Warning" collapse="true"}
This callout uses explicit title and collapse attributes.
:::

## Definition Lists

Markdown
: A lightweight markup language for creating formatted text.
: Created by John Gruber in 2004.

Apex
: A unified Markdown processor supporting multiple flavors.
: Built on cmark-gfm with extensive custom extensions.

CommonMark
: A standard specification for Markdown syntax.

  It can contain block-level content like paragraphs.

  ```
  code blocks
  ```

  And more!

**With blank lines:** A term can have blank lines before the first definition and between definitions:

Glossary
: First meaning of the term.

: Second meaning, after a blank line.

**One-line format:**

key::value

abbrev :: expanded form

## Critic Markup

Here's some text with {++additions++} and {--deletions--}.

You can also show {~~replacements~>substitutions~~} in your text.

{==Highlighted text==} draws attention to important content.

{>>This is a comment that explains the changes<<}

## Abbreviations

This document uses HTML, CSS, and JS extensively. The MMD processor is faster than the GFM processor, but the Apex processor is the fastest.

*[HTML]: HyperText Markup Language
*[CSS]: Cascading Style Sheets
*[JS]: JavaScript
*[MMD]: MultiMarkdown
*[GFM]: GitHub Flavored Markdown

You can also use [>API] inline or [>(URL) Uniform Resource Locator] abbreviations.

## Emoji Support

Apex supports GitHub emoji :rocket: :sparkles: :zap: :fire: :tada:

Common emoji: :smile: :heart: :thumbsup: :star: :warning:

Tech emoji: :computer: :keyboard: :bulb: :memo: :book:

## Attributes and Styling {: #attributes-section}

### Block Attributes

This is a paragraph with an ID and classes.

{: #special-para .important .highlighted}

#### Heading with Attributes {: .section-header #methods}

### Span Attributes

This is text with styled content in the middle.

Note: Inline IAL syntax like [text]{: .class} is not yet implemented.

## Special Markers

### Page Breaks

Content before page break.

<!--BREAK-->

Content after page break (useful for printing/PDF).

{::pagebreak /}

Another page break using Kramdown syntax.

### Autoscroll Pauses

This is content that will pause for 5 seconds.

<!--PAUSE:5-->

This content appears after the pause (useful for presentations).

## End of Block Markers

This list item ends here
^
And this starts a new paragraph despite no blank line.

## HTML with Markdown

<div class="wrapper" markdown="1">

## This is Markdown inside HTML

You can use **all** markdown features inside HTML blocks if you set `markdown="1"`.

- Lists work
- So do other features

</div>

<span markdown="span">This is *inline* markdown in HTML</span>

## File Inclusion

### Markdown Include

<<[test_basic.md]

### Code Include

### Raw HTML Include

<!--{include.html}-->

## Horizontal Rules

---

***

___

## Extended Features Stress Test

### Nested Structures

1. Outer list item with **bold**

   - Nested item with *italic*

     - Deeply nested with `code`

       > And a blockquote
       >
       > With multiple lines

2. Complex list item

   ```javascript
   // Code in list
   function nested() {
       return "complex";
   }
   ```

   - Back to nested lists
   - With [links](https://example.com)

### Mixed Content

Here's a paragraph with *italic*, **bold**, `code`, [link](url), ![image](img.jpg), footnote[^3], math $x^2$, emoji :smile:, ~~strike~~, ==highlight==, {++insert++}, wiki [[link]], abbreviation using HTML, and [styled text]{: .highlight}.

[^3]: Complex footnote with **formatting**, `code`, [links](url), and more!

## Inline Attribute Lists

### Image with Attributes

![Apex Logo](logo.png){: .center width="300" #logo}

### Link with Attributes

[Visit Docs](https://docs.example.com){: .external target="_blank" rel="noopener"}

## Attribute List Definitions

{:toc: .table-of-contents #toc}
{:note: .callout .callout-note}
{:warn: .callout .callout-warning}

Apply attributes by reference: {:toc}

## Large Table for Performance Testing

| ID  | Name            | Department  | Email              | Phone    | Location    | Status   | Join Date  |
| --- | --------------- | ----------- | ------------------ | -------- | ----------- | -------- | ---------- |
| 001 | Alice Johnson   | Engineering | alice@example.com  | 555-0101 | New York    | Active   | 2020-01-15 |
| 002 | Bob Smith       | Marketing   | bob@example.com    | 555-0102 | Chicago     | Active   | 2020-03-22 |
| 003 | Carol Williams  | Sales       | carol@example.com  | 555-0103 | Los Angeles | Active   | 2020-05-10 |
| 004 | David Brown     | Engineering | david@example.com  | 555-0104 | New York    | Active   | 2020-07-01 |
| 005 | Eve Davis       | HR          | eve@example.com    | 555-0105 | Boston      | Active   | 2020-09-15 |
| 006 | Frank Miller    | Engineering | frank@example.com  | 555-0106 | Seattle     | Active   | 2021-01-20 |
| 007 | Grace Wilson    | Marketing   | grace@example.com  | 555-0107 | Austin      | Active   | 2021-03-08 |
| 008 | Henry Moore     | Sales       | henry@example.com  | 555-0108 | Denver      | Active   | 2021-05-12 |
| 009 | Iris Taylor     | Engineering | iris@example.com   | 555-0109 | Portland    | Active   | 2021-07-25 |
| 010 | Jack Anderson   | Finance     | jack@example.com   | 555-0110 | Miami       | Active   | 2021-09-30 |
| 011 | Karen Thomas    | HR          | karen@example.com  | 555-0111 | Dallas      | Active   | 2022-01-15 |
| 012 | Larry Jackson   | Engineering | larry@example.com  | 555-0112 | San Jose    | Active   | 2022-03-20 |
| 013 | Mary White      | Marketing   | mary@example.com   | 555-0113 | Phoenix     | Active   | 2022-05-08 |
| 014 | Nathan Harris   | Sales       | nathan@example.com | 555-0114 | Atlanta     | On Leave | 2022-07-14 |
| 015 | Olivia Martin   | Engineering | olivia@example.com | 555-0115 | Seattle     | Active   | 2022-09-01 |
| 016 | Paul Thompson   | Finance     | paul@example.com   | 555-0116 | Houston     | Active   | 2023-01-10 |
| 017 | Quinn Garcia    | HR          | quinn@example.com  | 555-0117 | Tampa       | Active   | 2023-03-15 |
| 018 | Rachel Martinez | Engineering | rachel@example.com | 555-0118 | Orlando     | Active   | 2023-05-20 |
| 019 | Sam Robinson    | Marketing   | sam@example.com    | 555-0119 | Vegas       | Active   | 2023-07-12 |
| 020 | Tina Clark      | Sales       | tina@example.com   | 555-0120 | Nashville   | Active   | 2023-09-25 |

## Repeated Sections for Volume

### Section A: Engineering Documentation

The engineering team uses various tools and technologies:

```python
class Engineer:
    def __init__(self, name, specialty):
        self.name = name
        self.specialty = specialty

    def code(self, language):
        return f"{self.name} is coding in {language}"

    def review(self, pull_request):
        """Review code changes"""
        if self.validate(pull_request):
            return "LGTM"
        return "Needs changes"
```

Key technologies:

- **Frontend**: React, Vue, Angular
- **Backend**: Node.js, Python, Go
- **Database**: PostgreSQL, MongoDB, Redis
- **DevOps**: Docker, Kubernetes, CI/CD

### Section B: Marketing Campaigns

Marketing metrics for Q4:

| Campaign     | Impressions | Clicks | CTR  | Conversions | ROI  |
| ------------ | ----------- | ------ | ---- | ----------- | ---- |
| Social Media | 1,250,000   | 45,000 | 3.6% | 2,100       | 245% |
| Email        | 500,000     | 35,000 | 7.0% | 1,800       | 320% |
| Search Ads   | 850,000     | 28,000 | 3.3% | 1,400       | 180% |
| Display      | 2,100,000   | 52,000 | 2.5% | 1,900       | 150% |
| Content      | 650,000     | 41,000 | 6.3% | 2,300       | 410% |

> [!TIP]
> Content marketing shows the highest ROI. Consider increasing budget allocation.

### Section C: Sales Performance

Sales data visualization and analysis:

```javascript
const salesData = {
    regions: ['North', 'South', 'East', 'West'],
    q1: [125000, 98000, 145000, 112000],
    q2: [142000, 105000, 158000, 128000],
    q3: [138000, 118000, 162000, 135000],
    q4: [165000, 132000, 178000, 149000]
};

function calculateGrowth(data) {
    return data.map((val, idx) => {
        if (idx === 0) return 0;
        return ((val - data[idx-1]) / data[idx-1] * 100).toFixed(2);
    });
}
```

### Section D: Technical Specifications

API
: Application Programming Interface for system integration.

REST
: Representational State Transfer architecture.

GraphQL
: Query language for APIs providing flexible data fetching.

WebSocket
: Protocol for real-time bidirectional communication.

OAuth
: Open standard for access delegation and authorization.

*[API]: Application Programming Interface
*[REST]: Representational State Transfer
*[GraphQL]: Graph Query Language

### Section E: Complex Mathematics

The Fourier transform is defined as:

$$
\hat{f}(\xi) = \int_{-\infty}^{\infty} f(x) e^{-2\pi i x \xi} dx
$$

Taylor series expansion:

$$
f(x) = \sum_{n=0}^{\infty} \frac{f^{(n)}(a)}{n!}(x-a)^n
$$

Matrix operations: $\mathbf{A}\mathbf{x} = \mathbf{b}$ where $\mathbf{A} \in \mathbb{R}^{m \times n}$.

## Performance Testing Sections

### Repeated Content Block 1

This section contains {++new content++} with {--removed parts--} and {~~changes~>improvements~~}.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua[^perf1]. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.

- Item with **bold** and *italic*
- Item with `code` and [link](url)
- Item with :emoji: and ~~strike~~

[^perf1]: Performance test footnote with extensive content including lists, code, and more.

### Repeated Content Block 2

Mathematical formulas: $E = mc^2$, $a^2 + b^2 = c^2$, $\sum_{i=1}^{n} i = \frac{n(n+1)}{2}$

```bash
#!/bin/bash
for i in {1..100}; do
    echo "Processing item $i"
    ./process.sh "$i"
done
```

> [!WARNING]
> This is a performance testing warning that contains multiple lines and demonstrates
> callout rendering performance with extended content.

### Repeated Content Block 3

| Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 | Col8 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| A1   | B1   | C1   | D1   | E1   | F1   | G1   | H1   |
| A2   | B2   | C2   | D2   | E2   | F2   | G2   | H2   |
| A3   | B3   | C3   | D3   | E3   | F3   | G3   | H3   |
| A4   | B4   | C4   | D4   | E4   | F4   | G4   | H4   |
| A5   | B5   | C5   | D5   | E5   | F5   | G5   | H5   |

## Conclusion

This comprehensive document has tested:

1. ✅ Basic Markdown (headings, paragraphs, lists)
2. ✅ Extended syntax (tables, footnotes, task lists)
3. ✅ Metadata and variables
4. ✅ Wiki links and autolinks
5. ✅ Mathematics (inline and display)
6. ✅ Critic Markup (all types)
7. ✅ Callouts (multiple types)
8. ✅ Definition lists with block content
9. ✅ Abbreviations (multiple syntaxes)
10. ✅ Emoji support
11. ✅ Attributes (IAL and ALD)
12. ✅ Special markers
13. ✅ Smart typography
14. ✅ Advanced tables (spans and captions)
15. ✅ Code blocks with syntax highlighting
16. ✅ HTML with markdown attributes
17. ✅ File includes
18. ✅ And much more!

---

**Document Statistics**:
- Sections: 20+
- Tables: 10+
- Code blocks: 15+
- Footnotes: 5+
- Mathematical formulas: 10+
- Special features: All of them!

*Generated: 2025-12-05 | Version: 1.0 | Format: Markdown*

