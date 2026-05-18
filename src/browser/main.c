/* CoraOS Web Browser - Main Entry Point
 *
 * A real web browser using WebKitGTK for rendering.
 * Features:
 * - Tabbed browsing
 * - URL bar with search
 * - Back/forward/reload navigation
 * - Bookmarks
 * - Download manager
 * - Private browsing mode
 * - Custom homepage
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>
#include <libadwaita-1/adw.h>

#include "cora-browser-window.h"

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    GtkWindow *window;

    window = gtk_application_get_active_window (app);
    if (window == NULL) {
        window = GTK_WINDOW (cora_browser_window_new (app, NULL));
    }

    gtk_window_present (window);
}

static void
on_open (GApplication *app, GFile **files, int n_files,
         const char *hint, gpointer user_data)
{
    for (int i = 0; i < n_files; i++) {
        g_autofree char *uri = g_file_get_uri (files[i]);
        GtkWindow *window = GTK_WINDOW (
            cora_browser_window_new (GTK_APPLICATION (app), uri));
        gtk_window_present (window);
    }
}

int
main (int argc, char *argv[])
{
    g_autoptr(AdwApplication) app = NULL;
    int status;

    app = adw_application_new ("dev.coraos.Browser", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
    g_signal_connect (app, "open", G_CALLBACK (on_open), NULL);

    status = g_application_run (G_APPLICATION (app), argc, argv);

    return status;
}
