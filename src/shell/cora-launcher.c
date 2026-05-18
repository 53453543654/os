/* CoraOS Application Launcher
 * 
 * Full-screen application grid with:
 * - Search bar at top
 * - Grid of installed applications (from .desktop files)
 * - Categories/filtering
 * - Keyboard navigation
 * - Launch via D-Bus activation or exec
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-launcher.h"
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#define GRID_COLUMNS 6
#define ICON_SIZE    64

struct _CoraLauncher {
    GtkWindow       parent_instance;

    CoraShell      *shell;

    /* UI */
    GtkWidget      *main_box;
    GtkWidget      *search_entry;
    GtkWidget      *scroll_window;
    GtkWidget      *grid_flow;
    GtkWidget      *no_results_label;

    /* Data */
    GList          *all_apps;       /* GAppInfo list */
    char           *search_text;
};

G_DEFINE_TYPE (CoraLauncher, cora_launcher, GTK_TYPE_WINDOW)

typedef struct {
    GAppInfo   *app_info;
    GtkWidget  *widget;
} AppItem;

static void
launch_app (GAppInfo *app_info)
{
    g_autoptr(GError) error = NULL;

    if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
        g_warning ("Failed to launch %s: %s",
                   g_app_info_get_display_name (app_info),
                   error->message);
    }
}

static void
on_app_clicked (GtkButton *button, gpointer user_data)
{
    GAppInfo *info = G_APP_INFO (user_data);
    launch_app (info);

    /* Hide launcher after launch */
    GtkWidget *win = gtk_widget_get_ancestor (GTK_WIDGET (button), CORA_TYPE_LAUNCHER);
    if (win)
        gtk_widget_set_visible (win, FALSE);
}

static GtkWidget *
create_app_widget (GAppInfo *info)
{
    GtkWidget *button, *box, *image, *label;
    GIcon *icon;

    button = gtk_button_new ();
    gtk_widget_add_css_class (button, "cora-app-item");
    gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
    gtk_widget_set_size_request (button, 100, 100);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

    /* Icon */
    icon = g_app_info_get_icon (info);
    if (icon) {
        image = gtk_image_new_from_gicon (icon);
    } else {
        image = gtk_image_new_from_icon_name ("application-x-executable");
    }
    gtk_image_set_pixel_size (GTK_IMAGE (image), ICON_SIZE);
    gtk_box_append (GTK_BOX (box), image);

    /* Name */
    label = gtk_label_new (g_app_info_get_display_name (info));
    gtk_widget_add_css_class (label, "cora-app-label");
    gtk_label_set_max_width_chars (GTK_LABEL (label), 12);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap (GTK_LABEL (label), FALSE);
    gtk_box_append (GTK_BOX (box), label);

    gtk_button_set_child (GTK_BUTTON (button), box);
    g_signal_connect (button, "clicked", G_CALLBACK (on_app_clicked), info);

    return button;
}

static void
cora_launcher_load_apps (CoraLauncher *self)
{
    GList *apps = g_app_info_get_all ();
    GList *l;

    /* Filter to only show apps that should be visible */
    for (l = apps; l != NULL; l = l->next) {
        GAppInfo *info = G_APP_INFO (l->data);

        if (!G_IS_DESKTOP_APP_INFO (info))
            continue;

        GDesktopAppInfo *desktop_info = G_DESKTOP_APP_INFO (info);

        /* Skip NoDisplay and hidden apps */
        if (g_desktop_app_info_get_nodisplay (desktop_info))
            continue;

        if (!g_app_info_should_show (info))
            continue;

        self->all_apps = g_list_prepend (self->all_apps, g_object_ref (info));
    }

    self->all_apps = g_list_reverse (self->all_apps);
    g_list_free_full (apps, g_object_unref);
}

static void
cora_launcher_populate_grid (CoraLauncher *self)
{
    GList *l;
    GtkWidget *child;
    int count = 0;

    /* Clear existing grid */
    while ((child = gtk_widget_get_first_child (self->grid_flow)) != NULL)
        gtk_flow_box_remove (GTK_FLOW_BOX (self->grid_flow), child);

    for (l = self->all_apps; l != NULL; l = l->next) {
        GAppInfo *info = G_APP_INFO (l->data);

        /* Filter by search */
        if (self->search_text && self->search_text[0] != '\0') {
            const char *name = g_app_info_get_display_name (info);
            const char *desc = g_app_info_get_description (info);
            g_autofree char *name_lower = g_utf8_strdown (name, -1);
            g_autofree char *search_lower = g_utf8_strdown (self->search_text, -1);

            gboolean matches = (g_strstr_len (name_lower, -1, search_lower) != NULL);
            if (!matches && desc) {
                g_autofree char *desc_lower = g_utf8_strdown (desc, -1);
                matches = (g_strstr_len (desc_lower, -1, search_lower) != NULL);
            }
            if (!matches)
                continue;
        }

        GtkWidget *app_widget = create_app_widget (info);
        gtk_flow_box_append (GTK_FLOW_BOX (self->grid_flow), app_widget);
        count++;
    }

    /* Show/hide no results */
    gtk_widget_set_visible (self->no_results_label, count == 0);
    gtk_widget_set_visible (self->scroll_window, count > 0);
}

static void
on_search_changed (GtkEditable *editable, gpointer user_data)
{
    CoraLauncher *self = CORA_LAUNCHER (user_data);

    g_free (self->search_text);
    self->search_text = g_strdup (gtk_editable_get_text (editable));

    cora_launcher_populate_grid (self);
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                gpointer               user_data)
{
    CoraLauncher *self = CORA_LAUNCHER (user_data);

    if (keyval == GDK_KEY_Escape) {
        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
        return TRUE;
    }

    return FALSE;
}

static void
cora_launcher_build_ui (CoraLauncher *self)
{
    GtkEventController *key_controller;

    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
    gtk_window_set_modal (GTK_WINDOW (self), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (self), 800, 600);
    gtk_widget_add_css_class (GTK_WIDGET (self), "cora-launcher-window");

    /* Keyboard handler */
    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed",
                      G_CALLBACK (on_key_pressed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), key_controller);

    /* Main vertical layout */
    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start (self->main_box, 40);
    gtk_widget_set_margin_end (self->main_box, 40);
    gtk_widget_set_margin_top (self->main_box, 40);
    gtk_widget_set_margin_bottom (self->main_box, 40);
    gtk_window_set_child (GTK_WINDOW (self), self->main_box);

    /* Search entry */
    self->search_entry = gtk_search_entry_new ();
    gtk_widget_add_css_class (self->search_entry, "cora-launcher-search");
    gtk_widget_set_hexpand (self->search_entry, TRUE);
    g_object_set (self->search_entry,
                  "placeholder-text", "Search applications...",
                  NULL);
    g_signal_connect (self->search_entry, "changed",
                      G_CALLBACK (on_search_changed), self);
    gtk_box_append (GTK_BOX (self->main_box), self->search_entry);

    /* Scrollable area for app grid */
    self->scroll_window = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scroll_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (self->scroll_window, TRUE);
    gtk_box_append (GTK_BOX (self->main_box), self->scroll_window);

    /* Flow box for app grid */
    self->grid_flow = gtk_flow_box_new ();
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->grid_flow), GRID_COLUMNS);
    gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self->grid_flow), 3);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->grid_flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->grid_flow), TRUE);
    gtk_widget_set_valign (self->grid_flow, GTK_ALIGN_START);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scroll_window),
                                   self->grid_flow);

    /* No results label */
    self->no_results_label = gtk_label_new ("No applications found");
    gtk_widget_add_css_class (self->no_results_label, "cora-app-label");
    gtk_widget_set_visible (self->no_results_label, FALSE);
    gtk_box_append (GTK_BOX (self->main_box), self->no_results_label);

    /* Load and display apps */
    cora_launcher_load_apps (self);
    cora_launcher_populate_grid (self);
}

void
cora_launcher_focus_search (CoraLauncher *self)
{
    g_return_if_fail (CORA_IS_LAUNCHER (self));

    gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
    gtk_widget_grab_focus (self->search_entry);
}

static void
cora_launcher_finalize (GObject *object)
{
    CoraLauncher *self = CORA_LAUNCHER (object);

    g_free (self->search_text);
    g_list_free_full (self->all_apps, g_object_unref);

    G_OBJECT_CLASS (cora_launcher_parent_class)->finalize (object);
}

static void
cora_launcher_init (CoraLauncher *self)
{
    self->all_apps = NULL;
    self->search_text = NULL;
}

static void
cora_launcher_class_init (CoraLauncherClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = cora_launcher_finalize;
}

CoraLauncher *
cora_launcher_new (CoraShell *shell)
{
    CoraLauncher *self = g_object_new (CORA_TYPE_LAUNCHER,
                                       "application", shell,
                                       NULL);
    self->shell = shell;
    cora_launcher_build_ui (self);
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
    return self;
}
