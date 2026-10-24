#!/bin/bash
# Compare header ID generation between pandoc and apex

TEST_FILE="tests/gfm_header_id_test.md"

echo "=== Comparing Header IDs: Pandoc vs Apex ==="
echo ""

# Extract headings from test file
grep -E '^#+ ' "$TEST_FILE" | sed 's/^#* //' > /tmp/headings.txt

# Generate IDs with pandoc
echo "Pandoc IDs:"
cat "$TEST_FILE" | pandoc -f gfm -t html 2>&1 | grep -E '<h[1-6] id=' | sed 's/.*id="\([^"]*\)".*/\1/' > /tmp/pandoc_ids.txt

# Generate IDs with apex
echo "Apex IDs:"
cat "$TEST_FILE" | ./build/apex --mode gfm 2>&1 | grep -E '<h[1-6] id=' | sed 's/.*id="\([^"]*\)".*/\1/' > /tmp/apex_ids.txt

# Show comparison
echo ""
echo "=== Side-by-side Comparison ==="
echo "Heading Text | Pandoc ID | Apex ID"
echo "------------|-----------|---------"
paste -d '|' /tmp/headings.txt /tmp/pandoc_ids.txt /tmp/apex_ids.txt | head -30

# Show differences
echo ""
echo "=== Differences ==="
diff -u /tmp/pandoc_ids.txt /tmp/apex_ids.txt | head -50

