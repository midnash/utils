#!/usr/bin/env bash

set -euo pipefail

# Clean up common CMake artifacts from the repository root.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ITEMS=(
    "CMakeCache.txt"
    "CMakeFiles"
    "cmake_install.cmake"
    "Makefile"
    "bin/"
    "build/"
)

for item in "${ITEMS[@]}"; do
    target="$SCRIPT_DIR/$item"

    if [[ -e "$target" ]]; then
        echo "Removing ${item%/}..."
        rm -rf -- "$target"
    fi
done