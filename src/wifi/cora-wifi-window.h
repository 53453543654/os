/* CoraOS WiFi Window
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

#define CORA_TYPE_WIFI_WINDOW (cora_wifi_window_get_type ())
G_DECLARE_FINAL_TYPE (CoraWifiWindow, cora_wifi_window, CORA, WIFI_WINDOW, AdwApplicationWindow)

CoraWifiWindow *cora_wifi_window_new (GtkApplication *app);

G_END_DECLS
