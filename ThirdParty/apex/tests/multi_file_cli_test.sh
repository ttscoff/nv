#!/usr/bin/env bash

# Simple CLI-level sanity tests for --mmd-merge and --combine (GitBook SUMMARY.md)
# using the existing combine_summary fixtures.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APEX_BIN="$ROOT/build/apex"

if [[ ! -x "$APEX_BIN" ]]; then
	echo "Error: $APEX_BIN not found or not executable. Build Apex first (cmake --build build)." >&2
	exit 1
fi

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

FIXTURES="$ROOT/tests/fixtures/combine_summary"

TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/apex-multi-file-XXXXXX")"
trap 'rm -rf "$TMPDIR"' EXIT

echo "== Testing --mmd-merge with mmd_merge-style index =="

MMD_OUT="$TMPDIR/mmd_merge_output.md"
"$APEX_BIN" --mmd-merge "$FIXTURES/index.txt" >"$MMD_OUT"

# Expect Intro, Chapter 1, and Section 1.1 in order, with Section 1.1 headings shifted one level
grep -q '^# Intro' "$MMD_OUT" || fail "mmd-merge: missing Intro heading"
grep -q '^# Chapter 1' "$MMD_OUT" || fail "mmd-merge: missing Chapter 1 heading"
grep -q '^## Section 1\.1' "$MMD_OUT" || fail "mmd-merge: Section 1.1 heading was not shifted to level 2"

pass "mmd-merge index test passed."

echo
echo "== Testing --combine with GitBook-style SUMMARY.md =="

COMBINE_OUT="$TMPDIR/summary_combine_output.md"
"$APEX_BIN" --combine "$FIXTURES/SUMMARY.md" >"$COMBINE_OUT"

grep -q '^# Intro' "$COMBINE_OUT" || fail "combine: missing Intro heading from SUMMARY.md combine"
grep -q '^# Chapter 1' "$COMBINE_OUT" || fail "combine: missing Chapter 1 heading from SUMMARY.md combine"
grep -q '^# Section 1\.1' "$COMBINE_OUT" || fail "combine: missing Section 1.1 heading from SUMMARY.md combine"

pass "SUMMARY.md combine test passed."

echo
pass "All multi-file CLI tests passed."
