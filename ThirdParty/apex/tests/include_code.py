#!/usr/bin/env python3
"""Example code for inclusion"""

def process_document(filename):
    """Process a markdown document"""
    with open(filename, 'r') as f:
        content = f.read()
    return convert_markdown(content)

def convert_markdown(text):
    """Convert markdown to HTML"""
    processor = MarkdownProcessor()
    return processor.render(text)

if __name__ == '__main__':
    import sys
    result = process_document(sys.argv[1])
    print(result)

