/* CoraOS Wallpaper - Desktop background
 * 
 * Renders wallpaper behind all windows. Supports:
 * - Image files (PNG, JPEG, WebP)
 * - Solid color fallback
 * - Zoom/fill/center/tile modes
 *
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cora-wallpaper.h"

struct _CoraWallpaper {
    GtkWindow       parent_instance;

    CoraShell      *shell;
    GtkWidget      *picture;
    char           *image_path;
};

G_DEFINE_TYPE (CoraWallpaper, cora_wallpaper, GTK_TYPE_WINDOW)

static void
cora_wallpaper_build_ui (CoraWallpaper *self)
{
    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
    gtk_widget_add_css_class (GTK_WIDGET (self), "cora-wallpaper");

    /* Make it full screen, below everything */
    gtk_window_fullscreen (GTK_WINDOW (self));

    self->picture = gtk_picture_new ();
    gtk_picture_set_content_fit (GTK_PICTURE (self->picture), GTK_CONTENT_FIT_COVER);
    gtk_window_set_child (GTK_WINDOW (self), self->picture);

    /* Load default wallpaper */
    const char *wallpaper_paths[] = {
        NULL, /* user config - set below */
        "/usr/share/backgrounds/coraos/default.png",
        "/usr/share/backgrounds/coraos/default.jpg",
        NULL
    };

    /* Try user-configured wallpaper first */
    g_autofree char *user_wp = g_build_filename (
        g_get_user_config_dir (), "coraos", "wallpaper.png", NULL);
    wallpaper_paths[0] = user_wp;

    for (int i = 0; wallpaper_paths[i] != NULL; i++) {
        if (g_file_test (wallpaper_paths[i], G_FILE_TEST_EXISTS)) {
            cora_wallpaper_set_image (self, wallpaper_paths[i]);
            break;
        }
    }
}

void
cora_wallpaper_set_image (CoraWallpaper *self, const char *path)
{
    g_return_if_fail (CORA_IS_WALLPAPER (self));
    g_return_if_fail (path != NULL);

    g_free (self->image_path);
    self->image_path = g_strdup (path);

    g_autoptr(GFile) file = g_file_new_for_path (path);
    gtk_picture_set_file (GTK_PICTURE (self->picture), file);
}

static void
cora_wallpaper_finalize (GObject *object)
{
    CoraWallpaper *self = CORA_WALLPAPER (object);
    g_free (self->image_path);
    G_OBJECT_CLASS (cora_wallpaper_parent_class)->finalize (object);
}

static void
cora_wallpaper_init (CoraWallpaper *self)
{
    self->image_path = NULL;
}

static void
cora_wallpaper_class_init (CoraWallpaperClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = cora_wallpaper_finalize;
}

CoraWallpaper *
cora_wallpaper_new (CoraShell *shell)
{
    CoraWallpaper *self = g_object_new (CORA_TYPE_WALLPAPER,
                                        "application", shell,
                                        NULL);
    self->shell = shell;
    cora_wallpaper_build_ui (self);
    gtk_window_present (GTK_WINDOW (self));
    return self;
}
