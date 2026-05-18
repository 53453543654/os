/* CoraOS File Explorer - Window
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <libadwaita-1/adw.h>

G_BEGIN_DECLS

#define CORA_TYPE_FILES_WINDOW (cora_files_window_get_type ())
G_DECLARE_FINAL_TYPE (CoraFilesWindow, cora_files_window, CORA, FILES_WINDOW, AdwApplicationWindow)

CoraFilesWindow *cora_files_window_new        (GtkApplication *app, GFile *initial_dir);
void             cora_files_window_navigate   (CoraFilesWindow *self, GFile *directory);

G_END_DECLS
