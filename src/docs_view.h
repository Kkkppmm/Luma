#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_DOCS_VIEW (gab_docs_view_get_type ())
G_DECLARE_FINAL_TYPE (GabDocsView, gab_docs_view, GAB, DOCS_VIEW, GtkBox)

GtkWidget *gab_docs_view_new (void);

G_END_DECLS
