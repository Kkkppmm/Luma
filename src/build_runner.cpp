#include "build_runner.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sys/wait.h>
#include <vector>

namespace fs = std::filesystem;

std::string GabBuildRunner::run_command(const std::string &cmd, const std::string &cwd,
                                        int &exit_code, LogFn log) {
  std::string full = "cd " + cwd + " && " + cmd + " 2>&1";
  std::string output;
  std::array<char, 512> buffer{};
  FILE *pipe = popen(full.c_str(), "r");
  if (!pipe) {
    exit_code = 127;
    return "Failed to start process";
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
    if (log)
      log(buffer.data());
  }
  exit_code = pclose(pipe);
  if (WIFEXITED(exit_code))
    exit_code = WEXITSTATUS(exit_code);
  return output;
}

bool GabBuildRunner::has_meson_project(const std::string &project_path) {
  return fs::exists(project_path + "/meson.build");
}

bool GabBuildRunner::configure(const std::string &project_path, std::string &error,
                               LogFn log) {
  if (!has_meson_project(project_path)) {
    error = "No meson.build found in project";
    return false;
  }
  fs::create_directories(project_path + "/build");
  int code = 0;
  std::string cmd =
      "CC=gcc CXX=g++ meson setup build --reconfigure 2>/dev/null || "
      "CC=gcc CXX=g++ meson setup build";
  std::string out = run_command(cmd, project_path, code, log);
  if (code != 0) {
    error = "Meson setup failed:\n" + out;
    return false;
  }
  return true;
}

bool GabBuildRunner::build(const std::string &project_path, std::string &error,
                           LogFn log) {
  if (!fs::exists(project_path + "/build")) {
    if (!configure(project_path, error, log))
      return false;
  }
  int code = 0;
  std::string out = run_command("meson compile -C build", project_path, code, log);
  if (code != 0) {
    /* Retry after reconfigure */
    if (!configure(project_path, error, log))
      return false;
    out = run_command("meson compile -C build", project_path, code, log);
    if (code != 0) {
      error = "Build failed:\n" + out;
      return false;
    }
  }
  return true;
}

static std::string
read_meson_project_name(const std::string &project_path) {
  std::ifstream meson(project_path + "/meson.build");
  if (!meson)
    return {};
  std::string contents((std::istreambuf_iterator<char>(meson)),
                       std::istreambuf_iterator<char>());
  std::smatch m;
  if (std::regex_search(contents, m, std::regex("project\\s*\\(\\s*'([^']+)'")))
    return m[1].str();
  return {};
}

std::string GabBuildRunner::find_binary(const std::string &project_path) {
  std::string name = read_meson_project_name(project_path);

  std::vector<fs::path> candidates;
  std::error_code ec;
  fs::path build = project_path + "/build";
  if (fs::exists(build)) {
    for (auto it = fs::recursive_directory_iterator(build, ec);
         it != fs::recursive_directory_iterator(); ++it) {
      if (ec)
        break;
      if (!it->is_regular_file())
        continue;
      auto p = it->path();
      auto fname = p.filename().string();
      if (fname.find(".so") != std::string::npos || fname.find(".a") != std::string::npos)
        continue;
      auto perms = fs::status(p, ec).permissions();
      bool exec = (perms & fs::perms::owner_exec) != fs::perms::none;
      if (!exec)
        continue;
      if (!name.empty() && fname == name)
        return p.string();
      candidates.push_back(p);
    }
  }
  if (!candidates.empty())
    return candidates.front().string();
  return {};
}

std::string GabBuildRunner::find_python_script(const std::string &project_path) {
  std::string name = read_meson_project_name(project_path);
  std::error_code ec;

  if (!name.empty()) {
    fs::path preferred = fs::path(project_path) / "src" / (name + ".py");
    if (fs::is_regular_file(preferred, ec))
      return preferred.string();
    preferred = fs::path(project_path) / (name + ".py");
    if (fs::is_regular_file(preferred, ec))
      return preferred.string();
  }

  fs::path src = fs::path(project_path) / "src";
  if (fs::is_directory(src, ec)) {
    for (auto it = fs::directory_iterator(src, ec); it != fs::directory_iterator();
         ++it) {
      if (ec)
        break;
      if (!it->is_regular_file())
        continue;
      if (it->path().extension() == ".py")
        return it->path().string();
    }
  }

  /* Root-level .py fallback */
  for (auto it = fs::directory_iterator(project_path, ec);
       it != fs::directory_iterator(); ++it) {
    if (ec)
      break;
    if (!it->is_regular_file())
      continue;
    if (it->path().extension() == ".py")
      return it->path().string();
  }
  return {};
}

bool GabBuildRunner::run(const std::string &project_path, std::string &error,
                         LogFn log) {
  if (!build(project_path, error, log))
    return false;

  std::string bin = find_binary(project_path);
  std::string cmd;
  if (!bin.empty()) {
    if (log)
      log("Running " + bin + " …\n");
    cmd = "nohup \"" + bin + "\" >/tmp/luma-run-app.log 2>&1 & echo $!";
  } else {
    /* Python / install_data-only templates have nothing under build/ */
    std::string py = find_python_script(project_path);
    if (py.empty()) {
      error = "Build succeeded but no runnable binary was found under build/ "
              "(and no Python entry script under src/).";
      return false;
    }
    if (log)
      log("Running python3 " + py + " …\n");
    cmd = "nohup python3 \"" + py + "\" >/tmp/luma-run-app.log 2>&1 & echo $!";
  }

  int code = 0;
  std::string out = run_command(cmd, project_path, code, log);
  if (code != 0) {
    error = "Failed to launch:\n" + out;
    return false;
  }
  if (log)
    log("Started PID " + out);
  return true;
}
