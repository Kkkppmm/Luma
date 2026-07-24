#include "project_manager_c.h"
#include "project_manager.hpp"
#include "project_templates.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

bool gab_project_create(const char *name, const char *parent_dir,
                        const char *template_id, char **out_path, char **out_error) {
  GabProjectInfo info;
  info.name = name ? name : "";
  info.template_id = template_id ? template_id : "gtk4-adwaita";
  std::string parent = parent_dir ? parent_dir : GabProjectManager::default_projects_dir();
  info.path = parent + "/" + info.name;

  std::string error;
  if (!GabProjectManager::create_project(info, error)) {
    if (out_error)
      *out_error = strdup(error.c_str());
    return false;
  }
  if (out_path)
    *out_path = strdup(info.path.c_str());
  if (out_error)
    *out_error = nullptr;
  return true;
}

bool gab_project_is_project_dir(const char *path) {
  if (!path)
    return false;
  return GabProjectManager::is_project_dir(path);
}

char *gab_project_default_dir(void) {
  return strdup(GabProjectManager::default_projects_dir().c_str());
}

char **gab_project_list_files(const char *root, int *count) {
  auto files = GabProjectManager::list_files_recursive(root ? root : "");
  if (count)
    *count = static_cast<int>(files.size());
  char **list = static_cast<char **>(calloc(files.size() + 1, sizeof(char *)));
  for (size_t i = 0; i < files.size(); ++i)
    list[i] = strdup(files[i].c_str());
  return list;
}

void gab_project_free_file_list(char **list, int count) {
  if (!list)
    return;
  for (int i = 0; i < count; ++i)
    free(list[i]);
  free(list);
}

char *gab_project_detect_language(const char *path) {
  return strdup(GabProjectManager::detect_language(path ? path : "").c_str());
}

GabTemplateInfoC *gab_project_list_templates(int *count) {
  auto catalog = GabProjectManager::list_templates();
  if (count)
    *count = static_cast<int>(catalog.size());
  auto *list =
      static_cast<GabTemplateInfoC *>(calloc(catalog.size(), sizeof(GabTemplateInfoC)));
  for (size_t i = 0; i < catalog.size(); ++i) {
    list[i].id = strdup(catalog[i].id.c_str());
    list[i].title = strdup(catalog[i].title.c_str());
    list[i].description = strdup(catalog[i].description.c_str());
    list[i].category = strdup(catalog[i].category.c_str());
    list[i].display_title =
        strdup(GabProjectTemplates::display_title(catalog[i]).c_str());
  }
  return list;
}

void gab_project_free_templates(GabTemplateInfoC *list, int count) {
  if (!list)
    return;
  for (int i = 0; i < count; ++i) {
    free(list[i].id);
    free(list[i].title);
    free(list[i].description);
    free(list[i].category);
    free(list[i].display_title);
  }
  free(list);
}

} /* extern "C" */
