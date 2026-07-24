#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct {
  char *id;
  char *name;
  char *description;
  char *branch;
  char *kind;
  bool installed;
} GabSdkPackageC;

bool gab_sdk_flatpak_available(void);
GabSdkPackageC *gab_sdk_catalog(int *count);
void gab_sdk_refresh_installed(GabSdkPackageC *packages, int count);
void gab_sdk_free_catalog(GabSdkPackageC *packages, int count);
bool gab_sdk_install(const char *id, const char *branch, char **out_error);
bool gab_sdk_uninstall(const char *id, const char *branch, char **out_error);

#ifdef __cplusplus
}
#endif
