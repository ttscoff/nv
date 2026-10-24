---
title: Large Test Document for Performance and Feature Testing
author: Apex Test Suite
date: 2025-01-15
version: 2.0
description: A comprehensive large-scale markdown document for testing parser performance and edge cases
keywords: test, benchmark, performance, markdown, apex
---

# Large Test Document

This is a **large-scale** markdown document designed to test the Apex markdown processor with extensive content, various markdown features, and realistic document structure. This document contains thousands of lines of content across multiple sections.

{{TOC:2-6}}

## Introduction

This document serves as a comprehensive test suite for the Apex markdown processor. It includes:

- Multiple levels of headings
- Extensive text content
- Code blocks in various languages
- Tables with different structures
- Lists (ordered, unordered, nested)
- Links and references
- Images and media
- Metadata and front matter
- Footnotes and citations
- Definition lists
- Blockquotes
- Horizontal rules
- And much more...

The purpose is to create a document large enough to test performance characteristics while exercising all major features of the markdown processor.

## Chapter 1: Text Formatting and Typography

### Basic Text Styles

This section demonstrates various text formatting options available in markdown:

- **Bold text** for emphasis
- *Italic text* for subtle emphasis
- ***Bold italic*** for strong emphasis
- ~~Strikethrough text~~ for deletions
- `Inline code` for technical terms
- ==Highlighted text== for important information
- {++Inserted text++} for additions
- {--Deleted text--} for removals
- ^{Superscript text} for mathematical expressions
- ~{Subscript text} for chemical formulas

### Typography Features

Smart typography automatically converts:

- Three dashes `---` into an em-dash —
- Two dashes `--` into an en-dash –
- Three dots `...` into an ellipsis …
- Straight quotes `"like this"` into smart quotes "like this"
- Single quotes `'like this'` into smart quotes 'like this'
- Guillemets `<< and >>` for French quotes « and »

### Special Characters

The document includes various special characters:

- Copyright symbol: ©
- Registered trademark: ®
- Trademark: ™
- Euro: €
- Pound: £
- Yen: ¥
- Section: §
- Paragraph: ¶
- Degree: °
- Plus-minus: ±
- Multiplication: ×
- Division: ÷

## Chapter 2: Lists and Hierarchies

### Unordered Lists

Unordered lists can be nested to multiple levels:

- First level item
  - Second level item
    - Third level item
      - Fourth level item
        - Fifth level item
- Another first level item
  - Nested item with **bold text**
  - Nested item with *italic text*
  - Nested item with `code`
- Yet another first level item

### Ordered Lists

Ordered lists demonstrate numbering:

1. First item
2. Second item
   1. Sub-item A
   2. Sub-item B
      1. Sub-sub-item i
      2. Sub-sub-item ii
3. Third item
4. Fourth item
   1. Another sub-item
   2. Yet another sub-item

### Task Lists

Task lists show completion status:

- [x] Completed task one
- [x] Completed task two
- [ ] Pending task one
- [ ] Pending task two
- [x] Another completed task
- [ ] Future task
- [ ] Another future task

### Definition Lists

Definition lists provide term definitions:

Term 1
: This is the definition of term 1. It can span multiple lines and contain various markdown elements like **bold** and *italic* text.

Term 2
: Definition of term 2 with `inline code` and [links](https://example.com).

Term 3
: A longer definition that includes:
  - Nested lists
  - Multiple paragraphs

  And even code blocks:

  ```python
  def example():
      return "definition list code"
  ```

## Chapter 3: Code Blocks and Syntax Highlighting

### Python Code Examples

```python
#!/usr/bin/env python3
"""
A comprehensive Python example demonstrating various language features.
"""

import os
import sys
from typing import List, Dict, Optional
from dataclasses import dataclass
from pathlib import Path

@dataclass
class Document:
    """Represents a markdown document."""
    title: str
    content: str
    metadata: Dict[str, str]

    def render(self) -> str:
        """Render the document to HTML."""
        return f"<h1>{self.title}</h1>\n{self.content}"

def process_files(directory: Path) -> List[Document]:
    """Process all markdown files in a directory."""
    documents = []
    for file_path in directory.glob("*.md"):
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            doc = Document(
                title=file_path.stem,
                content=content,
                metadata={}
            )
            documents.append(doc)
    return documents

if __name__ == "__main__":
    base_dir = Path(__file__).parent
    docs = process_files(base_dir / "fixtures")
    for doc in docs:
        print(doc.render())
```

### JavaScript Code Examples

```javascript
/**
 * Comprehensive JavaScript example with modern ES6+ features
 */

class MarkdownProcessor {
    constructor(options = {}) {
        this.options = {
            gfm: true,
            tables: true,
            breaks: false,
            ...options
        };
        this.ast = null;
    }

    parse(markdown) {
        const lines = markdown.split('\n');
        const blocks = [];
        let currentBlock = null;

        for (const line of lines) {
            if (this.isHeading(line)) {
                if (currentBlock) blocks.push(currentBlock);
                currentBlock = this.parseHeading(line);
            } else if (this.isCodeBlock(line)) {
                if (currentBlock) blocks.push(currentBlock);
                currentBlock = this.parseCodeBlock(line);
            } else {
                if (currentBlock) {
                    currentBlock.content += '\n' + line;
                } else {
                    currentBlock = { type: 'paragraph', content: line };
                }
            }
        }

        if (currentBlock) blocks.push(currentBlock);
        this.ast = { type: 'document', children: blocks };
        return this.ast;
    }

    isHeading(line) {
        return /^#{1,6}\s/.test(line);
    }

    isCodeBlock(line) {
        return /^```/.test(line);
    }

    parseHeading(line) {
        const match = line.match(/^(#{1,6})\s+(.+)$/);
        return {
            type: 'heading',
            level: match[1].length,
            content: match[2]
        };
    }

    parseCodeBlock(line) {
        const language = line.slice(3).trim();
        return {
            type: 'code',
            language: language || null,
            content: ''
        };
    }

    render() {
        if (!this.ast) throw new Error('No AST to render');
        return this.ast.children.map(block => {
            switch (block.type) {
                case 'heading':
                    return `<h${block.level}>${block.content}</h${block.level}>`;
                case 'code':
                    return `<pre><code>${this.escapeHtml(block.content)}</code></pre>`;
                default:
                    return `<p>${block.content}</p>`;
            }
        }).join('\n');
    }

    escapeHtml(text) {
        const map = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#039;'
        };
        return text.replace(/[&<>"']/g, m => map[m]);
    }
}

// Usage example
const processor = new MarkdownProcessor({ gfm: true });
const markdown = `# Hello World\n\nThis is a test.`;
const ast = processor.parse(markdown);
const html = processor.render();
console.log(html);
```

### C Code Examples

```c
/*
 * Comprehensive C example demonstrating various language features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_BLOCKS 1000

typedef enum {
    BLOCK_PARAGRAPH,
    BLOCK_HEADING,
    BLOCK_CODE,
    BLOCK_LIST,
    BLOCK_QUOTE
} BlockType;

typedef struct {
    BlockType type;
    int level;
    char *content;
    size_t content_len;
} Block;

typedef struct {
    Block *blocks;
    size_t count;
    size_t capacity;
} Document;

Document* document_create(void) {
    Document *doc = malloc(sizeof(Document));
    if (!doc) return NULL;

    doc->capacity = 100;
    doc->count = 0;
    doc->blocks = malloc(sizeof(Block) * doc->capacity);
    if (!doc->blocks) {
        free(doc);
        return NULL;
    }

    return doc;
}

void document_destroy(Document *doc) {
    if (!doc) return;

    for (size_t i = 0; i < doc->count; i++) {
        free(doc->blocks[i].content);
    }
    free(doc->blocks);
    free(doc);
}

bool document_add_block(Document *doc, BlockType type, int level, const char *content) {
    if (doc->count >= doc->capacity) {
        size_t new_capacity = doc->capacity * 2;
        Block *new_blocks = realloc(doc->blocks, sizeof(Block) * new_capacity);
        if (!new_blocks) return false;
        doc->blocks = new_blocks;
        doc->capacity = new_capacity;
    }

    Block *block = &doc->blocks[doc->count];
    block->type = type;
    block->level = level;
    block->content_len = strlen(content);
    block->content = malloc(block->content_len + 1);
    if (!block->content) return false;

    strcpy(block->content, content);
    doc->count++;
    return true;
}

void document_print(const Document *doc) {
    printf("Document with %zu blocks:\n\n", doc->count);
    for (size_t i = 0; i < doc->count; i++) {
        const Block *block = &doc->blocks[i];
        printf("Block %zu: type=%d, level=%d\n", i, block->type, block->level);
        printf("Content: %s\n\n", block->content);
    }
}

int main(int argc, char *argv[]) {
    Document *doc = document_create();
    if (!doc) {
        fprintf(stderr, "Failed to create document\n");
        return 1;
    }

    document_add_block(doc, BLOCK_HEADING, 1, "Hello World");
    document_add_block(doc, BLOCK_PARAGRAPH, 0, "This is a test paragraph.");
    document_add_block(doc, BLOCK_CODE, 0, "int x = 42;");

    document_print(doc);
    document_destroy(doc);

    return 0;
}
```

### Rust Code Examples

```rust
/// Comprehensive Rust example demonstrating various language features

use std::collections::HashMap;
use std::fs::File;
use std::io::{BufRead, BufReader, Result};
use std::path::Path;

#[derive(Debug, Clone, PartialEq)]
enum BlockType {
    Paragraph,
    Heading(u8),
    Code(Option<String>),
    List(Vec<String>),
    Quote,
}

#[derive(Debug)]
struct Block {
    block_type: BlockType,
    content: String,
}

#[derive(Debug)]
struct Document {
    blocks: Vec<Block>,
    metadata: HashMap<String, String>,
}

impl Document {
    fn new() -> Self {
        Document {
            blocks: Vec::new(),
            metadata: HashMap::new(),
        }
    }

    fn add_block(&mut self, block_type: BlockType, content: String) {
        self.blocks.push(Block {
            block_type,
            content,
        });
    }

    fn parse_file<P: AsRef<Path>>(path: P) -> Result<Self> {
        let file = File::open(path)?;
        let reader = BufReader::new(file);
        let mut doc = Document::new();
        let mut current_block: Option<(BlockType, Vec<String>)> = None;

        for line in reader.lines() {
            let line = line?;

            if line.starts_with("#") {
                if let Some((bt, content)) = current_block.take() {
                    doc.add_block(bt, content.join("\n"));
                }
                let level = line.chars().take_while(|&c| c == '#').count() as u8;
                let content = line[level as usize..].trim().to_string();
                doc.add_block(BlockType::Heading(level), content);
            } else if line.starts_with("```") {
                if let Some((bt, content)) = current_block.take() {
                    doc.add_block(bt, content.join("\n"));
                }
                let language = if line.len() > 3 {
                    Some(line[3..].trim().to_string())
                } else {
                    None
                };
                current_block = Some((BlockType::Code(language), Vec::new()));
            } else if !line.trim().is_empty() {
                if let Some((_, ref mut content)) = current_block {
                    content.push(line);
                } else {
                    current_block = Some((BlockType::Paragraph, vec![line]));
                }
            } else {
                if let Some((bt, content)) = current_block.take() {
                    doc.add_block(bt, content.join("\n"));
                }
            }
        }

        if let Some((bt, content)) = current_block {
            doc.add_block(bt, content.join("\n"));
        }

        Ok(doc)
    }

    fn render_html(&self) -> String {
        let mut html = String::new();
        for block in &self.blocks {
            match block.block_type {
                BlockType::Heading(level) => {
                    html.push_str(&format!("<h{}>{}</h{}>\n", level, block.content, level));
                }
                BlockType::Code(ref lang) => {
                    let lang_attr = lang.as_ref()
                        .map(|l| format!(" class=\"language-{}\"", l))
                        .unwrap_or_default();
                    html.push_str(&format!("<pre><code{}>{}</code></pre>\n", lang_attr, block.content));
                }
                BlockType::Paragraph => {
                    html.push_str(&format!("<p>{}</p>\n", block.content));
                }
                _ => {}
            }
        }
        html
    }
}

fn main() -> Result<()> {
    let doc = Document::parse_file("example.md")?;
    println!("{}", doc.render_html());
    Ok(())
}
```

### Go Code Examples

```go
package main

import (
    "bufio"
    "fmt"
    "os"
    "strings"
)

// BlockType represents the type of a markdown block
type BlockType int

const (
    BlockParagraph BlockType = iota
    BlockHeading
    BlockCode
    BlockList
    BlockQuote
)

// Block represents a single markdown block
type Block struct {
    Type    BlockType
    Level   int
    Content string
}

// Document represents a complete markdown document
type Document struct {
    Blocks   []Block
    Metadata map[string]string
}

// NewDocument creates a new empty document
func NewDocument() *Document {
    return &Document{
        Blocks:   make([]Block, 0),
        Metadata: make(map[string]string),
    }
}

// AddBlock adds a new block to the document
func (d *Document) AddBlock(blockType BlockType, level int, content string) {
    d.Blocks = append(d.Blocks, Block{
        Type:    blockType,
        Level:   level,
        Content: content,
    })
}

// ParseFile parses a markdown file and returns a Document
func ParseFile(filename string) (*Document, error) {
    file, err := os.Open(filename)
    if err != nil {
        return nil, err
    }
    defer file.Close()

    doc := NewDocument()
    scanner := bufio.NewScanner(file)
    var currentBlock strings.Builder
    var inCodeBlock bool
    var codeLanguage string

    for scanner.Scan() {
        line := scanner.Text()

        if strings.HasPrefix(line, "```") {
            if inCodeBlock {
                doc.AddBlock(BlockCode, 0, currentBlock.String())
                currentBlock.Reset()
                inCodeBlock = false
            } else {
                codeLanguage = strings.TrimSpace(line[3:])
                inCodeBlock = true
            }
            continue
        }

        if inCodeBlock {
            currentBlock.WriteString(line)
            currentBlock.WriteString("\n")
            continue
        }

        if strings.HasPrefix(line, "#") {
            if currentBlock.Len() > 0 {
                doc.AddBlock(BlockParagraph, 0, currentBlock.String())
                currentBlock.Reset()
            }
            level := 0
            for _, char := range line {
                if char == '#' {
                    level++
                } else {
                    break
                }
            }
            content := strings.TrimSpace(line[level:])
            doc.AddBlock(BlockHeading, level, content)
            continue
        }

        if line == "" {
            if currentBlock.Len() > 0 {
                doc.AddBlock(BlockParagraph, 0, currentBlock.String())
                currentBlock.Reset()
            }
            continue
        }

        if currentBlock.Len() > 0 {
            currentBlock.WriteString(" ")
        }
        currentBlock.WriteString(line)
    }

    if currentBlock.Len() > 0 {
        doc.AddBlock(BlockParagraph, 0, currentBlock.String())
    }

    return doc, scanner.Err()
}

// RenderHTML renders the document as HTML
func (d *Document) RenderHTML() string {
    var html strings.Builder
    for _, block := range d.Blocks {
        switch block.Type {
        case BlockHeading:
            html.WriteString(fmt.Sprintf("<h%d>%s</h%d>\n", block.Level, block.Content, block.Level))
        case BlockCode:
            html.WriteString(fmt.Sprintf("<pre><code>%s</code></pre>\n", block.Content))
        case BlockParagraph:
            html.WriteString(fmt.Sprintf("<p>%s</p>\n", block.Content))
        }
    }
    return html.String()
}

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintf(os.Stderr, "Usage: %s <markdown-file>\n", os.Args[0])
        os.Exit(1)
    }

    doc, err := ParseFile(os.Args[1])
    if err != nil {
        fmt.Fprintf(os.Stderr, "Error: %v\n", err)
        os.Exit(1)
    }

    fmt.Print(doc.RenderHTML())
}
```

## Chapter 4: Tables

### Simple Tables

| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Row 1, Col 1 | Row 1, Col 2 | Row 1, Col 3 |
| Row 2, Col 1 | Row 2, Col 2 | Row 2, Col 3 |
| Row 3, Col 1 | Row 3, Col 2 | Row 3, Col 3 |

### Tables with Alignment

| Left Aligned | Center Aligned | Right Aligned |
|:-------------|:--------------:|--------------:|
| Left text | Center text | Right text |
| More left | More center | More right |
| Even more left | Even more center | Even more right |

### Tables with Formatting

| Feature | Status | Notes |
|---------|--------|-------|
| **Bold text** | ✅ Complete | Works well |
| *Italic text* | ✅ Complete | Works well |
| `Code spans` | ✅ Complete | Works well |
| [Links](https://example.com) | ✅ Complete | Works well |
| Images | 🚧 In Progress | Needs testing |
| Tables | ✅ Complete | Works well |

### Large Tables

| ID | Name | Email | Department | Salary | Start Date | Status |
|----|------|-------|------------|--------|------------|--------|
| 1 | John Doe | john@example.com | Engineering | $100,000 | 2020-01-15 | Active |
| 2 | Jane Smith | jane@example.com | Marketing | $85,000 | 2019-06-20 | Active |
| 3 | Bob Johnson | bob@example.com | Sales | $75,000 | 2021-03-10 | Active |
| 4 | Alice Williams | alice@example.com | Engineering | $110,000 | 2018-09-05 | Active |
| 5 | Charlie Brown | charlie@example.com | HR | $70,000 | 2022-01-10 | Active |
| 6 | Diana Prince | diana@example.com | Engineering | $120,000 | 2017-11-12 | Active |
| 7 | Edward Norton | edward@example.com | Marketing | $80,000 | 2020-08-22 | Active |
| 8 | Fiona Apple | fiona@example.com | Sales | $78,000 | 2021-05-18 | Active |
| 9 | George Lucas | george@example.com | Engineering | $115,000 | 2019-02-14 | Active |
| 10 | Helen Troy | helen@example.com | HR | $72,000 | 2022-07-30 | Active |

## Chapter 5: Links and References

### Standard Links

Here are various types of links:

- [Standard link](https://example.com)
- [Link with title](https://example.com "Example Website")
- [Relative link](../docs/readme.md)
- [Anchor link](#chapter-5-links-and-references)
- [Email link](mailto:test@example.com)
- [Phone link](tel:+1-555-123-4567)

### Autolinks

Automatic link detection:

- https://github.com/apex/markdown
- http://example.com
- www.example.com
- test@example.com

### Reference-Style Links

Reference-style links[^ref1] are useful for keeping the document clean[^ref2].

[^ref1]: https://example.com/reference1
[^ref2]: https://example.com/reference2

### Wiki Links

Wiki-style links for internal navigation:

- [[Home]]
- [[Documentation]]
- [[API Reference]]
- [[Getting Started|Start Here]]
- [[API#Methods]]
- [[Documentation#Installation]]

### Footnotes

Here's a simple footnote[^1] and another one[^2]. We also support inline footnotes^[This is an inline footnote with some **bold** and *italic* text] and MMD inline footnotes[^This is an MMD style inline footnote with spaces and formatting].

[^1]: This is the first footnote with **formatted** content and `code`.

[^2]: This footnote has multiple paragraphs.

    It can contain code blocks:

    ```python
    def footnote_example():
        return "footnote code"
    ```

    And other block-level content!

[^3]: Footnotes can reference other footnotes[^1] and include [links](https://example.com).

## Chapter 6: Images and Media

### Basic Images

![Alt text](https://example.com/image.png "Image title")

![Local image](../images/test.png)

### Images with Attributes

![Image with width](https://example.com/image.png){width=500}

![Image with height](https://example.com/image.png){height=300}

![Image with both](https://example.com/image.png){width=500 height=300}

### Image References

![Reference image][ref-img]

[ref-img]: https://example.com/reference.png "Reference image title"

## Chapter 7: Blockquotes

### Simple Blockquotes

> This is a simple blockquote. It can contain multiple lines of text and will be rendered as a quote block.

> This is another blockquote with **bold text** and *italic text*.

### Nested Blockquotes

> This is the outer blockquote.
>
> > This is a nested blockquote.
> >
> > > And this is even more nested.

### Blockquotes with Other Elements

> This blockquote contains:
>
> - A list item
> - Another list item
>
> And a code block:
>
> ```python
> def example():
>     return "blockquote code"
> ```
>
> And even a [link](https://example.com).

## Chapter 8: Horizontal Rules

Horizontal rules can be created in multiple ways:

---

***

___

- - -

* * *

## Chapter 9: Extended Content Sections

### Section 9.1: Detailed Explanations

This section contains extensive text content to test the parser's handling of long paragraphs and multiple sections. The content is designed to be realistic and representative of actual documentation or articles.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.

Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt.

### Section 9.2: Technical Details

When processing markdown documents, the parser must handle various edge cases and special situations. This includes:

1. **Nested structures**: Lists within lists, blockquotes within blockquotes, code blocks within other blocks
2. **Edge cases**: Empty lines, whitespace handling, special characters
3. **Performance**: Large documents, many nested elements, extensive content
4. **Compatibility**: Different markdown flavors, extensions, and custom syntax

The parser should maintain good performance even with very large documents containing thousands of lines and hundreds of elements.

### Section 9.3: Real-World Scenarios

In real-world usage, markdown documents can vary significantly in size and complexity. Some documents are simple and straightforward, while others contain:

- Extensive code examples
- Multiple levels of nesting
- Complex table structures
- Many cross-references and links
- Embedded media and images
- Custom extensions and syntax

This test document attempts to capture many of these scenarios to ensure the parser handles them correctly.

## Chapter 10: Advanced Features

### Callouts and Admonitions

> [!NOTE]
> This is a note callout with important information.

> [!TIP]
> This is a tip callout with helpful advice.

> [!WARNING]
> This is a warning callout with cautionary information.

> [!IMPORTANT]
> This is an important callout with critical information.

### Inline Attribute Lists

This is a paragraph with an inline attribute list{.class-name #id-name}.

This is another paragraph with attributes{style="color: red;"}.

### Math Expressions

Inline math: $E = mc^2$

Block math:

$$
\int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi}
$$

### Citations

This document references various sources[@smith2020; @jones2021; @brown2022].

## Chapter 11: Performance Test Content

This chapter contains repetitive content designed to test parser performance with large amounts of similar content.

### Section 11.1: Repeated Patterns

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.

### Section 11.2: Many Lists

- Item 1
- Item 2
- Item 3
- Item 4
- Item 5
- Item 6
- Item 7
- Item 8
- Item 9
- Item 10
- Item 11
- Item 12
- Item 13
- Item 14
- Item 15
- Item 16
- Item 17
- Item 18
- Item 19
- Item 20

1. Numbered item 1
2. Numbered item 2
3. Numbered item 3
4. Numbered item 4
5. Numbered item 5
6. Numbered item 6
7. Numbered item 7
8. Numbered item 8
9. Numbered item 9
10. Numbered item 10
11. Numbered item 11
12. Numbered item 12
13. Numbered item 13
14. Numbered item 14
15. Numbered item 15
16. Numbered item 16
17. Numbered item 17
18. Numbered item 18
19. Numbered item 19
20. Numbered item 20

### Section 11.3: Many Headings

#### Subsection 11.3.1

Content for subsection 11.3.1.

#### Subsection 11.3.2

Content for subsection 11.3.2.

#### Subsection 11.3.3

Content for subsection 11.3.3.

#### Subsection 11.3.4

Content for subsection 11.3.4.

#### Subsection 11.3.5

Content for subsection 11.3.5.

#### Subsection 11.3.6

Content for subsection 11.3.6.

#### Subsection 11.3.7

Content for subsection 11.3.7.

#### Subsection 11.3.8

Content for subsection 11.3.8.

#### Subsection 11.3.9

Content for subsection 11.3.9.

#### Subsection 11.3.10

Content for subsection 11.3.10.

## Chapter 12: Edge Cases and Special Situations

### Empty Sections

This section intentionally contains minimal content to test edge cases.

### Special Characters in Content

Testing special characters: < > & " ' ` { } [ ] ( ) * _ # - + = | \ / ~ ^

### Code Blocks with Special Content

```
This code block contains:
- Special characters: < > & " '
- Various symbols: { } [ ] ( ) * _ # - + = | \ / ~ ^
- Unicode: © ® ™ € £ ¥ § ¶ ° ± × ÷
```

### Tables with Special Content

| Special | Characters | Content |
|---------|------------|---------|
| < > & | " ' ` | { } [ ] |
| ( ) * | _ # - | + = | \ |
| / ~ ^ | © ® ™ | € £ ¥ |

### Links with Special Characters

- [Link with &](https://example.com?q=test&param=value)
- [Link with #](https://example.com#section)
- [Link with +](https://example.com?q=test+value)

## Chapter 13: Metadata and Variables

This document includes YAML front matter with metadata. The metadata can be referenced using variable syntax:

- Title: [%title]
- Author: [%author]
- Date: [%date]
- Version: [%version]
- Description: [%description]

## Chapter 14: Conclusion

This large test document has demonstrated:

- Various markdown syntax elements
- Multiple code languages
- Complex nested structures
- Tables with different formats
- Links and references
- Images and media
- Blockquotes and callouts
- Special characters and edge cases
- Performance testing scenarios

The document is designed to be comprehensive enough to test the Apex markdown processor thoroughly while remaining readable and maintainable.

---

*End of test document*
