#include "secondary_window.h"
#include "config.h"

GtkWindow *
gab_secondary_window_present (GtkApplication *app,
                              const char *title,
                              const char *subtitle,
                              GtkWidget *content,
                              int width,
                              int height)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (app), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (content), NULL);

  for (GList *l = gtk_application_get_windows (app); l != NULL; l = l->next)
    {
      GtkWindow *w = GTK_WINDOW (l->data);
      if (g_strcmp0 (gtk_window_get_title (w), title) == 0)
        {
          g_object_ref_sink (content);
          g_object_unref (content);
          gtk_window_present (w);
          return w;
        }
    }

  AdwApplicationWindow *win =
      ADW_APPLICATION_WINDOW (adw_application_window_new (app));
  gtk_window_set_title (GTK_WINDOW (win), title);
  gtk_window_set_default_size (GTK_WINDOW (win), width > 0 ? width : 900,
                               height > 0 ? height : 640);
  /* Independent windows — no transient parent (smoother multi-window use) */

  GtkWidget *toolbar_view = adw_toolbar_view_new ();
  AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
  adw_header_bar_set_title_widget (
      header, adw_window_title_new (title, subtitle ? subtitle : APP_NAME));
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), GTK_WIDGET (header));
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), content);

  adw_application_window_set_content (win, toolbar_view);
  gtk_window_present (GTK_WINDOW (win));
  return GTK_WINDOW (win);
}
