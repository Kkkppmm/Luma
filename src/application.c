#include "application.h"
#include "window.h"
#include "help_view.h"
#include "docs_view.h"
#include "sdk_view.h"
#include "about_view.h"
#include "secondary_window.h"
#include "config.h"

struct _GabApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_TYPE (GabApplication, gab_application, ADW_TYPE_APPLICATION)

static void
app_open_help (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  GtkApplication *app = GTK_APPLICATION (user_data);
  gab_secondary_window_present (app, "Help — GNOME App Builder",
                                "Guides and troubleshooting",
                                gab_help_view_new (), 960, 680);
}

static void
app_open_docs (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  GtkApplication *app = GTK_APPLICATION (user_data);
  gab_secondary_window_present (app, "Docs — GNOME App Builder",
                                "Built-in documentation",
                                gab_docs_view_new (), 1000, 720);
}

static void
app_open_sdk (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  GtkApplication *app = GTK_APPLICATION (user_data);
  GtkWidget *view = gab_sdk_view_new ();
  gab_sdk_view_refresh (GAB_SDK_VIEW (view));
  gab_secondary_window_present (app, "SDK Manager — GNOME App Builder",
                                "Platform, Sdk & API runtimes",
                                view, 920, 700);
}

static void
app_open_about (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  GtkApplication *app = GTK_APPLICATION (user_data);
  gab_secondary_window_present (app, "About — GNOME App Builder", NULL,
                                gab_about_view_new (), 520, 480);
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
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));
  for (GList *l = windows; l != NULL; l = l->next)
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
}

static void
gab_application_startup (GApplication *app)
{
  G_APPLICATION_CLASS (gab_application_parent_class)->startup (app);

  const GActionEntry entries[] = {
    { "help", app_open_help, NULL, NULL, NULL },
    { "docs", app_open_docs, NULL, NULL, NULL },
    { "sdk", app_open_sdk, NULL, NULL, NULL },
    { "about", app_open_about, NULL, NULL, NULL },
    { "quit", app_quit, NULL, NULL, NULL },
  };
  g_action_map_add_action_entries (G_ACTION_MAP (app), entries,
                                   G_N_ELEMENTS (entries), app);

  const char * const quit_accels[] = { "<primary>q", NULL };
  const char * const help_accels[] = { "F1", NULL };
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit",
                                         quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.help",
                                         help_accels);
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
