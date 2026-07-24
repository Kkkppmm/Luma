#include "code_formatter_c.h"
#include "code_formatter.hpp"

#include <cstdlib>
#include <cstring>

static GabFormatOptions to_opts(const GabFormatOptionsC *opts) {
  GabFormatOptions o;
  if (!opts)
    return o;
  o.tab_width = opts->tab_width > 0 ? opts->tab_width : 2;
  o.insert_spaces = opts->insert_spaces != 0;
  o.trim_trailing = opts->trim_trailing != 0;
  o.ensure_final_newline = opts->ensure_final_newline != 0;
  return o;
}

extern "C" {

char *gab_format_document(const char *text, const GabFormatOptionsC *opts) {
  auto out = GabCodeFormatter::format_document(text ? text : "", to_opts(opts));
  return strdup(out.c_str());
}

char *gab_format_lines(const char *text, int start_line, int end_line,
                       const GabFormatOptionsC *opts) {
  auto out =
      GabCodeFormatter::format_lines(text ? text : "", start_line, end_line, to_opts(opts));
  return strdup(out.c_str());
}

char *gab_indent_lines(const char *text, int start_line, int end_line,
                       const GabFormatOptionsC *opts, int unindent) {
  auto out = GabCodeFormatter::indent_lines(text ? text : "", start_line, end_line,
                                            to_opts(opts), unindent != 0);
  return strdup(out.c_str());
}

char *gab_trim_trailing(const char *text) {
  return strdup(GabCodeFormatter::trim_trailing_whitespace(text ? text : "").c_str());
}

char *gab_ensure_final_newline(const char *text) {
  return strdup(GabCodeFormatter::ensure_final_newline(text ? text : "").c_str());
}

char *gab_sort_lines(const char *text, int start_line, int end_line) {
  return strdup(
      GabCodeFormatter::sort_lines(text ? text : "", start_line, end_line).c_str());
}

char *gab_toggle_comments(const char *text, int start_line, int end_line) {
  return strdup(GabCodeFormatter::toggle_line_comments(text ? text : "", start_line,
                                                       end_line)
                    .c_str());
}

} /* extern "C" */
