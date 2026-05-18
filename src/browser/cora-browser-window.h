/* CoraOS Web Browser - Window
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define CORA_TYPE_BROWSER_WINDOW (cora_browser_window_get_type ())
G_DECLARE_FINAL_TYPE (CoraBrowserWindow, cora_browser_window, CORA, BROWSER_WINDOW, AdwApplicationWindow)

CoraBrowserWindow *cora_browser_window_new      (GtkApplication *app, const char *initial_uri);
void               cora_browser_window_new_tab  (CoraBrowserWindow *self, const char *uri);

G_END_DECLS
