/* CoraOS Shell - Main Entry Point
 * 
 * This is the real desktop shell for CoraOS. It acts as a Wayland compositor
 * session using libmutter, providing:
 * - Window management (tiling, stacking, workspaces)
 * - Top panel with clock, system tray, app menu
 * - Application launcher (grid with search)
 * - Notification center
 * - Workspace overview
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "cora-shell.h"
#include "cora-panel.h"
#include "cora-launcher.h"
#include "cora-wallpaper.h"

static CoraShell *shell_instance = NULL;

static void
on_activate (GtkApplication *app, gpointer user_data)
{
    CoraShell *shell = CORA_SHELL (app);
    cora_shell_start (shell);
}

static void
on_shutdown (GtkApplication *app, gpointer user_data)
{
    CoraShell *shell = CORA_SHELL (app);
    cora_shell_stop (shell);
}

int
main (int argc, char *argv[])
{
    g_autoptr(CoraShell) shell = NULL;

    /* Set environment for CoraOS session */
    g_setenv ("XDG_CURRENT_DESKTOP", "CoraOS", TRUE);
    g_setenv ("XDG_SESSION_DESKTOP", "coraos", TRUE);
    g_setenv ("DESKTOP_SESSION", "coraos", TRUE);

    shell = cora_shell_new ();
    shell_instance = shell;

    g_signal_connect (shell, "activate", G_CALLBACK (on_activate), NULL);
    g_signal_connect (shell, "shutdown", G_CALLBACK (on_shutdown), NULL);

    return g_application_run (G_APPLICATION (shell), argc, argv);
}
