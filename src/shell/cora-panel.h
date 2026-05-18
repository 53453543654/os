/* CoraOS Panel - Top bar with clock, system tray, and app button
 * Copyright (C) 2024 CoraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include "cora-shell.h"

G_BEGIN_DECLS

#define CORA_TYPE_PANEL (cora_panel_get_type ())
G_DECLARE_FINAL_TYPE (CoraPanel, cora_panel, CORA, PANEL, GtkApplicationWindow)

CoraPanel *cora_panel_new          (CoraShell *shell);
void       cora_panel_update_clock (CoraPanel *self);

G_END_DECLS
