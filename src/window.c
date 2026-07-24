#include "window.h"
#include "homescreen.h"
#include "editor_view.h"
#include "config.h"

struct _GabWindow
{
  AdwApplicationWindow parent_instance;
  AdwNavigationView *nav;
  GtkWidget *homescreen;
  GtkWidget *editor;
  GSettings *settings;
};

G_DEFINE_TYPE (GabWindow, gab_window, ADW_TYPE_APPLICATION_WINDOW)

void
gab_window_show_home (GabWindow *self)
{
  g_return_if_fail (GAB_IS_WINDOW (self));
  while (adw_navigation_view_get_previous_page (
           self->nav, adw_navigation_view_get_visible_page (self->nav)) != NULL)
    adw_navigation_view_pop (self->nav);
  gab_homescreen_refresh (GAB_HOMESCREEN (self->homescreen));
}

void
gab_window_open_project (GabWindow *self, const char *path)
{
  g_return_if_fail (GAB_IS_WINDOW (self));
  g_return_if_fail (path != NULL);

  gab_editor_view_open_project (GAB_EDITOR_VIEW (self->editor), path);

  g_autofree char **recents = g_settings_get_strv (self->settings, "recent-projects");
  GPtrArray *arr = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (arr, g_strdup (path));
  for (guint i = 0; recents != NULL && recents[i] != NULL; i++)
    {
      if (g_strcmp0 (recents[i], path) != 0)
        g_ptr_array_add (arr, g_strdup (recents[i]));
    }
  if (arr->len > 12)
    g_ptr_array_set_size (arr, 12);
  g_ptr_array_add (arr, NULL);
  g_settings_set_strv (self->settings, "recent-projects",
                       (const char * const *) arr->pdata);
  g_ptr_array_free (arr, TRUE);

  AdwNavigationPage *editor_page = adw_navigation_view_find_page (self->nav, "editor");
  if (editor_page != NULL)
    adw_navigation_view_push (self->nav, editor_page);
}

static void
activate_app_action (GabWindow *self, const char *name)
{
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
  if (app != NULL)
    g_action_group_activate_action (G_ACTION_GROUP (app), name, NULL);
}

static void
on_homescreen_action (GabHomescreen *home, const char *action, const char *payload,
                      GabWindow *self)
{
  (void) home;
  if (g_strcmp0 (action, "open-project") == 0 && payload != NULL)
    gab_window_open_project (self, payload);
  else if (g_strcmp0 (action, "show-sdk") == 0)
    activate_app_action (self, "sdk");
  else if (g_strcmp0 (action, "show-help") == 0)
    activate_app_action (self, "help");
  else if (g_strcmp0 (action, "show-docs") == 0)
    activate_app_action (self, "docs");
  else if (g_strcmp0 (action, "show-about") == 0)
    activate_app_action (self, "about");
}

static void
on_go_home (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  (void) param;
  gab_window_show_home (GAB_WINDOW (user_data));
}

static void
gab_window_dispose (GObject *object)
{
  GabWindow *self = GAB_WINDOW (object);
  g_clear_object (&self->settings);
  G_OBJECT_CLASS (gab_window_parent_class)->dispose (object);
}

static void
gab_window_class_init (GabWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = gab_window_dispose;
}

static AdwNavigationPage *
make_page (const char *tag, const char *title, GtkWidget *child)
{
  AdwNavigationPage *page = adw_navigation_page_new (child, title);
  adw_navigation_page_set_tag (page, tag);
  return page;
}

static void
gab_window_init (GabWindow *self)
{
  self->settings = g_settings_new (APP_ID);

  gtk_window_set_title (GTK_WINDOW (self), APP_NAME);
  gtk_window_set_default_size (
      GTK_WINDOW (self),
      g_settings_get_int (self->settings, "window-width"),
      g_settings_get_int (self->settings, "window-height"));
  if (g_settings_get_boolean (self->settings, "window-maximized"))
    gtk_window_maximize (GTK_WINDOW (self));

  self->nav = ADW_NAVIGATION_VIEW (adw_navigation_view_new ());
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (self),
                                      GTK_WIDGET (self->nav));

  self->homescreen = gab_homescreen_new ();
  self->editor = gab_editor_view_new ();

  adw_navigation_view_add (self->nav,
                           make_page ("home", "Home", self->homescreen));
  adw_navigation_view_add (self->nav,
                           make_page ("editor", "Editor", self->editor));

  g_signal_connect (self->homescreen, "action", G_CALLBACK (on_homescreen_action),
                    self);

  g_autoptr (GSimpleAction) home_action = g_simple_action_new ("go-home", NULL);
  g_signal_connect (home_action, "activate", G_CALLBACK (on_go_home), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (home_action));
}
