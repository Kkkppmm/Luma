#include "sdk_manager_c.h"
#include "sdk_manager.hpp"

#include <cstdlib>
#include <cstring>

extern "C" {

bool gab_sdk_flatpak_available(void) { return GabSdkManager::flatpak_available(); }

GabSdkPackageC *gab_sdk_catalog(int *count) {
  auto pkgs = GabSdkManager::catalog();
  GabSdkManager::refresh_installed(pkgs);
  if (count)
    *count = static_cast<int>(pkgs.size());
  auto *out = static_cast<GabSdkPackageC *>(calloc(pkgs.size(), sizeof(GabSdkPackageC)));
  for (size_t i = 0; i < pkgs.size(); ++i) {
    out[i].id = strdup(pkgs[i].id.c_str());
    out[i].name = strdup(pkgs[i].name.c_str());
    out[i].description = strdup(pkgs[i].description.c_str());
    out[i].branch = strdup(pkgs[i].branch.c_str());
    out[i].kind = strdup(pkgs[i].kind.c_str());
    out[i].installed = pkgs[i].installed;
  }
  return out;
}

void gab_sdk_refresh_installed(GabSdkPackageC *packages, int count) {
  if (!packages || count <= 0)
    return;
  auto pkgs = GabSdkManager::catalog();
  GabSdkManager::refresh_installed(pkgs);
  for (int i = 0; i < count; ++i) {
    packages[i].installed = false;
    for (const auto &p : pkgs) {
      if (packages[i].id && packages[i].branch && p.id == packages[i].id &&
          p.branch == packages[i].branch) {
        packages[i].installed = p.installed;
        break;
      }
    }
  }
}

void gab_sdk_free_catalog(GabSdkPackageC *packages, int count) {
  if (!packages)
    return;
  for (int i = 0; i < count; ++i) {
    free(packages[i].id);
    free(packages[i].name);
    free(packages[i].description);
    free(packages[i].branch);
    free(packages[i].kind);
  }
  free(packages);
}

bool gab_sdk_install(const char *id, const char *branch, char **out_error) {
  GabSdkPackage pkg;
  pkg.id = id ? id : "";
  pkg.branch = branch ? branch : "";
  std::string error;
  if (!GabSdkManager::install_package(pkg, error)) {
    if (out_error)
      *out_error = strdup(error.c_str());
    return false;
  }
  if (out_error)
    *out_error = nullptr;
  return true;
}

bool gab_sdk_uninstall(const char *id, const char *branch, char **out_error) {
  GabSdkPackage pkg;
  pkg.id = id ? id : "";
  pkg.branch = branch ? branch : "";
  std::string error;
  if (!GabSdkManager::uninstall_package(pkg, error)) {
    if (out_error)
      *out_error = strdup(error.c_str());
    return false;
  }
  if (out_error)
    *out_error = nullptr;
  return true;
}

} /* extern "C" */
