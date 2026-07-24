#include "docs_view.h"
#include "config.h"

#include <adwaita.h>

struct _GabDocsView
{
  GtkBox parent_instance;
  GtkStack *stack;
  GtkListBox *nav;
  GtkTextView *getting_started;
  GtkTextView *editor_guide;
  GtkTextView *sdk_guide;
  GtkTextView *api_ref;
  GtkTextView *faq;
};

G_DEFINE_TYPE (GabDocsView, gab_docs_view, GTK_TYPE_BOX)

static char *
read_doc_file (const char *name)
{
  const char *candidates[] = {
    "docs/",
    "../docs/",
    "../../docs/",
    DOCDIR "/",
    NULL
  };

  for (int i = 0; candidates[i] != NULL; i++)
    {
      g_autofree char *path = g_strconcat (candidates[i], name, NULL);
      gchar *contents = NULL;
      if (g_file_get_contents (path, &contents, NULL, NULL))
        return contents;
    }

  return g_strdup_printf ("# %s\n\nDocumentation file not found.\n"
                          "Expected under docs/ or %s.\n",
                          name, DOCDIR);
}

static GtkWidget *
make_text_page (GtkTextView **out_view)
{
  GtkWidget *scroll = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (scroll, TRUE);
  gtk_widget_set_vexpand (scroll, TRUE);
  GtkTextView *view = GTK_TEXT_VIEW (gtk_text_view_new ());
  gtk_text_view_set_wrap_mode (view, GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_editable (view, FALSE);
  gtk_text_view_set_monospace (view, TRUE);
  gtk_text_view_set_left_margin (view, 20);
  gtk_text_view_set_right_margin (view, 20);
  gtk_text_view_set_top_margin (view, 16);
  gtk_text_view_set_bottom_margin (view, 16);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), GTK_WIDGET (view));
  if (out_view)
    *out_view = view;
  return scroll;
}

static void
set_text (GtkTextView *view, const char *text)
{
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (view), text, -1);
}

static void
on_nav_row (GtkListBox *box, GtkListBoxRow *row, GabDocsView *self)
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
gab_docs_view_class_init (GabDocsViewClass *klass)
{
  (void) klass;
}

static void
gab_docs_view_init (GabDocsView *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  GtkWidget *sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (sidebar, 240, -1);

  GtkWidget *side_title = gtk_label_new ("Documentation");
  gtk_widget_add_css_class (side_title, "heading");
  gtk_widget_set_margin_start (side_title, 14);
  gtk_widget_set_margin_top (side_title, 14);
  gtk_widget_set_margin_bottom (side_title, 8);
  gtk_label_set_xalign (GTK_LABEL (side_title), 0);
  gtk_box_append (GTK_BOX (sidebar), side_title);

  self->nav = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->nav), "navigation-sidebar");
  gtk_list_box_set_selection_mode (self->nav, GTK_SELECTION_SINGLE);
  g_signal_connect (self->nav, "row-activated", G_CALLBACK (on_nav_row), self);
  g_signal_connect (self->nav, "row-selected", G_CALLBACK (on_nav_row), self);

  gtk_list_box_append (self->nav,
                       nav_row ("emblem-default-symbolic", "Getting Started", "start"));
  gtk_list_box_append (self->nav,
                       nav_row ("text-editor-symbolic", "Editor Guide", "editor"));
  gtk_list_box_append (self->nav,
                       nav_row ("folder-download-symbolic", "SDK & APIs", "sdk"));
  gtk_list_box_append (self->nav,
                       nav_row ("preferences-system-symbolic", "API Reference", "api"));
  gtk_list_box_append (self->nav,
                       nav_row ("dialog-question-symbolic", "FAQ", "faq"));
  gtk_box_append (GTK_BOX (sidebar), GTK_WIDGET (self->nav));
  gtk_box_append (GTK_BOX (self), sidebar);

  self->stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_set_transition_type (self->stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);

  gtk_stack_add_named (self->stack, make_text_page (&self->getting_started), "start");
  gtk_stack_add_named (self->stack, make_text_page (&self->editor_guide), "editor");
  gtk_stack_add_named (self->stack, make_text_page (&self->sdk_guide), "sdk");
  gtk_stack_add_named (self->stack, make_text_page (&self->api_ref), "api");
  gtk_stack_add_named (self->stack, make_text_page (&self->faq), "faq");

  g_autofree char *start = read_doc_file ("getting-started.md");
  g_autofree char *editor = read_doc_file ("editor-guide.md");
  g_autofree char *sdk = read_doc_file ("sdk-guide.md");
  set_text (self->getting_started, start);
  set_text (self->editor_guide, editor);
  set_text (self->sdk_guide, sdk);

  set_text (self->api_ref,
            "# API Reference (built-in overview)\n\n"
            "Luma Builder itself uses:\n\n"
            "## GTK 4\n"
            "- Windows, buttons, lists, paned layouts, file dialogs\n"
            "- Docs: https://docs.gtk.org/gtk4/\n\n"
            "## libadwaita\n"
            "- AdwApplication, AdwApplicationWindow, AdwHeaderBar\n"
            "- AdwAlertDialog, AdwEntryRow, AdwComboRow, AdwNavigationView\n"
            "- Docs: https://gnome.pages.gitlab.gnome.org/libadwaita/\n\n"
            "## GtkSourceView 5\n"
            "- Syntax highlighting, line numbers, auto-indent\n"
            "- Docs: https://gnome.pages.gitlab.gnome.org/gtksourceview/\n\n"
            "## Flatpak runtimes (via SDK Manager)\n"
            "- org.gnome.Platform // BRANCH\n"
            "- org.gnome.Sdk // BRANCH\n"
            "- Optional extensions (e.g. Rust)\n");

  set_text (self->faq,
            "# FAQ\n\n"
            "Q: Where are projects created?\n"
            "A: ~/LumaProjects/<name> by default (editable in the New Project dialog).\n\n"
            "Q: What templates are available?\n"
            "A: 36 starters — GNOME/Adwaita, GTK layouts, editor, console, libraries,\n"
            "   CMake, Flatpak, and Python. See Docs → Getting Started / templates.md.\n\n"
            "Q: Can Help and Docs stay open while I edit?\n"
            "A: Yes. They open as separate windows so you can tile them beside the editor.\n\n"
            "Q: Do I need Flatpak to develop?\n"
            "A: No. System packages (libgtk-4-dev, libadwaita-1-dev) work fine. Flatpak SDKs\n"
            "   are optional for sandboxed / reproducible builds.\n\n"
            "Q: Which languages are highlighted?\n"
            "A: C, C++, Meson, Python, XML/UI, CSS, Markdown, JSON, and plain text.\n");

  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->stack));
  gtk_list_box_select_row (self->nav,
                           gtk_list_box_get_row_at_index (self->nav, 0));
}

GtkWidget *
gab_docs_view_new (void)
{
  return g_object_new (GAB_TYPE_DOCS_VIEW, NULL);
}
