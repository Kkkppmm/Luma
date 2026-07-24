#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_ABOUT_VIEW (gab_about_view_get_type ())
G_DECLARE_FINAL_TYPE (GabAboutView, gab_about_view, GAB, ABOUT_VIEW, GtkBox)

GtkWidget *gab_about_view_new (void);

G_END_DECLS
