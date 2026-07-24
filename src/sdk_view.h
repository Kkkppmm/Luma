#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_SDK_VIEW (gab_sdk_view_get_type ())
G_DECLARE_FINAL_TYPE (GabSdkView, gab_sdk_view, GAB, SDK_VIEW, GtkBox)

GtkWidget *gab_sdk_view_new (void);
void gab_sdk_view_refresh (GabSdkView *self);

G_END_DECLS
