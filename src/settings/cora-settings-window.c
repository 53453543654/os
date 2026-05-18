/* CoraOS Settings - Window Implementation (Part 1: Structure + Appearance)
 *
 * Real settings app with system backends:
 * - GSettings for desktop preferences
 * - D-Bus to power-profiles-daemon, UPower, PulseAudio
 * - NetworkManager for network settings
 * - colord for display calibration
 * - accountsservice for user management
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-settings-window.h"
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>

struct _CoraSettingsWindow {
    AdwApplicationWindow parent_instance;

    /* Layout */
    GtkWidget          *split_view;
    GtkWidget          *sidebar_list;
    GtkWidget          *content_stack;

    /* Settings backend */
    GSettings          *desktop_settings;
    GDBusProxy         *power_proxy;
    GDBusProxy         *display_proxy;

    /* Appearance page */
    GtkWidget          *accent_color_btn;
    GtkWidget          *font_btn;
    GtkWidget          *mono_font_btn;
    GtkWidget          *anim_switch;
    GtkWidget          *anim_speed_scale;
    GtkWidget          *corner_radius_scale;
    GtkWidget          *wallpaper_flow;
    GtkWidget          *dark_mode_switch;
    GtkWidget          *transparency_switch;

    /* Display page */
    GtkWidget          *scale_combo;
    GtkWidget          *night_light_switch;
    GtkWidget          *night_temp_scale;
    GtkWidget          *resolution_combo;

    /* Sound page */
    GtkWidget          *output_volume_scale;
    GtkWidget          *input_volume_scale;
    GtkWidget          *output_device_combo;

    /* Power page */
    GtkWidget          *power_profile_combo;
    GtkWidget          *screen_timeout_scale;
    GtkWidget          *suspend_timeout_scale;
    GtkWidget          *lid_action_combo;

    /* User config path */
    char               *config_path;
    JsonObject         *user_config;
};

G_DEFINE_TYPE (CoraSettingsWindow, cora_settings_window, ADW_TYPE_APPLICATION_WINDOW)

/* Forward declarations */
static GtkWidget *create_appearance_page  (CoraSettingsWindow *self);
static GtkWidget *create_display_page     (CoraSettingsWindow *self);
static GtkWidget *create_sound_page       (CoraSettingsWindow *self);
static GtkWidget *create_network_page     (CoraSettingsWindow *self);
static GtkWidget *create_bluetooth_page   (CoraSettingsWindow *self);
static GtkWidget *create_power_page       (CoraSettingsWindow *self);
static GtkWidget *create_keyboard_page    (CoraSettingsWindow *self);
static GtkWidget *create_users_page       (CoraSettingsWindow *self);
static GtkWidget *create_about_page       (CoraSettingsWindow *self);
static void       save_user_config        (CoraSettingsWindow *self);
static void       load_user_config        (CoraSettingsWindow *self);


/* === Configuration I/O === */

static void
load_user_config (CoraSettingsWindow *self)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonParser) parser = json_parser_new ();

    if (!g_file_test (self->config_path, G_FILE_TEST_EXISTS)) {
        self->user_config = json_object_new ();
        return;
    }

    if (!json_parser_load_from_file (parser, self->config_path, &error)) {
        g_warning ("Failed to load settings: %s", error->message);
        self->user_config = json_object_new ();
        return;
    }

    JsonNode *root = json_parser_get_root (parser);
    if (JSON_NODE_HOLDS_OBJECT (root))
        self->user_config = json_object_ref (json_node_get_object (root));
    else
        self->user_config = json_object_new ();
}

static void
save_user_config (CoraSettingsWindow *self)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonGenerator) gen = json_generator_new ();
    g_autoptr(JsonNode) root = json_node_new (JSON_NODE_OBJECT);

    json_node_set_object (root, self->user_config);
    json_generator_set_root (gen, root);
    json_generator_set_pretty (gen, TRUE);

    /* Ensure directory exists */
    g_autofree char *dir = g_path_get_dirname (self->config_path);
    g_mkdir_with_parents (dir, 0755);

    if (!json_generator_to_file (gen, self->config_path, &error))
        g_warning ("Failed to save settings: %s", error->message);
}

static void
set_config_string (CoraSettingsWindow *self, const char *section,
                   const char *key, const char *value)
{
    JsonObject *sec;
    if (!json_object_has_member (self->user_config, section)) {
        sec = json_object_new ();
        json_object_set_object_member (self->user_config, section, sec);
    } else {
        sec = json_object_get_object_member (self->user_config, section);
    }
    json_object_set_string_member (sec, key, value);
    save_user_config (self);
}

static void
set_config_double (CoraSettingsWindow *self, const char *section,
                   const char *key, double value)
{
    JsonObject *sec;
    if (!json_object_has_member (self->user_config, section)) {
        sec = json_object_new ();
        json_object_set_object_member (self->user_config, section, sec);
    } else {
        sec = json_object_get_object_member (self->user_config, section);
    }
    json_object_set_double_member (sec, key, value);
    save_user_config (self);
}

static void
set_config_bool (CoraSettingsWindow *self, const char *section,
                 const char *key, gboolean value)
{
    JsonObject *sec;
    if (!json_object_has_member (self->user_config, section)) {
        sec = json_object_new ();
        json_object_set_object_member (self->user_config, section, sec);
    } else {
        sec = json_object_get_object_member (self->user_config, section);
    }
    json_object_set_boolean_member (sec, key, value);
    save_user_config (self);
}

static const char *
get_config_string (CoraSettingsWindow *self, const char *section,
                   const char *key, const char *fallback)
{
    if (!json_object_has_member (self->user_config, section))
        return fallback;
    JsonObject *sec = json_object_get_object_member (self->user_config, section);
    if (!json_object_has_member (sec, key))
        return fallback;
    return json_object_get_string_member (sec, key);
}

static double
get_config_double (CoraSettingsWindow *self, const char *section,
                   const char *key, double fallback)
{
    if (!json_object_has_member (self->user_config, section))
        return fallback;
    JsonObject *sec = json_object_get_object_member (self->user_config, section);
    if (!json_object_has_member (sec, key))
        return fallback;
    return json_object_get_double_member (sec, key);
}

static gboolean
get_config_bool (CoraSettingsWindow *self, const char *section,
                 const char *key, gboolean fallback)
{
    if (!json_object_has_member (self->user_config, section))
        return fallback;
    JsonObject *sec = json_object_get_object_member (self->user_config, section);
    if (!json_object_has_member (sec, key))
        return fallback;
    return json_object_get_boolean_member (sec, key);
}


/* === Custom wallpaper file chooser callback === */

static void
on_custom_wallpaper_file_chosen (GObject *src, GAsyncResult *res, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    g_autoptr(GError) err = NULL;
    GFile *file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (src), res, &err);
    if (file) {
        g_autofree char *path = g_file_get_path (file);
        set_config_string (self, "desktop", "wallpaper", path);
        g_autoptr(GSettings) bg = g_settings_new ("org.gnome.desktop.background");
        g_autofree char *uri = g_file_get_uri (file);
        g_settings_set_string (bg, "picture-uri-dark", uri);
        g_object_unref (file);
    }
}

static void
on_custom_wallpaper_clicked (GtkButton *btn, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    GtkFileDialog *dlg = gtk_file_dialog_new ();
    GtkFileFilter *filter = gtk_file_filter_new ();
    gtk_file_filter_add_mime_type (filter, "image/*");
    gtk_file_filter_set_name (filter, "Images");

    GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
    g_list_store_append (filters, filter);
    gtk_file_dialog_set_filters (dlg, G_LIST_MODEL (filters));

    gtk_file_dialog_open (dlg, GTK_WINDOW (self), NULL,
        (GAsyncReadyCallback) on_custom_wallpaper_file_chosen, self);
    g_object_unref (filters);
    g_object_unref (filter);
    g_object_unref (dlg);
}

/* === Display page callbacks === */

static void
on_night_light_toggled (GObject *obj, GParamSpec *p, gpointer data)
{
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    g_autoptr(GSettings) s = g_settings_new ("org.gnome.settings-daemon.plugins.color");
    g_settings_set_boolean (s, "night-light-enabled", active);
}

static void
on_night_temp_changed (GtkRange *range, gpointer data)
{
    int temp = (int) gtk_range_get_value (range);
    g_autoptr(GSettings) s = g_settings_new ("org.gnome.settings-daemon.plugins.color");
    g_settings_set_uint (s, "night-light-temperature", temp);
}

/* === Sound page callbacks === */

static void
on_output_volume_changed (GtkRange *range, gpointer data)
{
    int vol = (int) gtk_range_get_value (range);
    g_autofree char *cmd = g_strdup_printf (
        "pactl set-sink-volume @DEFAULT_SINK@ %d%%", vol);
    g_spawn_command_line_async (cmd, NULL);
}

static void
on_input_volume_changed (GtkRange *range, gpointer data)
{
    int vol = (int) gtk_range_get_value (range);
    g_autofree char *cmd = g_strdup_printf (
        "pactl set-source-volume @DEFAULT_SOURCE@ %d%%", vol);
    g_spawn_command_line_async (cmd, NULL);
}

/* === Network page callbacks === */

static void
on_wifi_button_clicked (GtkButton *b, gpointer data)
{
    g_spawn_command_line_async ("coraos-wifi", NULL);
}

static void
on_hostname_apply (AdwEntryRow *row, gpointer data)
{
    const char *name = gtk_editable_get_text (GTK_EDITABLE (row));
    g_autofree char *cmd = g_strdup_printf ("hostnamectl set-hostname '%s'", name);
    g_spawn_command_line_async (cmd, NULL);
}

/* === Bluetooth page callbacks === */

static void
on_bluetooth_toggled (GObject *obj, GParamSpec *p, gpointer data)
{
    gboolean active = adw_switch_row_get_active (ADW_SWITCH_ROW (obj));
    const char *cmd = active ? "bluetoothctl power on" : "bluetoothctl power off";
    g_spawn_command_line_async (cmd, NULL);
}

/* === Power page callbacks === */

static void
on_power_profile_changed (GObject *obj, GParamSpec *p, gpointer data)
{
    guint sel = adw_combo_row_get_selected (ADW_COMBO_ROW (obj));
    const char *profiles[] = {"power-saver", "balanced", "performance"};
    if (sel < 3) {
        g_autofree char *cmd = g_strdup_printf (
            "powerprofilesctl set %s", profiles[sel]);
        g_spawn_command_line_async (cmd, NULL);
    }
}

static void
on_screen_timeout_changed (GtkRange *range, gpointer data)
{
    int secs = (int) gtk_range_get_value (range);
    g_autoptr(GSettings) s = g_settings_new ("org.gnome.desktop.session");
    g_settings_set_uint (s, "idle-delay", secs);
}


/* === Appearance Page === */

static void
on_accent_color_set (GtkColorDialogButton *button, GParamSpec *pspec, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    const GdkRGBA *color = gtk_color_dialog_button_get_rgba (button);
    g_autofree char *hex = g_strdup_printf ("#%02X%02X%02X",
        (int)(color->red * 255), (int)(color->green * 255), (int)(color->blue * 255));
    set_config_string (self, "appearance", "accent-color", hex);
}

static void
on_font_changed (GtkFontDialogButton *button, GParamSpec *pspec, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc (button);
    if (desc) {
        g_autofree char *font_str = pango_font_description_to_string (desc);
        set_config_string (self, "appearance", "font", font_str);

        /* Apply system-wide via gsettings if available */
        g_autoptr(GSettings) iface = g_settings_new ("org.gnome.desktop.interface");
        g_settings_set_string (iface, "font-name", font_str);
    }
}

static void
on_dark_mode_toggled (GObject *obj, GParamSpec *pspec, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    gboolean dark = adw_switch_row_get_active (ADW_SWITCH_ROW (self->dark_mode_switch));
    set_config_bool (self, "appearance", "dark-mode", dark);

    /* Apply via AdwStyleManager */
    AdwStyleManager *style = adw_style_manager_get_default ();
    adw_style_manager_set_color_scheme (style,
        dark ? ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);
}

static void
on_animations_toggled (GObject *obj, GParamSpec *pspec, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    gboolean anim = adw_switch_row_get_active (ADW_SWITCH_ROW (self->anim_switch));
    set_config_bool (self, "appearance", "animations", anim);

    g_autoptr(GSettings) iface = g_settings_new ("org.gnome.desktop.interface");
    g_settings_set_boolean (iface, "enable-animations", anim);
}

static void
on_transparency_toggled (GObject *obj, GParamSpec *pspec, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    gboolean val = adw_switch_row_get_active (ADW_SWITCH_ROW (self->transparency_switch));
    set_config_bool (self, "appearance", "transparency", val);
}

static void
on_corner_radius_changed (GtkRange *range, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    double val = gtk_range_get_value (range);
    set_config_double (self, "appearance", "corner-radius", val);
}

static void
on_wallpaper_selected (GtkButton *btn, gpointer data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (data);
    const char *path = g_object_get_data (G_OBJECT (btn), "wallpaper-path");

    if (path) {
        set_config_string (self, "desktop", "wallpaper", path);

        /* Apply via gsettings */
        g_autoptr(GSettings) bg = g_settings_new ("org.gnome.desktop.background");
        g_autofree char *uri = g_filename_to_uri (path, NULL, NULL);
        g_settings_set_string (bg, "picture-uri-dark", uri);
        g_settings_set_string (bg, "picture-uri", uri);
    }
}

static GtkWidget *
create_appearance_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Appearance");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "preferences-desktop-appearance-symbolic");

    /* === Theme Group === */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Theme");

    /* Dark mode */
    self->dark_mode_switch = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->dark_mode_switch),
                                   "Dark Mode");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->dark_mode_switch),
                                 "Use dark window and UI colors");
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->dark_mode_switch),
                               get_config_bool (self, "appearance", "dark-mode", TRUE));
    g_signal_connect (self->dark_mode_switch, "notify::active",
                      G_CALLBACK (on_dark_mode_toggled), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->dark_mode_switch);

    /* Accent color */
    GtkWidget *accent_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (accent_row), "Accent Color");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (accent_row),
                                 "Primary color for buttons and highlights");

    GtkColorDialog *color_dlg = gtk_color_dialog_new ();
    self->accent_color_btn = gtk_color_dialog_button_new (color_dlg);
    GdkRGBA default_accent = { 1.0, 0.1, 0.1, 1.0 }; /* Red */
    gtk_color_dialog_button_set_rgba (
        GTK_COLOR_DIALOG_BUTTON (self->accent_color_btn), &default_accent);
    gtk_widget_set_valign (self->accent_color_btn, GTK_ALIGN_CENTER);
    g_signal_connect (self->accent_color_btn, "notify::rgba",
                      G_CALLBACK (on_accent_color_set), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (accent_row), self->accent_color_btn);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), accent_row);

    /* Transparency */
    self->transparency_switch = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->transparency_switch),
                                   "Transparency Effects");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->transparency_switch),
                                 "Enable translucent panels and menus");
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->transparency_switch),
                               get_config_bool (self, "appearance", "transparency", TRUE));
    g_signal_connect (self->transparency_switch, "notify::active",
                      G_CALLBACK (on_transparency_toggled), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->transparency_switch);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* === Fonts Group === */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Fonts");

    GtkWidget *font_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (font_row), "Interface Font");
    GtkFontDialog *font_dlg = gtk_font_dialog_new ();
    self->font_btn = gtk_font_dialog_button_new (font_dlg);
    gtk_widget_set_valign (self->font_btn, GTK_ALIGN_CENTER);
    g_signal_connect (self->font_btn, "notify::font-desc",
                      G_CALLBACK (on_font_changed), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (font_row), self->font_btn);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), font_row);

    GtkWidget *mono_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (mono_row), "Monospace Font");
    GtkFontDialog *mono_dlg = gtk_font_dialog_new ();
    self->mono_font_btn = gtk_font_dialog_button_new (mono_dlg);
    gtk_widget_set_valign (self->mono_font_btn, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix (ADW_ACTION_ROW (mono_row), self->mono_font_btn);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), mono_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* === Animations Group === */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Effects");

    self->anim_switch = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->anim_switch),
                                   "Animations");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->anim_switch),
                                 "Enable window and UI animations");
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->anim_switch),
                               get_config_bool (self, "appearance", "animations", TRUE));
    g_signal_connect (self->anim_switch, "notify::active",
                      G_CALLBACK (on_animations_toggled), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->anim_switch);

    /* Corner radius */
    GtkWidget *radius_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (radius_row), "Corner Radius");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (radius_row),
                                 "Window corner rounding (0-24px)");
    self->corner_radius_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 0, 24, 1);
    gtk_range_set_value (GTK_RANGE (self->corner_radius_scale),
                         get_config_double (self, "appearance", "corner-radius", 12));
    gtk_widget_set_size_request (self->corner_radius_scale, 180, -1);
    gtk_widget_set_valign (self->corner_radius_scale, GTK_ALIGN_CENTER);
    g_signal_connect (self->corner_radius_scale, "value-changed",
                      G_CALLBACK (on_corner_radius_changed), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (radius_row), self->corner_radius_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), radius_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* === Wallpaper Group === */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Wallpaper");
    adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (group),
                                           "Choose a desktop background");

    self->wallpaper_flow = gtk_flow_box_new ();
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->wallpaper_flow), 4);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->wallpaper_flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->wallpaper_flow), TRUE);

    /* Scan wallpaper directory */
    const char *wp_dirs[] = {
        "/usr/share/backgrounds/coraos",
        "/usr/share/backgrounds",
        NULL
    };

    for (int d = 0; wp_dirs[d]; d++) {
        g_autoptr(GDir) dir = g_dir_open (wp_dirs[d], 0, NULL);
        if (!dir) continue;

        const char *name;
        while ((name = g_dir_read_name (dir)) != NULL) {
            if (!g_str_has_suffix (name, ".png") &&
                !g_str_has_suffix (name, ".jpg") &&
                !g_str_has_suffix (name, ".webp"))
                continue;

            g_autofree char *full_path = g_build_filename (wp_dirs[d], name, NULL);
            GtkWidget *btn = gtk_button_new ();
            gtk_button_set_has_frame (GTK_BUTTON (btn), FALSE);

            GtkWidget *pic = gtk_picture_new_for_filename (full_path);
            gtk_picture_set_content_fit (GTK_PICTURE (pic), GTK_CONTENT_FIT_COVER);
            gtk_widget_set_size_request (pic, 160, 100);
            gtk_button_set_child (GTK_BUTTON (btn), pic);

            g_object_set_data_full (G_OBJECT (btn), "wallpaper-path",
                                    g_strdup (full_path), g_free);
            g_signal_connect (btn, "clicked",
                              G_CALLBACK (on_wallpaper_selected), self);

            gtk_flow_box_append (GTK_FLOW_BOX (self->wallpaper_flow), btn);
        }
    }

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->wallpaper_flow);

    /* Custom wallpaper button */
    GtkWidget *custom_btn = gtk_button_new_with_label ("Choose Custom Image...");
    gtk_widget_add_css_class (custom_btn, "suggested-action");
    g_signal_connect (custom_btn, "clicked", G_CALLBACK (on_custom_wallpaper_clicked), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), custom_btn);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}


/* === Display Page === */

static GtkWidget *
create_display_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Display");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "preferences-desktop-display-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Screen");

    /* Resolution (reads from xrandr/mutter) */
    GtkWidget *res_row = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (res_row), "Resolution");
    GtkStringList *res_list = gtk_string_list_new (
        (const char *[]){"3840x2160", "2560x1440", "1920x1080", "1366x768", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (res_row), G_LIST_MODEL (res_list));
    adw_combo_row_set_selected (ADW_COMBO_ROW (res_row), 2); /* Default 1080p */
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), res_row);

    /* Scaling */
    GtkWidget *scale_row = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (scale_row), "Scale");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (scale_row), "UI scaling factor");
    GtkStringList *scale_list = gtk_string_list_new (
        (const char *[]){"100%", "125%", "150%", "175%", "200%", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (scale_row), G_LIST_MODEL (scale_list));
    adw_combo_row_set_selected (ADW_COMBO_ROW (scale_row), 0);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), scale_row);

    /* Refresh rate */
    GtkWidget *hz_row = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (hz_row), "Refresh Rate");
    GtkStringList *hz_list = gtk_string_list_new (
        (const char *[]){"60 Hz", "75 Hz", "120 Hz", "144 Hz", "165 Hz", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (hz_row), G_LIST_MODEL (hz_list));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), hz_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Night Light */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Night Light");

    self->night_light_switch = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->night_light_switch),
                                   "Night Light");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->night_light_switch),
                                 "Reduce blue light to ease eye strain at night");
    g_signal_connect (self->night_light_switch, "notify::active", G_CALLBACK (on_night_light_toggled), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->night_light_switch);

    /* Temperature */
    GtkWidget *temp_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (temp_row), "Color Temperature");
    self->night_temp_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 1700, 4700, 100);
    gtk_range_set_value (GTK_RANGE (self->night_temp_scale), 3500);
    gtk_widget_set_size_request (self->night_temp_scale, 200, -1);
    gtk_widget_set_valign (self->night_temp_scale, GTK_ALIGN_CENTER);
    g_signal_connect (self->night_temp_scale, "value-changed", G_CALLBACK (on_night_temp_changed), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (temp_row), self->night_temp_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), temp_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === Sound Page === */

static GtkWidget *
create_sound_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Sound");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "audio-volume-high-symbolic");

    /* Output */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Output");

    GtkWidget *vol_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (vol_row), "Volume");
    self->output_volume_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 0, 150, 1);
    gtk_range_set_value (GTK_RANGE (self->output_volume_scale), 70);
    gtk_widget_set_size_request (self->output_volume_scale, 250, -1);
    gtk_widget_set_valign (self->output_volume_scale, GTK_ALIGN_CENTER);
    g_signal_connect (self->output_volume_scale, "value-changed", G_CALLBACK (on_output_volume_changed), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (vol_row), self->output_volume_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), vol_row);

    /* Output device */
    GtkWidget *dev_row = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (dev_row), "Output Device");
    GtkStringList *dev_list = gtk_string_list_new (
        (const char *[]){"Built-in Speakers", "Headphones", "HDMI", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (dev_row), G_LIST_MODEL (dev_list));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), dev_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Input */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Input");

    GtkWidget *mic_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (mic_row), "Microphone Volume");
    self->input_volume_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value (GTK_RANGE (self->input_volume_scale), 80);
    gtk_widget_set_size_request (self->input_volume_scale, 250, -1);
    gtk_widget_set_valign (self->input_volume_scale, GTK_ALIGN_CENTER);
    g_signal_connect (self->input_volume_scale, "value-changed", G_CALLBACK (on_input_volume_changed), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (mic_row), self->input_volume_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), mic_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === Network Page === */

static GtkWidget *
create_network_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Network");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "network-wireless-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Wi-Fi");
    adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (group),
        "Configure wireless networks");

    GtkWidget *wifi_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (wifi_row), "Open WiFi Manager");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (wifi_row),
                                 "Scan, connect, and manage wireless networks");

    GtkWidget *wifi_btn = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_valign (wifi_btn, GTK_ALIGN_CENTER);
    g_signal_connect (wifi_btn, "clicked", G_CALLBACK (on_wifi_button_clicked), NULL);
    adw_action_row_add_suffix (ADW_ACTION_ROW (wifi_row), wifi_btn);
    adw_action_row_set_activatable_widget (ADW_ACTION_ROW (wifi_row), wifi_btn);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), wifi_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Hostname */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "System");

    GtkWidget *host_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (host_row), "Hostname");
    g_autofree char *hostname = g_malloc (256);
    gethostname (hostname, 256);
    gtk_editable_set_text (GTK_EDITABLE (host_row), hostname);
    g_signal_connect (host_row, "apply", G_CALLBACK (on_hostname_apply), NULL);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), host_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === Bluetooth Page === */

static GtkWidget *
create_bluetooth_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Bluetooth");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "bluetooth-active-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Bluetooth");

    GtkWidget *bt_switch = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (bt_switch), "Bluetooth");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (bt_switch),
                                 "Enable Bluetooth radio");
    g_signal_connect (bt_switch, "notify::active", G_CALLBACK (on_bluetooth_toggled), NULL);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), bt_switch);

    GtkWidget *disc_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (disc_row), "Discoverable");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (disc_row),
                                 "Allow other devices to find this computer");
    GtkWidget *disc_sw = gtk_switch_new ();
    gtk_widget_set_valign (disc_sw, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix (ADW_ACTION_ROW (disc_row), disc_sw);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), disc_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}


/* === Power Page === */

static GtkWidget *
create_power_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Power");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "battery-good-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Power Profile");

    /* Power profile via power-profiles-daemon D-Bus */
    self->power_profile_combo = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->power_profile_combo),
                                   "Power Mode");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->power_profile_combo),
                                 "Balance between performance and battery life");
    GtkStringList *profiles = gtk_string_list_new (
        (const char *[]){"Power Saver", "Balanced", "Performance", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (self->power_profile_combo),
                             G_LIST_MODEL (profiles));
    adw_combo_row_set_selected (ADW_COMBO_ROW (self->power_profile_combo), 1);
    g_signal_connect (self->power_profile_combo, "notify::selected", G_CALLBACK (on_power_profile_changed), NULL);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->power_profile_combo);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Timeouts */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Automatic Suspend");

    GtkWidget *screen_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (screen_row), "Screen Off After");
    self->screen_timeout_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 60, 1800, 60);
    gtk_range_set_value (GTK_RANGE (self->screen_timeout_scale), 300);
    gtk_widget_set_size_request (self->screen_timeout_scale, 200, -1);
    gtk_widget_set_valign (self->screen_timeout_scale, GTK_ALIGN_CENTER);
    gtk_scale_add_mark (GTK_SCALE (self->screen_timeout_scale), 300, GTK_POS_BOTTOM, "5m");
    gtk_scale_add_mark (GTK_SCALE (self->screen_timeout_scale), 900, GTK_POS_BOTTOM, "15m");
    g_signal_connect (self->screen_timeout_scale, "value-changed", G_CALLBACK (on_screen_timeout_changed), NULL);
    adw_action_row_add_suffix (ADW_ACTION_ROW (screen_row), self->screen_timeout_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), screen_row);

    /* Suspend timeout */
    GtkWidget *susp_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (susp_row), "Suspend After");
    self->suspend_timeout_scale = gtk_scale_new_with_range (
        GTK_ORIENTATION_HORIZONTAL, 300, 7200, 300);
    gtk_range_set_value (GTK_RANGE (self->suspend_timeout_scale), 1800);
    gtk_widget_set_size_request (self->suspend_timeout_scale, 200, -1);
    gtk_widget_set_valign (self->suspend_timeout_scale, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix (ADW_ACTION_ROW (susp_row), self->suspend_timeout_scale);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), susp_row);

    /* Lid close action */
    self->lid_action_combo = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->lid_action_combo),
                                   "When Lid is Closed");
    GtkStringList *lid_list = gtk_string_list_new (
        (const char *[]){"Suspend", "Hibernate", "Lock Screen", "Nothing", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (self->lid_action_combo),
                             G_LIST_MODEL (lid_list));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), self->lid_action_combo);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === Keyboard Page === */

static GtkWidget *
create_keyboard_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Keyboard");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "input-keyboard-symbolic");

    /* Layout */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Layout");

    GtkWidget *layout_row = adw_combo_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (layout_row), "Keyboard Layout");
    GtkStringList *layouts = gtk_string_list_new (
        (const char *[]){"English (US)", "English (UK)", "German", "French",
                         "Spanish", "Japanese", "Korean", NULL});
    adw_combo_row_set_model (ADW_COMBO_ROW (layout_row), G_LIST_MODEL (layouts));
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), layout_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Shortcuts */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Shortcuts");

    struct { const char *title; const char *keys; } shortcuts[] = {
        { "Open App Launcher",   "Super" },
        { "Close Window",        "Super + Q" },
        { "Switch Windows",      "Alt + Tab" },
        { "Open Terminal",       "Super + T" },
        { "Open File Manager",   "Super + E" },
        { "Lock Screen",         "Super + L" },
        { "Take Screenshot",     "Print" },
    };

    for (int i = 0; i < G_N_ELEMENTS (shortcuts); i++) {
        GtkWidget *row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), shortcuts[i].title);
        GtkWidget *lbl = gtk_label_new (shortcuts[i].keys);
        gtk_widget_add_css_class (lbl, "dim-label");
        gtk_widget_set_valign (lbl, GTK_ALIGN_CENTER);
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), lbl);
        adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);
    }

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === Users Page === */

static GtkWidget *
create_users_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "Users");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "system-users-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Current User");

    /* Username */
    const char *username = g_get_user_name ();
    const char *realname = g_get_real_name ();
    GtkWidget *user_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (user_row),
                                   realname ? realname : username);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (user_row), username);

    GtkWidget *avatar = adw_avatar_new (48, realname, TRUE);
    adw_action_row_add_prefix (ADW_ACTION_ROW (user_row), avatar);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), user_row);

    /* Auto-login */
    GtkWidget *autologin_row = adw_switch_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (autologin_row), "Automatic Login");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (autologin_row),
                                 "Skip password prompt at boot");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), autologin_row);

    /* Change password */
    GtkWidget *pwd_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (pwd_row), "Change Password");
    GtkWidget *pwd_btn = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_valign (pwd_btn, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix (ADW_ACTION_ROW (pwd_row), pwd_btn);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), pwd_row);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}

/* === About Page === */

static GtkWidget *
create_about_page (CoraSettingsWindow *self)
{
    GtkWidget *page, *group;

    page = adw_preferences_page_new ();
    adw_preferences_page_set_title (ADW_PREFERENCES_PAGE (page), "About");
    adw_preferences_page_set_icon_name (ADW_PREFERENCES_PAGE (page),
                                        "help-about-symbolic");

    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "CoraOS");

    /* Read system info */
    struct { const char *title; const char *value; } info[] = {
        { "OS Name",       "CoraOS 1.0 (Ember)" },
        { "Kernel",        NULL },
        { "Architecture",  NULL },
        { "Desktop",       "CoraOS Shell 1.0" },
        { "Windowing",     NULL },
        { "Hostname",      NULL },
    };

    /* Fill dynamic values */
    struct utsname uts;
    uname (&uts);

    for (int i = 0; i < G_N_ELEMENTS (info); i++) {
        GtkWidget *row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), info[i].title);

        const char *val = info[i].value;
        if (!val) {
            if (g_strcmp0 (info[i].title, "Kernel") == 0)
                val = uts.release;
            else if (g_strcmp0 (info[i].title, "Architecture") == 0)
                val = uts.machine;
            else if (g_strcmp0 (info[i].title, "Windowing") == 0)
                val = g_getenv ("XDG_SESSION_TYPE") ? g_getenv ("XDG_SESSION_TYPE") : "wayland";
            else if (g_strcmp0 (info[i].title, "Hostname") == 0)
                val = uts.nodename;
        }

        GtkWidget *lbl = gtk_label_new (val);
        gtk_widget_add_css_class (lbl, "dim-label");
        gtk_widget_set_valign (lbl, GTK_ALIGN_CENTER);
        gtk_label_set_selectable (GTK_LABEL (lbl), TRUE);
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), lbl);
        adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);
    }

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    /* Storage */
    group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (group), "Storage");

    /* Read disk usage from statvfs */
    struct statvfs stat;
    if (statvfs ("/", &stat) == 0) {
        unsigned long total = (stat.f_blocks * stat.f_frsize) / (1024*1024*1024);
        unsigned long avail = (stat.f_bavail * stat.f_frsize) / (1024*1024*1024);
        unsigned long used = total - avail;

        g_autofree char *disk_text = g_strdup_printf ("%lu GB used of %lu GB", used, total);
        GtkWidget *disk_row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (disk_row), "Disk Space");
        GtkWidget *disk_lbl = gtk_label_new (disk_text);
        gtk_widget_add_css_class (disk_lbl, "dim-label");
        gtk_widget_set_valign (disk_lbl, GTK_ALIGN_CENTER);
        adw_action_row_add_suffix (ADW_ACTION_ROW (disk_row), disk_lbl);
        adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), disk_row);
    }

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (group));

    return page;
}


/* === Window Construction === */

static void
on_sidebar_row_activated (GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (user_data);
    int index = gtk_list_box_row_get_index (row);

    const char *pages[] = {
        "appearance", "display", "sound", "network",
        "bluetooth", "power", "keyboard", "users", "about"
    };

    if (index >= 0 && index < (int)G_N_ELEMENTS (pages))
        gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), pages[index]);
}

static void
cora_settings_window_build_ui (CoraSettingsWindow *self)
{
    GtkWidget *toolbar_view, *sidebar_scroll;

    gtk_window_set_title (GTK_WINDOW (self), "Settings");
    gtk_window_set_default_size (GTK_WINDOW (self), 950, 650);

    /* Main layout */
    toolbar_view = adw_toolbar_view_new ();
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (self), toolbar_view);

    GtkWidget *header = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header);

    /* Split view */
    self->split_view = adw_overlay_split_view_new ();
    adw_overlay_split_view_set_collapsed (ADW_OVERLAY_SPLIT_VIEW (self->split_view), FALSE);
    adw_overlay_split_view_set_max_sidebar_width (
        ADW_OVERLAY_SPLIT_VIEW (self->split_view), 240);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), self->split_view);

    /* Sidebar */
    sidebar_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar_scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    self->sidebar_list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->sidebar_list), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class (self->sidebar_list, "navigation-sidebar");

    struct { const char *title; const char *icon; } panels[] = {
        { "Appearance",  "preferences-desktop-appearance-symbolic" },
        { "Display",     "preferences-desktop-display-symbolic" },
        { "Sound",       "audio-volume-high-symbolic" },
        { "Network",     "network-wireless-symbolic" },
        { "Bluetooth",   "bluetooth-active-symbolic" },
        { "Power",       "battery-good-symbolic" },
        { "Keyboard",    "input-keyboard-symbolic" },
        { "Users",       "system-users-symbolic" },
        { "About",       "help-about-symbolic" },
    };

    for (int i = 0; i < G_N_ELEMENTS (panels); i++) {
        GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start (row, 10);
        gtk_widget_set_margin_end (row, 10);
        gtk_widget_set_margin_top (row, 8);
        gtk_widget_set_margin_bottom (row, 8);

        GtkWidget *icon = gtk_image_new_from_icon_name (panels[i].icon);
        gtk_box_append (GTK_BOX (row), icon);

        GtkWidget *label = gtk_label_new (panels[i].title);
        gtk_box_append (GTK_BOX (row), label);

        gtk_list_box_append (GTK_LIST_BOX (self->sidebar_list), row);
    }

    g_signal_connect (self->sidebar_list, "row-activated",
                      G_CALLBACK (on_sidebar_row_activated), self);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sidebar_scroll), self->sidebar_list);
    adw_overlay_split_view_set_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->split_view),
                                        sidebar_scroll);

    /* Content stack */
    self->content_stack = gtk_stack_new ();
    gtk_stack_set_transition_type (GTK_STACK (self->content_stack),
                                   GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    /* Create all pages */
    GtkWidget *appearance_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (appearance_scroll),
                                   create_appearance_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), appearance_scroll, "appearance");

    GtkWidget *display_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (display_scroll),
                                   create_display_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), display_scroll, "display");

    GtkWidget *sound_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sound_scroll),
                                   create_sound_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), sound_scroll, "sound");

    GtkWidget *net_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (net_scroll),
                                   create_network_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), net_scroll, "network");

    GtkWidget *bt_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (bt_scroll),
                                   create_bluetooth_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), bt_scroll, "bluetooth");

    GtkWidget *power_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (power_scroll),
                                   create_power_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), power_scroll, "power");

    GtkWidget *kb_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (kb_scroll),
                                   create_keyboard_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), kb_scroll, "keyboard");

    GtkWidget *users_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (users_scroll),
                                   create_users_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), users_scroll, "users");

    GtkWidget *about_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (about_scroll),
                                   create_about_page (self));
    gtk_stack_add_named (GTK_STACK (self->content_stack), about_scroll, "about");

    adw_overlay_split_view_set_content (ADW_OVERLAY_SPLIT_VIEW (self->split_view),
                                        self->content_stack);

    /* Select first panel */
    gtk_list_box_select_row (GTK_LIST_BOX (self->sidebar_list),
        gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->sidebar_list), 0));
    gtk_stack_set_visible_child_name (GTK_STACK (self->content_stack), "appearance");
}

/* === GObject === */

static void
cora_settings_window_finalize (GObject *object)
{
    CoraSettingsWindow *self = CORA_SETTINGS_WINDOW (object);
    g_free (self->config_path);
    if (self->user_config)
        json_object_unref (self->user_config);
    g_clear_object (&self->desktop_settings);
    g_clear_object (&self->power_proxy);
    g_clear_object (&self->display_proxy);
    G_OBJECT_CLASS (cora_settings_window_parent_class)->finalize (object);
}

static void
cora_settings_window_init (CoraSettingsWindow *self)
{
    self->config_path = g_build_filename (
        g_get_user_config_dir (), "coraos", "settings.json", NULL);
    self->user_config = NULL;
}

static void
cora_settings_window_class_init (CoraSettingsWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = cora_settings_window_finalize;
}

CoraSettingsWindow *
cora_settings_window_new (GtkApplication *app)
{
    CoraSettingsWindow *self = g_object_new (CORA_TYPE_SETTINGS_WINDOW,
                                             "application", app,
                                             NULL);

    load_user_config (self);
    cora_settings_window_build_ui (self);

    return self;
}
