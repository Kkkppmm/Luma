#include "project_manager.hpp"
#include "project_templates.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

std::string GabProjectManager::default_projects_dir() {
  const char *home = getenv("HOME");
  if (!home)
    return "/tmp/LumaProjects";
  return std::string(home) + "/LumaProjects";
}

bool GabProjectManager::ensure_dir(const std::string &path, std::string &error) {
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) {
    error = "Failed to create directory: " + path + " (" + ec.message() + ")";
    return false;
  }
  return true;
}

bool GabProjectManager::write_text_file(const std::string &path,
                                        const std::string &contents,
                                        std::string &error) {
  std::ofstream out(path);
  if (!out) {
    error = "Failed to write file: " + path;
    return false;
  }
  out << contents;
  return true;
}

bool GabProjectManager::is_project_dir(const std::string &path) {
  return fs::exists(path + "/meson.build") || fs::exists(path + "/CMakeLists.txt") ||
         fs::exists(path + "/Makefile") || fs::is_directory(path);
}

std::string GabProjectManager::detect_language(const std::string &path) {
  auto ext = fs::path(path).extension().string();
  if (ext == ".c" || ext == ".h")
    return "c";
  if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp" || ext == ".hh")
    return "cpp";
  if (ext == ".py")
    return "python";
  if (ext == ".js")
    return "javascript";
  if (ext == ".xml" || ext == ".ui")
    return "xml";
  if (ext == ".css")
    return "css";
  if (ext == ".md")
    return "markdown";
  if (ext == ".json")
    return "json";
  if (ext == ".build" || path.find("meson.build") != std::string::npos)
    return "meson";
  return "text";
}

std::vector<std::string>
GabProjectManager::list_files_recursive(const std::string &root, int max_depth) {
  std::vector<std::string> files;
  if (!fs::exists(root))
    return files;

  const std::vector<std::string> skip = {".git", "build", "_build", ".flatpak-builder",
                                         "node_modules", "__pycache__"};

  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, ec);
       it != fs::recursive_directory_iterator(); ++it) {
    if (ec)
      break;
    if (it.depth() > max_depth) {
      it.disable_recursion_pending();
      continue;
    }
    const auto name = it->path().filename().string();
    if (std::find(skip.begin(), skip.end(), name) != skip.end()) {
      if (it->is_directory())
        it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file())
      files.push_back(it->path().string());
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool GabProjectManager::create_project(const GabProjectInfo &info, std::string &error) {
  if (info.name.empty() || info.path.empty()) {
    error = "Project name and path are required";
    return false;
  }

  for (char c : info.name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) {
      error = "Project name may only contain letters, numbers, - and _";
      return false;
    }
  }

  if (fs::exists(info.path)) {
    error = "Destination already exists: " + info.path;
    return false;
  }

  /* Ensure parent folder exists (e.g. ~/LumaProjects) */
  const fs::path parent = fs::path(info.path).parent_path();
  if (!parent.empty() && !ensure_dir(parent.string(), error))
    return false;

  GabProjectInfo normalized = info;
  if (normalized.template_id.empty())
    normalized.template_id = "gtk4-adwaita";

  return GabProjectTemplates::generate(normalized, error);
}

std::vector<GabTemplateMeta> GabProjectManager::list_templates() {
  return GabProjectTemplates::catalog();
}
