#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p .local output
: "${CXX:=c++}"
# Rebuild only if the implementation or generated metadata changed.
exe=.local/offline-preview
if [ ! -x "$exe" ] || [ -n "$(find core firmware/main tools/offline_preview.cc -type f \( -name '*.cc' -o -name '*.h' \) -newer "$exe" 2>/dev/null)" ]; then
    "$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -I core -I firmware/main core/learning.cc core/offline.cc firmware/main/runtime_ui.cc tools/offline_preview.cc -o "$exe"
fi
exec "$exe" "$@"
