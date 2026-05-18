# CoraOS Live ISO Kickstart
# Fedora-based with CoraOS custom desktop environment
# Build with: sudo livemedia-creator --ks=coraos-live.ks --no-virt --resultdir=/var/tmp/coraos-iso

%include /usr/share/spin-kickstarts/fedora-live-base.ks

lang en_US.UTF-8
keyboard us
timezone US/Eastern
selinux --enforcing
firewall --enabled --service=mdns
xconfig --startxonboot
zerombr
clearpart --all
part / --size 8192 --fstype ext4
services --enabled=NetworkManager,ModemManager,bluetooth,power-profiles-daemon
network --bootproto=dhcp --device=link --activate

repo --name=fedora --mirrorlist=https://mirrors.fedoraproject.org/mirrorlist?repo=fedora-$releasever&arch=$basearch
repo --name=updates --mirrorlist=https://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f$releasever&arch=$basearch

%packages
# === Core System ===
@core
@base-x
@hardware-support
@printing
@fonts
@multimedia
@networkmanager-submodules
kernel
kernel-modules
kernel-modules-extra

# === Display Server / Compositor ===
mutter
gnome-session
gdm
xorg-x11-server-Xwayland
wayland-protocols
libwayland-client
libwayland-server

# === GTK4 Development (for CoraOS apps) ===
gtk4
libadwaita
glib2
gobject-introspection
cairo
pango
gdk-pixbuf2
json-glib

# === WebKit (for browser) ===
webkit2gtk4.1
webkit2gtk4.1-devel

# === Network ===
NetworkManager
NetworkManager-wifi
NetworkManager-bluetooth
NetworkManager-openvpn
NetworkManager-libnm
wpa_supplicant

# === Audio ===
pipewire
pipewire-pulseaudio
wireplumber

# === Bluetooth ===
bluez
bluez-tools

# === Power ===
power-profiles-daemon
upower

# === File Management ===
gvfs
gvfs-mtp
gvfs-goa
gvfs-smb
gvfs-gphoto2
gvfs-afp
udisks2
tumbler

# === System ===
systemd
dbus
polkit
flatpak
xdg-desktop-portal
xdg-desktop-portal-gtk
xdg-user-dirs
xdg-user-dirs-gtk

# === Fonts ===
google-noto-sans-fonts
google-noto-sans-mono-fonts
google-noto-emoji-fonts
mozilla-fira-mono-fonts
liberation-fonts
fontawesome-fonts-all

# === Theming base ===
adwaita-icon-theme
hicolor-icon-theme

# === Utilities ===
bash-completion
vim-minimal
wget
curl
git
unzip
p7zip

# === CoraOS packages (built from this repo) ===
coraos-shell
coraos-settings
coraos-files
coraos-wifi
coraos-browser
coraos-theme
coraos-branding
coraos-session

%end

%post
# === OS Branding ===
cat > /etc/os-release << 'EOF'
NAME="CoraOS"
VERSION="1.0 (Ember)"
ID=coraos
ID_LIKE=fedora
VERSION_ID=1.0
VERSION_CODENAME=Ember
PLATFORM_ID="platform:f39"
PRETTY_NAME="CoraOS 1.0 (Ember)"
ANSI_COLOR="1;31"
LOGO=coraos-logo
CPE_NAME="cpe:/o:coraos:coraos:1"
HOME_URL="https://coraos.dev"
BUG_REPORT_URL="https://github.com/coraos/os/issues"
SUPPORT_URL="https://coraos.dev/support"
EOF

echo "CoraOS release 1.0 (Ember)" > /etc/coraos-release
echo "CoraOS release 1.0 (Ember)" > /etc/issue
echo "CoraOS release 1.0 (Ember)" > /etc/issue.net

# === GDM Configuration ===
mkdir -p /etc/gdm
cat > /etc/gdm/custom.conf << 'EOF'
[daemon]
DefaultSession=coraos-wayland.desktop
WaylandEnable=true
AutomaticLoginEnable=false

[security]
AllowRoot=false

[xdmcp]

[chooser]

[debug]
EOF

# === Set CoraOS as default session ===
ln -sf /usr/share/wayland-sessions/coraos-wayland.desktop /usr/share/gdm/BuiltIn/default.session

# === Default user settings via skel ===
mkdir -p /etc/skel/.config/coraos
cat > /etc/skel/.config/coraos/settings.json << 'EOF'
{
    "appearance": {
        "theme": "CoraOS-Dark",
        "accent-color": "#FF1A1A",
        "icon-theme": "CoraOS",
        "cursor-theme": "Adwaita",
        "font": "Noto Sans 11",
        "monospace-font": "Fira Mono 10",
        "animations": true,
        "animation-speed": 1.0
    },
    "panel": {
        "position": "top",
        "height": 32,
        "opacity": 0.95,
        "autohide": false
    },
    "desktop": {
        "wallpaper": "/usr/share/backgrounds/coraos/default.png",
        "wallpaper-mode": "zoom"
    }
}
EOF

# === Enable services ===
systemctl enable gdm.service
systemctl enable NetworkManager.service
systemctl enable bluetooth.service
systemctl enable power-profiles-daemon.service
systemctl set-default graphical.target

# === Plymouth theme ===
plymouth-set-default-theme coraos

# === Flatpak remote ===
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo || true

# === Set GTK theme for all users ===
mkdir -p /etc/gtk-4.0
echo '[Settings]' > /etc/gtk-4.0/settings.ini
echo 'gtk-theme-name=CoraOS' >> /etc/gtk-4.0/settings.ini
echo 'gtk-icon-theme-name=CoraOS' >> /etc/gtk-4.0/settings.ini
echo 'gtk-font-name=Noto Sans 11' >> /etc/gtk-4.0/settings.ini
echo 'gtk-application-prefer-dark-theme=true' >> /etc/gtk-4.0/settings.ini

mkdir -p /etc/gtk-3.0
echo '[Settings]' > /etc/gtk-3.0/settings.ini
echo 'gtk-theme-name=CoraOS' >> /etc/gtk-3.0/settings.ini
echo 'gtk-icon-theme-name=CoraOS' >> /etc/gtk-3.0/settings.ini
echo 'gtk-font-name=Noto Sans 11' >> /etc/gtk-3.0/settings.ini
echo 'gtk-application-prefer-dark-theme=true' >> /etc/gtk-3.0/settings.ini

# === User directory defaults ===
mkdir -p /etc/skel/{Desktop,Documents,Downloads,Music,Pictures,Videos}

%end

%post --nochroot
# Copy branding
cp -a /var/tmp/coraos-branding/* $INSTALL_ROOT/usr/share/ 2>/dev/null || true
%end
