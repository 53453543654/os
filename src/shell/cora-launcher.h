/* CoraOS Application Launcher
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "cora-shell.h"

G_BEGIN_DECLS

#define CORA_TYPE_LAUNCHER (cora_launcher_get_type ())
G_DECLARE_FINAL_TYPE (CoraLauncher, cora_launcher, CORA, LAUNCHER, GtkWindow)

CoraLauncher *cora_launcher_new           (CoraShell *shell);
void          cora_launcher_focus_search  (CoraLauncher *self);

G_END_DECLS
