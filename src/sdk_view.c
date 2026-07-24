#include "sdk_view.h"
#include "sdk_manager_c.h"
#include "config.h"

#include <adwaita.h>
#include <stdlib.h>
#include <string.h>

struct _GabSdkView
{
  GtkBox parent_instance;
  GtkWidget *list_box;
  GtkWidget *log_view;
  GtkWidget *status;
  GabSdkPackageC *packages;
  int package_count;
};

G_DEFINE_TYPE (GabSdkView, gab_sdk_view, GTK_TYPE_BOX)

typedef struct {
  GabSdkView *self;
  char *id;
  char *branch;
  gboolean uninstall;
} InstallJob;

typedef struct {
  GabSdkView *self;
  char *message;
  gboolean ok;
} InstallDone;

static void
append_log (GabSdkView *self, const char *text)
{
  GtkTextBuffer *buffer =
      gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->log_view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_insert (buffer, &end, text, -1);
  if (text[strlen (text) - 1] != '\n')
    gtk_text_buffer_insert (buffer, &end, "\n", -1);
}

static void
clear_rows (GabSdkView *self)
{
  GtkWidget *child = gtk_widget_get_first_child (self->list_box);
  while (child != NULL)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_list_box_remove (GTK_LIST_BOX (self->list_box), child);
      child = next;
    }
}

static gboolean
on_install_done (gpointer user_data)
{
  InstallDone *done = user_data;
  append_log (done->self, done->message);
  gtk_label_set_text (GTK_LABEL (done->self->status),
                      done->ok ? "Operation finished" : "Operation failed");
  gab_sdk_view_refresh (done->self);
  g_free (done->message);
  g_free (done);
  return G_SOURCE_REMOVE;
}

static gpointer
install_thread (gpointer data)
{
  InstallJob *job = data;
  char *error = NULL;
  gboolean ok = job->uninstall
                    ? gab_sdk_uninstall (job->id, job->branch, &error)
                    : gab_sdk_install (job->id, job->branch, &error);

  InstallDone *done = g_new0 (InstallDone, 1);
  done->self = job->self;
  done->ok = ok;
  done->message = ok ? g_strdup_printf ("OK: %s//%s", job->id, job->branch)
                     : g_strdup_printf ("FAILED: %s", error ? error : "unknown");
  free (error);

  g_idle_add (on_install_done, done);
  g_free (job->id);
  g_free (job->branch);
  g_free (job);
  return NULL;
}

static void
start_job (GabSdkView *self, const char *id, const char *branch, gboolean uninstall)
{
  if (!gab_sdk_flatpak_available ())
    {
      append_log (self, "flatpak is not available on this system.");
      gtk_label_set_text (GTK_LABEL (self->status), "flatpak missing");
      return;
    }

  g_autofree char *msg =
      g_strdup_printf ("%s %s//%s …", uninstall ? "Uninstalling" : "Installing", id,
                       branch);
  append_log (self, msg);
  gtk_label_set_text (GTK_LABEL (self->status), msg);

  InstallJob *job = g_new0 (InstallJob, 1);
  job->self = self;
  job->id = g_strdup (id);
  job->branch = g_strdup (branch);
  job->uninstall = uninstall;
  g_thread_new ("sdk-install", install_thread, job);
}

static void
on_install_clicked (GtkButton *btn, GabSdkView *self)
{
  const char *id = g_object_get_data (G_OBJECT (btn), "id");
  const char *branch = g_object_get_data (G_OBJECT (btn), "branch");
  gboolean installed = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (btn), "installed"));
  if (id && branch)
    start_job (self, id, branch, installed);
}

void
gab_sdk_view_refresh (GabSdkView *self)
{
  g_return_if_fail (GAB_IS_SDK_VIEW (self));

  if (self->packages)
    {
      gab_sdk_free_catalog (self->packages, self->package_count);
      self->packages = NULL;
      self->package_count = 0;
    }

  clear_rows (self);
  self->packages = gab_sdk_catalog (&self->package_count);

  for (int i = 0; i < self->package_count; i++)
    {
      GabSdkPackageC *pkg = &self->packages[i];
      GtkWidget *row = gtk_list_box_row_new ();
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_set_margin_start (box, 12);
      gtk_widget_set_margin_end (box, 12);
      gtk_widget_set_margin_top (box, 10);
      gtk_widget_set_margin_bottom (box, 10);

      GtkWidget *texts = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
      gtk_widget_set_hexpand (texts, TRUE);
      g_autofree char *title =
          g_strdup_printf ("%s (%s)", pkg->name, pkg->branch);
      GtkWidget *t = gtk_label_new (title);
      gtk_widget_add_css_class (t, "heading");
      gtk_label_set_xalign (GTK_LABEL (t), 0);
      g_autofree char *sub =
          g_strdup_printf ("%s · %s · %s", pkg->id, pkg->kind,
                           pkg->installed ? "installed" : "not installed");
      GtkWidget *s = gtk_label_new (sub);
      gtk_widget_add_css_class (s, "dim-label");
      gtk_label_set_xalign (GTK_LABEL (s), 0);
      gtk_label_set_wrap (GTK_LABEL (s), TRUE);
      GtkWidget *d = gtk_label_new (pkg->description);
      gtk_label_set_xalign (GTK_LABEL (d), 0);
      gtk_label_set_wrap (GTK_LABEL (d), TRUE);
      gtk_box_append (GTK_BOX (texts), t);
      gtk_box_append (GTK_BOX (texts), s);
      gtk_box_append (GTK_BOX (texts), d);

      GtkWidget *btn =
          gtk_button_new_with_label (pkg->installed ? "Remove" : "Install");
      if (!pkg->installed)
        gtk_widget_add_css_class (btn, "suggested-action");
      g_object_set_data_full (G_OBJECT (btn), "id", g_strdup (pkg->id), g_free);
      g_object_set_data_full (G_OBJECT (btn), "branch", g_strdup (pkg->branch),
                              g_free);
      g_object_set_data (G_OBJECT (btn), "installed",
                         GINT_TO_POINTER (pkg->installed));
      g_signal_connect (btn, "clicked", G_CALLBACK (on_install_clicked), self);

      gtk_box_append (GTK_BOX (box), texts);
      gtk_box_append (GTK_BOX (box), btn);
      gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
      gtk_list_box_append (GTK_LIST_BOX (self->list_box), row);
    }

  if (!gab_sdk_flatpak_available ())
    gtk_label_set_text (GTK_LABEL (self->status),
                        "flatpak not found — install flatpak to download SDKs");
  else
    gtk_label_set_text (GTK_LABEL (self->status),
                        "Select a runtime to install from Flathub");
}

static void
gab_sdk_view_dispose (GObject *object)
{
  GabSdkView *self = GAB_SDK_VIEW (object);
  if (self->packages)
    {
      gab_sdk_free_catalog (self->packages, self->package_count);
      self->packages = NULL;
    }
  G_OBJECT_CLASS (gab_sdk_view_parent_class)->dispose (object);
}

static void
gab_sdk_view_class_init (GabSdkViewClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = gab_sdk_view_dispose;
}

static void
gab_sdk_view_init (GabSdkView *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  GtkWidget *toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (toolbar, 12);
  gtk_widget_set_margin_end (toolbar, 12);
  gtk_widget_set_margin_top (toolbar, 8);
  gtk_widget_set_margin_bottom (toolbar, 4);
  GtkWidget *intro = gtk_label_new (
      "Download GNOME Platform, Sdk, extensions, and API doc runtimes from Flathub.");
  gtk_widget_add_css_class (intro, "dim-label");
  gtk_label_set_xalign (GTK_LABEL (intro), 0);
  gtk_label_set_wrap (GTK_LABEL (intro), TRUE);
  gtk_widget_set_hexpand (intro, TRUE);
  GtkWidget *refresh_btn = gtk_button_new_from_icon_name ("view-refresh-symbolic");
  gtk_widget_set_tooltip_text (refresh_btn, "Refresh install status");
  g_signal_connect_swapped (refresh_btn, "clicked", G_CALLBACK (gab_sdk_view_refresh),
                            self);
  gtk_box_append (GTK_BOX (toolbar), intro);
  gtk_box_append (GTK_BOX (toolbar), refresh_btn);
  gtk_box_append (GTK_BOX (self), toolbar);

  GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_vexpand (paned, TRUE);
  gtk_box_append (GTK_BOX (self), paned);

  GtkWidget *scroll = gtk_scrolled_window_new ();
  self->list_box = gtk_list_box_new ();
  gtk_widget_add_css_class (self->list_box, "boxed-list");
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->list_box), GTK_SELECTION_NONE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), self->list_box);
  gtk_paned_set_start_child (GTK_PANED (paned), scroll);

  GtkWidget *log_scroll = gtk_scrolled_window_new ();
  self->log_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (self->log_view), FALSE);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (self->log_view), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->log_view), GTK_WRAP_WORD_CHAR);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (log_scroll), self->log_view);
  gtk_paned_set_end_child (GTK_PANED (paned), log_scroll);
  gtk_paned_set_position (GTK_PANED (paned), 420);

  self->status = gtk_label_new ("");
  gtk_widget_add_css_class (self->status, "dim-label");
  gtk_label_set_xalign (GTK_LABEL (self->status), 0);
  gtk_widget_set_margin_start (self->status, 12);
  gtk_widget_set_margin_bottom (self->status, 8);
  gtk_box_append (GTK_BOX (self), self->status);

  gab_sdk_view_refresh (self);
}

GtkWidget *
gab_sdk_view_new (void)
{
  return g_object_new (GAB_TYPE_SDK_VIEW, NULL);
}
