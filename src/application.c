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
      { .name = "quit", .activate = app_quit },
  };
  g_action_map_add_action_entries (G_ACTION_MAP (app), entries,
                                   G_N_ELEMENTS (entries), app);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit",
                                         (const char *[]){ "<primary>q", NULL });
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.help",
                                         (const char *[]){ "F1", NULL });
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
