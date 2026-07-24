#include "homescreen.h"
#include "project_manager_c.h"
#include "config.h"

#include <adwaita.h>
#include <stdlib.h>

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
show_message (GtkWidget *widget, const char *title, const char *message)
{
  GtkRoot *root = gtk_widget_get_root (widget);
  AdwDialog *dialog = adw_alert_dialog_new (title, message);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", "OK");
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");
  adw_dialog_present (dialog, GTK_WIDGET (root));
}

typedef struct {
  GabHomescreen *self;
  AdwDialog *dialog;
  AdwEntryRow *name_row;
  AdwEntryRow *dir_row;
  GtkListBox *tmpl_list;
  GabTemplateInfoC *templates;
  int template_count;
  GtkLabel *tmpl_desc;
  int selected_index;
} CreateDialogData;

static void
free_create_data (gpointer user_data)
{
  CreateDialogData *data = user_data;
  if (data == NULL)
    return;
  gab_project_free_templates (data->templates, data->template_count);
  g_free (data);
}

static void
on_template_row_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  (void) box;
  CreateDialogData *data = user_data;
  if (row == NULL || data->templates == NULL)
    return;
  int index = gtk_list_box_row_get_index (row);
  if (index < 0 || index >= data->template_count)
    return;
  data->selected_index = index;
  gtk_label_set_text (data->tmpl_desc, data->templates[index].description);
}

static CreateDialogData *
steal_create_data (AdwDialog *dialog)
{
  return g_object_steal_data (G_OBJECT (dialog), "create-data");
}

static void
on_create_clicked (GtkButton *btn, gpointer user_data)
{
  (void) btn;
  AdwDialog *dialog = ADW_DIALOG (user_data);
  CreateDialogData *data = steal_create_data (dialog);
  if (data == NULL)
    return;

  const char *name = gtk_editable_get_text (GTK_EDITABLE (data->name_row));
  const char *parent = gtk_editable_get_text (GTK_EDITABLE (data->dir_row));
  const char *tmpl = "gtk4-adwaita";
  if (data->templates != NULL && data->selected_index >= 0 &&
      data->selected_index < data->template_count)
    tmpl = data->templates[data->selected_index].id;

  if (name == NULL || *name == '\0')
    {
      /* Put data back so Cancel/close can still free it, and user can retry */
      g_object_set_data_full (G_OBJECT (dialog), "create-data", data, free_create_data);
      show_message (GTK_WIDGET (data->self), "New Project",
                    "Please enter a project name.");
      return;
    }

  char *out_path = NULL;
  char *out_error = NULL;
  if (!gab_project_create (name, parent, tmpl, &out_path, &out_error))
    {
      g_object_set_data_full (G_OBJECT (dialog), "create-data", data, free_create_data);
      show_message (GTK_WIDGET (data->self), "New Project",
                    out_error ? out_error : "Failed to create project");
      free (out_error);
      return;
    }

  GabHomescreen *self = data->self;
  free_create_data (data);
  adw_dialog_close (dialog);
  emit_action (self, "open-project", out_path);
  free (out_path);
}

static void
on_cancel_clicked (GtkButton *btn, gpointer user_data)
{
  (void) btn;
  AdwDialog *dialog = ADW_DIALOG (user_data);
  /* Destroy notify on create-data will free */
  adw_dialog_close (dialog);
}

static void
on_new_project (GtkButton *btn, GabHomescreen *self)
{
  (void) btn;
  int tmpl_count = 0;
  GabTemplateInfoC *templates = gab_project_list_templates (&tmpl_count);

  AdwDialog *dialog = adw_dialog_new ();
  adw_dialog_set_title (dialog, "New Project");
  adw_dialog_set_content_width (dialog, 560);
  adw_dialog_set_content_height (dialog, 640);
  adw_dialog_set_can_close (dialog, TRUE);

  GtkWidget *toolbar = adw_toolbar_view_new ();
  GtkWidget *header = adw_header_bar_new ();
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header), TRUE);
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), header);

  GtkWidget *outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start (outer, 16);
  gtk_widget_set_margin_end (outer, 16);
  gtk_widget_set_margin_top (outer, 8);
  gtk_widget_set_margin_bottom (outer, 16);

  GtkWidget *intro = gtk_label_new (
      "Click a template in the list below, then Create.");
  gtk_widget_add_css_class (intro, "dim-label");
  gtk_label_set_wrap (GTK_LABEL (intro), TRUE);
  gtk_label_set_xalign (GTK_LABEL (intro), 0.0);
  gtk_box_append (GTK_BOX (outer), intro);

  AdwEntryRow *name_row = ADW_ENTRY_ROW (adw_entry_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (name_row), "Project name");
  gtk_editable_set_text (GTK_EDITABLE (name_row), "my-gnome-app");
  gtk_widget_add_css_class (GTK_WIDGET (name_row), "card");

  char *def_dir = gab_project_default_dir ();
  AdwEntryRow *dir_row = ADW_ENTRY_ROW (adw_entry_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dir_row), "Parent folder");
  gtk_editable_set_text (GTK_EDITABLE (dir_row), def_dir);
  free (def_dir);
  gtk_widget_add_css_class (GTK_WIDGET (dir_row), "card");

  gtk_box_append (GTK_BOX (outer), GTK_WIDGET (name_row));
  gtk_box_append (GTK_BOX (outer), GTK_WIDGET (dir_row));

  GtkWidget *tmpl_title = gtk_label_new ("Template");
  gtk_widget_add_css_class (tmpl_title, "heading");
  gtk_label_set_xalign (GTK_LABEL (tmpl_title), 0.0);
  gtk_box_append (GTK_BOX (outer), tmpl_title);

  GtkWidget *desc = gtk_label_new (
      tmpl_count > 0 ? templates[0].description : "No templates available");
  gtk_widget_add_css_class (desc, "dim-label");
  gtk_label_set_wrap (GTK_LABEL (desc), TRUE);
  gtk_label_set_xalign (GTK_LABEL (desc), 0.0);
  gtk_box_append (GTK_BOX (outer), desc);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_widget_set_size_request (scroll, -1, 280);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  GtkWidget *list = gtk_list_box_new ();
  gtk_widget_add_css_class (list, "boxed-list");
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list), TRUE);

  for (int i = 0; i < tmpl_count; i++)
    {
      GtkWidget *row = gtk_list_box_row_new ();
      GtkWidget *row_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
      gtk_widget_set_margin_start (row_box, 12);
      gtk_widget_set_margin_end (row_box, 12);
      gtk_widget_set_margin_top (row_box, 10);
      gtk_widget_set_margin_bottom (row_box, 10);

      GtkWidget *title = gtk_label_new (templates[i].display_title);
      gtk_label_set_xalign (GTK_LABEL (title), 0.0);
      gtk_widget_add_css_class (title, "title-4");

      GtkWidget *sub = gtk_label_new (templates[i].description);
      gtk_label_set_xalign (GTK_LABEL (sub), 0.0);
      gtk_widget_add_css_class (sub, "dim-label");
      gtk_label_set_ellipsize (GTK_LABEL (sub), PANGO_ELLIPSIZE_END);

      gtk_box_append (GTK_BOX (row_box), title);
      gtk_box_append (GTK_BOX (row_box), sub);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), row_box);
      gtk_list_box_append (GTK_LIST_BOX (list), row);
    }

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list);
  gtk_box_append (GTK_BOX (outer), scroll);

  GtkWidget *actions = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign (actions, GTK_ALIGN_END);
  GtkWidget *cancel_btn = gtk_button_new_with_label ("Cancel");
  GtkWidget *create_btn = gtk_button_new_with_label ("Create");
  gtk_widget_add_css_class (create_btn, "suggested-action");
  gtk_box_append (GTK_BOX (actions), cancel_btn);
  gtk_box_append (GTK_BOX (actions), create_btn);
  gtk_box_append (GTK_BOX (outer), actions);

  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar), outer);
  adw_dialog_set_child (dialog, toolbar);

  CreateDialogData *data = g_new0 (CreateDialogData, 1);
  data->self = self;
  data->dialog = dialog;
  data->name_row = name_row;
  data->dir_row = dir_row;
  data->tmpl_list = GTK_LIST_BOX (list);
  data->templates = templates;
  data->template_count = tmpl_count;
  data->tmpl_desc = GTK_LABEL (desc);
  data->selected_index = 0;

  g_object_set_data_full (G_OBJECT (dialog), "create-data", data, free_create_data);

  if (tmpl_count > 0)
    {
      GtkListBoxRow *first = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), 0);
      if (first != NULL)
        gtk_list_box_select_row (GTK_LIST_BOX (list), first);
    }

  g_signal_connect (list, "row-selected", G_CALLBACK (on_template_row_selected), data);
  g_signal_connect (cancel_btn, "clicked", G_CALLBACK (on_cancel_clicked), dialog);
  g_signal_connect (create_btn, "clicked", G_CALLBACK (on_create_clicked), dialog);

  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  adw_dialog_present (dialog, GTK_WIDGET (root));
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
