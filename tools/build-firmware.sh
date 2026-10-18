#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${IDF_PATH:?Activate ESP-IDF export.sh first}"
export IDF_COMPONENT_MANAGER=0
export PYTHONDONTWRITEBYTECODE=1
python "$IDF_PATH/tools/idf.py" -C firmware -DIDF_TARGET=esp32s3 build
python tools/build_manifest.py
