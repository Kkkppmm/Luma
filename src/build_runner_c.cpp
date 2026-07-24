#include "build_runner_c.h"
#include "build_runner.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

static char *dup_or_null(const std::string &s) {
  return s.empty() ? nullptr : strdup(s.c_str());
}

extern "C" {

bool gab_build_configure(const char *project_path, char **out_log, char **out_error) {
  std::string log;
  std::string error;
  bool ok = GabBuildRunner::configure(
      project_path ? project_path : "", error,
      [&](const std::string &line) { log += line; });
  if (out_log)
    *out_log = dup_or_null(log);
  if (out_error)
    *out_error = ok ? nullptr : dup_or_null(error);
  return ok;
}

bool gab_build_compile(const char *project_path, char **out_log, char **out_error) {
  std::string log;
  std::string error;
  bool ok = GabBuildRunner::build(
      project_path ? project_path : "", error,
      [&](const std::string &line) { log += line; });
  if (out_log)
    *out_log = dup_or_null(log);
  if (out_error)
    *out_error = ok ? nullptr : dup_or_null(error);
  return ok;
}

bool gab_build_run(const char *project_path, char **out_log, char **out_error) {
  std::string log;
  std::string error;
  bool ok = GabBuildRunner::run(
      project_path ? project_path : "", error,
      [&](const std::string &line) { log += line; });
  if (out_log)
    *out_log = dup_or_null(log);
  if (out_error)
    *out_error = ok ? nullptr : dup_or_null(error);
  return ok;
}

char *gab_build_find_binary(const char *project_path) {
  return dup_or_null(GabBuildRunner::find_binary(project_path ? project_path : ""));
}

} /* extern "C" */
