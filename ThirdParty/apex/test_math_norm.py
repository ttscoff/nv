import re

def normalize_math_spacing(line, is_in_code_block=False):
    """Normalize display math block spacing

    For display math ($$...$$):
    - Single line: $$ text $$ -> $$text$$ (remove spaces inside)
    - Multi-line: No space between opening $$ and first line, or between last line and closing $$

    For inline math ($...$), be conservative - only normalize if clearly math (not currency).
    """
    if is_in_code_block:
        return line

    # Preserve newline
    has_newline = line.endswith('\n')
    line_no_nl = line.rstrip('\n')

    # Pattern for display math: $$...$$
    # This handles both single-line and multi-line
    display_math_pattern = r'\$\$([^\$]*?)\$\$'

    def normalize_display_math(match):
        content = match.group(1)
        # For multi-line, remove leading/trailing whitespace but preserve internal newlines
        # Split by newlines, strip first and last lines, rejoin
        lines = content.split('\n')
        if len(lines) > 1:
            # Multi-line: strip first and last lines, preserve middle
            if lines[0].strip() == '':
                lines = lines[1:]
            if lines and lines[-1].strip() == '':
                lines = lines[:-1]
            if lines:
                lines[0] = lines[0].lstrip()
                lines[-1] = lines[-1].rstrip()
            normalized = '\n'.join(lines)
        else:
            # Single line: just strip
            normalized = content.strip()
        return '$$' + normalized + '$$'

    # Replace display math blocks
    normalized = re.sub(display_math_pattern, normalize_display_math, line_no_nl)

    # For inline math, be very conservative - only normalize if it looks like math
    # (contains operators, letters, or is clearly mathematical)
    # Skip currency patterns like $1.50, $2, etc.
    inline_math_pattern = r'\$([^\$]+?)\$'

    def normalize_inline_math(match):
        content = match.group(1).strip()
        # Only normalize if it looks like math (has operators, letters, or is clearly math)
        # Skip if it's just digits and punctuation (likely currency)
        if re.match(r'^[\d.,\s]+$', content):
            # Looks like currency, don't normalize
            return '$' + match.group(1) + '$'
        # Looks like math, normalize spacing
        return '$' + content + '$'

    # Replace inline math (conservatively)
    normalized = re.sub(inline_math_pattern, normalize_inline_math, normalized)

    return normalized + ('\n' if has_newline else '')

# Test cases
test_cases = [
    '$$ x^2 + y^2 = z^2 $$',
    '$$\nx = 1\ny = 2\n$$',
    '$$\n  x = 1\n  y = 2\n  $$',
    'This costs $1.50.',
    'Math: $x^2 + y^2$',
]

for test in test_cases:
    result = normalize_math_spacing(test)
    print(f'Input:  {repr(test)}')
    print(f'Output: {repr(result)}')
    print()
