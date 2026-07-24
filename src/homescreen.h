#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_HOMESCREEN (gab_homescreen_get_type ())
G_DECLARE_FINAL_TYPE (GabHomescreen, gab_homescreen, GAB, HOMESCREEN, GtkBox)

GtkWidget *gab_homescreen_new (void);
void gab_homescreen_refresh (GabHomescreen *self);

G_END_DECLS
