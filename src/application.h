#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define GAB_TYPE_APPLICATION (gab_application_get_type ())
G_DECLARE_FINAL_TYPE (GabApplication, gab_application, GAB, APPLICATION, AdwApplication)

GabApplication *gab_application_new (void);

G_END_DECLS
