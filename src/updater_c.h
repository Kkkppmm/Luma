#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GabUpdateInfo {
  bool update_available;
  char *current_version;
  char *latest_version;
  char *tag;
  char *release_notes;
  char *release_url;
  char *asset_name;
  char *asset_url;
  char *error;
} GabUpdateInfo;

typedef struct GabUpdateInstallResult {
  bool success;
  char *message;
} GabUpdateInstallResult;

/* Checks GitHub releases/latest for Kkkppmm/Luma. Caller must free with gab_update_info_free. */
GabUpdateInfo *gab_update_check(const char *current_version);
void gab_update_info_free(GabUpdateInfo *info);

/* Downloads preferred package (.deb/.rpm/.tar.gz) and installs it. */
GabUpdateInstallResult *gab_update_download_and_install(const GabUpdateInfo *info);
void gab_update_install_result_free(GabUpdateInstallResult *result);

char *gab_update_detect_pkg_kind(void);

#ifdef __cplusplus
}
#endif
