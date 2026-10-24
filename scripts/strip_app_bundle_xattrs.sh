#!/usr/bin/env bash
# Strip extended attributes from an .app bundle before packaging.
# Prevents AppleDouble (._*) sidecar entries when archiving with ditto -c -k.
set -euo pipefail

APP_PATH="${1:?Usage: $0 path/to/App.app}"

if [[ ! -d "$APP_PATH" || "$APP_PATH" != *.app ]]; then
  echo "Not an app bundle: $APP_PATH" >&2
  exit 1
fi

/usr/bin/xattr -cr "$APP_PATH"
