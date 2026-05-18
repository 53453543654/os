#!/bin/bash
# CoraOS ISO Build Script
# Builds a bootable live ISO from a Fedora base + CoraOS packages
# Requires: lorax, livemedia-creator, mock (run as root or with sudo)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VERSION="1.0"
CODENAME="Ember"
ARCH="x86_64"
RESULTDIR="/var/tmp/coraos-live"
ISO_NAME="CoraOS-${VERSION}-${CODENAME}-${ARCH}.iso"
ISO_LABEL="CoraOS-1-0"

echo "============================================"
echo "  CoraOS ${VERSION} (${CODENAME}) ISO Builder"
echo "============================================"
echo ""

# Check root
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script must be run as root (or with sudo)."
    exit 1
fi

# Check dependencies
for cmd in livemedia-creator lorax mkksiso; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: Required tool '$cmd' not found."
        echo "Install with: dnf install lorax livecd-tools"
        exit 1
    fi
done

# Step 1: Build CoraOS RPMs
echo "[1/4] Building CoraOS RPM packages..."
if [ -d "${PROJECT_ROOT}/builddir" ]; then
    pushd "${PROJECT_ROOT}"
    meson setup builddir --prefix=/usr --buildtype=release 2>/dev/null || true
    ninja -C builddir
    DESTDIR="${RESULTDIR}/rpmbuild" ninja -C builddir install
    popd
fi

# Step 2: Create local repo with CoraOS RPMs
echo "[2/4] Setting up local package repository..."
mkdir -p "${RESULTDIR}/repo"
if ls "${RESULTDIR}"/rpmbuild/usr/ &>/dev/null; then
    # Create RPMs from installed files using fpm or rpmbuild
    echo "  (CoraOS packages will be installed directly in post-install)"
fi

# Step 3: Build live image
echo "[3/4] Building live image with livemedia-creator..."
rm -rf "${RESULTDIR}/iso"
mkdir -p "${RESULTDIR}/iso"

livemedia-creator \
    --ks="${SCRIPT_DIR}/kickstart/coraos-live.ks" \
    --no-virt \
    --resultdir="${RESULTDIR}/iso" \
    --project="CoraOS" \
    --volid="${ISO_LABEL}" \
    --iso-only \
    --iso-name="${ISO_NAME}" \
    --releasever=39 \
    --macboot

# Step 4: Done
echo "[4/4] Build complete!"
echo ""
echo "ISO: ${RESULTDIR}/iso/${ISO_NAME}"
echo "Size: $(du -h "${RESULTDIR}/iso/${ISO_NAME}" | cut -f1)"
echo ""
echo "Write to USB: sudo dd if=${RESULTDIR}/iso/${ISO_NAME} of=/dev/sdX bs=4M status=progress oflag=sync"
