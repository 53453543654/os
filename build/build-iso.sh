#!/bin/bash
# ============================================================
# CoraOS ISO Build Script
# ============================================================
# Builds a bootable CoraOS live ISO from source.
#
# Requirements:
#   - Fedora 41+ host system
#   - Root/sudo access
#   - ~4GB free disk space
#   - Internet access (for package downloads)
#
# Usage:
#   sudo ./build/build-iso.sh
#
# Output:
#   ./output/CoraOS-1.0-Ember-x86_64.iso
# ============================================================

set -euo pipefail

# === Configuration ===
VERSION="1.0"
CODENAME="Ember"
ARCH="x86_64"
ISO_NAME="CoraOS-${VERSION}-${CODENAME}-${ARCH}.iso"
ISO_LABEL="CoraOS-1-0"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_ROOT}/output"
BUILD_DIR="${PROJECT_ROOT}/builddir"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

log()   { echo -e "${GREEN}[CoraOS]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# === Preflight checks ===
echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║   CoraOS ${VERSION} (${CODENAME}) ISO Builder          ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════╝${NC}"
echo ""

# Must be root
if [ "$(id -u)" -ne 0 ]; then
    error "This script must be run as root (or with sudo)."
    echo "  Usage: sudo $0"
    exit 1
fi

# Check OS
if [ ! -f /etc/fedora-release ]; then
    warn "This script is designed for Fedora. Other distros may not work."
fi

# Check required tools
log "Checking required tools..."
MISSING=""
for cmd in meson ninja-build gcc livemedia-creator mksquashfs mkfs.ext4; do
    ACTUAL_CMD="${cmd}"
    # ninja-build binary is 'ninja' on Fedora
    [ "$cmd" = "ninja-build" ] && ACTUAL_CMD="ninja"
    if ! command -v "$ACTUAL_CMD" &>/dev/null; then
        MISSING="${MISSING} ${cmd}"
    fi
done

if [ -n "$MISSING" ]; then
    error "Missing required tools:${MISSING}"
    echo ""
    log "Installing missing dependencies..."
    dnf install -y --setopt=install_weak_deps=False \
        meson ninja-build gcc gcc-c++ pkg-config \
        gtk4-devel libadwaita-devel glib2-devel \
        json-glib-devel cairo-devel pango-devel \
        NetworkManager-libnm-devel \
        webkitgtk6.0-devel \
        desktop-file-utils \
        lorax livecd-tools squashfs-tools \
        grub2-efi-x64 shim-x64 \
        dracut dracut-live \
        e2fsprogs dosfstools parted
fi

# Check disk space (need ~4GB)
AVAIL_MB=$(df --output=avail -BM /tmp | tail -1 | tr -d ' M')
if [ "$AVAIL_MB" -lt 4000 ]; then
    error "Insufficient disk space. Need ~4GB, have ${AVAIL_MB}MB."
    exit 1
fi

log "All checks passed."
echo ""

# === Step 1: Compile CoraOS ===
log "[1/4] Compiling CoraOS components..."

cd "$PROJECT_ROOT"

if [ -d "$BUILD_DIR" ]; then
    meson setup "$BUILD_DIR" --prefix=/usr --buildtype=release --wipe
else
    meson setup "$BUILD_DIR" --prefix=/usr --buildtype=release
fi

ninja -C "$BUILD_DIR"
log "Compilation successful."

# === Step 2: Install to staging ===
log "[2/4] Installing to staging area..."

STAGING="/tmp/coraos-iso-staging"
rm -rf "$STAGING"
DESTDIR="$STAGING" ninja -C "$BUILD_DIR" install

FILE_COUNT=$(find "$STAGING" -type f | wc -l)
log "Installed ${FILE_COUNT} files to staging."

# === Step 3: Build ISO ===
log "[3/4] Building live ISO with livemedia-creator..."
echo "  This may take 10-20 minutes depending on your internet speed."
echo ""

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

livemedia-creator \
    --ks="${SCRIPT_DIR}/kickstart/coraos-live.ks" \
    --no-virt \
    --resultdir="$OUTPUT_DIR" \
    --project="CoraOS" \
    --volid="$ISO_LABEL" \
    --iso-only \
    --iso-name="$ISO_NAME" \
    --releasever=41 \
    --macboot

# === Step 4: Done ===
echo ""
log "[4/4] Build complete!"
echo ""

if [ -f "${OUTPUT_DIR}/${ISO_NAME}" ]; then
    ISO_SIZE=$(du -h "${OUTPUT_DIR}/${ISO_NAME}" | cut -f1)
    echo -e "${BOLD}╔══════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}║  ISO Ready!                                  ║${NC}"
    echo -e "${BOLD}╚══════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  ${GREEN}File:${NC} ${OUTPUT_DIR}/${ISO_NAME}"
    echo -e "  ${GREEN}Size:${NC} ${ISO_SIZE}"
    echo ""
    echo "  Write to USB:"
    echo "    sudo dd if=${OUTPUT_DIR}/${ISO_NAME} of=/dev/sdX bs=4M status=progress oflag=sync"
    echo ""
    echo "  Test in QEMU:"
    echo "    qemu-system-x86_64 -enable-kvm -m 4G -cdrom ${OUTPUT_DIR}/${ISO_NAME}"
    echo ""
else
    error "ISO file not found. Check livemedia-creator output above."
    exit 1
fi
