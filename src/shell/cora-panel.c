/* CoraOS Panel - Top bar implementation
 * 
 * Features:
 * - Activities button (opens launcher)
 * - Center clock with date
 * - System tray: WiFi, Volume, Battery, Power
 * - Workspace indicators
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-panel.h"
#include "cora-shell.h"

#include <time.h>
/* UPower integration would go here - removed to avoid build dep */

struct _CoraPanel {
    GtkApplicationWindow parent_instance;

    CoraShell    *shell;

    /* Layout */
    GtkWidget    *panel_box;      /* Main horizontal box */
    GtkWidget    *left_box;       /* Activities + workspace */
    GtkWidget    *center_box;     /* Clock */
    GtkWidget    *right_box;      /* System tray */

    /* Widgets */
    GtkWidget    *activities_btn;
    GtkWidget    *clock_label;
    GtkWidget    *wifi_btn;
    GtkWidget    *volume_btn;
    GtkWidget    *battery_btn;
    GtkWidget    *power_btn;

    /* Timer */
    guint         clock_timer_id;
};

G_DEFINE_TYPE (CoraPanel, cora_panel, GTK_TYPE_APPLICATION_WINDOW)

static gboolean
on_clock_tick (gpointer user_data)
{
    CoraPanel *self = CORA_PANEL (user_data);
    time_t now;
    struct tm *tm_info;
    char buf[64];

    time (&now);
    tm_info = localtime (&now);

    /* Format: "Mon 15 Jan  14:30" */
    strftime (buf, sizeof (buf), "%a %d %b  %H:%M", tm_info);
    gtk_label_set_text (GTK_LABEL (self->clock_label), buf);

    return G_SOURCE_CONTINUE;
}

static void
on_activities_clicked (GtkButton *button, gpointer user_data)
{
    CoraPanel *self = CORA_PANEL (user_data);
    cora_shell_toggle_launcher (self->shell);
}

static void
on_wifi_clicked (GtkButton *button, gpointer user_data)
{
    /* Launch CoraOS WiFi manager */
    g_autoptr(GError) error = NULL;
    GAppInfo *app = g_app_info_create_from_commandline (
        "coraos-wifi", "WiFi", G_APP_INFO_CREATE_NONE, &error);
    
    if (app) {
        g_app_info_launch (app, NULL, NULL, &error);
        g_object_unref (app);
    }
    if (error)
        g_warning ("Failed to launch WiFi manager: %s", error->message);
}

static void
on_power_command_clicked (GtkButton *b, gpointer d)
{
    const char *cmd = (const char *)g_object_get_data (G_OBJECT (b), "cmd");
    g_autoptr(GError) err = NULL;
    if (!g_spawn_command_line_async (cmd, &err))
        g_warning ("Power command failed: %s", err->message);
}

static void
on_power_clicked (GtkButton *button, gpointer user_data)
{
    /* Show power menu (shutdown, restart, logout, suspend) */
    CoraPanel *self = CORA_PANEL (user_data);

    GtkWidget *popover = gtk_popover_new ();
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start (box, 8);
    gtk_widget_set_margin_end (box, 8);
    gtk_widget_set_margin_top (box, 8);
    gtk_widget_set_margin_bottom (box, 8);

    const char *labels[] = { "Lock Screen", "Suspend", "Restart", "Shut Down", "Log Out" };
    const char *commands[] = {
        "loginctl lock-session",
        "systemctl suspend",
        "systemctl reboot",
        "systemctl poweroff",
        "gnome-session-quit --logout --no-prompt"
    };

    for (int i = 0; i < 5; i++) {
        GtkWidget *btn = gtk_button_new_with_label (labels[i]);
        gtk_widget_add_css_class (btn, "cora-panel-button");
        gtk_widget_set_hexpand (btn, TRUE);
        g_object_set_data_full (G_OBJECT (btn), "cmd", g_strdup (commands[i]), g_free);
        g_signal_connect (btn, "clicked", G_CALLBACK (on_power_command_clicked), NULL);
        gtk_box_append (GTK_BOX (box), btn);
    }

    gtk_popover_set_child (GTK_POPOVER (popover), box);
    gtk_widget_set_parent (popover, GTK_WIDGET (button));
    gtk_popover_popup (GTK_POPOVER (popover));
}

static void
on_volume_scale_changed (GtkRange *range, gpointer d)
{
    int vol = (int) gtk_range_get_value (range);
    g_autofree char *cmd = g_strdup_printf (
        "pactl set-sink-volume @DEFAULT_SINK@ %d%%", vol);
    g_spawn_command_line_async (cmd, NULL);
}

static void
on_mute_toggled (GtkButton *b, gpointer d)
{
    g_spawn_command_line_async (
        "pactl set-sink-mute @DEFAULT_SINK@ toggle", NULL);
}

static void
on_volume_clicked (GtkButton *button, gpointer user_data)
{
    /* Simple volume slider popover using pactl */
    GtkWidget *popover = gtk_popover_new ();
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start (box, 12);
    gtk_widget_set_margin_end (box, 12);
    gtk_widget_set_margin_top (box, 12);
    gtk_widget_set_margin_bottom (box, 12);

    GtkWidget *label = gtk_label_new ("Volume");
    gtk_widget_add_css_class (label, "cora-panel-button");
    gtk_box_append (GTK_BOX (box), label);

    GtkWidget *scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value (GTK_RANGE (scale), 70);
    gtk_widget_set_size_request (scale, 200, -1);
    g_signal_connect (scale, "value-changed", G_CALLBACK (on_volume_scale_changed), NULL);
    gtk_box_append (GTK_BOX (box), scale);

    /* Mute toggle */
    GtkWidget *mute_btn = gtk_button_new_with_label ("Mute");
    gtk_widget_add_css_class (mute_btn, "cora-panel-button");
    g_signal_connect (mute_btn, "clicked", G_CALLBACK (on_mute_toggled), NULL);
    gtk_box_append (GTK_BOX (box), mute_btn);

    gtk_popover_set_child (GTK_POPOVER (popover), box);
    gtk_widget_set_parent (popover, GTK_WIDGET (button));
    gtk_popover_popup (GTK_POPOVER (popover));
}

static GtkWidget *
create_panel_button (const char *icon_name, const char *tooltip)
{
    GtkWidget *btn = gtk_button_new_from_icon_name (icon_name);
    gtk_widget_add_css_class (btn, "cora-panel-button");
    gtk_widget_set_tooltip_text (btn, tooltip);
    gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE);
    return btn;
}

static void
cora_panel_build_ui (CoraPanel *self)
{
    /* Configure window as panel (layer shell on Wayland, or fullwidth window) */
    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
    gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (self), -1, 32);

    /* Anchor to top of screen */
    if (self->shell) {
        GdkMonitor *mon = cora_shell_get_primary_monitor (self->shell);
        if (mon) {
            GdkRectangle geom;
            gdk_monitor_get_geometry (mon, &geom);
            gtk_window_set_default_size (GTK_WINDOW (self), geom.width, 32);
        }
    }

    gtk_widget_add_css_class (GTK_WIDGET (self), "cora-panel");

    /* Main layout: left | center | right */
    self->panel_box = gtk_center_box_new ();
    gtk_window_set_child (GTK_WINDOW (self), self->panel_box);

    /* === LEFT: Activities button === */
    self->left_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start (self->left_box, 8);

    self->activities_btn = gtk_button_new_with_label ("Activities");
    gtk_widget_add_css_class (self->activities_btn, "cora-panel-button");
    gtk_button_set_has_frame (GTK_BUTTON (self->activities_btn), FALSE);
    g_signal_connect (self->activities_btn, "clicked",
                      G_CALLBACK (on_activities_clicked), self);
    gtk_box_append (GTK_BOX (self->left_box), self->activities_btn);

    gtk_center_box_set_start_widget (GTK_CENTER_BOX (self->panel_box), self->left_box);

    /* === CENTER: Clock === */
    self->clock_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->clock_label, "cora-clock");
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (self->panel_box), self->clock_label);

    /* Start clock timer */
    on_clock_tick (self);
    self->clock_timer_id = g_timeout_add_seconds (1, on_clock_tick, self);

    /* === RIGHT: System tray === */
    self->right_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_end (self->right_box, 8);

    /* WiFi indicator */
    self->wifi_btn = create_panel_button ("network-wireless-symbolic", "WiFi");
    g_signal_connect (self->wifi_btn, "clicked", G_CALLBACK (on_wifi_clicked), self);
    gtk_box_append (GTK_BOX (self->right_box), self->wifi_btn);

    /* Volume */
    self->volume_btn = create_panel_button ("audio-volume-high-symbolic", "Volume");
    g_signal_connect (self->volume_btn, "clicked", G_CALLBACK (on_volume_clicked), self);
    gtk_box_append (GTK_BOX (self->right_box), self->volume_btn);

    /* Battery */
    self->battery_btn = create_panel_button ("battery-good-symbolic", "Battery");
    gtk_box_append (GTK_BOX (self->right_box), self->battery_btn);

    /* Power */
    self->power_btn = create_panel_button ("system-shutdown-symbolic", "Power");
    g_signal_connect (self->power_btn, "clicked", G_CALLBACK (on_power_clicked), self);
    gtk_box_append (GTK_BOX (self->right_box), self->power_btn);

    gtk_center_box_set_end_widget (GTK_CENTER_BOX (self->panel_box), self->right_box);
}

static void
cora_panel_dispose (GObject *object)
{
    CoraPanel *self = CORA_PANEL (object);

    if (self->clock_timer_id > 0) {
        g_source_remove (self->clock_timer_id);
        self->clock_timer_id = 0;
    }

    G_OBJECT_CLASS (cora_panel_parent_class)->dispose (object);
}

static void
cora_panel_init (CoraPanel *self)
{
    self->clock_timer_id = 0;
}

static void
cora_panel_class_init (CoraPanelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = cora_panel_dispose;
}

CoraPanel *
cora_panel_new (CoraShell *shell)
{
    CoraPanel *self = g_object_new (CORA_TYPE_PANEL,
                                    "application", shell,
                                    NULL);
    self->shell = shell;
    cora_panel_build_ui (self);
    return self;
}
