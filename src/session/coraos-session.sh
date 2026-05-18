#!/bin/bash
# CoraOS Session Startup Script
# Launched by GDM/display manager via coraos-wayland.desktop
# Sets up the environment and starts the shell compositor

set -e

# === Environment ===
export XDG_CURRENT_DESKTOP="CoraOS"
export XDG_SESSION_DESKTOP="coraos"
export DESKTOP_SESSION="coraos"
export XDG_MENU_PREFIX="coraos-"

# GTK Theme
export GTK_THEME="CoraOS"
export GTK_ICON_THEME="CoraOS"

# Wayland
export MOZ_ENABLE_WAYLAND=1
export QT_QPA_PLATFORM=wayland
export CLUTTER_BACKEND=wayland
export SDL_VIDEODRIVER=wayland
export _JAVA_AWT_WM_NONREPARENTING=1
export GDK_BACKEND=wayland

# XDG directories
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
export XDG_STATE_HOME="${XDG_STATE_HOME:-$HOME/.local/state}"

# Ensure user directories exist
xdg-user-dirs-update 2>/dev/null || true

# === D-Bus Session ===
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    eval $(dbus-launch --sh-syntax)
    export DBUS_SESSION_BUS_ADDRESS
fi

# === Start services ===

# PipeWire audio
if command -v pipewire &>/dev/null; then
    pipewire &
    sleep 0.2
    wireplumber &
    pipewire-pulse &
fi

# XDG Desktop Portal
if command -v /usr/libexec/xdg-desktop-portal &>/dev/null; then
    /usr/libexec/xdg-desktop-portal &
fi

# GSettings daemon (for media keys, power, etc.)
if command -v gsd-xsettings &>/dev/null; then
    /usr/libexec/gsd-xsettings &
fi
if command -v gsd-power &>/dev/null; then
    /usr/libexec/gsd-power &
fi
if command -v gsd-media-keys &>/dev/null; then
    /usr/libexec/gsd-media-keys &
fi

# Polkit agent
if [ -x /usr/libexec/polkit-gnome-authentication-agent-1 ]; then
    /usr/libexec/polkit-gnome-authentication-agent-1 &
elif [ -x /usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1 ]; then
    /usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1 &
fi

# === Autostart ===
AUTOSTART_DIRS="${XDG_CONFIG_HOME}/autostart:/etc/xdg/autostart"
IFS=':' read -ra DIRS <<< "$AUTOSTART_DIRS"
for dir in "${DIRS[@]}"; do
    if [ -d "$dir" ]; then
        for desktop_file in "$dir"/*.desktop; do
            [ -f "$desktop_file" ] || continue
            # Check OnlyShowIn/NotShowIn
            if grep -q "OnlyShowIn=" "$desktop_file"; then
                if ! grep -q "CoraOS" "$desktop_file" && ! grep -q "GNOME" "$desktop_file"; then
                    continue
                fi
            fi
            if grep -q "NotShowIn=.*CoraOS" "$desktop_file"; then
                continue
            fi
            # Launch it
            exec_line=$(grep "^Exec=" "$desktop_file" | head -1 | cut -d= -f2-)
            if [ -n "$exec_line" ]; then
                $exec_line &
            fi
        done
    fi
done

# === Launch CoraOS Shell (main process) ===
# The shell manages the compositor, panel, launcher, etc.
# When it exits, the session ends.

if [ "$1" = "--x11" ]; then
    # X11 fallback mode
    exec mutter --x11 --replace -- coraos-shell
else
    # Default: Wayland via mutter
    exec gnome-session --session=coraos --systemd
fi
