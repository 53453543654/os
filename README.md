# CoraOS

A real, Fedora-based Linux distribution with a custom black & red desktop environment built entirely in C using GTK4, libadwaita, and native system APIs.

## Features

- **CoraOS Shell** — Custom desktop shell with top panel (clock, WiFi, volume, battery, power), application launcher with search, wallpaper management
- **CoraOS WiFi** — WiFi manager using NetworkManager (libnm) for scanning, connecting, saved networks, WPA/WPA2 auth
- **CoraOS Files** — File explorer with grid/list views, navigation history, bookmarks sidebar, new folder, trash support
- **CoraOS Browser** — WebKitGTK-based web browser with tabs, downloads, private browsing, smart URL bar
- **CoraOS Settings** — Full system settings: appearance (theme, accent color, fonts, animations, wallpaper), display, sound, network, bluetooth, power profiles, keyboard shortcuts, users, about
- **Black & Red Theme** — Complete GTK4/GTK3 CSS theme, custom icon set, Plymouth boot splash, branded wallpaper

## Architecture

```
CoraOS 1.0 (Ember)
├── Base: Fedora Linux 39 (kernel, systemd, DNF, SELinux)
├── Display: Wayland (via GDM + gnome-session)
├── Compositor: Mutter (libmutter)
├── Shell: coraos-shell (C, GTK4)
├── Audio: PipeWire + WirePlumber
├── Network: NetworkManager
├── Theme: CoraOS-Dark (GTK4 CSS)
└── Apps: coraos-wifi, coraos-files, coraos-browser, coraos-settings (all C/GTK4)
```

## Building

### Prerequisites

A Fedora 39+ system with development packages:

```bash
sudo dnf install meson ninja-build gcc \
    gtk4-devel libadwaita-devel glib2-devel json-glib-devel \
    NetworkManager-libnm-devel webkit2gtk5.0-devel \
    cairo-devel desktop-file-utils
```

### Compile

```bash
meson setup builddir
ninja -C builddir
```

### Install (system-wide)

```bash
sudo ninja -C builddir install
# Compile GSettings schemas
sudo glib-compile-schemas /usr/share/glib-2.0/schemas/
# Update icon cache
sudo gtk-update-icon-cache /usr/share/icons/CoraOS/
```

### Build Live ISO

Requires root and `lorax`/`livemedia-creator`:

```bash
sudo dnf install lorax livecd-tools
sudo ./build/build-iso.sh
```

The ISO will be at `/var/tmp/coraos-live/CoraOS-1.0-Ember-x86_64.iso`

## Running (Development)

After installing, select "CoraOS" from the GDM session picker, or test in a nested session:

```bash
dbus-run-session coraos-shell
```

## Project Structure

```
os/
├── meson.build              # Top-level build system
├── src/
│   ├── shell/               # Desktop shell (panel, launcher, wallpaper)
│   ├── wifi/                # WiFi manager (libnm)
│   ├── files/               # File explorer (GIO)
│   ├── browser/             # Web browser (WebKitGTK)
│   ├── settings/            # System settings
│   └── session/             # Session startup script
├── data/
│   ├── themes/CoraOS/       # GTK4 + GTK3 CSS theme
│   ├── icons/CoraOS/        # SVG icon theme
│   ├── sessions/            # GDM/Wayland session files
│   ├── applications/        # .desktop files
│   ├── systemd/             # User session units
│   ├── glib-2.0/schemas/    # GSettings schemas
│   ├── backgrounds/         # Default wallpaper
│   ├── plymouth/            # Boot splash
│   ├── gdm/                 # GDM configuration
│   └── xdg/                 # MIME associations
├── build/
│   ├── kickstart/           # Fedora kickstart for ISO
│   ├── specs/               # RPM spec files
│   ├── lorax/               # Lorax bootloader template
│   └── build-iso.sh         # ISO build script
└── branding/                # Logo and wordmark SVGs
```

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C (GNU C17) |
| UI Toolkit | GTK4 + libadwaita |
| Build System | Meson + Ninja |
| Window Management | Mutter / libmutter |
| Display Server | Wayland |
| Audio | PipeWire |
| Network | NetworkManager (libnm) |
| Web Engine | WebKitGTK |
| Session | gnome-session + systemd |
| Package Format | RPM (Fedora) |
| Init System | systemd |
| Filesystem | Btrfs (default) |

## License

GPL-3.0-or-later
