#include "updater_c.h"
#include "updater.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

static char *dup_cstr(const std::string &s) {
  return strdup(s.c_str());
}

extern "C" {

GabUpdateInfo *gab_update_check(const char *current_version) {
  auto *out = static_cast<GabUpdateInfo *>(calloc(1, sizeof(GabUpdateInfo)));
  const std::string current = current_version ? current_version : "0.0.0";
  out->current_version = dup_cstr(current);

  GabReleaseInfo info;
  std::string error;
  if (!GabUpdater::fetch_latest(info, error)) {
    out->error = dup_cstr(error);
    return out;
  }

  out->latest_version = dup_cstr(info.version);
  out->tag = dup_cstr(info.tag);
  out->release_notes = dup_cstr(info.body);
  out->release_url = dup_cstr(info.html_url);

  GabReleaseAsset asset = GabUpdater::pick_asset(info);
  out->asset_name = dup_cstr(asset.name);
  out->asset_url = dup_cstr(asset.url);
  out->update_available = GabUpdater::is_newer(info, current) && !asset.url.empty();
  return out;
}

void gab_update_info_free(GabUpdateInfo *info) {
  if (!info)
    return;
  free(info->current_version);
  free(info->latest_version);
  free(info->tag);
  free(info->release_notes);
  free(info->release_url);
  free(info->asset_name);
  free(info->asset_url);
  free(info->error);
  free(info);
}

GabUpdateInstallResult *gab_update_download_and_install(const GabUpdateInfo *info) {
  auto *out =
      static_cast<GabUpdateInstallResult *>(calloc(1, sizeof(GabUpdateInstallResult)));
  if (!info || !info->asset_url || !info->asset_name) {
    out->message = dup_cstr("No package selected for install");
    return out;
  }

  GabReleaseInfo release;
  release.tag = info->tag ? info->tag : "";
  release.version = info->latest_version ? info->latest_version : "";
  release.body = info->release_notes ? info->release_notes : "";
  release.html_url = info->release_url ? info->release_url : "";

  GabReleaseAsset asset;
  asset.name = info->asset_name;
  asset.url = info->asset_url;
  release.assets.push_back(asset);

  std::string error;
  std::string log;
  bool ok = GabUpdater::download_and_install(
      release, error, [&](const std::string &line) { log += line; });
  out->success = ok;
  if (ok)
    out->message = dup_cstr(log.empty() ? "Update installed successfully." : log);
  else
    out->message = dup_cstr(error.empty() ? log : (error + "\n" + log));
  return out;
}

void gab_update_install_result_free(GabUpdateInstallResult *result) {
  if (!result)
    return;
  free(result->message);
  free(result);
}

char *gab_update_detect_pkg_kind(void) {
  return dup_cstr(GabUpdater::detect_pkg_kind());
}

} /* extern "C" */
