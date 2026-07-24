#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

/* Present child content in its own AdwApplicationWindow belonging to app. */
GtkWindow *gab_secondary_window_present (GtkApplication *app,
                                         const char *title,
                                         const char *subtitle,
                                         GtkWidget *content,
                                         int width,
                                         int height);

G_END_DECLS
