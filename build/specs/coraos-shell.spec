Name:           coraos-shell
Version:        1.0.0
Release:        1%{?dist}
Summary:        CoraOS Desktop Shell - Window management, panel, and app launcher
License:        GPL-3.0-or-later
URL:            https://github.com/coraos/os
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  meson >= 0.60
BuildRequires:  gcc
BuildRequires:  pkgconfig(gtk4) >= 4.10
BuildRequires:  pkgconfig(libadwaita-1) >= 1.3
BuildRequires:  pkgconfig(glib-2.0) >= 2.74
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(json-glib-1.0)
BuildRequires:  pkgconfig(libmutter-13)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(pango)
BuildRequires:  desktop-file-utils

Requires:       gtk4 >= 4.10
Requires:       libadwaita >= 1.3
Requires:       mutter >= 45
Requires:       gdm
Requires:       json-glib
Requires:       coraos-theme
Requires:       coraos-session

%description
CoraOS Shell provides the desktop environment for CoraOS including
the top panel with system tray, application launcher, window management
via libmutter, workspace switching, and notifications.

%prep
%autosetup

%build
%meson -Dcomponent=shell
%meson_build

%install
%meson_install

%files
%license COPYING
%{_bindir}/coraos-shell
%{_libdir}/coraos/libcora-shell.so
%{_datadir}/coraos/shell/
%{_datadir}/glib-2.0/schemas/dev.coraos.shell.gschema.xml
