/* CoraOS File Explorer - Main Entry Point
 *
 * A real file manager using GIO/GVfs for filesystem operations.
 * Features:
 * - Directory browsing with grid and list views
 * - Sidebar with bookmarks, devices, network
 * - File operations: copy, move, delete, rename, new folder
 * - Thumbnails for images
 * - Open files with default application
 * - Drag and drop
 * - Path bar navigation
 * - Search
 * - Trash support
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>
#include <adwaita.h>

#include "cora-files-window.h"

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    GtkWindow *window;

    window = gtk_application_get_active_window (app);
    if (window == NULL) {
        window = GTK_WINDOW (cora_files_window_new (app, NULL));
    }

    gtk_window_present (window);
}

static void
on_open (GApplication *app, GFile **files, int n_files,
         const char *hint, gpointer user_data)
{
    for (int i = 0; i < n_files; i++) {
        GtkWindow *window = GTK_WINDOW (
            cora_files_window_new (GTK_APPLICATION (app), files[i]));
        gtk_window_present (window);
    }
}

int
main (int argc, char *argv[])
{
    g_autoptr(AdwApplication) app = NULL;
    int status;

    app = adw_application_new ("dev.coraos.Files", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
    g_signal_connect (app, "open", G_CALLBACK (on_open), NULL);

    status = g_application_run (G_APPLICATION (app), argc, argv);

    return status;
}
