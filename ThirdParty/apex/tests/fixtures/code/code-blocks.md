# Code Block Test Fixtures

This file contains various code block formats for testing syntax highlighting.

## Fenced Code Blocks with Language

### Python

```python
def fibonacci(n):
    """Calculate the nth Fibonacci number."""
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

# Example usage
for i in range(10):
    print(f"fib({i}) = {fibonacci(i)}")
```

### JavaScript

```javascript
class Calculator {
    constructor() {
        this.result = 0;
    }

    add(x) {
        this.result += x;
        return this;
    }

    multiply(x) {
        this.result *= x;
        return this;
    }

    getValue() {
        return this.result;
    }
}

const calc = new Calculator();
console.log(calc.add(5).multiply(3).getValue()); // 15
```

### Ruby

```ruby
class Person
  attr_accessor :name, :age

  def initialize(name, age)
    @name = name
    @age = age
  end

  def greet
    "Hello, my name is #{name} and I am #{age} years old."
  end
end

person = Person.new("Alice", 30)
puts person.greet
```

### C

```c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <name>\n", argv[0]);
        return 1;
    }

    printf("Hello, %s!\n", argv[1]);
    return 0;
}
```

### Bash/Shell

```bash
#!/bin/bash

# Simple script to backup files
BACKUP_DIR="/tmp/backup"
SOURCE_DIR="$1"

if [ -z "$SOURCE_DIR" ]; then
    echo "Usage: $0 <source_directory>"
    exit 1
fi

mkdir -p "$BACKUP_DIR"
tar -czf "$BACKUP_DIR/backup-$(date +%Y%m%d).tar.gz" "$SOURCE_DIR"
echo "Backup complete!"
```

### JSON

```json
{
    "name": "apex",
    "version": "0.1.52",
    "description": "Unified Markdown Processor",
    "features": [
        "CommonMark",
        "GFM",
        "MultiMarkdown",
        "Kramdown"
    ],
    "config": {
        "syntax_highlight": true,
        "line_numbers": false
    }
}
```

### YAML

```yaml
project:
  name: apex
  version: 0.1.52

features:
  - name: tables
    enabled: true
  - name: footnotes
    enabled: true
  - name: syntax_highlighting
    tools:
      - pygments
      - skylighting
```

### HTML

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Example Page</title>
    <style>
        .highlight { background: yellow; }
    </style>
</head>
<body>
    <h1>Hello, World!</h1>
    <p class="highlight">This is highlighted text.</p>
</body>
</html>
```

### CSS

```css
/* Modern button styles */
.button {
    display: inline-block;
    padding: 0.5em 1em;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    transition: transform 0.2s ease;
}

.button:hover {
    transform: translateY(-2px);
}
```

### SQL

```sql
SELECT
    users.name,
    COUNT(orders.id) AS order_count,
    SUM(orders.total) AS total_spent
FROM users
LEFT JOIN orders ON users.id = orders.user_id
WHERE users.created_at >= '2024-01-01'
GROUP BY users.id
HAVING order_count > 5
ORDER BY total_spent DESC
LIMIT 10;
```

## Fenced Code Block Without Language

```
This is a plain code block without a language specifier.
It should be rendered as-is without syntax highlighting.
    Indentation is preserved.
```

## Indented Code Blocks

The following is an indented code block (4 spaces):

    def simple_function():
        return "This is an indented code block"

    # No syntax highlighting for indented blocks
    print(simple_function())

Another indented block:

    {
        "type": "indented",
        "highlighted": false
    }

## Code Blocks with Special Characters

```python
# Test HTML entities in code
def html_example():
    return "<div class=\"test\">&amp; &lt; &gt;</div>"

# Test quotes and apostrophes
message = "It's a \"quoted\" string"
```

## Short Code Blocks

```python
x = 42
```

```javascript
const y = "hello";
```

## Multi-line String Literals

```python
docstring = """
This is a multi-line
string literal that spans
several lines.
"""

raw_string = r"C:\Users\test\path"
```

## Code with Long Lines

```javascript
// This is a very long line that might cause horizontal scrolling in some renderers and should be handled gracefully by the syntax highlighter
const veryLongVariableName = "This string contains a very long piece of text that demonstrates how the highlighter handles lines that exceed typical display widths";
```
