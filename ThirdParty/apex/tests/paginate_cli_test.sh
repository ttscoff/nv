#!/usr/bin/env bash
# Sanity checks for -p/--paginate and paginate config option for terminal output.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="${APEX_BIN:-$ROOT/build/apex}"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Set APEX_BIN or build first." >&2
	exit 1
fi

DOC="# Heading

Some *styled* text for testing."

echo "== Testing --paginate with APEX_PAGER=cat =="

BASE_OUT=$("$APEX_BIN" -t terminal <<< "$DOC")
PAGED_OUT=$(APEX_PAGER=cat "$APEX_BIN" -t terminal --paginate <<< "$DOC")

if [[ "$BASE_OUT" != "$PAGED_OUT" ]]; then
	echo "paginate_cli_test: -p output differs from baseline when using APEX_PAGER=cat" >&2
	exit 1
fi

echo "paginate_cli_test: -p with APEX_PAGER=cat matches baseline output."

echo
echo "== Testing paginate: true via config metadata with APEX_PAGER=cat =="

TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/apex-paginate-XXXXXX")"
trap 'rm -rf "$TMPDIR"' EXIT

CFG="$TMPDIR/config.yml"
cat >"$CFG" <<'YAML'
paginate: true
YAML

CFG_OUT=$(APEX_PAGER=cat "$APEX_BIN" --meta-file "$CFG" -t terminal <<< "$DOC")

if [[ "$BASE_OUT" != "$CFG_OUT" ]]; then
	echo "paginate_cli_test: paginate: true output differs from baseline when using APEX_PAGER=cat" >&2
	exit 1
fi

echo "paginate_cli_test: paginate: true config matches baseline output with APEX_PAGER=cat."

echo
echo "== Testing paginate: symbols via config metadata with APEX_PAGER=cat =="

CFG_SYM="$TMPDIR/config-symbols.yml"
cat >"$CFG_SYM" <<'YAML'
paginate: symbols
YAML

SYM_OUT=$(APEX_PAGER=cat "$APEX_BIN" --meta-file "$CFG_SYM" -t terminal <<< "$DOC")
if [[ "$BASE_OUT" != "$SYM_OUT" ]]; then
	echo "paginate_cli_test: paginate: symbols output differs from baseline when using APEX_PAGER=cat" >&2
	exit 1
fi

echo "paginate_cli_test: paginate: symbols config matches baseline output with APEX_PAGER=cat."

echo
echo "== Testing that --paginate is ignored for non-terminal formats =="

HTML_BASE=$("$APEX_BIN" -t html <<< "$DOC")
HTML_PAGED=$(APEX_PAGER=cat "$APEX_BIN" -t html --paginate <<< "$DOC")

if [[ "$HTML_BASE" != "$HTML_PAGED" ]]; then
	echo "paginate_cli_test: -p should be a no-op for -t html" >&2
	exit 1
fi

echo "paginate_cli_test: -p ignored for non-terminal formats as expected."

echo
echo "All paginate CLI tests passed."

