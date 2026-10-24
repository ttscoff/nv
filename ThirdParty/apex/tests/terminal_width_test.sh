#!/usr/bin/env bash
# Sanity check that -t terminal --width N wraps output (CLI applies wrap after render).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="${APEX_BIN:-$ROOT/build/apex}"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Set APEX_BIN or build first." >&2
	exit 1
fi

# Long line without ANSI: with --width 10 we expect more than one line
LONG="this is a long line of plain text that should wrap"
OUT=$("$APEX_BIN" -t terminal --width 10 <<< "$LONG")
LINES=$(echo "$OUT" | wc -l | tr -d ' ')
if [[ "$LINES" -lt 2 ]]; then
	echo "Expected --width 10 to wrap output into multiple lines, got $LINES line(s)" >&2
	exit 1
fi

# With --width 80 the same line may stay on one line (or wrap less)
OUT2=$("$APEX_BIN" -t terminal --width 80 <<< "$LONG")
if [[ -z "$OUT2" ]]; then
	echo "Expected non-empty output with --width 80" >&2
	exit 1
fi

echo "terminal_width_test: --width wrapping OK"
