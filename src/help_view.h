#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_HELP_VIEW (gab_help_view_get_type ())
G_DECLARE_FINAL_TYPE (GabHelpView, gab_help_view, GAB, HELP_VIEW, GtkBox)

GtkWidget *gab_help_view_new (void);

G_END_DECLS
