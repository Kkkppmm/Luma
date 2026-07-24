#include "editor_view.h"
#include "code_formatter_c.h"
#include "project_manager_c.h"
#include "config.h"

#include <gtksourceview/gtksource.h>
#include <adwaita.h>
#include <stdlib.h>
#include <string.h>

struct _GabEditorView
{
  GtkBox parent_instance;
  char *project_path;
  char *current_file;
  gboolean dirty;

  GtkWidget *title_widget;
  GtkListBox *file_list;
  GtkSourceView *source_view;
  GtkSourceBuffer *buffer;
  GtkLabel *status;
  GSettings *settings;
};

G_DEFINE_TYPE (GabEditorView, gab_editor_view, GTK_TYPE_BOX)

static GabFormatOptionsC
current_format_opts (GabEditorView *self)
{
  GabFormatOptionsC opts;
  opts.tab_width = g_settings_get_int (self->settings, "tab-width");
  opts.insert_spaces = g_settings_get_boolean (self->settings, "insert-spaces");
  opts.trim_trailing = TRUE;
  opts.ensure_final_newline = TRUE;
  return opts;
}

static void
set_status (GabEditorView *self, const char *msg)
{
  gtk_label_set_text (self->status, msg);
}

static void
get_selection_lines (GtkTextBuffer *buffer, int *start_line, int *end_line)
{
  GtkTextIter start, end;
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      *start_line = gtk_text_iter_get_line (&start);
      *end_line = gtk_text_iter_get_line (&end);
      if (gtk_text_iter_starts_line (&end) && *end_line > *start_line)
        (*end_line)--;
    }
  else
    {
      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                        gtk_text_buffer_get_insert (buffer));
      *start_line = *end_line = gtk_text_iter_get_line (&iter);
    }
}

static char *
buffer_get_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  return gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
}

static void
buffer_set_text_preserve (GtkTextBuffer *buffer, const char *text)
{
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_set_text (buffer, text, -1);
  gtk_text_buffer_end_user_action (buffer);
}

static void
apply_language (GabEditorView *self, const char *path)
{
  char *lang_id = gab_project_detect_language (path);
  GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
  const char *id = "c";
  if (g_strcmp0 (lang_id, "cpp") == 0)
    id = "cpp";
  else if (g_strcmp0 (lang_id, "c") == 0)
    id = "c";
  else if (g_strcmp0 (lang_id, "python") == 0)
    id = "python";
  else if (g_strcmp0 (lang_id, "xml") == 0)
    id = "xml";
  else if (g_strcmp0 (lang_id, "css") == 0)
    id = "css";
  else if (g_strcmp0 (lang_id, "markdown") == 0)
    id = "markdown";
  else if (g_strcmp0 (lang_id, "json") == 0)
    id = "json";
  else if (g_strcmp0 (lang_id, "meson") == 0)
    id = "meson";
  GtkSourceLanguage *lang = gtk_source_language_manager_get_language (lm, id);
  gtk_source_buffer_set_language (self->buffer, lang);
  free (lang_id);
}

static gboolean
save_current_file (GabEditorView *self)
{
  if (self->current_file == NULL)
    {
      set_status (self, "No file open");
      return FALSE;
    }

  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  g_autoptr (GError) error = NULL;
  if (!g_file_set_contents (self->current_file, text, -1, &error))
    {
      set_status (self, error->message);
      return FALSE;
    }

  self->dirty = FALSE;
  g_autofree char *msg = g_strdup_printf ("Saved %s", self->current_file);
  set_status (self, msg);
  return TRUE;
}

static void
open_file (GabEditorView *self, const char *path)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize len = 0;
  if (!g_file_get_contents (path, &contents, &len, &error))
    {
      set_status (self, error->message);
      return;
    }

  g_free (self->current_file);
  self->current_file = g_strdup (path);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (self->buffer), contents, (gssize) len);
  apply_language (self, path);
  self->dirty = FALSE;

  g_autofree char *base = g_path_get_basename (path);
  g_autofree char *subtitle = g_strdup_printf ("%s — %s", base, self->project_path);
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->title_widget), subtitle);
  set_status (self, path);
}

static void
on_file_row_activated (GtkListBox *box, GtkListBoxRow *row, GabEditorView *self)
{
  (void) box;
  const char *path = g_object_get_data (G_OBJECT (row), "path");
  if (path != NULL)
    open_file (self, path);
}

static void
rebuild_file_tree (GabEditorView *self)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->file_list));
  while (child != NULL)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (self->file_list, child);
      child = next;
    }

  if (self->project_path == NULL)
    return;

  int count = 0;
  char **files = gab_project_list_files (self->project_path, &count);
  size_t root_len = strlen (self->project_path);
  for (int i = 0; i < count; i++)
    {
      const char *full = files[i];
      const char *rel = full;
      if (strncmp (full, self->project_path, root_len) == 0)
        {
          rel = full + root_len;
          if (*rel == '/')
            rel++;
        }

      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
      gtk_widget_set_margin_start (box, 8);
      gtk_widget_set_margin_end (box, 8);
      gtk_widget_set_margin_top (box, 4);
      gtk_widget_set_margin_bottom (box, 4);
      GtkWidget *icon = gtk_image_new_from_icon_name ("text-x-generic-symbolic");
      GtkWidget *label = gtk_label_new (rel);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_box_append (GTK_BOX (box), icon);
      gtk_box_append (GTK_BOX (box), label);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      g_object_set_data_full (G_OBJECT (row), "path", g_strdup (full), g_free);
      gtk_list_box_append (self->file_list, row);
    }
  gab_project_free_file_list (files, count);
}

static void
on_changed (GtkTextBuffer *buffer, GabEditorView *self)
{
  (void) buffer;
  self->dirty = TRUE;
}

static void
on_save (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  save_current_file (self);
}

static void
on_format_document (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  GabFormatOptionsC opts = current_format_opts (self);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_format_document (text, &opts);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Formatted document");
}

static void
on_format_selection (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  GabFormatOptionsC opts = current_format_opts (self);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_format_lines (text, start_line, end_line, &opts);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Formatted selected lines");
}

static void
on_indent (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  GabFormatOptionsC opts = current_format_opts (self);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_indent_lines (text, start_line, end_line, &opts, 0);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Indented lines");
}

static void
on_unindent (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  GabFormatOptionsC opts = current_format_opts (self);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_indent_lines (text, start_line, end_line, &opts, 1);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Unindented lines");
}

static void
on_trim (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_trim_trailing (text);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Trimmed trailing whitespace");
}

static void
on_sort_lines (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_sort_lines (text, start_line, end_line);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Sorted selected lines");
}

static void
on_toggle_comment (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_toggle_comments (text, start_line, end_line);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Toggled comments");
}

static void
on_go_home (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  if (G_IS_ACTION_MAP (root))
    g_action_group_activate_action (G_ACTION_GROUP (root), "go-home", NULL);
}

static gboolean
on_save_shortcut (GtkWidget *widget, GVariant *args, gpointer user_data)
{
  (void) widget;
  (void) args;
  save_current_file (GAB_EDITOR_VIEW (user_data));
  return TRUE;
}

static gboolean
on_format_shortcut (GtkWidget *widget, GVariant *args, gpointer user_data)
{
  (void) widget;
  (void) args;
  on_format_document (NULL, GAB_EDITOR_VIEW (user_data));
  return TRUE;
}

static GtkWidget *
tool_button (const char *label, GCallback cb, gpointer data)
{
  GtkWidget *btn = gtk_button_new_with_label (label);
  gtk_widget_add_css_class (btn, "flat");
  g_signal_connect (btn, "clicked", cb, data);
  return btn;
}

void
gab_editor_view_open_project (GabEditorView *self, const char *path)
{
  g_return_if_fail (GAB_IS_EDITOR_VIEW (self));
  g_free (self->project_path);
  self->project_path = g_strdup (path);
  g_free (self->current_file);
  self->current_file = NULL;
  rebuild_file_tree (self);

  g_autofree char *base = g_path_get_basename (path);
  adw_window_title_set_title (ADW_WINDOW_TITLE (self->title_widget), base);
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->title_widget), path);

  g_autofree char *main_c = g_build_filename (path, "src", "main.c", NULL);
  g_autofree char *meson = g_build_filename (path, "meson.build", NULL);
  if (g_file_test (main_c, G_FILE_TEST_EXISTS))
    open_file (self, main_c);
  else if (g_file_test (meson, G_FILE_TEST_EXISTS))
    open_file (self, meson);
  else
    {
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (self->buffer),
                                "/* Select a file from the tree */\n", -1);
      set_status (self, "Project opened");
    }
}

const char *
gab_editor_view_get_project_path (GabEditorView *self)
{
  g_return_val_if_fail (GAB_IS_EDITOR_VIEW (self), NULL);
  return self->project_path;
}

static void
gab_editor_view_dispose (GObject *object)
{
  GabEditorView *self = GAB_EDITOR_VIEW (object);
  g_clear_pointer (&self->project_path, g_free);
  g_clear_pointer (&self->current_file, g_free);
  g_clear_object (&self->settings);
  G_OBJECT_CLASS (gab_editor_view_parent_class)->dispose (object);
}

static void
gab_editor_view_class_init (GabEditorViewClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = gab_editor_view_dispose;
}

static void
gab_editor_view_init (GabEditorView *self)
{
  self->settings = g_settings_new (APP_ID);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
  self->title_widget = adw_window_title_new ("Editor", NULL);
  adw_header_bar_set_title_widget (header, self->title_widget);

  GtkWidget *home_btn = gtk_button_new_from_icon_name ("go-home-symbolic");
  gtk_widget_set_tooltip_text (home_btn, "Back to homescreen");
  g_signal_connect (home_btn, "clicked", G_CALLBACK (on_go_home), self);
  adw_header_bar_pack_start (header, home_btn);

  GtkWidget *save_btn = gtk_button_new_from_icon_name ("document-save-symbolic");
  gtk_widget_set_tooltip_text (save_btn, "Save (Ctrl+S)");
  g_signal_connect (save_btn, "clicked", G_CALLBACK (on_save), self);
  adw_header_bar_pack_end (header, save_btn);
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (header));

  GtkWidget *tools = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_add_css_class (tools, "toolbar");
  gtk_widget_set_margin_start (tools, 6);
  gtk_widget_set_margin_end (tools, 6);
  gtk_widget_set_margin_top (tools, 4);
  gtk_widget_set_margin_bottom (tools, 4);
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Format", G_CALLBACK (on_format_document), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Format Lines", G_CALLBACK (on_format_selection), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Indent", G_CALLBACK (on_indent), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Unindent", G_CALLBACK (on_unindent), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Trim", G_CALLBACK (on_trim), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Sort Lines", G_CALLBACK (on_sort_lines), self));
  gtk_box_append (GTK_BOX (tools),
                  tool_button ("Comment", G_CALLBACK (on_toggle_comment), self));
  gtk_box_append (GTK_BOX (self), tools);

  GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_vexpand (paned, TRUE);
  gtk_widget_set_hexpand (paned, TRUE);
  gtk_paned_set_resize_start_child (GTK_PANED (paned), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (paned), FALSE);
  gtk_box_append (GTK_BOX (self), paned);

  GtkWidget *sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (sidebar, 260, -1);
  GtkWidget *side_label = gtk_label_new ("Files");
  gtk_widget_add_css_class (side_label, "heading");
  gtk_widget_set_margin_start (side_label, 12);
  gtk_widget_set_margin_top (side_label, 10);
  gtk_widget_set_margin_bottom (side_label, 6);
  gtk_label_set_xalign (GTK_LABEL (side_label), 0.0);
  gtk_box_append (GTK_BOX (sidebar), side_label);

  GtkWidget *side_scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (side_scroll, TRUE);
  self->file_list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (self->file_list, GTK_SELECTION_SINGLE);
  g_signal_connect (self->file_list, "row-activated",
                    G_CALLBACK (on_file_row_activated), self);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (side_scroll),
                                 GTK_WIDGET (self->file_list));
  gtk_box_append (GTK_BOX (sidebar), side_scroll);
  gtk_paned_set_start_child (GTK_PANED (paned), sidebar);

  self->buffer = gtk_source_buffer_new (NULL);
  self->source_view = GTK_SOURCE_VIEW (gtk_source_view_new_with_buffer (self->buffer));
  gtk_source_view_set_show_line_numbers (
      self->source_view, g_settings_get_boolean (self->settings, "show-line-numbers"));
  gtk_source_view_set_auto_indent (
      self->source_view, g_settings_get_boolean (self->settings, "auto-indent"));
  gtk_source_view_set_tab_width (self->source_view,
                                 g_settings_get_int (self->settings, "tab-width"));
  gtk_source_view_set_insert_spaces_instead_of_tabs (
      self->source_view, g_settings_get_boolean (self->settings, "insert-spaces"));
  gtk_source_view_set_highlight_current_line (self->source_view, TRUE);

  g_autofree char *font = g_settings_get_string (self->settings, "editor-font");
  /* GSettings stores Pango font strings like "Monospace 12" */
  g_autoptr (PangoFontDescription) desc = pango_font_description_from_string (font);
  const char *family = pango_font_description_get_family (desc);
  int size_pt = pango_font_description_get_size (desc) / PANGO_SCALE;
  if (size_pt <= 0)
    size_pt = 12;
  g_autofree char *css =
      g_strdup_printf ("textview { font-family: %s; font-size: %dpt; }",
                       family ? family : "monospace", size_pt);
  GtkCssProvider *provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider, css);
  gtk_style_context_add_provider_for_display (
      gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  g_signal_connect (self->buffer, "changed", G_CALLBACK (on_changed), self);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll),
                                 GTK_WIDGET (self->source_view));
  gtk_paned_set_end_child (GTK_PANED (paned), scroll);
  gtk_paned_set_position (GTK_PANED (paned), 280);

  self->status = GTK_LABEL (gtk_label_new (""));
  gtk_widget_add_css_class (GTK_WIDGET (self->status), "dim-label");
  gtk_label_set_xalign (self->status, 0.0);
  gtk_widget_set_margin_start (GTK_WIDGET (self->status), 10);
  gtk_widget_set_margin_end (GTK_WIDGET (self->status), 10);
  gtk_widget_set_margin_top (GTK_WIDGET (self->status), 4);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self->status), 6);
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->status));

  GtkEventController *keys = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (keys),
                                     GTK_SHORTCUT_SCOPE_LOCAL);
  gtk_shortcut_controller_add_shortcut (
      GTK_SHORTCUT_CONTROLLER (keys),
      gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_s, GDK_CONTROL_MASK),
                        gtk_callback_action_new (on_save_shortcut, self, NULL)));
  gtk_shortcut_controller_add_shortcut (
      GTK_SHORTCUT_CONTROLLER (keys),
      gtk_shortcut_new (
          gtk_keyval_trigger_new (GDK_KEY_F, GDK_CONTROL_MASK | GDK_SHIFT_MASK),
          gtk_callback_action_new (on_format_shortcut, self, NULL)));
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
}

GtkWidget *
gab_editor_view_new (void)
{
  return g_object_new (GAB_TYPE_EDITOR_VIEW, NULL);
}
