#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GAB_TYPE_WINDOW (gab_window_get_type ())
G_DECLARE_FINAL_TYPE (GabWindow, gab_window, GAB, WINDOW, AdwApplicationWindow)

void gab_window_open_project (GabWindow *self, const char *path);
void gab_window_show_home (GabWindow *self);

G_END_DECLS
