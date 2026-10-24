#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT"

echo "=== Swift debug build ==="
swift build

echo "=== Swift release build ==="
swift build -c release

echo "Swift package OK."
