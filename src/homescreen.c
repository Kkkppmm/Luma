#include "homescreen.h"
#include "project_manager_c.h"
#include "config.h"

#include <adwaita.h>
#include <stdlib.h>
#include <string.h>

struct _GabHomescreen
{
  GtkBox parent_instance;
  GtkWidget *recent_box;
  GtkWidget *status_label;
  GtkWidget *actions_box;
};

G_DEFINE_TYPE (GabHomescreen, gab_homescreen, GTK_TYPE_BOX)

enum {
  SIGNAL_ACTION,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
emit_action (GabHomescreen *self, const char *action, const char *payload)
{
  g_signal_emit (self, signals[SIGNAL_ACTION], 0, action, payload);
}

static void
show_message (GtkWidget *parent, const char *title, const char *message)
{
  GtkWidget *anchor = parent;
  if (anchor == NULL)
    return;
  /* Prefer an explicit window parent so alerts are not hidden behind a modal. */
  if (!GTK_IS_WINDOW (anchor))
    {
      GtkRoot *root = gtk_widget_get_root (anchor);
      if (root != NULL)
        anchor = GTK_WIDGET (root);
    }
  AdwDialog *dialog = adw_alert_dialog_new (title, message);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", "OK");
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");
  adw_dialog_present (dialog, anchor);
}

typedef struct {
  GabHomescreen *self;
  GtkWindow *window;
  AdwEntryRow *name_row;
  AdwEntryRow *dir_row;
  GtkListBox *tmpl_list;
  GtkWidget *search_entry;
  GabTemplateInfoC *templates;
  int template_count;
  GtkLabel *tmpl_desc;
  int selected_index;
  GCancellable *browse_cancellable;
  char *filter_text; /* owned, casefolded; used by list filter */
} CreateDialogData;

static void
free_create_data (gpointer user_data)
{
  CreateDialogData *data = user_data;
  if (data == NULL)
    return;
  if (data->browse_cancellable != NULL)
    {
      g_cancellable_cancel (data->browse_cancellable);
      g_object_unref (data->browse_cancellable);
    }
  g_free (data->filter_text);
  gab_project_free_templates (data->templates, data->template_count);
  g_free (data);
}

static gboolean
template_matches_filter (const GabTemplateInfoC *tmpl, const char *filter)
{
  if (filter == NULL || *filter == '\0')
    return TRUE;
  g_autofree char *hay1 =
      g_utf8_casefold (tmpl->display_title ? tmpl->display_title : "", -1);
  g_autofree char *hay2 =
      g_utf8_casefold (tmpl->description ? tmpl->description : "", -1);
  g_autofree char *hay3 =
      g_utf8_casefold (tmpl->category ? tmpl->category : "", -1);
  return strstr (hay1, filter) != NULL || strstr (hay2, filter) != NULL ||
         strstr (hay3, filter) != NULL;
}

static gboolean
template_list_filter (GtkListBoxRow *row, gpointer user_data)
{
  CreateDialogData *data = user_data;
  int index;
  if (data == NULL || row == NULL)
    return FALSE;
  index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "template-index")) - 1;
  if (index < 0 || index >= data->template_count)
    return FALSE;
  return template_matches_filter (&data->templates[index], data->filter_text);
}

static int
template_row_catalog_index (GtkListBoxRow *row)
{
  if (row == NULL)
    return -1;
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "template-index")) - 1;
}

static void
update_template_description (CreateDialogData *data)
{
  if (data == NULL || data->tmpl_desc == NULL || !GTK_IS_LABEL (data->tmpl_desc))
    return;
  if (data->templates != NULL && data->selected_index >= 0 &&
      data->selected_index < data->template_count)
    {
      gtk_label_set_text (data->tmpl_desc,
                          data->templates[data->selected_index].description);
    }
  else
    {
      gtk_label_set_text (data->tmpl_desc, "No templates match your search.");
    }
}

static void
select_template_row (CreateDialogData *data, GtkListBoxRow *row)
{
  int index;
  if (data == NULL || data->window == NULL)
    return;
  index = template_row_catalog_index (row);
  if (index < 0 || index >= data->template_count)
    {
      data->selected_index = -1;
      update_template_description (data);
      return;
    }
  data->selected_index = index;
  update_template_description (data);
}

static void
on_template_row_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  CreateDialogData *data = user_data;
  (void) box;
  /* During window teardown ListBox emits row-selected(NULL); ignore it. */
  if (data == NULL || row == NULL)
    return;
  select_template_row (data, row);
}

static void
on_template_row_activated (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  CreateDialogData *data = user_data;
  (void) box;
  if (data == NULL || row == NULL)
    return;
  gtk_list_box_select_row (data->tmpl_list, row);
  select_template_row (data, row);
}

static void
ensure_visible_selection (CreateDialogData *data)
{
  GtkListBoxRow *selected;
  GtkListBoxRow *row;
  int i;

  if (data == NULL || data->tmpl_list == NULL)
    return;

  selected = gtk_list_box_get_selected_row (data->tmpl_list);
  if (selected != NULL && gtk_widget_get_mapped (GTK_WIDGET (selected)))
    {
      select_template_row (data, selected);
      return;
    }

  /* Prefer previously selected catalog index if still visible. */
  if (data->selected_index >= 0)
    {
      for (i = 0; (row = gtk_list_box_get_row_at_index (data->tmpl_list, i)) != NULL; i++)
        {
          if (template_row_catalog_index (row) == data->selected_index &&
              gtk_widget_get_child_visible (GTK_WIDGET (row)))
            {
              gtk_list_box_select_row (data->tmpl_list, row);
              select_template_row (data, row);
              return;
            }
        }
    }

  /* Otherwise select first visible filtered row. */
  for (i = 0; (row = gtk_list_box_get_row_at_index (data->tmpl_list, i)) != NULL; i++)
    {
      if (gtk_widget_get_child_visible (GTK_WIDGET (row)))
        {
          gtk_list_box_select_row (data->tmpl_list, row);
          select_template_row (data, row);
          return;
        }
    }

  data->selected_index = -1;
  update_template_description (data);
}

static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
  CreateDialogData *data = user_data;
  const char *text;
  if (data == NULL)
    return;
  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_free (data->filter_text);
  data->filter_text =
      (text != NULL && *text != '\0') ? g_utf8_casefold (text, -1) : NULL;
  gtk_list_box_invalidate_filter (data->tmpl_list);
  ensure_visible_selection (data);
}

static void
on_browse_folder_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
  CreateDialogData *data = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  if (data == NULL)
    return;
  file = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source), res, &error);
  if (file == NULL)
    return;
  if (g_cancellable_is_cancelled (data->browse_cancellable))
    return;
  g_autofree char *path = g_file_get_path (file);
  if (path != NULL)
    gtk_editable_set_text (GTK_EDITABLE (data->dir_row), path);
}

static void
on_browse_folder (GtkButton *btn, gpointer user_data)
{
  (void) btn;
  CreateDialogData *data = user_data;
  GtkFileDialog *dialog;
  const char *cur;
  if (data == NULL || data->window == NULL)
    return;
  if (data->browse_cancellable == NULL)
    data->browse_cancellable = g_cancellable_new ();
  else
    g_cancellable_reset (data->browse_cancellable);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Choose projects folder");
  cur = gtk_editable_get_text (GTK_EDITABLE (data->dir_row));
  if (cur && *cur)
    {
      g_autoptr (GFile) initial = g_file_new_for_path (cur);
      gtk_file_dialog_set_initial_folder (dialog, initial);
    }
  gtk_file_dialog_select_folder (dialog, data->window, data->browse_cancellable,
                                 on_browse_folder_done, data);
}

typedef struct {
  GabHomescreen *self;
  GtkWindow *window;
  char *path;
} FinishCreateData;

static gboolean
finish_create_idle (gpointer user_data)
{
  FinishCreateData *finish = user_data;
  GabHomescreen *self;
  char *path;
  CreateDialogData *data;

  if (finish == NULL)
    return G_SOURCE_REMOVE;

  self = finish->self;
  path = finish->path;

  if (finish->window != NULL)
    {
      data = g_object_get_data (G_OBJECT (finish->window), "create-data");
      if (data != NULL)
        {
          if (data->tmpl_list != NULL)
            g_signal_handlers_disconnect_by_data (data->tmpl_list, data);
          if (data->search_entry != NULL)
            g_signal_handlers_disconnect_by_data (data->search_entry, data);
        }
      gtk_window_destroy (finish->window);
    }
  g_free (finish);

  if (self != NULL && path != NULL)
    emit_action (self, "open-project", path);
  free (path);
  return G_SOURCE_REMOVE;
}

static void
on_create_clicked (GtkButton *btn, gpointer user_data)
{
  (void) btn;
  CreateDialogData *data = user_data;
  const char *name;
  const char *parent;
  const char *tmpl = "gtk4-adwaita";
  char *out_path = NULL;
  char *out_error = NULL;
  FinishCreateData *finish;

  if (data == NULL)
    return;

  name = gtk_editable_get_text (GTK_EDITABLE (data->name_row));
  parent = gtk_editable_get_text (GTK_EDITABLE (data->dir_row));
  if (data->templates != NULL && data->selected_index >= 0 &&
      data->selected_index < data->template_count)
    tmpl = data->templates[data->selected_index].id;

  if (name == NULL || *name == '\0')
    {
      show_message (GTK_WIDGET (data->window), "New Project",
                    "Please enter a project name.");
      return;
    }
  if (data->selected_index < 0)
    {
      show_message (GTK_WIDGET (data->window), "New Project",
                    "Please select a template from the list.");
      return;
    }

  if (!gab_project_create (name, parent, tmpl, &out_path, &out_error))
    {
      show_message (GTK_WIDGET (data->window), "New Project",
                    out_error ? out_error : "Failed to create project");
      free (out_error);
      return;
    }

  /* Destroy the modal and open the project after this click handler returns.
   * Destroying the window mid-signal crashes GTK (seen as GPF in libgtk). */
  finish = g_new0 (FinishCreateData, 1);
  finish->self = data->self;
  finish->window = data->window;
  finish->path = out_path;
  gtk_widget_set_sensitive (GTK_WIDGET (data->window), FALSE);
  g_idle_add (finish_create_idle, finish);
}

static void
on_cancel_clicked (GtkButton *btn, gpointer user_data)
{
  (void) btn;
  CreateDialogData *data = user_data;
  if (data == NULL || data->window == NULL)
    return;
  gtk_window_destroy (data->window);
}

static GtkWidget *
make_template_row (const GabTemplateInfoC *tmpl, int catalog_index)
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *row_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget *title;
  GtkWidget *sub;
  GtkWidget *cat;

  gtk_widget_set_margin_start (row_box, 12);
  gtk_widget_set_margin_end (row_box, 12);
  gtk_widget_set_margin_top (row_box, 10);
  gtk_widget_set_margin_bottom (row_box, 10);

  title = gtk_label_new (tmpl->display_title);
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  gtk_widget_add_css_class (title, "title-4");

  cat = gtk_label_new (tmpl->category);
  gtk_label_set_xalign (GTK_LABEL (cat), 0.0);
  gtk_widget_add_css_class (cat, "caption");
  gtk_widget_add_css_class (cat, "accent");

  sub = gtk_label_new (tmpl->description);
  gtk_label_set_xalign (GTK_LABEL (sub), 0.0);
  gtk_widget_add_css_class (sub, "dim-label");
  gtk_label_set_ellipsize (GTK_LABEL (sub), PANGO_ELLIPSIZE_END);

  gtk_box_append (GTK_BOX (row_box), title);
  gtk_box_append (GTK_BOX (row_box), cat);
  gtk_box_append (GTK_BOX (row_box), sub);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), row_box);
  g_object_set_data (G_OBJECT (row), "template-index",
                     GINT_TO_POINTER (catalog_index + 1));
  return row;
}

static void
on_new_project (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  int tmpl_count = 0;
  GabTemplateInfoC *templates = gab_project_list_templates (&tmpl_count);
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  GtkApplication *app = NULL;
  GtkWidget *win_w;
  GtkWindow *window;
  GtkWidget *toolbar;
  AdwHeaderBar *header;
  GtkWidget *outer;
  GtkWidget *intro;
  AdwPreferencesGroup *meta;
  AdwEntryRow *name_row;
  AdwEntryRow *dir_row;
  char *def_dir;
  GtkWidget *browse;
  GtkWidget *search;
  GtkWidget *desc;
  GtkWidget *scroll;
  GtkWidget *list;
  GtkWidget *actions;
  GtkWidget *cancel_btn;
  GtkWidget *create_btn;
  CreateDialogData *data;
  int i;

  if (GTK_IS_WINDOW (root))
    app = gtk_window_get_application (GTK_WINDOW (root));

  win_w = adw_window_new ();
  window = GTK_WINDOW (win_w);
  gtk_window_set_title (window, "New Project");
  gtk_window_set_default_size (window, 640, 760);
  gtk_window_set_modal (window, TRUE);
  if (GTK_IS_WINDOW (root))
    gtk_window_set_transient_for (window, GTK_WINDOW (root));
  if (app != NULL)
    gtk_window_set_application (window, app);

  toolbar = adw_toolbar_view_new ();
  header = ADW_HEADER_BAR (adw_header_bar_new ());
  adw_header_bar_set_title_widget (
      header, adw_window_title_new ("New Project", "Pick a starter template"));
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), GTK_WIDGET (header));

  outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start (outer, 18);
  gtk_widget_set_margin_end (outer, 18);
  gtk_widget_set_margin_top (outer, 10);
  gtk_widget_set_margin_bottom (outer, 18);

  intro = gtk_label_new (
      "Click a template in the list to select it. Use search to filter by name or category.");
  gtk_widget_add_css_class (intro, "dim-label");
  gtk_label_set_wrap (GTK_LABEL (intro), TRUE);
  gtk_label_set_xalign (GTK_LABEL (intro), 0.0);
  gtk_box_append (GTK_BOX (outer), intro);

  meta = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  name_row = ADW_ENTRY_ROW (adw_entry_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Project name");
  adw_entry_row_set_show_apply_button (name_row, FALSE);
  gtk_editable_set_text (GTK_EDITABLE (name_row), "my-gnome-app");
  adw_preferences_group_add (meta, GTK_WIDGET (name_row));

  def_dir = gab_project_default_dir ();
  dir_row = ADW_ENTRY_ROW (adw_entry_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dir_row), "Parent folder");
  adw_entry_row_set_show_apply_button (dir_row, FALSE);
  gtk_editable_set_text (GTK_EDITABLE (dir_row), def_dir);
  free (def_dir);
  browse = gtk_button_new_from_icon_name ("folder-symbolic");
  gtk_widget_set_valign (browse, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (browse, "Browse for folder");
  gtk_widget_add_css_class (browse, "flat");
  adw_entry_row_add_suffix (dir_row, browse);
  adw_preferences_group_add (meta, GTK_WIDGET (dir_row));
  gtk_box_append (GTK_BOX (outer), GTK_WIDGET (meta));

  search = gtk_search_entry_new ();
  gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (search),
                                         "Search templates…");
  gtk_widget_set_hexpand (search, TRUE);
  gtk_box_append (GTK_BOX (outer), search);

  desc = gtk_label_new (tmpl_count > 0 ? templates[0].description
                                       : "No templates available");
  gtk_widget_add_css_class (desc, "dim-label");
  gtk_label_set_wrap (GTK_LABEL (desc), TRUE);
  gtk_label_set_xalign (GTK_LABEL (desc), 0.0);
  gtk_box_append (GTK_BOX (outer), desc);

  scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_size_request (scroll, -1, 340);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  list = gtk_list_box_new ();
  gtk_widget_add_css_class (list, "boxed-list");
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list), TRUE);

  for (i = 0; i < tmpl_count; i++)
    gtk_list_box_append (GTK_LIST_BOX (list), make_template_row (&templates[i], i));

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list);
  gtk_box_append (GTK_BOX (outer), scroll);

  actions = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign (actions, GTK_ALIGN_END);
  cancel_btn = gtk_button_new_with_label ("Cancel");
  create_btn = gtk_button_new_with_label ("Create");
  gtk_widget_add_css_class (create_btn, "suggested-action");
  gtk_box_append (GTK_BOX (actions), cancel_btn);
  gtk_box_append (GTK_BOX (actions), create_btn);
  gtk_box_append (GTK_BOX (outer), actions);

  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar), outer);
  adw_window_set_content (ADW_WINDOW (window), toolbar);

  data = g_new0 (CreateDialogData, 1);
  data->self = self;
  data->window = window;
  data->name_row = name_row;
  data->dir_row = dir_row;
  data->tmpl_list = GTK_LIST_BOX (list);
  data->search_entry = search;
  data->templates = templates;
  data->template_count = tmpl_count;
  data->tmpl_desc = GTK_LABEL (desc);
  data->selected_index = tmpl_count > 0 ? 0 : -1;
  data->browse_cancellable = g_cancellable_new ();
  data->filter_text = NULL;

  g_object_set_data_full (G_OBJECT (window), "create-data", data, free_create_data);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (list), template_list_filter, data, NULL);

  if (tmpl_count > 0)
    {
      GtkListBoxRow *first = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), 0);
      if (first != NULL)
        gtk_list_box_select_row (GTK_LIST_BOX (list), first);
    }

  g_signal_connect (list, "row-selected", G_CALLBACK (on_template_row_selected), data);
  g_signal_connect (list, "row-activated", G_CALLBACK (on_template_row_activated), data);
  g_signal_connect (search, "search-changed", G_CALLBACK (on_search_changed), data);
  g_signal_connect (browse, "clicked", G_CALLBACK (on_browse_folder), data);
  g_signal_connect (cancel_btn, "clicked", G_CALLBACK (on_cancel_clicked), data);
  g_signal_connect (create_btn, "clicked", G_CALLBACK (on_create_clicked), data);
  gtk_window_set_default_widget (window, create_btn);

  gtk_window_present (window);
  gtk_widget_grab_focus (GTK_WIDGET (name_row));
}

static void
on_open_folder_finish (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GabHomescreen *self = GAB_HOMESCREEN (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file =
      gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source), res, &error);
  if (file == NULL)
    return;
  g_autofree char *path = g_file_get_path (file);
  if (path != NULL)
    emit_action (self, "open-project", path);
}

static void
on_open_project_clicked (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open Project Folder");
  gtk_file_dialog_select_folder (dialog, GTK_WINDOW (root), NULL,
                                 on_open_folder_finish, self);
}

static void
on_show_sdk (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  emit_action (self, "show-sdk", NULL);
}

static void
on_show_help (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  emit_action (self, "show-help", NULL);
}

static void
on_show_docs (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  emit_action (self, "show-docs", NULL);
}

static void
on_show_about (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  emit_action (self, "show-about", NULL);
}

static void
on_recent_clicked (GtkButton *btn, GabHomescreen *self)
{
  const char *path = g_object_get_data (G_OBJECT (btn), "path");
  if (path != NULL)
    emit_action (self, "open-project", path);
}

static GtkWidget *
make_action_card (const char *icon, const char *title, const char *subtitle,
                  GCallback callback, gpointer data)
{
  GtkWidget *btn = gtk_button_new ();
  gtk_widget_add_css_class (btn, "card");
  gtk_widget_set_hexpand (btn, TRUE);

  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_margin_start (row, 16);
  gtk_widget_set_margin_end (row, 16);
  gtk_widget_set_margin_top (row, 14);
  gtk_widget_set_margin_bottom (row, 14);

  GtkWidget *image = gtk_image_new_from_icon_name (icon);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);

  GtkWidget *texts = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand (texts, TRUE);
  GtkWidget *t = gtk_label_new (title);
  gtk_widget_add_css_class (t, "title-3");
  gtk_label_set_xalign (GTK_LABEL (t), 0.0);
  GtkWidget *s = gtk_label_new (subtitle);
  gtk_widget_add_css_class (s, "dim-label");
  gtk_label_set_xalign (GTK_LABEL (s), 0.0);
  gtk_box_append (GTK_BOX (texts), t);
  gtk_box_append (GTK_BOX (texts), s);

  gtk_box_append (GTK_BOX (row), image);
  gtk_box_append (GTK_BOX (row), texts);
  gtk_button_set_child (GTK_BUTTON (btn), row);
  g_signal_connect (btn, "clicked", callback, data);
  return btn;
}

void
gab_homescreen_refresh (GabHomescreen *self)
{
  g_return_if_fail (GAB_IS_HOMESCREEN (self));

  GtkWidget *child = gtk_widget_get_first_child (self->recent_box);
  while (child != NULL)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_box_remove (GTK_BOX (self->recent_box), child);
      child = next;
    }

  g_autoptr (GSettings) settings = g_settings_new (APP_ID);
  g_autofree char **recents = g_settings_get_strv (settings, "recent-projects");
  guint shown = 0;
  for (guint i = 0; recents != NULL && recents[i] != NULL; i++)
    {
      if (!g_file_test (recents[i], G_FILE_TEST_IS_DIR))
        continue;

      g_autofree char *name = g_path_get_basename (recents[i]);
      GtkWidget *btn = gtk_button_new ();
      gtk_widget_add_css_class (btn, "flat");

      GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_set_margin_start (row, 8);
      gtk_widget_set_margin_end (row, 8);
      gtk_widget_set_margin_top (row, 6);
      gtk_widget_set_margin_bottom (row, 6);

      GtkWidget *icon = gtk_image_new_from_icon_name ("folder-symbolic");
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_hexpand (box, TRUE);
      GtkWidget *title = gtk_label_new (name);
      gtk_label_set_xalign (GTK_LABEL (title), 0);
      gtk_widget_add_css_class (title, "heading");
      GtkWidget *sub = gtk_label_new (recents[i]);
      gtk_label_set_xalign (GTK_LABEL (sub), 0);
      gtk_widget_add_css_class (sub, "dim-label");
      gtk_label_set_ellipsize (GTK_LABEL (sub), PANGO_ELLIPSIZE_MIDDLE);
      gtk_box_append (GTK_BOX (box), title);
      gtk_box_append (GTK_BOX (box), sub);
      gtk_box_append (GTK_BOX (row), icon);
      gtk_box_append (GTK_BOX (row), box);
      gtk_button_set_child (GTK_BUTTON (btn), row);

      g_object_set_data_full (G_OBJECT (btn), "path", g_strdup (recents[i]), g_free);
      g_signal_connect (btn, "clicked", G_CALLBACK (on_recent_clicked), self);
      gtk_box_append (GTK_BOX (self->recent_box), btn);
      shown++;
    }

  if (shown == 0)
    {
      GtkWidget *empty =
          gtk_label_new ("No recent projects yet. Create one to get started.");
      gtk_widget_add_css_class (empty, "dim-label");
      gtk_widget_set_margin_top (empty, 12);
      gtk_widget_set_margin_bottom (empty, 12);
      gtk_widget_set_margin_start (empty, 12);
      gtk_box_append (GTK_BOX (self->recent_box), empty);
    }

  gtk_label_set_text (GTK_LABEL (self->status_label),
                      "Ready · Luma Builder · C/C++ · GTK4");
}

static void
gab_homescreen_class_init (GabHomescreenClass *klass)
{
  signals[SIGNAL_ACTION] =
      g_signal_new ("action", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                    NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}

static void
gab_homescreen_init (GabHomescreen *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
  GtkWidget *title = adw_window_title_new (APP_NAME, "Home");
  adw_header_bar_set_title_widget (header, title);

  /* Primary menu for smooth access to secondary windows */
  GMenu *menu = g_menu_new ();
  g_menu_append (menu, "SDK Manager", "app.sdk");
  g_menu_append (menu, "Check for Updates", "app.check-updates");
  g_menu_append (menu, "Help", "app.help");
  g_menu_append (menu, "Docs", "app.docs");
  g_menu_append (menu, "About Luma Builder", "app.about");
  g_menu_append (menu, "Quit", "app.quit");
  GtkWidget *menu_btn = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (menu_btn), "open-menu-symbolic");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (menu_btn), G_MENU_MODEL (menu));
  g_object_unref (menu);
  adw_header_bar_pack_end (header, menu_btn);
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (header));

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_box_append (GTK_BOX (self), scroll);

  GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
  gtk_widget_set_halign (content, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (content, GTK_ALIGN_START);
  gtk_widget_set_size_request (content, 720, -1);
  gtk_widget_set_margin_top (content, 28);
  gtk_widget_set_margin_bottom (content, 28);
  gtk_widget_set_margin_start (content, 24);
  gtk_widget_set_margin_end (content, 24);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), content);

  GtkWidget *hero = gtk_label_new ("Build with Luma");
  gtk_widget_add_css_class (hero, "title-1");
  gtk_label_set_xalign (GTK_LABEL (hero), 0.0);
  GtkWidget *hero_sub =
      gtk_label_new ("Create GNOME apps, manage SDKs, and edit C/C++ smoothly.");
  gtk_widget_add_css_class (hero_sub, "dim-label");
  gtk_label_set_xalign (GTK_LABEL (hero_sub), 0.0);
  gtk_box_append (GTK_BOX (content), hero);
  gtk_box_append (GTK_BOX (content), hero_sub);

  self->actions_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_box_append (GTK_BOX (self->actions_box),
                  make_action_card ("document-new-symbolic", "New Project",
                                    "Choose from 30+ GNOME, GTK, console, and library templates",
                                    G_CALLBACK (on_new_project), self));
  gtk_box_append (GTK_BOX (self->actions_box),
                  make_action_card ("document-open-symbolic", "Open Project",
                                    "Open an existing project folder",
                                    G_CALLBACK (on_open_project_clicked), self));
  gtk_box_append (GTK_BOX (self->actions_box),
                  make_action_card ("folder-download-symbolic", "SDK Manager",
                                    "Download GNOME Platform, Sdk, and API docs",
                                    G_CALLBACK (on_show_sdk), self));
  gtk_box_append (GTK_BOX (content), self->actions_box);

  GtkWidget *recent_title = gtk_label_new ("Recent Projects");
  gtk_widget_add_css_class (recent_title, "title-3");
  gtk_label_set_xalign (GTK_LABEL (recent_title), 0.0);
  gtk_widget_set_margin_top (recent_title, 12);
  gtk_box_append (GTK_BOX (content), recent_title);

  self->recent_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class (self->recent_box, "card");
  gtk_box_append (GTK_BOX (content), self->recent_box);

  GtkWidget *links = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_top (links, 8);
  GtkWidget *help_btn = gtk_button_new_with_label ("Help");
  GtkWidget *docs_btn = gtk_button_new_with_label ("Docs");
  GtkWidget *about_btn = gtk_button_new_with_label ("About");
  gtk_widget_add_css_class (help_btn, "pill");
  gtk_widget_add_css_class (docs_btn, "pill");
  gtk_widget_add_css_class (about_btn, "pill");
  g_signal_connect (help_btn, "clicked", G_CALLBACK (on_show_help), self);
  g_signal_connect (docs_btn, "clicked", G_CALLBACK (on_show_docs), self);
  g_signal_connect (about_btn, "clicked", G_CALLBACK (on_show_about), self);
  gtk_box_append (GTK_BOX (links), help_btn);
  gtk_box_append (GTK_BOX (links), docs_btn);
  gtk_box_append (GTK_BOX (links), about_btn);
  gtk_box_append (GTK_BOX (content), links);

  self->status_label = gtk_label_new ("");
  gtk_widget_add_css_class (self->status_label, "dim-label");
  gtk_label_set_xalign (GTK_LABEL (self->status_label), 0.0);
  gtk_box_append (GTK_BOX (content), self->status_label);

  gab_homescreen_refresh (self);
}

GtkWidget *
gab_homescreen_new (void)
{
  return g_object_new (GAB_TYPE_HOMESCREEN, NULL);
}
