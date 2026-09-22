#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build
"${CXX:-clang++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  tests/test_jarvis_renderer.cpp firmware/Copilot/src/JarvisSpriteRenderer.cpp \
  -o build/test-jarvis-renderer
build/test-jarvis-renderer
