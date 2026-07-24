#include "secondary_window.h"

static void
on_close_clicked (GtkButton *btn, GtkWindow *window)
{
  (void) btn;
  gtk_window_close (window);
}

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

  /* Reuse an existing window with the same title if still open */
  GList *windows = gtk_application_get_windows (app);
  for (GList *l = windows; l != NULL; l = l->next)
    {
      GtkWindow *w = GTK_WINDOW (l->data);
      const char *t = gtk_window_get_title (w);
      if (g_strcmp0 (t, title) == 0)
        {
          /* Drop unused fresh content; present existing window */
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

  GtkWindow *main_win = NULL;
  for (GList *l = gtk_application_get_windows (app); l != NULL; l = l->next)
    {
      const char *t = gtk_window_get_title (GTK_WINDOW (l->data));
      if (g_strcmp0 (t, "GNOME App Builder") == 0)
        {
          main_win = GTK_WINDOW (l->data);
          break;
        }
    }
  if (main_win != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (win), main_win);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  AdwHeaderBar *header = ADW_HEADER_BAR (adw_header_bar_new ());
  adw_header_bar_set_show_end_title_buttons (header, TRUE);
  adw_header_bar_set_title_widget (header, adw_window_title_new (title, subtitle));

  GtkWidget *close_btn = gtk_button_new_with_label ("Close");
  gtk_widget_add_css_class (close_btn, "flat");
  g_signal_connect (close_btn, "clicked", G_CALLBACK (on_close_clicked), win);
  adw_header_bar_pack_end (header, close_btn);

  gtk_box_append (GTK_BOX (box), GTK_WIDGET (header));
  gtk_widget_set_vexpand (content, TRUE);
  gtk_widget_set_hexpand (content, TRUE);
  gtk_box_append (GTK_BOX (box), content);

  adw_application_window_set_content (win, box);
  gtk_window_present (GTK_WINDOW (win));
  return GTK_WINDOW (win);
}
