#!/usr/bin/env bash

# CLI tests for --info, --extract-meta, and --extract-meta-value using tests/fixtures/metadata.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="${APEX_BIN:-$ROOT/build/apex}"
FIXTURES="$ROOT/tests/fixtures/metadata"
YAML="$FIXTURES/yaml-frontmatter.md"
MMD="$FIXTURES/mmd-metadata.md"
PANDOC="$FIXTURES/pandoc-meta.md"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Build Apex first (cmake --build build --target apex_cli)." >&2
	exit 1
fi

for f in "$YAML" "$MMD" "$PANDOC"; do
	if [[ ! -f "$f" ]]; then
		echo "Error: missing fixture $f" >&2
		exit 1
	fi
done

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
	C_PASS=$'\033[1;92m'
	C_RESET=$'\033[0m'
else
	C_PASS=''
	C_RESET=''
fi
if [[ -t 2 && -z "${NO_COLOR:-}" ]]; then
	C_FAIL=$'\033[1;91m'
	C_RESET_ERR=$'\033[0m'
else
	C_FAIL=''
	C_RESET_ERR=''
fi

pass() { printf '%s%s%s\n' "$C_PASS" "$1" "$C_RESET"; }
fail() { printf '%s%s%s\n' "$C_FAIL" "$1" "$C_RESET_ERR" >&2; exit 1; }

TMPDIR="${TMPDIR:-/tmp}"
WORK="$(mktemp -d "${TMPDIR}/apex-metadata-cli-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

echo "== Testing -i / --info (no input files; info on stdout) =="

INFO_OUT="$WORK/info_stdout.txt"
# Close stdin so the CLI does not wait for interactive input in some environments.
"$APEX_BIN" -i </dev/null >"$INFO_OUT"
grep -q '^version:' "$INFO_OUT" || fail "-i without files: stdout is missing the version line"
grep -q 'plugins:' "$INFO_OUT" || fail "-i without files: stdout is missing the plugins section"
pass "-i without files: stdout contains version and plugins section."

echo
echo "== Testing -i with a file (info on stderr, HTML on stdout) =="

STDOUT="$WORK/out.html"
STDERR="$WORK/info.err"
"$APEX_BIN" -i "$YAML" >"$STDOUT" 2>"$STDERR"
grep -q '^version:' "$STDERR" || fail "metadata_cli_test: expected version line on stderr when using -i with a file"
grep -q '<p>' "$STDOUT" || fail "metadata_cli_test: expected HTML fragment on stdout with -i and a file"
pass "-i with file: version/config on stderr, conversion on stdout."

echo
echo "== Testing --extract-meta (YAML fixture) =="

META_OUT="$WORK/meta.yaml"
"$APEX_BIN" --extract-meta "$YAML" >"$META_OUT"
grep -q '^title: A YAML test$' "$META_OUT" || fail "metadata_cli_test: --extract-meta missing expected title from YAML fixture"
grep -q '^random_key: Flargle$' "$META_OUT" || fail "metadata_cli_test: --extract-meta missing random_key from YAML fixture"
pass "--extract-meta on YAML front matter: expected keys present."

echo
echo "== Testing -e / --extract-meta-value =="

val="$("$APEX_BIN" -e title "$YAML")"
[[ "$val" == "A YAML test" ]] || fail "metadata_cli_test: -e title from YAML expected 'A YAML test', got '$val'"

val="$("$APEX_BIN" --extract-meta-value random_key "$YAML")"
[[ "$val" == "Flargle" ]] || fail "metadata_cli_test: -e random_key expected 'Flargle', got '$val'"

val="$("$APEX_BIN" -e randomkey "$MMD")"
[[ "$val" == "Bargle" ]] || fail "metadata_cli_test: -e randomkey (MMD) expected 'Bargle', got '$val'"

val="$("$APEX_BIN" -e title "$PANDOC")"
[[ "$val" == "Pandoc Metadata" ]] || fail "metadata_cli_test: -e title (Pandoc) expected 'Pandoc Metadata', got '$val'"

if "$APEX_BIN" -e definitely_missing_key_xyz "$YAML" 2>"$WORK/miss.err"; then
	fail "metadata_cli_test: expected non-zero exit for missing metadata key"
fi
grep -q "not found" "$WORK/miss.err" || fail "metadata_cli_test: expected error message for missing key on stderr"
pass "-e returns values and exit 1 when key is absent."

echo
echo "== Testing --combine --extract-meta (merge order, later file wins) =="

COMBINED="$WORK/combined_meta.yaml"
"$APEX_BIN" --combine --extract-meta "$MMD" "$YAML" >"$COMBINED"
grep -q '^title: A YAML test$' "$COMBINED" || fail "metadata_cli_test: combined extract expected last file to win for title"
pass "--combine --extract-meta: later file overrides title as expected."

echo
echo "== Testing global config metadata in modes without front matter =="

mkdir -p "$WORK/xdg/apex"
printf 'title: Config Title\n' >"$WORK/xdg/apex/config.yml"
for mode in gfm commonmark; do
	OUT="$(printf 'Hello world\n' | XDG_CONFIG_HOME="$WORK/xdg" "$APEX_BIN" --no-plugins -m "$mode")"
	if grep -qE 'Config Title|<hr' <<<"$OUT" || ! grep -q '<p>Hello world</p>' <<<"$OUT"; then
		printf '%s%s%s\n' "$C_FAIL" "metadata_cli_test: -m $mode rendered config metadata as document text:" "$C_RESET_ERR" >&2
		printf '%s\n' "$OUT" >&2
		exit 1
	fi
done

printf 'title: Config Title\nmode: gfm\n' >"$WORK/xdg/apex/config.yml"
OUT="$(printf 'Hello world\n' | XDG_CONFIG_HOME="$WORK/xdg" "$APEX_BIN" --no-plugins)"
if grep -qE 'Config Title|<hr' <<<"$OUT"; then
	printf '%s%s%s\n' "$C_FAIL" "metadata_cli_test: config 'mode: gfm' rendered config metadata as document text:" "$C_RESET_ERR" >&2
	printf '%s\n' "$OUT" >&2
	exit 1
fi

printf 'title: Config Title\n' >"$WORK/xdg/apex/config.yml"
OUT="$(printf 'Hello [%%title]\n' | XDG_CONFIG_HOME="$WORK/xdg" "$APEX_BIN" --no-plugins -m unified)"
if ! grep -q 'Hello Config Title' <<<"$OUT"; then
	printf '%s%s%s\n' "$C_FAIL" "metadata_cli_test: unified mode should still expose config metadata to the document:" "$C_RESET_ERR" >&2
	printf '%s\n' "$OUT" >&2
	exit 1
fi
pass "Config metadata is not injected as text in gfm/commonmark and still reaches unified documents."

echo
pass "All metadata CLI tests passed."
