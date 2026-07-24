#include "project_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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

std::string GabProjectManager::template_meson(const GabProjectInfo &info,
                                              bool with_adwaita) {
  std::ostringstream o;
  o << "project('" << info.name << "', 'c',\n"
    << "  version: '0.1.0',\n"
    << "  meson_version: '>= 1.0.0',\n"
    << "  default_options: ['warning_level=2', 'c_std=c11'],\n"
    << ")\n\n"
    << "gtk_dep = dependency('gtk4')\n";
  if (with_adwaita)
    o << "adw_dep = dependency('libadwaita-1')\n";
  o << "\nexecutable('" << info.name << "',\n"
    << "  'src/main.c',\n"
    << "  dependencies: [gtk_dep";
  if (with_adwaita)
    o << ", adw_dep";
  o << "],\n"
    << "  install: true,\n"
    << ")\n";
  return o.str();
}

std::string GabProjectManager::template_main_c(const GabProjectInfo &info,
                                               bool with_adwaita) {
  std::ostringstream o;
  if (with_adwaita) {
    o << "#include <adwaita.h>\n\n"
      << "static void\n"
      << "activate (GtkApplication *app, gpointer user_data)\n"
      << "{\n"
      << "  GtkWidget *window = adw_application_window_new (app);\n"
      << "  gtk_window_set_title (GTK_WINDOW (window), \"" << info.name << "\");\n"
      << "  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);\n"
      << "  GtkWidget *label = gtk_label_new (\"Hello from " << info.name << "!\");\n"
      << "  adw_application_window_set_content (ADW_APPLICATION_WINDOW (window), label);\n"
      << "  gtk_window_present (GTK_WINDOW (window));\n"
      << "}\n\n"
      << "int\n"
      << "main (int argc, char **argv)\n"
      << "{\n"
      << "  AdwApplication *app = adw_application_new (\"org.example." << info.name
      << "\", G_APPLICATION_DEFAULT_FLAGS);\n"
      << "  g_signal_connect (app, \"activate\", G_CALLBACK (activate), NULL);\n"
      << "  return g_application_run (G_APPLICATION (app), argc, argv);\n"
      << "}\n";
  } else {
    o << "#include <gtk/gtk.h>\n\n"
      << "static void\n"
      << "activate (GtkApplication *app, gpointer user_data)\n"
      << "{\n"
      << "  GtkWidget *window = gtk_application_window_new (app);\n"
      << "  gtk_window_set_title (GTK_WINDOW (window), \"" << info.name << "\");\n"
      << "  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);\n"
      << "  GtkWidget *label = gtk_label_new (\"Hello from " << info.name << "!\");\n"
      << "  gtk_window_set_child (GTK_WINDOW (window), label);\n"
      << "  gtk_window_present (GTK_WINDOW (window));\n"
      << "}\n\n"
      << "int\n"
      << "main (int argc, char **argv)\n"
      << "{\n"
      << "  GtkApplication *app = gtk_application_new (\"org.example." << info.name
      << "\", G_APPLICATION_DEFAULT_FLAGS);\n"
      << "  g_signal_connect (app, \"activate\", G_CALLBACK (activate), NULL);\n"
      << "  return g_application_run (G_APPLICATION (app), argc, argv);\n"
      << "}\n";
  }
  return o.str();
}

std::string GabProjectManager::template_desktop(const GabProjectInfo &info) {
  std::ostringstream o;
  o << "[Desktop Entry]\n"
    << "Name=" << info.name << "\n"
    << "Exec=" << info.name << "\n"
    << "Icon=" << info.name << "\n"
    << "Terminal=false\n"
    << "Type=Application\n"
    << "Categories=GNOME;GTK;\n";
  return o.str();
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

  bool with_adwaita = (info.template_id != "gtk4");
  if (!ensure_dir(info.path + "/src", error))
    return false;
  if (!ensure_dir(info.path + "/data", error))
    return false;

  if (!write_text_file(info.path + "/meson.build", template_meson(info, with_adwaita),
                       error))
    return false;
  if (!write_text_file(info.path + "/src/main.c", template_main_c(info, with_adwaita),
                       error))
    return false;
  if (!write_text_file(info.path + "/data/" + info.name + ".desktop",
                       template_desktop(info), error))
    return false;
  if (!write_text_file(info.path + "/README.md",
                       "# " + info.name + "\n\nGenerated by Luma Builder.\n\n"
                       "```bash\nmeson setup build && meson compile -C build\n```\n",
                       error))
    return false;

  return true;
}
