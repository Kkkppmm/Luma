#include "application.h"
#include "window.h"
#include "help_view.h"
#include "docs_view.h"
#include "sdk_view.h"
#include "about_view.h"
#include "secondary_window.h"
#include "updater_c.h"
#include "config.h"

#include <adwaita.h>

struct _GabApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_TYPE (GabApplication, gab_application, ADW_TYPE_APPLICATION)

static GtkWindow *
get_active_app_window (GtkApplication *app)
{
  for (GList *l = gtk_application_get_windows (app); l; l = l->next)
    {
      if (GAB_IS_WINDOW (l->data))
        return GTK_WINDOW (l->data);
    }
  return gtk_application_get_active_window (app);
}

static void
show_message (GtkApplication *app, const char *heading, const char *body)
{
  GtkWindow *parent = get_active_app_window (app);
  AdwDialog *dialog = adw_alert_dialog_new (heading, body);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "ok", "OK");
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "ok");
  adw_dialog_present (dialog, GTK_WIDGET (parent));
}

typedef struct {
  GtkApplication *app;
  GabUpdateInfo *info;
} UpdatePromptData;

static void
on_update_response (AdwAlertDialog *dialog, gchar *response, gpointer user_data)
{
  (void) dialog;
  UpdatePromptData *data = user_data;
  if (g_strcmp0 (response, "install") == 0)
    {
      GabUpdateInstallResult *result = gab_update_download_and_install (data->info);
      if (result->success)
        show_message (data->app, "Update installed",
                      result->message ? result->message
                                      : "Restart Luma Builder to use the new version.");
      else
        show_message (data->app, "Update failed",
                      result->message ? result->message : "Install failed.");
      gab_update_install_result_free (result);
    }
  gab_update_info_free (data->info);
  g_object_unref (data->app);
  g_free (data);
}

static void
present_update_dialog (GtkApplication *app, GabUpdateInfo *info, gboolean quiet_if_current)
{
  if (info->error != NULL)
    {
      if (!quiet_if_current)
        show_message (app, "Update check failed", info->error);
      gab_update_info_free (info);
      return;
    }

  if (!info->update_available)
    {
      if (!quiet_if_current)
        {
          g_autofree char *body =
              g_strdup_printf ("You are on %s — the latest release is %s.",
                               info->current_version ? info->current_version : "?",
                               info->latest_version ? info->latest_version : "?");
          show_message (app, "You're up to date", body);
        }
      gab_update_info_free (info);
      return;
    }

  g_autofree char *heading =
      g_strdup_printf ("Luma Builder %s is available",
                       info->latest_version ? info->latest_version : "?");

  g_autofree char *notes = NULL;
  if (info->release_notes && *info->release_notes)
    {
      if (strlen (info->release_notes) > 400)
        notes = g_strdup_printf ("%.*s…", 400, info->release_notes);
      else
        notes = g_strdup (info->release_notes);
    }
  else
    {
      notes = g_strdup ("New package from GitHub.");
    }

  g_autofree char *body = g_strdup_printf (
      "You have %s.\n\nPackage: %s\n\n%s\n\nInstall now? A system password prompt may appear.",
      info->current_version ? info->current_version : "?",
      info->asset_name ? info->asset_name : "(unknown)", notes);

  UpdatePromptData *data = g_new0 (UpdatePromptData, 1);
  data->app = g_object_ref (app);
  data->info = info;

  AdwDialog *dialog = adw_alert_dialog_new (heading, body);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "later", "Later");
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "install", "Install");
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "install",
                                            ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "install");
  g_signal_connect (dialog, "response", G_CALLBACK (on_update_response), data);
  adw_dialog_present (dialog, GTK_WIDGET (get_active_app_window (app)));
}

typedef struct {
  GtkApplication *app;
  gboolean quiet_if_current;
  GabUpdateInfo *info;
} UpdateCheckDone;

static gboolean
on_update_check_done (gpointer user_data)
{
  UpdateCheckDone *done = user_data;
  present_update_dialog (done->app, done->info, done->quiet_if_current);
  g_object_unref (done->app);
  g_free (done);
  return G_SOURCE_REMOVE;
}

typedef struct {
  GtkApplication *app;
  gboolean quiet_if_current;
} UpdateCheckJob;

static gpointer
update_check_thread (gpointer user_data)
{
  UpdateCheckJob *job = user_data;
  GabUpdateInfo *info = gab_update_check (PACKAGE_VERSION);
  UpdateCheckDone *done = g_new0 (UpdateCheckDone, 1);
  done->app = job->app;
  done->quiet_if_current = job->quiet_if_current;
  done->info = info;
  g_idle_add (on_update_check_done, done);
  g_free (job);
  return NULL;
}

static void
start_update_check (GtkApplication *app, gboolean quiet_if_current)
{
  UpdateCheckJob *job = g_new0 (UpdateCheckJob, 1);
  job->app = g_object_ref (app);
  job->quiet_if_current = quiet_if_current;
  g_thread_new ("luma-update-check", update_check_thread, job);
}

static void
app_open_help (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  gab_secondary_window_present (GTK_APPLICATION (user_data), "Help",
                                "Luma Builder", gab_help_view_new (), 920, 640);
}

static void
app_open_docs (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  gab_secondary_window_present (GTK_APPLICATION (user_data), "Docs",
                                "Luma Builder", gab_docs_view_new (), 960, 700);
}

static void
app_open_sdk (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  GtkWidget *view = gab_sdk_view_new ();
  gab_sdk_view_refresh (GAB_SDK_VIEW (view));
  gab_secondary_window_present (GTK_APPLICATION (user_data), "SDK Manager",
                                "Luma Builder", view, 880, 660);
}

static void
app_open_about (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  gab_secondary_window_present (GTK_APPLICATION (user_data), "About",
                                "Luma Builder", gab_about_view_new (), 480, 440);
}

static void
app_check_updates (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  start_update_check (GTK_APPLICATION (user_data), FALSE);
}

static void
app_quit (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  g_application_quit (G_APPLICATION (user_data));
}

static void
gab_application_activate (GApplication *app)
{
  GtkWindow *window = NULL;
  for (GList *l = gtk_application_get_windows (GTK_APPLICATION (app)); l; l = l->next)
    {
      if (GAB_IS_WINDOW (l->data))
        {
          window = GTK_WINDOW (l->data);
          break;
        }
    }
  if (window == NULL)
    window = g_object_new (GAB_TYPE_WINDOW, "application", app, NULL);
  gtk_window_present (window);

  g_autoptr (GSettings) settings = g_settings_new (APP_ID);
  if (g_settings_get_boolean (settings, "auto-check-updates"))
    start_update_check (GTK_APPLICATION (app), TRUE);
}

static void
gab_application_startup (GApplication *app)
{
  G_APPLICATION_CLASS (gab_application_parent_class)->startup (app);

  const GActionEntry entries[] = {
      { .name = "help", .activate = app_open_help },
      { .name = "docs", .activate = app_open_docs },
      { .name = "sdk", .activate = app_open_sdk },
      { .name = "about", .activate = app_open_about },
      { .name = "check-updates", .activate = app_check_updates },
      { .name = "quit", .activate = app_quit },
  };
  g_action_map_add_action_entries (G_ACTION_MAP (app), entries,
                                   G_N_ELEMENTS (entries), app);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit",
                                         (const char *[]){ "<primary>q", NULL });
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.help",
                                         (const char *[]){ "F1", NULL });
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.check-updates",
                                         (const char *[]){ "<primary>u", NULL });
}

static void
gab_application_class_init (GabApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  app_class->startup = gab_application_startup;
  app_class->activate = gab_application_activate;
}

static void
gab_application_init (GabApplication *self)
{
  (void) self;
}

GabApplication *
gab_application_new (void)
{
  return g_object_new (GAB_TYPE_APPLICATION,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_DEFAULT_FLAGS,
                       NULL);
}
