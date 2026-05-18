/* CoraOS Settings - Window
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <libadwaita-1/adw.h>

G_BEGIN_DECLS

#define CORA_TYPE_SETTINGS_WINDOW (cora_settings_window_get_type ())
G_DECLARE_FINAL_TYPE (CoraSettingsWindow, cora_settings_window, CORA, SETTINGS_WINDOW, AdwApplicationWindow)

CoraSettingsWindow *cora_settings_window_new (GtkApplication *app);

G_END_DECLS
