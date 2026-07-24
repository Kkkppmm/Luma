#include "application.h"
#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

int
main (int argc, char **argv)
{
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_source_init ();
  adw_init ();

  g_autoptr (GabApplication) app = gab_application_new ();
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  gtk_source_finalize ();
  return status;
}
