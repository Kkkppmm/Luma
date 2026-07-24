#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct GabTemplateInfoC {
  char *id;
  char *title;
  char *description;
  char *category;
  char *display_title;
} GabTemplateInfoC;

bool gab_project_create(const char *name, const char *parent_dir,
                        const char *template_id, char **out_path, char **out_error);
bool gab_project_is_project_dir(const char *path);
char *gab_project_default_dir(void);
char **gab_project_list_files(const char *root, int *count);
void gab_project_free_file_list(char **list, int count);
char *gab_project_detect_language(const char *path);

/* Returns heap array of templates; free with gab_project_free_templates. */
GabTemplateInfoC *gab_project_list_templates(int *count);
void gab_project_free_templates(GabTemplateInfoC *list, int count);

#ifdef __cplusplus
}
#endif
