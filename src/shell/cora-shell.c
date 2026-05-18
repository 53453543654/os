/* CoraOS Shell - Core Shell Implementation
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-shell.h"
#include "cora-panel.h"
#include "cora-launcher.h"
#include "cora-wallpaper.h"

struct _CoraShell {
    GtkApplication  parent_instance;

    /* Core components */
    CoraPanel      *panel;
    CoraLauncher   *launcher;
    CoraWallpaper  *wallpaper;

    /* Settings */
    GSettings      *settings;

    /* Display */
    GdkDisplay     *display;
    GdkMonitor     *primary_monitor;

    /* State */
    gboolean        running;
};

G_DEFINE_TYPE (CoraShell, cora_shell, GTK_TYPE_APPLICATION)

static void
cora_shell_load_css (CoraShell *self)
{
    GtkCssProvider *provider;
    g_autofree char *css_path = NULL;

    provider = gtk_css_provider_new ();

    /* Try user override first */
    css_path = g_build_filename (g_get_user_config_dir (), "coraos", "shell.css", NULL);
    if (!g_file_test (css_path, G_FILE_TEST_EXISTS)) {
        g_free (css_path);
        css_path = g_strdup ("/usr/share/coraos/shell/shell.css");
    }

    if (g_file_test (css_path, G_FILE_TEST_EXISTS)) {
        gtk_css_provider_load_from_path (provider, css_path);
    } else {
        /* Embedded fallback CSS */
        const char *css =
            ".cora-panel {"
            "  background-color: rgba(10, 10, 10, 0.95);"
            "  border-bottom: 1px solid rgba(255, 26, 26, 0.3);"
            "  color: #f0f0f0;"
            "}"
            ".cora-panel-button {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 4px 10px;"
            "  color: rgba(255,255,255,0.85);"
            "  min-height: 24px;"
            "}"
            ".cora-panel-button:hover {"
            "  background-color: rgba(255, 26, 26, 0.15);"
            "}"
            ".cora-launcher-window {"
            "  background-color: rgba(10, 10, 10, 0.97);"
            "  border: 1px solid rgba(255, 26, 26, 0.2);"
            "  border-radius: 16px;"
            "}"
            ".cora-launcher-search {"
            "  background-color: rgba(30, 30, 30, 0.9);"
            "  border: 1px solid rgba(255, 26, 26, 0.3);"
            "  border-radius: 10px;"
            "  padding: 10px 14px;"
            "  font-size: 15px;"
            "  color: #ffffff;"
            "  min-height: 40px;"
            "}"
            ".cora-launcher-search:focus {"
            "  border-color: #FF1A1A;"
            "  box-shadow: 0 0 0 2px rgba(255, 26, 26, 0.2);"
            "}"
            ".cora-app-item {"
            "  border-radius: 12px;"
            "  padding: 12px;"
            "}"
            ".cora-app-item:hover {"
            "  background-color: rgba(255, 26, 26, 0.1);"
            "}"
            ".cora-app-label {"
            "  color: rgba(255, 255, 255, 0.9);"
            "  font-size: 11px;"
            "}"
            ".cora-clock {"
            "  font-weight: 600;"
            "  font-size: 13px;"
            "  color: #ffffff;"
            "}"
            ".cora-wallpaper {"
            "  background-color: #0a0a0a;"
            "}";
        gtk_css_provider_load_from_string (provider, css);
    }

    gtk_style_context_add_provider_for_display (
        gdk_display_get_default (),
        GTK_STYLE_PROVIDER (provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref (provider);
}

static void
cora_shell_detect_monitors (CoraShell *self)
{
    GListModel *monitors;

    self->display = gdk_display_get_default ();
    monitors = gdk_display_get_monitors (self->display);

    if (g_list_model_get_n_items (monitors) > 0) {
        self->primary_monitor = GDK_MONITOR (g_list_model_get_item (monitors, 0));
    }
}

void
cora_shell_start (CoraShell *self)
{
    g_return_if_fail (CORA_IS_SHELL (self));

    if (self->running)
        return;

    g_message ("CoraOS Shell starting...");

    /* Detect displays */
    cora_shell_detect_monitors (self);

    /* Load CSS */
    cora_shell_load_css (self);

    /* Create wallpaper layer */
    self->wallpaper = cora_wallpaper_new (self);

    /* Create panel */
    self->panel = cora_panel_new (self);
    gtk_window_present (GTK_WINDOW (self->panel));

    /* Create launcher (hidden) */
    self->launcher = cora_launcher_new (self);

    self->running = TRUE;
    g_message ("CoraOS Shell started successfully");
}

void
cora_shell_stop (CoraShell *self)
{
    g_return_if_fail (CORA_IS_SHELL (self));

    if (!self->running)
        return;

    g_message ("CoraOS Shell stopping...");

    g_clear_object (&self->wallpaper);
    self->running = FALSE;
}

void
cora_shell_toggle_launcher (CoraShell *self)
{
    g_return_if_fail (CORA_IS_SHELL (self));

    if (self->launcher) {
        if (gtk_widget_get_visible (GTK_WIDGET (self->launcher)))
            cora_shell_hide_launcher (self);
        else
            cora_shell_show_launcher (self);
    }
}

void
cora_shell_show_launcher (CoraShell *self)
{
    g_return_if_fail (CORA_IS_SHELL (self));

    if (self->launcher) {
        gtk_window_present (GTK_WINDOW (self->launcher));
        cora_launcher_focus_search (self->launcher);
    }
}

void
cora_shell_hide_launcher (CoraShell *self)
{
    g_return_if_fail (CORA_IS_SHELL (self));

    if (self->launcher) {
        gtk_widget_set_visible (GTK_WIDGET (self->launcher), FALSE);
    }
}

CoraPanel *
cora_shell_get_panel (CoraShell *self)
{
    g_return_val_if_fail (CORA_IS_SHELL (self), NULL);
    return self->panel;
}

GSettings *
cora_shell_get_settings (CoraShell *self)
{
    g_return_val_if_fail (CORA_IS_SHELL (self), NULL);
    return self->settings;
}

GdkMonitor *
cora_shell_get_primary_monitor (CoraShell *self)
{
    g_return_val_if_fail (CORA_IS_SHELL (self), NULL);
    return self->primary_monitor;
}

static void
cora_shell_init (CoraShell *self)
{
    self->running = FALSE;
    self->panel = NULL;
    self->launcher = NULL;
    self->wallpaper = NULL;
    self->settings = NULL;
    self->primary_monitor = NULL;
}

static void
cora_shell_class_init (CoraShellClass *klass)
{
}

CoraShell *
cora_shell_new (void)
{
    return g_object_new (CORA_TYPE_SHELL,
                         "application-id", "dev.coraos.Shell",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
