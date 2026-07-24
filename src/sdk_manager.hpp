#pragma once

#include <functional>
#include <string>
#include <vector>

struct GabSdkPackage {
  std::string id;
  std::string name;
  std::string description;
  std::string branch;
  std::string kind; /* platform | sdk | extension | docs */
  bool installed = false;
};

class GabSdkManager {
public:
  using ProgressFn = std::function<void(const std::string &line)>;

  static std::vector<GabSdkPackage> catalog();
  static void refresh_installed(std::vector<GabSdkPackage> &packages);
  static bool ensure_flathub(std::string &error, ProgressFn progress = nullptr);
  static bool install_package(const GabSdkPackage &pkg, std::string &error,
                              ProgressFn progress = nullptr);
  static bool uninstall_package(const GabSdkPackage &pkg, std::string &error,
                                ProgressFn progress = nullptr);
  static bool flatpak_available();
  static std::string run_command(const std::string &cmd, int &exit_code,
                                 ProgressFn progress = nullptr);
};
