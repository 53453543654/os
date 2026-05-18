/* CoraOS WiFi Manager - Main Entry Point
 * 
 * A real WiFi management application using libnm (NetworkManager API).
 * Features:
 * - Scan for available networks
 * - Connect/disconnect from networks
 * - WPA/WPA2/WPA3 password entry
 * - Saved network management
 * - Connection status and signal strength
 * - Hotspot creation
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>
#include <libadwaita-1/adw.h>
#include <NetworkManager.h>

#include "cora-wifi-window.h"

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    GtkWindow *window;

    window = gtk_application_get_active_window (app);
    if (window == NULL) {
        window = GTK_WINDOW (cora_wifi_window_new (app));
    }

    gtk_window_present (window);
}

int
main (int argc, char *argv[])
{
    g_autoptr(AdwApplication) app = NULL;
    int status;

    app = adw_application_new ("dev.coraos.WiFi", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

    status = g_application_run (G_APPLICATION (app), argc, argv);

    return status;
}
