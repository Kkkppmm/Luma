#pragma once

#include <functional>
#include <string>

class GabBuildRunner {
public:
  using LogFn = std::function<void(const std::string &line)>;

  static bool has_meson_project(const std::string &project_path);
  static bool configure(const std::string &project_path, std::string &error,
                        LogFn log = nullptr);
  static bool build(const std::string &project_path, std::string &error,
                    LogFn log = nullptr);
  static bool run(const std::string &project_path, std::string &error,
                  LogFn log = nullptr);
  static std::string find_binary(const std::string &project_path);
  static std::string run_command(const std::string &cmd, const std::string &cwd,
                                 int &exit_code, LogFn log = nullptr);
};
