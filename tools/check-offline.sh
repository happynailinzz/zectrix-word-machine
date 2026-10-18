#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${CXX:=c++}"
work=$(mktemp -d /tmp/zectrix-offline-tests.XXXXXX)
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core core/learning.cc tests/core_test.cc -o "$work/core"
"$work/core"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core core/learning.cc core/offline.cc tests/offline_test.cc -o "$work/offline"
"$work/offline"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core core/learning.cc core/offline.cc core/snapshot_store.cc tests/books_store_test.cc -o "$work/books"
"$work/books"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core core/learning.cc core/offline.cc tests/dashboard_test.cc -o "$work/dashboard"
"$work/dashboard"
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core -I firmware/main core/learning.cc core/offline.cc firmware/main/runtime_ui.cc tests/runtime_ui_test.cc -o "$work/ui"
mkdir -p output/runtime-v2
"$work/ui" firmware/main/generated/glyphs.bin output/runtime-v2
mkdir -p output/home-design
"$CXX" -std=c++17 -Wall -Wextra -Werror -I core -I firmware/main core/learning.cc core/offline.cc firmware/main/runtime_ui.cc tests/home_design_test.cc -o "$work/home-design"
"$work/home-design"
