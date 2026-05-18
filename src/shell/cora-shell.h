/* CoraOS Shell - Core Shell Object
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CORA_TYPE_SHELL (cora_shell_get_type ())
G_DECLARE_FINAL_TYPE (CoraShell, cora_shell, CORA, SHELL, GtkApplication)

typedef struct _CoraPanel    CoraPanel;
typedef struct _CoraLauncher CoraLauncher;
typedef struct _CoraWallpaper CoraWallpaper;

CoraShell    *cora_shell_new           (void);
void          cora_shell_start         (CoraShell *self);
void          cora_shell_stop          (CoraShell *self);
void          cora_shell_toggle_launcher (CoraShell *self);
void          cora_shell_show_launcher (CoraShell *self);
void          cora_shell_hide_launcher (CoraShell *self);
CoraPanel    *cora_shell_get_panel     (CoraShell *self);
GSettings    *cora_shell_get_settings  (CoraShell *self);
GdkMonitor   *cora_shell_get_primary_monitor (CoraShell *self);

G_END_DECLS
