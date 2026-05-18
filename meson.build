# CoraOS - Top-level Meson Build System
# Build all components of CoraOS: shell, apps, theme, session data
#
# Usage:
#   meson setup builddir
#   ninja -C builddir
#   sudo ninja -C builddir install
#
# To build ISO after install:
#   sudo ./build/build-iso.sh

project('coraos', 'c',
    version: '1.0.0',
    license: 'GPL-3.0-or-later',
    meson_version: '>= 0.60.0',
    default_options: [
        'c_std=gnu17',
        'warning_level=2',
        'buildtype=release',
    ],
)

# Project metadata
coraos_version = meson.project_version()
coraos_prefix = get_option('prefix')
coraos_bindir = join_paths(coraos_prefix, get_option('bindir'))
coraos_datadir = join_paths(coraos_prefix, get_option('datadir'))
coraos_libdir = join_paths(coraos_prefix, get_option('libdir'))

# Required system dependencies (check availability)
gtk4_dep = dependency('gtk4', version: '>= 4.10', required: true)
adw_dep = dependency('libadwaita-1', version: '>= 1.3', required: true)
glib_dep = dependency('glib-2.0', version: '>= 2.74', required: true)
gio_dep = dependency('gio-2.0', required: true)
json_dep = dependency('json-glib-1.0', required: true)
cairo_dep = dependency('cairo', required: true)

# Optional dependencies
nm_dep = dependency('libnm', version: '>= 1.40', required: false)
webkit_dep = dependency('webkitgtk-6.0', required: false)
if not webkit_dep.found()
    webkit_dep = dependency('webkit2gtk-5.0', required: false)
endif

# === Build Components ===

# Shell (always built)
subdir('src/shell')

# WiFi manager (requires libnm)
if nm_dep.found()
    subdir('src/wifi')
else
    warning('libnm not found - skipping WiFi manager')
endif

# File Explorer (always built)
subdir('src/files')

# Browser (requires WebKitGTK)
if webkit_dep.found()
    subdir('src/browser')
else
    warning('WebKitGTK not found - skipping browser')
endif

# Settings (always built)
subdir('src/settings')

# === Install Data Files ===

# Session launcher script
install_data('src/session/coraos-session.sh',
    install_dir: coraos_bindir,
    rename: 'coraos-session',
    install_mode: 'rwxr-xr-x',
)

# Wayland session desktop file (for GDM)
install_data('data/sessions/coraos-wayland.desktop',
    install_dir: join_paths(coraos_datadir, 'wayland-sessions'),
)

# X11 session desktop file (fallback)
install_data('data/sessions/coraos-xorg.desktop',
    install_dir: join_paths(coraos_datadir, 'xsessions'),
)

# GNOME session file
install_data('data/sessions/coraos.session',
    install_dir: join_paths(coraos_datadir, 'gnome-session', 'sessions'),
)

# Application .desktop files
install_data(
    'data/applications/dev.coraos.shell.desktop',
    'data/applications/dev.coraos.browser.desktop',
    'data/applications/dev.coraos.files.desktop',
    'data/applications/dev.coraos.wifi.desktop',
    'data/applications/dev.coraos.settings.desktop',
    install_dir: join_paths(coraos_datadir, 'applications'),
)

# GSettings schemas
install_data('data/glib-2.0/schemas/dev.coraos.shell.gschema.xml',
    install_dir: join_paths(coraos_datadir, 'glib-2.0', 'schemas'),
)

# GTK Theme
install_subdir('data/themes/CoraOS',
    install_dir: join_paths(coraos_datadir, 'themes'),
)

# Icon Theme
install_subdir('data/icons/CoraOS',
    install_dir: join_paths(coraos_datadir, 'icons'),
)

# Backgrounds
install_data('data/backgrounds/coraos-default.svg',
    install_dir: join_paths(coraos_datadir, 'backgrounds', 'coraos'),
    rename: 'default.svg',
)

# Plymouth theme
install_data(
    'data/plymouth/coraos/coraos.plymouth',
    'data/plymouth/coraos/coraos.script',
    install_dir: join_paths(coraos_datadir, 'plymouth', 'themes', 'coraos'),
)

# systemd user units
install_data('data/systemd/coraos-shell.service',
    install_dir: join_paths(coraos_libdir, 'systemd', 'user'),
)
install_data('data/systemd/coraos-session.target',
    install_dir: join_paths(coraos_libdir, 'systemd', 'user'),
)

# XDG defaults
install_data('data/xdg/coraos-mimeapps.list',
    install_dir: join_paths(coraos_datadir, 'applications'),
    rename: 'mimeapps.list',
)

# Branding
install_data(
    'branding/coraos-logo.svg',
    'branding/coraos-wordmark.svg',
    install_dir: join_paths(coraos_datadir, 'coraos', 'branding'),
)

# GDM configuration (installed to /etc via DESTDIR during ISO build)
install_data('data/gdm/coraos-gdm.conf',
    install_dir: join_paths(get_option('sysconfdir'), 'gdm'),
    rename: 'custom.conf',
)

# === Post-install: compile GSettings schemas ===
gnome = import('gnome')
gnome.post_install(
    glib_compile_schemas: true,
    gtk_update_icon_cache: true,
    update_desktop_database: true,
)

# === Summary ===
summary({
    'Version': coraos_version,
    'Prefix': coraos_prefix,
    'Shell': true,
    'WiFi Manager': nm_dep.found(),
    'File Explorer': true,
    'Browser': webkit_dep.found(),
    'Settings': true,
}, section: 'CoraOS Components')
