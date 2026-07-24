#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool gab_build_configure(const char *project_path, char **out_log, char **out_error);
bool gab_build_compile(const char *project_path, char **out_log, char **out_error);
bool gab_build_run(const char *project_path, char **out_log, char **out_error);
char *gab_build_find_binary(const char *project_path);

#ifdef __cplusplus
}
#endif
