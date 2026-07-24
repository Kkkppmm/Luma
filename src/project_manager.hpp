#pragma once

#include <string>
#include <vector>

struct GabProjectInfo {
  std::string name;
  std::string path;
  std::string template_id;
};

class GabProjectManager {
public:
  static std::string default_projects_dir();
  static bool create_project(const GabProjectInfo &info, std::string &error);
  static std::vector<std::string> list_files_recursive(const std::string &root,
                                                       int max_depth = 8);
  static bool is_project_dir(const std::string &path);
  static std::string detect_language(const std::string &path);

private:
  static bool write_text_file(const std::string &path, const std::string &contents,
                              std::string &error);
  static bool ensure_dir(const std::string &path, std::string &error);
  static std::string template_meson(const GabProjectInfo &info, bool with_adwaita);
  static std::string template_main_c(const GabProjectInfo &info, bool with_adwaita);
  static std::string template_desktop(const GabProjectInfo &info);
};
