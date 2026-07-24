#!/usr/bin/env bash
# Unit checks for the C++ formatter (no GUI required)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/fmt_test.cpp" <<'EOF'
#include "code_formatter.hpp"
#include <cassert>
#include <iostream>
#include <string>

int main() {
  GabFormatOptions opts;
  opts.tab_width = 2;
  opts.insert_spaces = true;

  std::string messy = "int main(){  \n\treturn 0;   \n}";
  auto formatted = GabCodeFormatter::format_document(messy, opts);
  assert(formatted.find("  return 0;") != std::string::npos);
  assert(!formatted.empty() && formatted.back() == '\n');

  auto indented = GabCodeFormatter::indent_lines("a\nb\n", 0, 1, opts, false);
  assert(indented.rfind("  a\n  b", 0) == 0);

  auto commented = GabCodeFormatter::toggle_line_comments("int x;\nint y;\n", 0, 1);
  assert(commented.find("// int x;") != std::string::npos);

  auto sorted = GabCodeFormatter::sort_lines("c\na\nb\n", 0, 2);
  assert(sorted == "a\nb\nc\n" || sorted.rfind("a\nb\nc", 0) == 0);

  std::cout << "formatter tests OK\n";
  return 0;
}
EOF

g++ -std=c++17 -I"$ROOT/src" "$ROOT/src/code_formatter.cpp" "$TMP/fmt_test.cpp" -o "$TMP/fmt_test"
"$TMP/fmt_test"
