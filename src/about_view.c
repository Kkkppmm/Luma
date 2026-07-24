#include "about_view.h"
#include "config.h"

#include <adwaita.h>

struct _GabAboutView
{
  GtkBox parent_instance;
};

G_DEFINE_TYPE (GabAboutView, gab_about_view, GTK_TYPE_BOX)

static void
gab_about_view_class_init (GabAboutViewClass *klass)
{
  (void) klass;
}

static void
gab_about_view_init (GabAboutView *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (GTK_WIDGET (self), 32);
  gtk_widget_set_margin_end (GTK_WIDGET (self), 32);
  gtk_widget_set_margin_top (GTK_WIDGET (self), 28);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 28);

  const char *icon_name = "applications-engineering-symbolic";
  GtkIconTheme *theme =
      gtk_icon_theme_get_for_display (gdk_display_get_default ());
  if (gtk_icon_theme_has_icon (theme, APP_ID))
    icon_name = APP_ID;

  GtkWidget *icon = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 96);
  gtk_box_append (GTK_BOX (self), icon);

  GtkWidget *name = gtk_label_new (APP_NAME);
  gtk_widget_add_css_class (name, "title-1");
  gtk_widget_set_margin_top (name, 12);
  gtk_box_append (GTK_BOX (self), name);

  g_autofree char *ver = g_strdup_printf ("Version %s", PACKAGE_VERSION);
  GtkWidget *version = gtk_label_new (ver);
  gtk_widget_add_css_class (version, "dim-label");
  gtk_box_append (GTK_BOX (self), version);

  GtkWidget *desc = gtk_label_new (
      "A smooth lightweight IDE for GTK4 / libadwaita apps in C and C++.\n"
      "Right-click in the editor for format tools. Help, Docs, and SDK\n"
      "Manager open in their own windows.");
  gtk_label_set_justify (GTK_LABEL (desc), GTK_JUSTIFY_CENTER);
  gtk_label_set_wrap (GTK_LABEL (desc), TRUE);
  gtk_widget_set_margin_top (desc, 16);
  gtk_box_append (GTK_BOX (self), desc);

  GtkWidget *tech = gtk_label_new ("GTK 4 · libadwaita · GtkSourceView · Meson");
  gtk_widget_add_css_class (tech, "dim-label");
  gtk_widget_set_margin_top (tech, 18);
  gtk_box_append (GTK_BOX (self), tech);

  GtkWidget *license = gtk_label_new ("License: MIT");
  gtk_widget_add_css_class (license, "dim-label");
  gtk_widget_set_margin_top (license, 6);
  gtk_box_append (GTK_BOX (self), license);
}

GtkWidget *
gab_about_view_new (void)
{
  return g_object_new (GAB_TYPE_ABOUT_VIEW, NULL);
}
