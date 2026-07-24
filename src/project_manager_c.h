#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool gab_project_create(const char *name, const char *parent_dir,
                        const char *template_id, char **out_path, char **out_error);
bool gab_project_is_project_dir(const char *path);
char *gab_project_default_dir(void);
char **gab_project_list_files(const char *root, int *count);
void gab_project_free_file_list(char **list, int count);
char *gab_project_detect_language(const char *path);

#ifdef __cplusplus
}
#endif
