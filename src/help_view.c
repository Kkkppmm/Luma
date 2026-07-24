#include "help_view.h"
#include "config.h"

#include <adwaita.h>

struct _GabHelpView
{
  GtkBox parent_instance;
  GtkStack *stack;
  GtkListBox *nav;
};

G_DEFINE_TYPE (GabHelpView, gab_help_view, GTK_TYPE_BOX)

static GtkWidget *
make_page (const char *title, const char *body)
{
  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 24);

  GtkWidget *h = gtk_label_new (title);
  gtk_widget_add_css_class (h, "title-1");
  gtk_label_set_xalign (GTK_LABEL (h), 0);
  gtk_box_append (GTK_BOX (box), h);

  GtkWidget *p = gtk_label_new (body);
  gtk_label_set_xalign (GTK_LABEL (p), 0);
  gtk_label_set_wrap (GTK_LABEL (p), TRUE);
  gtk_widget_add_css_class (p, "body");
  gtk_box_append (GTK_BOX (box), p);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), box);
  return scroll;
}

static void
on_nav_row (GtkListBox *box, GtkListBoxRow *row, GabHelpView *self)
{
  (void) box;
  if (row == NULL)
    return;
  const char *name = g_object_get_data (G_OBJECT (row), "page");
  if (name != NULL)
    gtk_stack_set_visible_child_name (self->stack, name);
}

static GtkWidget *
nav_row (const char *icon, const char *label, const char *page)
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_margin_start (hb, 10);
  gtk_widget_set_margin_end (hb, 10);
  gtk_widget_set_margin_top (hb, 8);
  gtk_widget_set_margin_bottom (hb, 8);
  gtk_box_append (GTK_BOX (hb), gtk_image_new_from_icon_name (icon));
  GtkWidget *l = gtk_label_new (label);
  gtk_label_set_xalign (GTK_LABEL (l), 0);
  gtk_widget_set_hexpand (l, TRUE);
  gtk_box_append (GTK_BOX (hb), l);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), hb);
  g_object_set_data_full (G_OBJECT (row), "page", g_strdup (page), g_free);
  return row;
}

static void
gab_help_view_class_init (GabHelpViewClass *klass)
{
  (void) klass;
}

static void
gab_help_view_init (GabHelpView *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  GtkWidget *sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (sidebar, 220, -1);
  gtk_widget_add_css_class (sidebar, "sidebar");

  GtkWidget *side_title = gtk_label_new ("Help Topics");
  gtk_widget_add_css_class (side_title, "heading");
  gtk_widget_set_margin_start (side_title, 14);
  gtk_widget_set_margin_top (side_title, 14);
  gtk_widget_set_margin_bottom (side_title, 8);
  gtk_label_set_xalign (GTK_LABEL (side_title), 0);
  gtk_box_append (GTK_BOX (sidebar), side_title);

  self->nav = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (self->nav, GTK_SELECTION_SINGLE);
  gtk_widget_add_css_class (GTK_WIDGET (self->nav), "navigation-sidebar");
  g_signal_connect (self->nav, "row-activated", G_CALLBACK (on_nav_row), self);
  g_signal_connect (self->nav, "row-selected", G_CALLBACK (on_nav_row), self);

  gtk_list_box_append (self->nav,
                       nav_row ("help-about-symbolic", "Overview", "overview"));
  gtk_list_box_append (self->nav,
                       nav_row ("user-home-symbolic", "Homescreen", "home"));
  gtk_list_box_append (self->nav,
                       nav_row ("text-editor-symbolic", "Editor", "editor"));
  gtk_list_box_append (self->nav,
                       nav_row ("input-keyboard-symbolic", "Shortcuts", "shortcuts"));
  gtk_list_box_append (self->nav,
                       nav_row ("folder-download-symbolic", "SDK Manager", "sdk"));
  gtk_list_box_append (self->nav,
                       nav_row ("dialog-warning-symbolic", "Troubleshooting", "trouble"));

  gtk_box_append (GTK_BOX (sidebar), GTK_WIDGET (self->nav));
  gtk_box_append (GTK_BOX (self), sidebar);

  self->stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_set_transition_type (self->stack, GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_widget_set_hexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);

  gtk_stack_add_named (self->stack,
                       make_page ("Overview",
                                  "Luma Builder is a lightweight IDE for GTK4 and "
                                  "libadwaita apps in C and C++.\n\n"
                                  "From the home screen you can create projects, reopen "
                                  "recent work, download GNOME SDKs, and open Help or Docs "
                                  "in their own windows.\n\n"
                                  "In the editor, right-click for Format, Indent, Trim, "
                                  "Sort Lines, and Comment tools — no toolbar required."),
                       "overview");

  gtk_stack_add_named (self->stack,
                       make_page ("Homescreen",
                                  "• New Project — scaffold a Meson GTK4 (+ optional Adwaita) app\n"
                                  "• Open Project — choose any folder of sources\n"
                                  "• Recent Projects — one-click reopen\n"
                                  "• SDK Manager — install Platform / Sdk Flatpak runtimes\n"
                                  "• Help / Docs — open dedicated helper windows\n\n"
                                  "Created projects land under ~/LumaProjects by default."),
                       "home");

  gtk_stack_add_named (self->stack,
                       make_page ("Editor",
                                  "The editor shows a file tree on the left and the source on the right.\n\n"
                                  "Right-click anywhere in the code for tools:\n"
                                  "• Format Document / Format Selected Lines\n"
                                  "• Indent / Unindent\n"
                                  "• Trim Trailing Space\n"
                                  "• Sort Selected Lines\n"
                                  "• Toggle Comment\n"
                                  "• Save\n\n"
                                  "The same menu is also in the header Tools button."),
                       "editor");

  gtk_stack_add_named (self->stack,
                       make_page ("Keyboard Shortcuts",
                                  "Ctrl+S — Save current file\n"
                                  "Ctrl+B — Build the open project (meson)\n"
                                  "F5 — Build and run the project\n"
                                  "Ctrl+Shift+F — Format whole document\n"
                                  "Ctrl+U — Check for updates from GitHub\n"
                                  "F1 — Help window\n"
                                  "Ctrl+Q — Quit\n\n"
                                  "Right-click (or the Tools menu) for indent, unindent, trim, "
                                  "sort lines, and comment toggling."),
                       "shortcuts");

  gtk_stack_add_named (self->stack,
                       make_page ("SDK Manager",
                                  "Open SDK Manager from the home screen — it opens in its "
                                  "own window.\n\n"
                                  "Install org.gnome.Platform and org.gnome.Sdk for a chosen "
                                  "branch (46/47), plus optional extensions and API docs.\n\n"
                                  "Requires the flatpak CLI. Flathub is registered automatically "
                                  "for the user installation when you click Install."),
                       "sdk");

  gtk_stack_add_named (self->stack,
                       make_page ("Troubleshooting",
                                  "App exits immediately on second launch\n"
                                  "  → Only one instance runs (GApplication). Focus the existing window.\n\n"
                                  "GSettings schema not found\n"
                                  "  → Export GSETTINGS_SCHEMA_DIR=build/data when running uninstalled.\n\n"
                                  "SDK install fails\n"
                                  "  → Install flatpak, ensure network access, check the log pane.\n\n"
                                  "Docs empty\n"
                                  "  → Run from the repository root so docs/*.md can be found."),
                       "trouble");

  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->stack));
  gtk_list_box_select_row (self->nav,
                           gtk_list_box_get_row_at_index (self->nav, 0));
}

GtkWidget *
gab_help_view_new (void)
{
  return g_object_new (GAB_TYPE_HELP_VIEW, NULL);
}
