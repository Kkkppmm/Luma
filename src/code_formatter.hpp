#pragma once

#include <string>
#include <vector>

struct GabFormatOptions {
  int tab_width = 2;
  bool insert_spaces = true;
  bool trim_trailing = true;
  bool ensure_final_newline = true;
};

class GabCodeFormatter {
public:
  static std::string format_document(const std::string &text,
                                     const GabFormatOptions &opts);
  static std::string format_lines(const std::string &text, int start_line, int end_line,
                                  const GabFormatOptions &opts);
  static std::string indent_lines(const std::string &text, int start_line, int end_line,
                                  const GabFormatOptions &opts, bool unindent);
  static std::string trim_trailing_whitespace(const std::string &text);
  static std::string ensure_final_newline(const std::string &text);
  static std::string sort_lines(const std::string &text, int start_line, int end_line);
  static std::string toggle_line_comments(const std::string &text, int start_line,
                                          int end_line);
  static std::vector<std::string> split_lines(const std::string &text);
  static std::string join_lines(const std::vector<std::string> &lines,
                                bool keep_final_newline);
};
