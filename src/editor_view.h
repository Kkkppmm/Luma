#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GAB_TYPE_EDITOR_VIEW (gab_editor_view_get_type ())
G_DECLARE_FINAL_TYPE (GabEditorView, gab_editor_view, GAB, EDITOR_VIEW, GtkBox)

GtkWidget *gab_editor_view_new (void);
void gab_editor_view_open_project (GabEditorView *self, const char *path);
const char *gab_editor_view_get_project_path (GabEditorView *self);

G_END_DECLS
