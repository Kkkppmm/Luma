#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int tab_width;
  int insert_spaces;
  int trim_trailing;
  int ensure_final_newline;
} GabFormatOptionsC;

char *gab_format_document(const char *text, const GabFormatOptionsC *opts);
char *gab_format_lines(const char *text, int start_line, int end_line,
                       const GabFormatOptionsC *opts);
char *gab_indent_lines(const char *text, int start_line, int end_line,
                       const GabFormatOptionsC *opts, int unindent);
char *gab_trim_trailing(const char *text);
char *gab_ensure_final_newline(const char *text);
char *gab_sort_lines(const char *text, int start_line, int end_line);
char *gab_toggle_comments(const char *text, int start_line, int end_line);

#ifdef __cplusplus
}
#endif
