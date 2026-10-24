#!/bin/bash
# Generate GFM-compliant header IDs using available tools
# This script tries multiple tools to generate header IDs for comparison

TEST_FILE="tests/gfm_header_id_test.md"

echo "=== Generating Header IDs with Available Tools ==="
echo ""

# Extract headings from test file
grep -E '^#+ ' "$TEST_FILE" | sed 's/^#* //' > /tmp/headings.txt

# Try pandoc
if command -v pandoc &> /dev/null; then
    echo "Using Pandoc:"
    cat "$TEST_FILE" | pandoc -f gfm -t html 2>&1 | grep -E '<h[1-6] id=' | sed 's/.*id="\([^"]*\)".*/\1/' > /tmp/pandoc_ids.txt
    echo "Generated $(wc -l < /tmp/pandoc_ids.txt) IDs"
    echo ""
fi

# Try comrak
if command -v comrak &> /dev/null; then
    echo "Using Comrak:"
    # Comrak uses anchor tags with IDs: <a ... id="header-id"></a>
    cat "$TEST_FILE" | comrak --gfm --header-ids "" 2>&1 | grep -E 'id="[^"]*"' | sed 's/.*id="\([^"]*\)".*/\1/' > /tmp/comrak_ids.txt
    if [ -f /tmp/comrak_ids.txt ] && [ -s /tmp/comrak_ids.txt ]; then
        echo "Generated $(wc -l < /tmp/comrak_ids.txt) IDs"
    else
        echo "Generated 0 IDs (comrak may not generate IDs in this format)"
    fi
    echo ""
fi

# Our implementation
echo "Using Apex:"
cat "$TEST_FILE" | ./build/apex --mode gfm 2>&1 | grep -E '<h[1-6] id=' | sed 's/.*id="\([^"]*\)".*/\1/' > /tmp/apex_ids.txt
echo "Generated $(wc -l < /tmp/apex_ids.txt) IDs"
echo ""

# Show comparison if we have multiple tools
echo "=== Comparison ==="
HEADERS="Heading"
COLS="/tmp/headings.txt"

if [ -f /tmp/pandoc_ids.txt ] && [ -s /tmp/pandoc_ids.txt ]; then
    HEADERS="$HEADERS|Pandoc"
    COLS="$COLS /tmp/pandoc_ids.txt"
fi

if [ -f /tmp/comrak_ids.txt ] && [ -s /tmp/comrak_ids.txt ]; then
    HEADERS="$HEADERS|Comrak"
    COLS="$COLS /tmp/comrak_ids.txt"
fi

if [ -f /tmp/marked_ids.txt ] && [ -s /tmp/marked_ids.txt ]; then
    HEADERS="$HEADERS|Marked"
    COLS="$COLS /tmp/marked_ids.txt"
fi

HEADERS="$HEADERS|Apex"
COLS="$COLS /tmp/apex_ids.txt"

echo "$HEADERS"
echo "$(echo "$HEADERS" | sed 's/[^|]/-/g')"
paste -d '|' $COLS | head -50
echo ""

# Try marked (JavaScript) with gfm-heading-id plugin if available
if command -v node &> /dev/null && npm list -g marked-gfm-heading-id &> /dev/null; then
    echo "Using Marked (JavaScript) with GFM Heading ID plugin:"
    # Find the global node_modules path
    NODE_PATH=$(npm root -g)
    node -e "
        const fs = require('fs');
        const path = require('path');
        const { marked } = require('$NODE_PATH/marked');
        const { gfmHeadingId } = require('$NODE_PATH/marked-gfm-heading-id');
        marked.use(gfmHeadingId());
        const text = fs.readFileSync('$TEST_FILE', 'utf8');
        const html = marked(text);
        const ids = html.match(/<h[1-6] id=\"([^\"]+)\"/g) || [];
        ids.forEach(id => {
            const match = id.match(/id=\"([^\"]+)\"/);
            if (match) console.log(match[1]);
        });
    " > /tmp/marked_ids.txt 2>/dev/null
    if [ -f /tmp/marked_ids.txt ] && [ -s /tmp/marked_ids.txt ]; then
        echo "Generated $(wc -l < /tmp/marked_ids.txt) IDs"
        echo ""
    else
        echo "Generated 0 IDs"
        echo ""
    fi
fi

echo ""
echo "=== Summary ==="
echo "Available tools tested:"
[ -f /tmp/pandoc_ids.txt ] && [ -s /tmp/pandoc_ids.txt ] && echo "  ✓ Pandoc"
[ -f /tmp/comrak_ids.txt ] && [ -s /tmp/comrak_ids.txt ] && echo "  ✓ Comrak"
[ -f /tmp/marked_ids.txt ] && [ -s /tmp/marked_ids.txt ] && echo "  ✓ Marked (JavaScript)"
echo "  ✓ Apex (our implementation)"
echo ""
echo "Note: GitHub's API doesn't return header IDs."

