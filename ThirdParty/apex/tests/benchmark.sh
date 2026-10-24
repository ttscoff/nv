#!/bin/bash
# Comprehensive Apex Performance Benchmark

# Get script directory and ensure we're in the right place
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

APEX="$PROJECT_ROOT/build/apex"
TEST_FILE="$PROJECT_ROOT/tests/fixtures/comprehensive_test.md"
ITERATIONS=50

# Verify files exist
if [ ! -f "$APEX" ]; then
	echo "ERROR: Apex binary not found at $APEX"
	echo "Please build the project first: make"
	exit 1
fi

if [ ! -f "$TEST_FILE" ]; then
	echo "ERROR: Test file not found at $TEST_FILE"
	exit 1
fi

echo "# Apex Markdown Processor - Performance Benchmark"
echo ""
echo "## Test Document"
echo ""
LINES=$(wc -l <"$TEST_FILE")
WORDS=$(wc -w <"$TEST_FILE")
BYTES=$(wc -c <"$TEST_FILE")

echo "- **File:** \`$TEST_FILE\`"
echo "- **Lines:** $LINES"
echo "- **Words:** $WORDS"
echo "- **Size:** $BYTES bytes"
echo ""

# Function to run benchmark and return results
benchmark() {
	local mode="$1"
	local args="$2"
	local desc="$3"

	# Warm-up run
	if ! $APEX $args "$TEST_FILE" >/dev/null 2>&1; then
		echo "ERROR: Failed to run apex command. Check if binary exists and test file is valid." >&2
		return 1
	fi

	# Timed runs
	local total=0
	local min=999999
	local max=0

	for i in $(seq 1 $ITERATIONS); do
		local start=$(gdate +%s%N 2>/dev/null || echo "$(date +%s)000000000")
		if ! $APEX $args "$TEST_FILE" >/dev/null 2>&1; then
			echo "ERROR: Failed on iteration $i" >&2
			return 1
		fi
		local end=$(gdate +%s%N 2>/dev/null || echo "$(date +%s)000000000")
		local elapsed=$(((end - start) / 1000000))

		# Sanity check - elapsed should be positive
		if [ $elapsed -lt 0 ]; then
			echo "WARNING: Negative elapsed time on iteration $i, skipping" >&2
			continue
		fi

		total=$((total + elapsed))
		[ $elapsed -lt $min ] && min=$elapsed
		[ $elapsed -gt $max ] && max=$elapsed
	done

	local avg=$((total / ITERATIONS))
	local throughput="0"
	if [ $avg -gt 0 ]; then
		throughput=$(echo "scale=2; $WORDS / ($avg / 1000)" | bc 2>/dev/null || echo "0")
	fi

	# Output as table row
	printf "| %s | %d | %d | %d | %d | %.2f |\n" "$desc" "$ITERATIONS" "$avg" "$min" "$max" "$throughput"
}

# Run benchmarks
echo "## Output Modes"
echo ""
echo "| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |"
echo "|------|------------|--------------|---------|---------|------------------------|"

benchmark "fragment" "" "Fragment Mode (default HTML output)"
benchmark "pretty" "--pretty" "Pretty-Print Mode (formatted HTML)"
benchmark "standalone" "--standalone" "Standalone Mode (complete HTML document)"
benchmark "combined" "--standalone --pretty" "Standalone + Pretty (full features)"

echo ""
echo "## Mode Comparison"
echo ""
echo "| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |"
echo "|------|------------|--------------|---------|---------|------------------------|"

benchmark "commonmark" "--mode commonmark" "CommonMark Mode (minimal, spec-compliant)"
benchmark "gfm" "--mode gfm" "GFM Mode (GitHub Flavored Markdown)"
benchmark "mmd" "--mode mmd" "MultiMarkdown Mode (metadata, footnotes, tables)"
benchmark "kramdown" "--mode kramdown" "Kramdown Mode (attributes, definition lists)"
benchmark "unified" "--mode unified" "Unified Mode (all features enabled)"
benchmark "default" "" "Default Mode (unified, all features)"

echo ""
echo "---"
echo ""
echo "*Benchmark Complete*"
