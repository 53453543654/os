/* CoraOS File Explorer - Window Implementation
 *
 * Real file manager using GIO for all filesystem operations.
 * Supports local files, GVfs mounts, trash://, and network.
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-files-window.h"
#include <gio/gio.h>
#include <string.h>

#define ICON_SIZE_GRID 64
#define ICON_SIZE_LIST 24

typedef enum {
    VIEW_MODE_GRID,
    VIEW_MODE_LIST,
} ViewMode;

struct _CoraFilesWindow {
    AdwApplicationWindow parent_instance;

    /* Current location */
    GFile              *current_dir;
    GCancellable       *cancellable;

    /* Navigation history */
    GList              *back_history;
    GList              *forward_history;

    /* UI */
    GtkWidget          *toolbar_view;
    GtkWidget          *header_bar;
    GtkWidget          *split_view;
    GtkWidget          *sidebar;
    GtkWidget          *content_box;
    GtkWidget          *path_bar;
    GtkWidget          *search_entry;
    GtkWidget          *view_stack;
    GtkWidget          *grid_view;
    GtkWidget          *list_view;
    GtkWidget          *empty_page;

    /* Header buttons */
    GtkWidget          *back_btn;
    GtkWidget          *forward_btn;
    GtkWidget          *up_btn;
    GtkWidget          *search_btn;
    GtkWidget          *view_toggle_btn;
    GtkWidget          *new_folder_btn;

    /* Models */
    GListStore         *file_store;
    GtkSortListModel   *sort_model;
    GtkFilterListModel *filter_model;

    /* State */
    ViewMode            view_mode;
    gboolean            searching;
    char               *search_text;
};

G_DEFINE_TYPE (CoraFilesWindow, cora_files_window, ADW_TYPE_APPLICATION_WINDOW)

/* Forward declarations */
static void load_directory        (CoraFilesWindow *self, GFile *dir);
static void populate_sidebar      (CoraFilesWindow *self);
static void update_path_bar       (CoraFilesWindow *self);
static void on_file_activated     (CoraFilesWindow *self, guint position);


/* === File Info Object for GListStore === */

#define CORA_TYPE_FILE_ITEM (cora_file_item_get_type ())
G_DECLARE_FINAL_TYPE (CoraFileItem, cora_file_item, CORA, FILE_ITEM, GObject)

struct _CoraFileItem {
    GObject     parent_instance;
    GFileInfo  *info;
    GFile      *file;
    char       *display_name;
    char       *content_type;
    goffset     size;
    gboolean    is_directory;
    gboolean    is_hidden;
    GDateTime  *modified;
};

G_DEFINE_TYPE (CoraFileItem, cora_file_item, G_TYPE_OBJECT)

static void
cora_file_item_finalize (GObject *object)
{
    CoraFileItem *self = CORA_FILE_ITEM (object);
    g_clear_object (&self->info);
    g_clear_object (&self->file);
    g_free (self->display_name);
    g_free (self->content_type);
    g_clear_pointer (&self->modified, g_date_time_unref);
    G_OBJECT_CLASS (cora_file_item_parent_class)->finalize (object);
}

static void cora_file_item_init (CoraFileItem *self) {}
static void cora_file_item_class_init (CoraFileItemClass *klass) {
    G_OBJECT_CLASS (klass)->finalize = cora_file_item_finalize;
}

static CoraFileItem *
cora_file_item_new (GFile *file, GFileInfo *info)
{
    CoraFileItem *item = g_object_new (CORA_TYPE_FILE_ITEM, NULL);
    item->file = g_object_ref (file);
    item->info = g_object_ref (info);
    item->display_name = g_strdup (g_file_info_get_display_name (info));
    item->content_type = g_strdup (g_file_info_get_content_type (info));
    item->size = g_file_info_get_size (info);
    item->is_directory = (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY);
    item->is_hidden = g_file_info_get_is_hidden (info);
    item->modified = g_file_info_get_modification_date_time (info);
    if (item->modified)
        g_date_time_ref (item->modified);
    return item;
}

/* === Utility === */

static char *
format_file_size (goffset size)
{
    if (size < 1024)
        return g_strdup_printf ("%ld B", (long)size);
    else if (size < 1024 * 1024)
        return g_strdup_printf ("%.1f KB", size / 1024.0);
    else if (size < 1024 * 1024 * 1024)
        return g_strdup_printf ("%.1f MB", size / (1024.0 * 1024.0));
    else
        return g_strdup_printf ("%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
}

static GIcon *
get_icon_for_file (CoraFileItem *item)
{
    GIcon *icon = g_file_info_get_icon (item->info);
    if (icon)
        return g_object_ref (icon);

    if (item->is_directory)
        return g_themed_icon_new ("folder");

    return g_themed_icon_new ("text-x-generic");
}


/* === Directory Loading === */

static void
on_enumerate_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);
    g_autoptr(GError) error = NULL;
    GFileEnumerator *enumerator;

    enumerator = g_file_enumerate_children_finish (G_FILE (source), res, &error);
    if (error) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("Failed to list directory: %s", error->message);
        return;
    }

    /* Read entries */
    while (TRUE) {
        GFileInfo *info = NULL;
        GFile *child = NULL;

        if (!g_file_enumerator_iterate (enumerator, &info, &child, NULL, &error)) {
            g_warning ("Enumerate error: %s", error->message);
            break;
        }

        if (!info)
            break;

        /* Skip hidden files unless showing them */
        if (g_file_info_get_is_hidden (info))
            continue;

        CoraFileItem *item = cora_file_item_new (child, info);
        g_list_store_append (self->file_store, item);
        g_object_unref (item);
    }

    g_object_unref (enumerator);

    /* Show empty page if no files */
    if (g_list_model_get_n_items (G_LIST_MODEL (self->file_store)) == 0) {
        gtk_stack_set_visible_child_name (GTK_STACK (self->view_stack), "empty");
    } else {
        gtk_stack_set_visible_child_name (GTK_STACK (self->view_stack),
            self->view_mode == VIEW_MODE_GRID ? "grid" : "list");
    }

    update_path_bar (self);
}

static void
load_directory (CoraFilesWindow *self, GFile *dir)
{
    /* Cancel any pending operation */
    if (self->cancellable) {
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
    }

    self->cancellable = g_cancellable_new ();

    /* Clear file store */
    g_list_store_remove_all (self->file_store);

    /* Update current dir */
    g_set_object (&self->current_dir, dir);

    /* Start async enumeration */
    g_file_enumerate_children_async (
        dir,
        G_FILE_ATTRIBUTE_STANDARD_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE ","
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_ICON ","
        G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED,
        G_FILE_QUERY_INFO_NONE,
        G_PRIORITY_DEFAULT,
        self->cancellable,
        on_enumerate_done,
        self
    );
}

/* === Navigation === */

void
cora_files_window_navigate (CoraFilesWindow *self, GFile *directory)
{
    g_return_if_fail (CORA_IS_FILES_WINDOW (self));
    g_return_if_fail (G_IS_FILE (directory));

    /* Push current to back history */
    if (self->current_dir) {
        self->back_history = g_list_prepend (self->back_history,
                                             g_object_ref (self->current_dir));
    }

    /* Clear forward history */
    g_list_free_full (self->forward_history, g_object_unref);
    self->forward_history = NULL;

    /* Update button sensitivity */
    gtk_widget_set_sensitive (self->back_btn, self->back_history != NULL);
    gtk_widget_set_sensitive (self->forward_btn, FALSE);

    load_directory (self, directory);
}

static void
on_back_clicked (GtkButton *btn, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);

    if (!self->back_history)
        return;

    /* Push current to forward */
    if (self->current_dir)
        self->forward_history = g_list_prepend (self->forward_history,
                                                g_object_ref (self->current_dir));

    /* Pop from back */
    GFile *prev = G_FILE (self->back_history->data);
    self->back_history = g_list_delete_link (self->back_history, self->back_history);

    gtk_widget_set_sensitive (self->back_btn, self->back_history != NULL);
    gtk_widget_set_sensitive (self->forward_btn, TRUE);

    load_directory (self, prev);
    g_object_unref (prev);
}

static void
on_forward_clicked (GtkButton *btn, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);

    if (!self->forward_history)
        return;

    if (self->current_dir)
        self->back_history = g_list_prepend (self->back_history,
                                             g_object_ref (self->current_dir));

    GFile *next = G_FILE (self->forward_history->data);
    self->forward_history = g_list_delete_link (self->forward_history,
                                                self->forward_history);

    gtk_widget_set_sensitive (self->back_btn, TRUE);
    gtk_widget_set_sensitive (self->forward_btn, self->forward_history != NULL);

    load_directory (self, next);
    g_object_unref (next);
}

static void
on_up_clicked (GtkButton *btn, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);

    if (!self->current_dir)
        return;

    g_autoptr(GFile) parent = g_file_get_parent (self->current_dir);
    if (parent)
        cora_files_window_navigate (self, parent);
}


/* === File Operations === */

static void
on_file_activated (CoraFilesWindow *self, guint position)
{
    CoraFileItem *item = g_list_model_get_item (
        G_LIST_MODEL (self->file_store), position);

    if (!item)
        return;

    if (item->is_directory) {
        cora_files_window_navigate (self, item->file);
    } else {
        /* Open file with default application */
        g_autoptr(GError) error = NULL;
        GAppInfo *app = g_app_info_get_default_for_type (item->content_type, FALSE);

        if (app) {
            GList *files = g_list_append (NULL, item->file);
            g_app_info_launch (app, files, NULL, &error);
            g_list_free (files);
            g_object_unref (app);
        } else {
            /* Fallback: use xdg-open */
            g_autofree char *uri = g_file_get_uri (item->file);
            g_autofree char *cmd = g_strdup_printf ("xdg-open '%s'", uri);
            g_spawn_command_line_async (cmd, &error);
        }

        if (error)
            g_warning ("Failed to open file: %s", error->message);
    }

    g_object_unref (item);
}

static void
on_new_folder_clicked (GtkButton *btn, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);
    GtkWidget *dialog;

    dialog = adw_message_dialog_new (GTK_WINDOW (self), "New Folder", NULL);
    adw_message_dialog_set_body (ADW_MESSAGE_DIALOG (dialog),
                                 "Enter a name for the new folder:");

    GtkWidget *entry = gtk_entry_new ();
    g_object_set (entry, "placeholder-text", "Folder name", NULL);
    gtk_editable_set_text (GTK_EDITABLE (entry), "New Folder");
    adw_message_dialog_set_extra_child (ADW_MESSAGE_DIALOG (dialog), entry);
    adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                      "cancel", "Cancel",
                                      "create", "Create",
                                      NULL);
    adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                                "create", ADW_RESPONSE_SUGGESTED);

    g_object_set_data (G_OBJECT (dialog), "entry", entry);

    g_signal_connect (dialog, "response", G_CALLBACK (
        +[](AdwMessageDialog *dlg, const char *response, gpointer data) {
            CoraFilesWindow *self = CORA_FILES_WINDOW (data);
            if (g_strcmp0 (response, "create") == 0) {
                GtkWidget *entry = g_object_get_data (G_OBJECT (dlg), "entry");
                const char *name = gtk_editable_get_text (GTK_EDITABLE (entry));

                if (name && name[0] != '\0' && self->current_dir) {
                    g_autoptr(GFile) new_dir = g_file_get_child (self->current_dir, name);
                    g_autoptr(GError) error = NULL;

                    if (!g_file_make_directory (new_dir, NULL, &error)) {
                        g_warning ("Failed to create folder: %s", error->message);
                    } else {
                        /* Reload directory */
                        load_directory (self, self->current_dir);
                    }
                }
            }
            gtk_window_destroy (GTK_WINDOW (dlg));
        }), self);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
delete_file_action (CoraFilesWindow *self, GFile *file, const char *name)
{
    GtkWidget *dialog;
    g_autofree char *body = g_strdup_printf (
        "Are you sure you want to move \"%s\" to the Trash?", name);

    dialog = adw_message_dialog_new (GTK_WINDOW (self), "Move to Trash?", NULL);
    adw_message_dialog_set_body (ADW_MESSAGE_DIALOG (dialog), body);
    adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                      "cancel", "Cancel",
                                      "trash", "Move to Trash",
                                      NULL);
    adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                                "trash", ADW_RESPONSE_DESTRUCTIVE);

    g_object_set_data_full (G_OBJECT (dialog), "file", g_object_ref (file), g_object_unref);

    g_signal_connect (dialog, "response", G_CALLBACK (
        +[](AdwMessageDialog *dlg, const char *response, gpointer data) {
            CoraFilesWindow *self = CORA_FILES_WINDOW (data);
            if (g_strcmp0 (response, "trash") == 0) {
                GFile *file = g_object_get_data (G_OBJECT (dlg), "file");
                g_autoptr(GError) error = NULL;

                if (!g_file_trash (file, NULL, &error))
                    g_warning ("Trash failed: %s", error->message);
                else
                    load_directory (self, self->current_dir);
            }
            gtk_window_destroy (GTK_WINDOW (dlg));
        }), self);

    gtk_window_present (GTK_WINDOW (dialog));
}

/* === Path Bar === */

static void
update_path_bar (CoraFilesWindow *self)
{
    GtkWidget *child;

    if (!self->current_dir)
        return;

    /* Clear path bar */
    while ((child = gtk_widget_get_first_child (self->path_bar)) != NULL)
        gtk_box_remove (GTK_BOX (self->path_bar), child);

    /* Build path segments */
    g_autofree char *path = g_file_get_path (self->current_dir);
    if (!path)
        path = g_file_get_uri (self->current_dir);

    char **segments = g_strsplit (path, "/", -1);
    GString *cumulative = g_string_new ("/");

    for (int i = 0; segments[i] != NULL; i++) {
        if (segments[i][0] == '\0')
            continue;

        g_string_append (cumulative, segments[i]);
        g_string_append_c (cumulative, '/');

        const char *label = segments[i];
        /* Replace home dir with ~ */
        g_autofree char *home = g_strdup (g_get_home_dir ());
        if (g_strcmp0 (cumulative->str, g_strdup_printf ("%s/", home)) == 0)
            label = "Home";

        GtkWidget *btn = gtk_button_new_with_label (label);
        gtk_widget_add_css_class (btn, "flat");
        g_object_set_data_full (G_OBJECT (btn), "path",
                                g_strdup (cumulative->str), g_free);
        g_signal_connect (btn, "clicked", G_CALLBACK (
            +[](GtkButton *b, gpointer data) {
                CoraFilesWindow *self = CORA_FILES_WINDOW (data);
                const char *path = g_object_get_data (G_OBJECT (b), "path");
                g_autoptr(GFile) dir = g_file_new_for_path (path);
                cora_files_window_navigate (self, dir);
            }), self);
        gtk_box_append (GTK_BOX (self->path_bar), btn);

        /* Separator */
        if (segments[i + 1] != NULL) {
            GtkWidget *sep = gtk_label_new ("/");
            gtk_widget_set_opacity (sep, 0.5);
            gtk_box_append (GTK_BOX (self->path_bar), sep);
        }
    }

    g_strfreev (segments);
    g_string_free (cumulative, TRUE);
}


/* === Sidebar === */

static void
populate_sidebar (CoraFilesWindow *self)
{
    GtkWidget *list = self->sidebar;
    GtkWidget *row;

    struct { const char *name; const char *icon; const char *path; } places[] = {
        { "Home",      "user-home-symbolic",      NULL },
        { "Documents", "folder-documents-symbolic", "Documents" },
        { "Downloads", "folder-download-symbolic",  "Downloads" },
        { "Music",     "folder-music-symbolic",     "Music" },
        { "Pictures",  "folder-pictures-symbolic",  "Pictures" },
        { "Videos",    "folder-videos-symbolic",    "Videos" },
        { "Trash",     "user-trash-symbolic",       NULL },
    };

    for (int i = 0; i < G_N_ELEMENTS (places); i++) {
        row = adw_action_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), places[i].name);

        GtkWidget *icon = gtk_image_new_from_icon_name (places[i].icon);
        adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);

        /* Determine target path */
        char *target;
        if (g_strcmp0 (places[i].name, "Trash") == 0) {
            target = g_strdup ("trash:///");
        } else if (places[i].path) {
            target = g_build_filename (g_get_home_dir (), places[i].path, NULL);
        } else {
            target = g_strdup (g_get_home_dir ());
        }

        g_object_set_data_full (G_OBJECT (row), "target-path", target, g_free);

        gtk_list_box_append (GTK_LIST_BOX (list), row);
    }

    /* Handle sidebar clicks */
    g_signal_connect (list, "row-activated", G_CALLBACK (
        +[](GtkListBox *box, GtkListBoxRow *row, gpointer data) {
            CoraFilesWindow *self = CORA_FILES_WINDOW (data);
            const char *target = g_object_get_data (G_OBJECT (row), "target-path");
            if (target) {
                g_autoptr(GFile) dir = NULL;
                if (g_str_has_prefix (target, "trash://"))
                    dir = g_file_new_for_uri (target);
                else
                    dir = g_file_new_for_path (target);
                cora_files_window_navigate (self, dir);
            }
        }), self);
}

/* === Grid View Setup === */

static void
setup_grid_item (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    GtkWidget *box, *image, *label;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request (box, 100, 100);

    image = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (image), ICON_SIZE_GRID);
    gtk_box_append (GTK_BOX (box), image);

    label = gtk_label_new (NULL);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 14);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap (GTK_LABEL (label), FALSE);
    gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX (box), label);

    gtk_list_item_set_child (item, box);
}

static void
bind_grid_item (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    GtkWidget *box = gtk_list_item_get_child (item);
    GtkWidget *image = gtk_widget_get_first_child (box);
    GtkWidget *label = gtk_widget_get_next_sibling (image);
    CoraFileItem *file_item = gtk_list_item_get_item (item);

    if (!file_item) return;

    g_autoptr(GIcon) icon = get_icon_for_file (file_item);
    gtk_image_set_from_gicon (GTK_IMAGE (image), icon);
    gtk_label_set_text (GTK_LABEL (label), file_item->display_name);
}

/* === List View Setup === */

static void
setup_list_item (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    GtkWidget *box, *image, *name_label, *size_label;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start (box, 8);
    gtk_widget_set_margin_end (box, 8);
    gtk_widget_set_margin_top (box, 4);
    gtk_widget_set_margin_bottom (box, 4);

    image = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (image), ICON_SIZE_LIST);
    gtk_box_append (GTK_BOX (box), image);

    name_label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (name_label), 0);
    gtk_widget_set_hexpand (name_label, TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
    gtk_box_append (GTK_BOX (box), name_label);

    size_label = gtk_label_new (NULL);
    gtk_widget_set_opacity (size_label, 0.6);
    gtk_box_append (GTK_BOX (box), size_label);

    gtk_list_item_set_child (item, box);
}

static void
bind_list_item (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    GtkWidget *box = gtk_list_item_get_child (item);
    GtkWidget *image = gtk_widget_get_first_child (box);
    GtkWidget *name_label = gtk_widget_get_next_sibling (image);
    GtkWidget *size_label = gtk_widget_get_next_sibling (name_label);
    CoraFileItem *file_item = gtk_list_item_get_item (item);

    if (!file_item) return;

    g_autoptr(GIcon) icon = get_icon_for_file (file_item);
    gtk_image_set_from_gicon (GTK_IMAGE (image), icon);
    gtk_label_set_text (GTK_LABEL (name_label), file_item->display_name);

    if (file_item->is_directory) {
        gtk_label_set_text (GTK_LABEL (size_label), "Folder");
    } else {
        g_autofree char *size_str = format_file_size (file_item->size);
        gtk_label_set_text (GTK_LABEL (size_label), size_str);
    }
}


/* === UI Construction === */

static void
on_view_toggle (GtkButton *btn, gpointer user_data)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (user_data);

    if (self->view_mode == VIEW_MODE_GRID) {
        self->view_mode = VIEW_MODE_LIST;
        gtk_stack_set_visible_child_name (GTK_STACK (self->view_stack), "list");
        gtk_button_set_icon_name (GTK_BUTTON (btn), "view-grid-symbolic");
    } else {
        self->view_mode = VIEW_MODE_GRID;
        gtk_stack_set_visible_child_name (GTK_STACK (self->view_stack), "grid");
        gtk_button_set_icon_name (GTK_BUTTON (btn), "view-list-symbolic");
    }
}

static void
cora_files_window_build_ui (CoraFilesWindow *self)
{
    GtkWidget *toolbar_view, *content_scroll;

    gtk_window_set_title (GTK_WINDOW (self), "Files");
    gtk_window_set_default_size (GTK_WINDOW (self), 900, 600);

    /* Main toolbar view */
    toolbar_view = adw_toolbar_view_new ();
    adw_application_window_set_content (ADW_APPLICATION_WINDOW (self), toolbar_view);

    /* Header bar */
    self->header_bar = adw_header_bar_new ();

    /* Navigation buttons */
    self->back_btn = gtk_button_new_from_icon_name ("go-previous-symbolic");
    gtk_widget_set_sensitive (self->back_btn, FALSE);
    g_signal_connect (self->back_btn, "clicked", G_CALLBACK (on_back_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->back_btn);

    self->forward_btn = gtk_button_new_from_icon_name ("go-next-symbolic");
    gtk_widget_set_sensitive (self->forward_btn, FALSE);
    g_signal_connect (self->forward_btn, "clicked", G_CALLBACK (on_forward_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->forward_btn);

    self->up_btn = gtk_button_new_from_icon_name ("go-up-symbolic");
    g_signal_connect (self->up_btn, "clicked", G_CALLBACK (on_up_clicked), self);
    adw_header_bar_pack_start (ADW_HEADER_BAR (self->header_bar), self->up_btn);

    /* View toggle */
    self->view_toggle_btn = gtk_button_new_from_icon_name ("view-list-symbolic");
    g_signal_connect (self->view_toggle_btn, "clicked",
                      G_CALLBACK (on_view_toggle), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->view_toggle_btn);

    /* New folder button */
    self->new_folder_btn = gtk_button_new_from_icon_name ("folder-new-symbolic");
    gtk_widget_set_tooltip_text (self->new_folder_btn, "New Folder");
    g_signal_connect (self->new_folder_btn, "clicked",
                      G_CALLBACK (on_new_folder_clicked), self);
    adw_header_bar_pack_end (ADW_HEADER_BAR (self->header_bar), self->new_folder_btn);

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), self->header_bar);

    /* Path bar under header */
    self->path_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_start (self->path_bar, 12);
    gtk_widget_set_margin_end (self->path_bar, 12);
    gtk_widget_set_margin_top (self->path_bar, 4);
    gtk_widget_set_margin_bottom (self->path_bar, 4);
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), self->path_bar);

    /* Split view: sidebar + content */
    self->split_view = adw_overlay_split_view_new ();
    adw_overlay_split_view_set_collapsed (ADW_OVERLAY_SPLIT_VIEW (self->split_view), FALSE);
    adw_overlay_split_view_set_max_sidebar_width (ADW_OVERLAY_SPLIT_VIEW (self->split_view), 220);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), self->split_view);

    /* Sidebar */
    GtkWidget *sidebar_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar_scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    self->sidebar = gtk_list_box_new ();
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->sidebar), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class (self->sidebar, "navigation-sidebar");
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sidebar_scroll), self->sidebar);
    adw_overlay_split_view_set_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->split_view),
                                        sidebar_scroll);
    populate_sidebar (self);

    /* Content: stack with grid, list, empty views */
    self->view_stack = gtk_stack_new ();
    adw_overlay_split_view_set_content (ADW_OVERLAY_SPLIT_VIEW (self->split_view),
                                        self->view_stack);

    /* File store (shared model) */
    self->file_store = g_list_store_new (CORA_TYPE_FILE_ITEM);

    /* Grid view */
    GtkListItemFactory *grid_factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (grid_factory, "setup", G_CALLBACK (setup_grid_item), self);
    g_signal_connect (grid_factory, "bind", G_CALLBACK (bind_grid_item), self);

    GtkNoSelection *grid_sel = gtk_no_selection_new (
        G_LIST_MODEL (g_object_ref (self->file_store)));
    self->grid_view = gtk_grid_view_new (GTK_SELECTION_MODEL (grid_sel), grid_factory);
    gtk_grid_view_set_max_columns (GTK_GRID_VIEW (self->grid_view), 8);
    gtk_grid_view_set_min_columns (GTK_GRID_VIEW (self->grid_view), 3);
    g_signal_connect_swapped (self->grid_view, "activate",
                              G_CALLBACK (on_file_activated), self);

    content_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_scroll), self->grid_view);
    gtk_stack_add_named (GTK_STACK (self->view_stack), content_scroll, "grid");

    /* List view */
    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (list_factory, "setup", G_CALLBACK (setup_list_item), self);
    g_signal_connect (list_factory, "bind", G_CALLBACK (bind_list_item), self);

    GtkNoSelection *list_sel = gtk_no_selection_new (
        G_LIST_MODEL (g_object_ref (self->file_store)));
    self->list_view = gtk_list_view_new (GTK_SELECTION_MODEL (list_sel), list_factory);
    g_signal_connect_swapped (self->list_view, "activate",
                              G_CALLBACK (on_file_activated), self);

    GtkWidget *list_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (list_scroll), self->list_view);
    gtk_stack_add_named (GTK_STACK (self->view_stack), list_scroll, "list");

    /* Empty page */
    self->empty_page = adw_status_page_new ();
    adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->empty_page),
                                   "folder-symbolic");
    adw_status_page_set_title (ADW_STATUS_PAGE (self->empty_page), "Folder is Empty");
    gtk_stack_add_named (GTK_STACK (self->view_stack), self->empty_page, "empty");

    /* Default to grid view */
    self->view_mode = VIEW_MODE_GRID;
    gtk_stack_set_visible_child_name (GTK_STACK (self->view_stack), "grid");
}

/* === GObject === */

static void
cora_files_window_dispose (GObject *object)
{
    CoraFilesWindow *self = CORA_FILES_WINDOW (object);

    if (self->cancellable) {
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
    }

    g_clear_object (&self->current_dir);
    g_clear_object (&self->file_store);
    g_list_free_full (self->back_history, g_object_unref);
    g_list_free_full (self->forward_history, g_object_unref);
    self->back_history = NULL;
    self->forward_history = NULL;
    g_free (self->search_text);
    self->search_text = NULL;

    G_OBJECT_CLASS (cora_files_window_parent_class)->dispose (object);
}

static void
cora_files_window_init (CoraFilesWindow *self)
{
    self->current_dir = NULL;
    self->cancellable = NULL;
    self->back_history = NULL;
    self->forward_history = NULL;
    self->view_mode = VIEW_MODE_GRID;
    self->search_text = NULL;
}

static void
cora_files_window_class_init (CoraFilesWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = cora_files_window_dispose;
}

CoraFilesWindow *
cora_files_window_new (GtkApplication *app, GFile *initial_dir)
{
    CoraFilesWindow *self = g_object_new (CORA_TYPE_FILES_WINDOW,
                                          "application", app,
                                          NULL);
    cora_files_window_build_ui (self);

    /* Navigate to initial directory */
    if (initial_dir) {
        load_directory (self, initial_dir);
    } else {
        g_autoptr(GFile) home = g_file_new_for_path (g_get_home_dir ());
        load_directory (self, home);
    }

    return self;
}
