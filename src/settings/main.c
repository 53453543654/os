/* CoraOS Settings - Main Entry Point
 *
 * System settings application with real backend integration.
 * Panels:
 * - Appearance (theme, accent color, fonts, animations, wallpaper)
 * - Display (resolution, scaling, night light)
 * - Sound (volume, output device, input device)
 * - Network (WiFi, Ethernet, VPN)
 * - Bluetooth
 * - Power (profiles, screen timeout, lid action)
 * - Users & Accounts
 * - Keyboard (shortcuts, layouts)
 * - About (system info)
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>
#include <libadwaita-1/adw.h>

#include "cora-settings-window.h"

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    GtkWindow *window;

    window = gtk_application_get_active_window (app);
    if (window == NULL) {
        window = GTK_WINDOW (cora_settings_window_new (app));
    }

    gtk_window_present (window);
}

int
main (int argc, char *argv[])
{
    g_autoptr(AdwApplication) app = NULL;
    int status;

    app = adw_application_new ("dev.coraos.Settings", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

    status = g_application_run (G_APPLICATION (app), argc, argv);

    return status;
}
