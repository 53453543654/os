Name:           coraos-apps
Version:        1.0.0
Release:        1%{?dist}
Summary:        CoraOS Core Applications - WiFi, Files, Browser, Settings
License:        GPL-3.0-or-later
URL:            https://github.com/coraos/os

BuildRequires:  meson >= 0.60
BuildRequires:  gcc
BuildRequires:  pkgconfig(gtk4) >= 4.10
BuildRequires:  pkgconfig(libadwaita-1) >= 1.3
BuildRequires:  pkgconfig(glib-2.0) >= 2.74
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(libnm) >= 1.40
BuildRequires:  pkgconfig(webkit2gtk-5.0)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  desktop-file-utils

%description
Core applications for CoraOS desktop environment.

%package -n coraos-wifi
Summary:        CoraOS WiFi Manager
Requires:       NetworkManager
Requires:       gtk4
Requires:       libadwaita
Requires:       coraos-theme

%description -n coraos-wifi
WiFi network manager with scanning, connecting, saved networks, and hotspot.

%package -n coraos-files
Summary:        CoraOS File Explorer
Requires:       gtk4
Requires:       libadwaita
Requires:       gvfs
Requires:       udisks2
Requires:       coraos-theme

%description -n coraos-files
Modern file manager with grid/list views, bookmarks, thumbnails, search.

%package -n coraos-browser
Summary:        CoraOS Web Browser
Requires:       gtk4
Requires:       webkit2gtk4.1
Requires:       libadwaita
Requires:       coraos-theme

%description -n coraos-browser
WebKit-based web browser with tabs, bookmarks, and privacy features.

%package -n coraos-settings
Summary:        CoraOS Settings
Requires:       gtk4
Requires:       libadwaita
Requires:       NetworkManager
Requires:       bluez
Requires:       power-profiles-daemon
Requires:       coraos-theme
Requires:       coraos-shell

%description -n coraos-settings
System settings application with appearance, network, display, sound,
power, users, keyboard, and accessibility configuration.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install

%files -n coraos-wifi
%{_bindir}/coraos-wifi
%{_datadir}/applications/dev.coraos.wifi.desktop

%files -n coraos-files
%{_bindir}/coraos-files
%{_datadir}/applications/dev.coraos.files.desktop

%files -n coraos-browser
%{_bindir}/coraos-browser
%{_datadir}/applications/dev.coraos.browser.desktop

%files -n coraos-settings
%{_bindir}/coraos-settings
%{_datadir}/applications/dev.coraos.settings.desktop
%{_datadir}/glib-2.0/schemas/dev.coraos.settings.gschema.xml
