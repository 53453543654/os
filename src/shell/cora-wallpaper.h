/* CoraOS Wallpaper - Desktop background rendering
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "cora-shell.h"

G_BEGIN_DECLS

#define CORA_TYPE_WALLPAPER (cora_wallpaper_get_type ())
G_DECLARE_FINAL_TYPE (CoraWallpaper, cora_wallpaper, CORA, WALLPAPER, GtkWindow)

CoraWallpaper *cora_wallpaper_new         (CoraShell *shell);
void           cora_wallpaper_set_image   (CoraWallpaper *self, const char *path);

G_END_DECLS
