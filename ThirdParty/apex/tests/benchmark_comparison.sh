#!/bin/bash
# Comparative benchmark: Apex vs other Markdown processors

set -e

# Get script directory and ensure we're in the right place
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

ITERATIONS=20
TEST_FILES=(
	"$PROJECT_ROOT/tests/fixtures/comprehensive_test.md"
)

# Add larger test files if they exist
[ -f "$PROJECT_ROOT/tests/fixtures/large_doc.md" ] && TEST_FILES+=("$PROJECT_ROOT/tests/fixtures/large_doc.md")

echo "# Markdown Processor Comparison Benchmark"
echo ""

# Check which tools are available
TOOLS=()
APEX_BIN="$PROJECT_ROOT/build/apex"
[ -f "$APEX_BIN" ] && TOOLS+=("apex:$APEX_BIN")
command -v cmark-gfm >/dev/null && TOOLS+=("cmark-gfm:cmark-gfm -e table -e strikethrough -e autolink")
command -v cmark >/dev/null && TOOLS+=("cmark:cmark")
command -v pandoc >/dev/null && TOOLS+=("pandoc:pandoc -f markdown -t html")
command -v multimarkdown >/dev/null && TOOLS+=("multimarkdown:multimarkdown")
command -v kramdown >/dev/null && TOOLS+=("kramdown:kramdown")
command -v marked >/dev/null && TOOLS+=("marked:marked")

echo "## Available Tools"
echo ""
echo "Found ${#TOOLS[@]} tools:"
for tool in "${TOOLS[@]}"; do
	echo "- ${tool%%:*}"
done
echo ""

# Function to benchmark a single tool
benchmark_tool() {
	local name="$1"
	local cmd="$2"
	local file="$3"
	local iterations="$4"

	# Warm-up
	eval "$cmd \"$file\"" >/dev/null 2>&1 || return 1

	# Timed runs using hyperfine if available, else manual timing
	if command -v hyperfine >/dev/null 2>&1; then
		result=$(hyperfine --warmup 3 --runs "$iterations" --export-json /dev/stdout \
			"$cmd \"$file\"" 2>/dev/null | tail -n +5 | jq -r '.results[0].mean * 1000' 2>/dev/null)
		echo "${result:-N/A}"
	else
		local total=0
		for i in $(seq 1 $iterations); do
			local start=$(python3 -c 'import time; print(int(time.time() * 1000))')
			eval "$cmd \"$file\"" >/dev/null 2>&1
			local end=$(python3 -c 'import time; print(int(time.time() * 1000))')
			total=$((total + end - start))
		done
		echo "$((total / iterations))"
	fi
}

# Run benchmarks for each file
for file in "${TEST_FILES[@]}"; do
	if [ ! -f "$file" ]; then
		echo "⚠️  File not found: $file" >&2
		continue
	fi

	size=$(wc -c <"$file" | tr -d ' ')
	lines=$(wc -l <"$file" | tr -d ' ')

	echo "## Processor Comparison"
	echo ""
	echo "**File:** \`$file\` ($size bytes, $lines lines)"
	echo ""
	echo "| Processor | Time (ms) | Relative |"
	echo "|-----------|-----------|----------|"

	baseline=""
	for tool in "${TOOLS[@]}"; do
		name="${tool%%:*}"
		cmd="${tool#*:}"

		result=$(benchmark_tool "$name" "$cmd" "$file" "$ITERATIONS" 2>/dev/null)

		if [ -n "$result" ] && [ "$result" != "N/A" ]; then
			if [ -z "$baseline" ]; then
				baseline="$result"
				relative="1.00x"
			else
				relative=$(echo "scale=2; $result / $baseline" | bc 2>/dev/null || echo "N/A")
				relative="${relative}x"
			fi
			printf "| %s | %.2f | %s |\n" "$name" "$result" "$relative"
		else
			printf "| %s | failed | - |\n" "$name"
		fi
	done
	echo ""
done

# Apex mode comparison
echo "## Apex Mode Comparison"
echo ""
echo "**Test File:** \`${TEST_FILES[0]}\`"
echo ""
echo "| Mode | Time (ms) | Relative |"
echo "|------|-----------|----------|"

mode_baseline=""
for mode in "commonmark" "gfm" "mmd" "kramdown" "unified" "default"; do
	if [ "$mode" = "default" ]; then
		cmd="$APEX_BIN"
		display="default (unified)"
	else
		cmd="$APEX_BIN --mode $mode"
		display="$mode"
	fi

	result=$(benchmark_tool "apex-$mode" "$cmd" "${TEST_FILES[0]}" "$ITERATIONS" 2>/dev/null)

	if [ -n "$result" ] && [ "$result" != "N/A" ]; then
		if [ -z "$mode_baseline" ]; then
			mode_baseline="$result"
			relative="1.00x"
		else
			relative=$(echo "scale=2; $result / $mode_baseline" | bc 2>/dev/null || echo "N/A")
			relative="${relative}x"
		fi
		printf "| %s | %.2f | %s |\n" "$display" "$result" "$relative"
	else
		printf "| %s | failed | - |\n" "$display"
	fi
done

echo ""
echo "## Apex Feature Overhead"
echo ""
echo "| Features | Time (ms) |"
echo "|----------|-----------|"

base=$(benchmark_tool "base" "$APEX_BIN --mode commonmark --no-ids" "${TEST_FILES[0]}" "$ITERATIONS")
printf "| CommonMark (minimal) | %.2f |\n" "$base"

with_tables=$(benchmark_tool "tables" "$APEX_BIN --mode gfm" "${TEST_FILES[0]}" "$ITERATIONS")
printf "| + GFM tables/strikethrough | %.2f |\n" "$with_tables"

with_all=$(benchmark_tool "all" "$APEX_BIN" "${TEST_FILES[0]}" "$ITERATIONS")
printf "| + All Apex features | %.2f |\n" "$with_all"

with_pretty=$(benchmark_tool "pretty" "$APEX_BIN --pretty" "${TEST_FILES[0]}" "$ITERATIONS")
printf "| + Pretty printing | %.2f |\n" "$with_pretty"

with_standalone=$(benchmark_tool "standalone" "$APEX_BIN --standalone --pretty" "${TEST_FILES[0]}" "$ITERATIONS")
printf "| + Standalone document | %.2f |\n" "$with_standalone"

echo ""
echo "---"
echo ""
echo "*Benchmark Complete*"
