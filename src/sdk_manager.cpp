#include "sdk_manager.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <sys/wait.h>

bool GabSdkManager::flatpak_available() {
  int code = 1;
  run_command("command -v flatpak >/dev/null 2>&1", code);
  return code == 0;
}

std::string GabSdkManager::run_command(const std::string &cmd, int &exit_code,
                                       ProgressFn progress) {
  std::string output;
  std::array<char, 512> buffer{};
  std::string full = cmd + " 2>&1";
  FILE *pipe = popen(full.c_str(), "r");
  if (!pipe) {
    exit_code = 127;
    return "Failed to start process";
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
    if (progress)
      progress(buffer.data());
  }
  exit_code = pclose(pipe);
  if (WIFEXITED(exit_code))
    exit_code = WEXITSTATUS(exit_code);
  return output;
}

std::vector<GabSdkPackage> GabSdkManager::catalog() {
  return {
      {"org.gnome.Platform", "GNOME Platform",
       "Runtime libraries used to run GNOME applications", "46", "platform", false},
      {"org.gnome.Sdk", "GNOME Sdk",
       "Headers, compilers, and tools to build GNOME applications", "46", "sdk",
       false},
      {"org.gnome.Platform", "GNOME Platform",
       "Runtime libraries used to run GNOME applications", "47", "platform", false},
      {"org.gnome.Sdk", "GNOME Sdk",
       "Headers, compilers, and tools to build GNOME applications", "47", "sdk",
       false},
      {"org.freedesktop.Sdk.Extension.rust-stable", "Rust Stable Extension",
       "Optional Rust toolchain for the Freedesktop Sdk", "24.08", "extension",
       false},
      {"org.gnome.Sdk.Docs", "GNOME API Docs",
       "Offline API documentation packages when available on Flathub", "46", "docs",
       false},
  };
}

void GabSdkManager::refresh_installed(std::vector<GabSdkPackage> &packages) {
  if (!flatpak_available()) {
    for (auto &p : packages)
      p.installed = false;
    return;
  }
  int code = 0;
  std::string out =
      run_command("flatpak list --user --app --runtime --columns=application,branch "
                  "2>/dev/null; flatpak list --system --app --runtime "
                  "--columns=application,branch 2>/dev/null",
                  code);
  for (auto &p : packages) {
    p.installed = false;
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.find(p.id) != std::string::npos &&
          line.find(p.branch) != std::string::npos) {
        p.installed = true;
        break;
      }
    }
  }
}

bool GabSdkManager::ensure_flathub(std::string &error, ProgressFn progress) {
  if (!flatpak_available()) {
    error = "flatpak is not installed. Install it with your package manager.";
    return false;
  }
  int code = 0;
  run_command("flatpak remote-add --if-not-exists --user flathub "
              "https://dl.flathub.org/repo/flathub.flatpakrepo",
              code, progress);
  if (code != 0) {
    error = "Failed to add Flathub remote";
    return false;
  }
  return true;
}

bool GabSdkManager::install_package(const GabSdkPackage &pkg, std::string &error,
                                    ProgressFn progress) {
  if (!ensure_flathub(error, progress))
    return false;
  int code = 0;
  std::string cmd = "flatpak install -y --user flathub " + pkg.id + "//" + pkg.branch;
  std::string out = run_command(cmd, code, progress);
  if (code != 0) {
    error = "Install failed for " + pkg.id + ":\n" + out;
    return false;
  }
  return true;
}

bool GabSdkManager::uninstall_package(const GabSdkPackage &pkg, std::string &error,
                                      ProgressFn progress) {
  if (!flatpak_available()) {
    error = "flatpak is not installed";
    return false;
  }
  int code = 0;
  std::string cmd = "flatpak uninstall -y --user " + pkg.id + "//" + pkg.branch;
  std::string out = run_command(cmd, code, progress);
  if (code != 0) {
    error = "Uninstall failed for " + pkg.id + ":\n" + out;
    return false;
  }
  return true;
}
