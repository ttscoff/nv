#!/usr/bin/env bash
#
# Generate optional code coverage reports using clang + llvm-cov.
#
# This does not affect the normal build/test flow; it builds into build-coverage/.
#
# Usage:
#   ./test_coverage.sh            # build, run tests, generate HTML report
#   ./test_coverage.sh --no-html  # build, run tests, generate text summary only
#   ./test_coverage.sh --no-open  # don't open the report automatically (macOS)
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build-coverage"
PROFRAW_GLOB="${BUILD_DIR}/coverage-*.profraw"
PROFDATA="${BUILD_DIR}/coverage.profdata"
HTML_DIR="${BUILD_DIR}/coverage-html"

NO_HTML=0
NO_OPEN=0

while [[ $# -gt 0 ]]; do
	case "$1" in
	--no-html)
		NO_HTML=1
		shift
		;;
	--no-open)
		NO_OPEN=1
		shift
		;;
	-h | --help)
		sed -n '1,80p' "$0"
		exit 0
		;;
	*)
		echo "Unknown argument: $1" >&2
		exit 2
		;;
	esac
done

llvm_cov() {
	if command -v xcrun >/dev/null 2>&1; then
		xcrun llvm-cov "$@"
	else
		llvm-cov "$@"
	fi
}

llvm_profdata() {
	if command -v xcrun >/dev/null 2>&1; then
		xcrun llvm-profdata "$@"
	else
		llvm-profdata "$@"
	fi
}

if ! command -v cmake >/dev/null 2>&1; then
	echo "Error: cmake not found" >&2
	exit 1
fi

if ! command -v clang >/dev/null 2>&1; then
	echo "Error: clang not found (required for llvm-cov coverage)" >&2
	exit 1
fi

if ! (command -v llvm-cov >/dev/null 2>&1 || command -v xcrun >/dev/null 2>&1); then
	echo "Error: llvm-cov not found (install LLVM, or use Xcode's xcrun on macOS)" >&2
	exit 1
fi

if ! (command -v llvm-profdata >/dev/null 2>&1 || command -v xcrun >/dev/null 2>&1); then
	echo "Error: llvm-profdata not found (install LLVM, or use Xcode's xcrun on macOS)" >&2
	exit 1
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_C_COMPILER=clang \
	-DCMAKE_C_FLAGS="-O0 -g -fprofile-instr-generate -fcoverage-mapping" \
	-DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" \
	-DCMAKE_SHARED_LINKER_FLAGS="-fprofile-instr-generate"

cmake --build "${BUILD_DIR}" --target apex_test_runner

rm -f ${PROFRAW_GLOB} 2>/dev/null || true
LLVM_PROFILE_FILE="${BUILD_DIR}/coverage-%p.profraw" "${BUILD_DIR}/apex_test_runner"

llvm_profdata merge -sparse ${PROFRAW_GLOB} -o "${PROFDATA}"

echo ""
echo "Coverage summary:"
llvm_cov report "${BUILD_DIR}/apex_test_runner" \
	-instr-profile="${PROFDATA}" \
	-ignore-filename-regex='/(vendor|deps)/' \
	-ignore-filename-regex='/tests/'

if [[ "${NO_HTML}" -eq 0 ]]; then
	rm -rf "${HTML_DIR}"
	mkdir -p "${HTML_DIR}"

	llvm_cov show "${BUILD_DIR}/apex_test_runner" \
		-instr-profile="${PROFDATA}" \
		-format=html \
		-output-dir "${HTML_DIR}" \
		-ignore-filename-regex='/(vendor|deps)/' \
		-ignore-filename-regex='/tests/'

	echo ""
	echo "HTML report: ${HTML_DIR}/index.html"

	if [[ "${NO_OPEN}" -eq 0 ]] && command -v open >/dev/null 2>&1; then
		open "${HTML_DIR}/index.html" >/dev/null 2>&1 || true
	fi
fi
