#include "editor_view.h"
#include "build_runner_c.h"
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
  gboolean busy;

  GtkWidget *title_widget;
  GtkListBox *file_list;
  GtkSourceView *source_view;
  GtkSourceBuffer *buffer;
  GtkTextBuffer *build_log;
  GtkLabel *status;
  GtkWidget *build_btn;
  GtkWidget *run_btn;
  GSettings *settings;
  GSimpleActionGroup *actions;
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
  g_autofree char *base = g_path_get_basename (self->current_file);
  g_autofree char *msg = g_strdup_printf ("Saved %s", base);
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
  adw_window_title_set_title (ADW_WINDOW_TITLE (self->title_widget), base);
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->title_widget),
                                 self->project_path);
  set_status (self, "Right-click the editor for format tools");
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
      gtk_widget_set_margin_start (box, 10);
      gtk_widget_set_margin_end (box, 10);
      gtk_widget_set_margin_top (box, 6);
      gtk_widget_set_margin_bottom (box, 6);
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

/* ---- Tool actions (used by right-click menu + shortcuts) ---- */

static void
act_save (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  save_current_file (GAB_EDITOR_VIEW (user_data));
}

static void
act_format_document (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
  GabFormatOptionsC opts = current_format_opts (self);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_format_document (text, &opts);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Formatted document");
}

static void
act_format_lines (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
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
act_indent (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
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
act_unindent (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
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
act_trim (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_trim_trailing (text);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Trimmed trailing whitespace");
}

static void
act_sort_lines (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
  int start_line = 0, end_line = 0;
  get_selection_lines (GTK_TEXT_BUFFER (self->buffer), &start_line, &end_line);
  g_autofree char *text = buffer_get_text (GTK_TEXT_BUFFER (self->buffer));
  char *formatted = gab_sort_lines (text, start_line, end_line);
  buffer_set_text_preserve (GTK_TEXT_BUFFER (self->buffer), formatted);
  free (formatted);
  set_status (self, "Sorted selected lines");
}

static void
act_toggle_comment (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  GabEditorView *self = GAB_EDITOR_VIEW (user_data);
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

static void
on_save_clicked (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  save_current_file (self);
}

static void
append_build_log (GabEditorView *self, const char *text)
{
  if (text == NULL || *text == '\0')
    return;
  GtkTextIter end;
  gtk_text_buffer_get_end_iter (self->build_log, &end);
  gtk_text_buffer_insert (self->build_log, &end, text, -1);
  if (text[strlen (text) - 1] != '\n')
    gtk_text_buffer_insert (self->build_log, &end, "\n", -1);
}

static void
set_busy (GabEditorView *self, gboolean busy)
{
  self->busy = busy;
  gtk_widget_set_sensitive (self->build_btn, !busy);
  gtk_widget_set_sensitive (self->run_btn, !busy);
}

typedef struct {
  GabEditorView *self;
  char *log;
  char *error;
  gboolean ok;
  gboolean is_run;
} BuildJobResult;

static gboolean
on_build_job_done (gpointer user_data)
{
  BuildJobResult *res = user_data;
  GabEditorView *self = res->self;
  append_build_log (self, res->log);
  if (!res->ok && res->error)
    append_build_log (self, res->error);
  if (res->ok)
    set_status (self, res->is_run ? "App launched" : "Build succeeded");
  else
    set_status (self, res->is_run ? "Run failed — see build log" : "Build failed — see build log");
  set_busy (self, FALSE);
  free (res->log);
  free (res->error);
  g_object_unref (self);
  g_free (res);
  return G_SOURCE_REMOVE;
}

typedef struct {
  GabEditorView *self;
  char *project_path;
  gboolean is_run;
} BuildJob;

static gpointer
build_job_thread (gpointer user_data)
{
  BuildJob *job = user_data;
  char *log = NULL;
  char *error = NULL;
  gboolean ok;
  if (job->is_run)
    ok = gab_build_run (job->project_path, &log, &error);
  else
    ok = gab_build_compile (job->project_path, &log, &error);

  BuildJobResult *res = g_new0 (BuildJobResult, 1);
  res->self = job->self;
  res->log = log;
  res->error = error;
  res->ok = ok;
  res->is_run = job->is_run;
  g_idle_add (on_build_job_done, res);

  g_free (job->project_path);
  g_free (job);
  return NULL;
}

static void
start_build_job (GabEditorView *self, gboolean is_run)
{
  if (self->project_path == NULL)
    {
      set_status (self, "No project open");
      return;
    }
  if (self->busy)
    return;

  if (self->dirty)
    save_current_file (self);

  gtk_text_buffer_set_text (self->build_log, "", 0);
  append_build_log (self, is_run ? "=== Run ===" : "=== Build ===");
  set_busy (self, TRUE);
  set_status (self, is_run ? "Building and launching…" : "Building…");

  BuildJob *job = g_new0 (BuildJob, 1);
  job->self = g_object_ref (self);
  job->project_path = g_strdup (self->project_path);
  job->is_run = is_run;
  g_thread_new ("luma-build", build_job_thread, job);
}

static void
on_build_clicked (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  start_build_job (self, FALSE);
}

static void
on_run_clicked (GtkButton *btn, GabEditorView *self)
{
  (void) btn;
  start_build_job (self, TRUE);
}

static void
act_build (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  start_build_job (GAB_EDITOR_VIEW (user_data), FALSE);
}

static void
act_run (GSimpleAction *a, GVariant *p, gpointer user_data)
{
  (void) a;
  (void) p;
  start_build_job (GAB_EDITOR_VIEW (user_data), TRUE);
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
  act_format_document (NULL, NULL, user_data);
  return TRUE;
}

static gboolean
on_build_shortcut (GtkWidget *widget, GVariant *args, gpointer user_data)
{
  (void) widget;
  (void) args;
  start_build_job (GAB_EDITOR_VIEW (user_data), FALSE);
  return TRUE;
}

static gboolean
on_run_shortcut (GtkWidget *widget, GVariant *args, gpointer user_data)
{
  (void) widget;
  (void) args;
  start_build_job (GAB_EDITOR_VIEW (user_data), TRUE);
  return TRUE;
}

static GMenuModel *
build_tools_menu (void)
{
  GMenu *root = g_menu_new ();
  GMenu *format = g_menu_new ();
  GMenu *edit = g_menu_new ();

  g_menu_append (format, "Format Document", "editor.format-document");
  g_menu_append (format, "Format Selected Lines", "editor.format-lines");
  g_menu_append (format, "Indent Lines", "editor.indent");
  g_menu_append (format, "Unindent Lines", "editor.unindent");
  g_menu_append_section (root, "Format", G_MENU_MODEL (format));

  g_menu_append (edit, "Trim Trailing Space", "editor.trim");
  g_menu_append (edit, "Sort Selected Lines", "editor.sort-lines");
  g_menu_append (edit, "Toggle Comment", "editor.toggle-comment");
  g_menu_append_section (root, "Edit", G_MENU_MODEL (edit));

  g_menu_append (root, "Save", "editor.save");

  GMenu *build = g_menu_new ();
  g_menu_append (build, "Build Project", "editor.build");
  g_menu_append (build, "Run Project", "editor.run");
  g_menu_append_section (root, "Build", G_MENU_MODEL (build));
  g_object_unref (build);

  g_object_unref (format);
  g_object_unref (edit);
  return G_MENU_MODEL (root);
}

static void
install_editor_actions (GabEditorView *self)
{
  const GActionEntry entries[] = {
      { .name = "save", .activate = act_save },
      { .name = "format-document", .activate = act_format_document },
      { .name = "format-lines", .activate = act_format_lines },
      { .name = "indent", .activate = act_indent },
      { .name = "unindent", .activate = act_unindent },
      { .name = "trim", .activate = act_trim },
      { .name = "sort-lines", .activate = act_sort_lines },
      { .name = "toggle-comment", .activate = act_toggle_comment },
      { .name = "build", .activate = act_build },
      { .name = "run", .activate = act_run },
  };

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor",
                                  G_ACTION_GROUP (self->actions));
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
      set_status (self, "Project opened — right-click for tools");
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
  g_clear_object (&self->actions);
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
  install_editor_actions (self);

  AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
  self->title_widget = adw_window_title_new ("Editor", NULL);
  adw_header_bar_set_title_widget (header, self->title_widget);

  GtkWidget *home_btn = gtk_button_new_from_icon_name ("go-home-symbolic");
  gtk_widget_set_tooltip_text (home_btn, "Back to home");
  gtk_widget_add_css_class (home_btn, "flat");
  g_signal_connect (home_btn, "clicked", G_CALLBACK (on_go_home), self);
  adw_header_bar_pack_start (header, home_btn);

  /* Compact Tools menu (same actions as right-click) */
  GtkWidget *tools_btn = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (tools_btn), "open-menu-symbolic");
  gtk_widget_set_tooltip_text (tools_btn, "Tools (also available via right-click)");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (tools_btn), build_tools_menu ());
  adw_header_bar_pack_end (header, tools_btn);

  GtkWidget *save_btn = gtk_button_new_from_icon_name ("document-save-symbolic");
  gtk_widget_set_tooltip_text (save_btn, "Save (Ctrl+S)");
  gtk_widget_add_css_class (save_btn, "flat");
  g_signal_connect (save_btn, "clicked", G_CALLBACK (on_save_clicked), self);
  adw_header_bar_pack_end (header, save_btn);

  self->run_btn = gtk_button_new_from_icon_name ("media-playback-start-symbolic");
  gtk_widget_set_tooltip_text (self->run_btn, "Build & Run (F5)");
  gtk_widget_add_css_class (self->run_btn, "suggested-action");
  g_signal_connect (self->run_btn, "clicked", G_CALLBACK (on_run_clicked), self);
  adw_header_bar_pack_end (header, self->run_btn);

  self->build_btn = gtk_button_new_from_icon_name ("system-run-symbolic");
  gtk_widget_set_tooltip_text (self->build_btn, "Build (Ctrl+B)");
  gtk_widget_add_css_class (self->build_btn, "flat");
  g_signal_connect (self->build_btn, "clicked", G_CALLBACK (on_build_clicked), self);
  adw_header_bar_pack_end (header, self->build_btn);

  gtk_box_append (GTK_BOX (self), GTK_WIDGET (header));

  GtkWidget *outer = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_vexpand (outer, TRUE);
  gtk_widget_set_hexpand (outer, TRUE);
  gtk_box_append (GTK_BOX (self), outer);

  GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_vexpand (paned, TRUE);
  gtk_widget_set_hexpand (paned, TRUE);
  gtk_paned_set_resize_start_child (GTK_PANED (paned), FALSE);
  gtk_paned_set_shrink_start_child (GTK_PANED (paned), FALSE);
  gtk_paned_set_start_child (GTK_PANED (outer), paned);

  GtkWidget *sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (sidebar, 240, -1);
  gtk_widget_add_css_class (sidebar, "sidebar");
  GtkWidget *side_label = gtk_label_new ("Files");
  gtk_widget_add_css_class (side_label, "heading");
  gtk_widget_set_margin_start (side_label, 14);
  gtk_widget_set_margin_top (side_label, 12);
  gtk_widget_set_margin_bottom (side_label, 6);
  gtk_label_set_xalign (GTK_LABEL (side_label), 0.0);
  gtk_box_append (GTK_BOX (sidebar), side_label);

  GtkWidget *side_scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (side_scroll, TRUE);
  self->file_list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->file_list), "navigation-sidebar");
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

  /* Right-click context menu with format tools */
  gtk_text_view_set_extra_menu (GTK_TEXT_VIEW (self->source_view), build_tools_menu ());

  g_autofree char *font = g_settings_get_string (self->settings, "editor-font");
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
  gtk_paned_set_position (GTK_PANED (paned), 260);

  GtkWidget *log_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *log_label = gtk_label_new ("Build Log");
  gtk_widget_add_css_class (log_label, "heading");
  gtk_label_set_xalign (GTK_LABEL (log_label), 0.0);
  gtk_widget_set_margin_start (log_label, 10);
  gtk_widget_set_margin_top (log_label, 6);
  gtk_widget_set_margin_bottom (log_label, 4);
  gtk_box_append (GTK_BOX (log_box), log_label);

  self->build_log = gtk_text_buffer_new (NULL);
  GtkWidget *log_view = gtk_text_view_new_with_buffer (self->build_log);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (log_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (log_view), FALSE);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (log_view), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (log_view), GTK_WRAP_WORD_CHAR);
  GtkWidget *log_scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (log_scroll, TRUE);
  gtk_widget_set_size_request (log_scroll, -1, 120);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (log_scroll), log_view);
  gtk_box_append (GTK_BOX (log_box), log_scroll);
  gtk_paned_set_end_child (GTK_PANED (outer), log_box);
  gtk_paned_set_resize_end_child (GTK_PANED (outer), FALSE);
  gtk_paned_set_position (GTK_PANED (outer), 520);

  self->status = GTK_LABEL (gtk_label_new ("Build · Run · right-click for format tools"));
  gtk_widget_add_css_class (GTK_WIDGET (self->status), "dim-label");
  gtk_label_set_xalign (self->status, 0.0);
  gtk_widget_set_margin_start (GTK_WIDGET (self->status), 12);
  gtk_widget_set_margin_end (GTK_WIDGET (self->status), 12);
  gtk_widget_set_margin_top (GTK_WIDGET (self->status), 4);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self->status), 8);
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
  gtk_shortcut_controller_add_shortcut (
      GTK_SHORTCUT_CONTROLLER (keys),
      gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_b, GDK_CONTROL_MASK),
                        gtk_callback_action_new (on_build_shortcut, self, NULL)));
  gtk_shortcut_controller_add_shortcut (
      GTK_SHORTCUT_CONTROLLER (keys),
      gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F5, 0),
                        gtk_callback_action_new (on_run_shortcut, self, NULL)));
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
}

GtkWidget *
gab_editor_view_new (void)
{
  return g_object_new (GAB_TYPE_EDITOR_VIEW, NULL);
}
