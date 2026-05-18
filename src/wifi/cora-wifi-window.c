/* CoraOS WiFi Manager - Main Window Implementation
 * 
 * Uses libnm (NetworkManager) to:
 * - List WiFi devices
 * - Scan for access points
 * - Show signal strength, security type
 * - Connect with password dialog
 * - Manage saved connections
 * - Toggle WiFi on/off
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-wifi-window.h"

struct _CoraWifiWindow {
    AdwApplicationWindow parent_instance;

    /* NetworkManager */
    NMClient       *nm_client;
    NMDeviceWifi   *wifi_device;

    /* UI Widgets */
    GtkWidget      *header_bar;
    GtkWidget      *main_stack;
    GtkWidget      *wifi_switch;
    GtkWidget      *scan_button;
    GtkWidget      *spinner;
    GtkWidget      *network_list;
    GtkWidget      *saved_list;
    GtkWidget      *status_page;

    /* State */
    gboolean        scanning;
    gulong          ap_added_id;
    gulong          ap_removed_id;
};

G_DEFINE_TYPE (CoraWifiWindow, cora_wifi_window, ADW_TYPE_APPLICATION_WINDOW)

/* Forward declarations */
static void refresh_network_list   (CoraWifiWindow *self);
static void on_scan_done           (GObject *source, GAsyncResult *res, gpointer user_data);
static void connect_to_ap          (CoraWifiWindow *self, NMAccessPoint *ap);
static void show_password_dialog   (CoraWifiWindow *self, NMAccessPoint *ap);

/* === Utility Functions === */

static const char *
security_to_string (NMAccessPoint *ap)
{
    NM80211ApFlags flags = nm_access_point_get_flags (ap);
    NM80211ApSecurityFlags wpa = nm_access_point_get_wpa_flags (ap);
    NM80211ApSecurityFlags rsn = nm_access_point_get_rsn_flags (ap);

    if ((flags & NM_802_11_AP_FLAGS_PRIVACY) && !wpa && !rsn)
        return "WEP";
    if (wpa)
        return "WPA";
    if (rsn)
        return "WPA2";
    if (flags == NM_802_11_AP_FLAGS_NONE)
        return "Open";

    return "Secured";
}

static const char *
signal_icon_name (guint8 strength)
{
    if (strength > 75)
        return "network-wireless-signal-excellent-symbolic";
    else if (strength > 50)
        return "network-wireless-signal-good-symbolic";
    else if (strength > 25)
        return "network-wireless-signal-ok-symbolic";
    else
        return "network-wireless-signal-weak-symbolic";
}

/* === Network List === */

static void
on_connect_btn_clicked (GtkButton *btn, gpointer data)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (data);
    NMAccessPoint *ap = g_object_get_data (G_OBJECT (btn), "ap");
    connect_to_ap (self, ap);
}

static GtkWidget *
create_network_row (CoraWifiWindow *self, NMAccessPoint *ap)
{
    GtkWidget *row, *icon, *connect_btn;
    GBytes *ssid_bytes;
    g_autofree char *ssid = NULL;
    guint8 strength;
    const char *security;

    ssid_bytes = nm_access_point_get_ssid (ap);
    if (!ssid_bytes)
        return NULL;

    ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid_bytes, NULL),
                                   g_bytes_get_size (ssid_bytes));
    if (!ssid || ssid[0] == '\0')
        return NULL;

    strength = nm_access_point_get_strength (ap);
    security = security_to_string (ap);

    /* Row */
    row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), ssid);

    /* Subtitle with signal and security */
    g_autofree char *subtitle = g_strdup_printf ("%s · %d%%", security, strength);
    adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);

    /* Signal icon */
    icon = gtk_image_new_from_icon_name (signal_icon_name (strength));
    adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);

    /* Connect button */
    connect_btn = gtk_button_new_with_label ("Connect");
    gtk_widget_add_css_class (connect_btn, "suggested-action");
    gtk_widget_set_valign (connect_btn, GTK_ALIGN_CENTER);
    g_object_set_data_full (G_OBJECT (connect_btn), "ap",
                            g_object_ref (ap), g_object_unref);
    g_signal_connect (connect_btn, "clicked",
                      G_CALLBACK (on_connect_btn_clicked), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), connect_btn);

    /* Check if already connected */
    NMActiveConnection *active = nm_client_get_primary_connection (self->nm_client);
    if (active) {
        NMAccessPoint *active_ap = nm_device_wifi_get_active_access_point (self->wifi_device);
        if (active_ap == ap) {
            gtk_button_set_label (GTK_BUTTON (connect_btn), "Connected");
            gtk_widget_remove_css_class (connect_btn, "suggested-action");
            gtk_widget_set_sensitive (connect_btn, FALSE);
        }
    }

    return row;
}

static void
refresh_network_list (CoraWifiWindow *self)
{
    const GPtrArray *aps;
    GtkWidget *child;

    if (!self->wifi_device)
        return;

    /* Clear list */
    while ((child = gtk_widget_get_first_child (self->network_list)) != NULL)
        gtk_list_box_remove (GTK_LIST_BOX (self->network_list), child);

    /* Get access points */
    aps = nm_device_wifi_get_access_points (self->wifi_device);

    /* Track SSIDs to avoid duplicates (show strongest signal) */
    GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    for (guint i = 0; i < aps->len; i++) {
        NMAccessPoint *ap = g_ptr_array_index (aps, i);
        GBytes *ssid_bytes = nm_access_point_get_ssid (ap);

        if (!ssid_bytes)
            continue;

        g_autofree char *ssid = nm_utils_ssid_to_utf8 (
            g_bytes_get_data (ssid_bytes, NULL),
            g_bytes_get_size (ssid_bytes));

        if (!ssid || ssid[0] == '\0')
            continue;

        /* Skip duplicates */
        if (g_hash_table_contains (seen, ssid))
            continue;
        g_hash_table_add (seen, g_strdup (ssid));

        GtkWidget *row = create_network_row (self, ap);
        if (row)
            gtk_list_box_append (GTK_LIST_BOX (self->network_list), row);
    }

    g_hash_table_unref (seen);
}

/* === WiFi Scanning === */

static void
start_scan (CoraWifiWindow *self)
{
    if (!self->wifi_device || self->scanning)
        return;

    self->scanning = TRUE;
    gtk_spinner_start (GTK_SPINNER (self->spinner));
    gtk_widget_set_visible (self->spinner, TRUE);

    nm_device_wifi_request_scan_async (self->wifi_device,
                                       NULL,
                                       on_scan_done,
                                       self);
}

static void
on_scan_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (user_data);
    g_autoptr(GError) error = NULL;

    nm_device_wifi_request_scan_finish (NM_DEVICE_WIFI (source), res, &error);

    if (error && !g_error_matches (error, NM_DEVICE_ERROR, NM_DEVICE_ERROR_NOT_ALLOWED))
        g_warning ("WiFi scan failed: %s", error->message);

    self->scanning = FALSE;
    gtk_spinner_stop (GTK_SPINNER (self->spinner));
    gtk_widget_set_visible (self->spinner, FALSE);

    refresh_network_list (self);
}

/* === Connection === */

static void
on_connection_added (GObject *source, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    NMActiveConnection *active;

    active = nm_client_add_and_activate_connection_finish (
        NM_CLIENT (source), res, &error);

    if (error) {
        g_warning ("Connection failed: %s", error->message);
        /* TODO: Show error in UI */
    } else {
        g_message ("Connected successfully");
        g_object_unref (active);
    }
}

static void
connect_to_ap (CoraWifiWindow *self, NMAccessPoint *ap)
{
    NM80211ApFlags flags = nm_access_point_get_flags (ap);
    NM80211ApSecurityFlags wpa = nm_access_point_get_wpa_flags (ap);
    NM80211ApSecurityFlags rsn = nm_access_point_get_rsn_flags (ap);

    /* Check if open network */
    if (flags == NM_802_11_AP_FLAGS_NONE && !wpa && !rsn) {
        /* Connect directly without password */
        NMConnection *connection = nm_simple_connection_new ();
        NMSettingConnection *s_con = (NMSettingConnection *)
            nm_setting_connection_new ();
        NMSettingWireless *s_wifi = (NMSettingWireless *)
            nm_setting_wireless_new ();

        g_object_set (s_con,
                      NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
                      NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                      NULL);
        g_object_set (s_wifi,
                      NM_SETTING_WIRELESS_SSID, nm_access_point_get_ssid (ap),
                      NULL);

        nm_connection_add_setting (connection, NM_SETTING (s_con));
        nm_connection_add_setting (connection, NM_SETTING (s_wifi));

        nm_client_add_and_activate_connection_async (
            self->nm_client, connection,
            NM_DEVICE (self->wifi_device), NULL,
            NULL, on_connection_added, self);

        g_object_unref (connection);
    } else {
        /* Need password */
        show_password_dialog (self, ap);
    }
}

static void
on_password_dialog_response (AdwMessageDialog *dlg, const char *response, gpointer data)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (data);

    if (g_strcmp0 (response, "connect") == 0) {
        NMAccessPoint *ap = g_object_get_data (G_OBJECT (dlg), "ap");
        GtkWidget *entry = g_object_get_data (G_OBJECT (dlg), "entry");
        const char *password = gtk_editable_get_text (GTK_EDITABLE (entry));

        /* Create connection with WPA password */
        NMConnection *conn = nm_simple_connection_new ();
        NMSettingConnection *s_con = (NMSettingConnection *)
            nm_setting_connection_new ();
        NMSettingWireless *s_wifi = (NMSettingWireless *)
            nm_setting_wireless_new ();
        NMSettingWirelessSecurity *s_sec = (NMSettingWirelessSecurity *)
            nm_setting_wireless_security_new ();

        g_object_set (s_con,
            NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
            NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
            NULL);
        g_object_set (s_wifi,
            NM_SETTING_WIRELESS_SSID, nm_access_point_get_ssid (ap),
            NULL);
        g_object_set (s_sec,
            NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk",
            NM_SETTING_WIRELESS_SECURITY_PSK, password,
            NULL);

        nm_connection_add_setting (conn, NM_SETTING (s_con));
        nm_connection_add_setting (conn, NM_SETTING (s_wifi));
        nm_connection_add_setting (conn, NM_SETTING (s_sec));

        nm_client_add_and_activate_connection_async (
            self->nm_client, conn,
            NM_DEVICE (self->wifi_device), NULL,
            NULL, on_connection_added, self);

        g_object_unref (conn);
    }

    gtk_window_destroy (GTK_WINDOW (dlg));
}

static void
show_password_dialog (CoraWifiWindow *self, NMAccessPoint *ap)
{
    GtkWidget *dialog, *entry, *box;
    GBytes *ssid_bytes = nm_access_point_get_ssid (ap);
    g_autofree char *ssid = nm_utils_ssid_to_utf8 (
        g_bytes_get_data (ssid_bytes, NULL),
        g_bytes_get_size (ssid_bytes));

    dialog = adw_message_dialog_new (GTK_WINDOW (self),
                                     "Connect to Network", NULL);

    g_autofree char *body = g_strdup_printf (
        "Enter the password for \"%s\"", ssid);
    adw_message_dialog_set_body (ADW_MESSAGE_DIALOG (dialog), body);

    /* Password entry */
    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start (box, 24);
    gtk_widget_set_margin_end (box, 24);

    entry = gtk_password_entry_new ();
    gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry), TRUE);
    g_object_set (entry, "placeholder-text", "Password", NULL);
    gtk_box_append (GTK_BOX (box), entry);

    adw_message_dialog_set_extra_child (ADW_MESSAGE_DIALOG (dialog), box);
    adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                      "cancel", "Cancel",
                                      "connect", "Connect",
                                      NULL);
    adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                                "connect", ADW_RESPONSE_SUGGESTED);
    adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "connect");

    /* Store references for callback */
    g_object_set_data_full (G_OBJECT (dialog), "ap", g_object_ref (ap), g_object_unref);
    g_object_set_data (G_OBJECT (dialog), "entry", entry);

    g_signal_connect (dialog, "response", G_CALLBACK (on_password_dialog_response), self);

    gtk_window_present (GTK_WINDOW (dialog));
}

/* === WiFi Toggle === */

static void
on_wifi_switch_toggled (GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (user_data);
    gboolean active = gtk_switch_get_active (GTK_SWITCH (self->wifi_switch));

    nm_client_wireless_set_enabled (self->nm_client, active);

    if (active) {
        /* Show network list and trigger scan */
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "networks");
        start_scan (self);
    } else {
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "disabled");
    }
}

/* === NM Client Initialization === */

static void
on_nm_client_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (user_data);
    g_autoptr(GError) error = NULL;
    const GPtrArray *devices;

    self->nm_client = nm_client_new_finish (res, &error);
    if (error) {
        g_warning ("Failed to connect to NetworkManager: %s", error->message);
        adw_status_page_set_title (ADW_STATUS_PAGE (self->status_page),
                                   "NetworkManager Unavailable");
        adw_status_page_set_description (ADW_STATUS_PAGE (self->status_page),
                                         error->message);
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "error");
        return;
    }

    /* Find WiFi device */
    devices = nm_client_get_devices (self->nm_client);
    for (guint i = 0; i < devices->len; i++) {
        NMDevice *dev = g_ptr_array_index (devices, i);
        if (NM_IS_DEVICE_WIFI (dev)) {
            self->wifi_device = NM_DEVICE_WIFI (dev);
            break;
        }
    }

    if (!self->wifi_device) {
        adw_status_page_set_title (ADW_STATUS_PAGE (self->status_page),
                                   "No WiFi Adapter Found");
        adw_status_page_set_description (ADW_STATUS_PAGE (self->status_page),
                                         "No wireless network adapter was detected.");
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "error");
        return;
    }

    /* Set WiFi switch state */
    gboolean wifi_enabled = nm_client_wireless_get_enabled (self->nm_client);
    gtk_switch_set_active (GTK_SWITCH (self->wifi_switch), wifi_enabled);

    if (wifi_enabled) {
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "networks");
        start_scan (self);
    } else {
        gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "disabled");
    }

    /* Watch for AP changes */
    self->ap_added_id = g_signal_connect_swapped (
        self->wifi_device, "access-point-added",
        G_CALLBACK (refresh_network_list), self);
    self->ap_removed_id = g_signal_connect_swapped (
        self->wifi_device, "access-point-removed",
        G_CALLBACK (refresh_network_list), self);
}

/* === UI Construction === */

static void
cora_wifi_window_build_ui (CoraWifiWindow *self)
{
    GtkWidget *toolbar_view, *scroll, *networks_box;
    GtkWidget *wifi_row, *header_group, *disabled_page;

    /* Window setup */
    gtk_window_set_title (GTK_WINDOW (self), "WiFi");
    gtk_window_set_default_size (GTK_WINDOW (self), 450, 600);

    /* AdwToolbarView for proper header bar */
    toolbar_view = adw_toolbar_view_new ();
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (self), toolbar_view);

    /* Header bar */
    self->header_bar = adw_header_bar_new ();
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), self->header_bar);

    /* Scan button in header */
    self->scan_button = gtk_button_new_from_icon_name ("view-refresh-symbolic");
    gtk_widget_set_tooltip_text (self->scan_button, "Scan for networks");
    g_signal_connect_swapped (self->scan_button, "clicked",
                              G_CALLBACK (start_scan), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->scan_button);

    /* Spinner */
    self->spinner = gtk_spinner_new ();
    gtk_widget_set_visible (self->spinner, FALSE);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->spinner);

    /* Main stack (networks / disabled / error) */
    self->main_stack = gtk_stack_new ();
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), self->main_stack);

    /* === Networks page === */
    scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    networks_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start (networks_box, 16);
    gtk_widget_set_margin_end (networks_box, 16);
    gtk_widget_set_margin_top (networks_box, 16);
    gtk_widget_set_margin_bottom (networks_box, 16);

    /* WiFi toggle row */
    header_group = adw_preferences_group_new ();
    wifi_row = adw_action_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (wifi_row), "Wi-Fi");
    self->wifi_switch = gtk_switch_new ();
    gtk_widget_set_valign (self->wifi_switch, GTK_ALIGN_CENTER);
    g_signal_connect (self->wifi_switch, "notify::active",
                      G_CALLBACK (on_wifi_switch_toggled), self);
    adw_action_row_add_suffix (ADW_ACTION_ROW (wifi_row), self->wifi_switch);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (header_group), wifi_row);
    gtk_box_append (GTK_BOX (networks_box), header_group);

    /* Available networks group */
    GtkWidget *networks_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (networks_group),
                                     "Available Networks");
    self->network_list = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->network_list),
                                     GTK_SELECTION_NONE);
    gtk_widget_add_css_class (self->network_list, "boxed-list");
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (networks_group),
                               self->network_list);
    gtk_box_append (GTK_BOX (networks_box), networks_group);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), networks_box);
    gtk_stack_add_named (GTK_STACK (self->main_stack), scroll, "networks");

    /* === Disabled page === */
    disabled_page = adw_status_page_new ();
    adw_status_page_set_icon_name (ADW_STATUS_PAGE (disabled_page),
                                   "network-wireless-disabled-symbolic");
    adw_status_page_set_title (ADW_STATUS_PAGE (disabled_page), "Wi-Fi is Off");
    adw_status_page_set_description (ADW_STATUS_PAGE (disabled_page),
                                     "Turn on Wi-Fi to see available networks.");
    gtk_stack_add_named (GTK_STACK (self->main_stack), disabled_page, "disabled");

    /* === Error page === */
    self->status_page = adw_status_page_new ();
    adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->status_page),
                                   "dialog-error-symbolic");
    gtk_stack_add_named (GTK_STACK (self->main_stack), self->status_page, "error");

    /* Show loading initially */
    gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "networks");
}

/* === GObject === */

static void
cora_wifi_window_dispose (GObject *object)
{
    CoraWifiWindow *self = CORA_WIFI_WINDOW (object);

    if (self->wifi_device) {
        if (self->ap_added_id > 0)
            g_signal_handler_disconnect (self->wifi_device, self->ap_added_id);
        if (self->ap_removed_id > 0)
            g_signal_handler_disconnect (self->wifi_device, self->ap_removed_id);
    }

    g_clear_object (&self->nm_client);

    G_OBJECT_CLASS (cora_wifi_window_parent_class)->dispose (object);
}

static void
cora_wifi_window_init (CoraWifiWindow *self)
{
    self->nm_client = NULL;
    self->wifi_device = NULL;
    self->scanning = FALSE;
    self->ap_added_id = 0;
    self->ap_removed_id = 0;
}

static void
cora_wifi_window_class_init (CoraWifiWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = cora_wifi_window_dispose;
}

CoraWifiWindow *
cora_wifi_window_new (GtkApplication *app)
{
    CoraWifiWindow *self = g_object_new (CORA_TYPE_WIFI_WINDOW,
                                         "application", app,
                                         NULL);

    cora_wifi_window_build_ui (self);

    /* Initialize NetworkManager client async */
    nm_client_new_async (NULL, on_nm_client_ready, self);

    return self;
}
