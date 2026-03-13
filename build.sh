#!/usr/bin/env bash
# build.sh — Ghost Operator build & release automation (PlatformIO)
# Usage:
#   ./build.sh            Compile firmware, report sizes
#   ./build.sh --release  Compile + create versioned DFU zip in releases/
#   ./build.sh --flash    Compile + flash via USB serial (PlatformIO + adafruit-nrfutil)
#   ./build.sh --setup    One-time: install PlatformIO (auto-installs board + libs on first build)

set -euo pipefail

# ── Constants ──────────────────────────────────────────────────────────────────
PIO_ENV="seeed_xiao_nrf52840"
BUILD_DIR=".pio/build/$PIO_ENV"

# ── Helpers ────────────────────────────────────────────────────────────────────
die()  { echo "ERROR: $*" >&2; exit 1; }
info() { echo "── $*"; }

# Extract version from src/config.h
get_version() {
    local ver
    ver=$(grep -E '^#define VERSION "' src/config.h | sed 's/.*"\(.*\)".*/\1/')
    [[ -n "$ver" ]] || die "Could not extract VERSION from src/config.h"
    echo "$ver"
}

# If HEAD is an exact git tag, validate it matches src/config.h version
validate_git_tag() {
    local ver="$1"
    local tag
    tag=$(git describe --exact-match --tags HEAD 2>/dev/null || true)
    if [[ -n "$tag" ]]; then
        local tag_ver="${tag#v}"  # strip v prefix
        if [[ "$tag_ver" != "$ver" ]]; then
            die "Git tag '$tag' (version $tag_ver) does not match src/config.h version '$ver'"
        fi
        info "Git tag $tag matches src/config.h version"
    fi
}

# ── Setup ──────────────────────────────────────────────────────────────────────
do_setup() {
    info "Installing PlatformIO..."
    if command -v pio &>/dev/null; then
        info "PlatformIO already installed: $(pio --version)"
    else
        pip3 install platformio
    fi

    # adafruit-nrfutil is needed for DFU upload
    if ! command -v adafruit-nrfutil &>/dev/null; then
        info "Installing adafruit-nrfutil..."
        pip3 install adafruit-nrfutil
    else
        info "adafruit-nrfutil already installed"
    fi

    echo ""
    info "Setup complete! Run 'make build' — PlatformIO auto-installs board + libs on first build."
}

# ── Compile ────────────────────────────────────────────────────────────────────
do_compile() {
    local ver
    ver=$(get_version)
    info "Compiling Ghost Operator v${ver}..."

    pio run -e "$PIO_ENV"

    local zip_file="$BUILD_DIR/firmware.zip"

    [[ -f "$zip_file" ]] || die "Expected DFU zip not found: $zip_file"

    # Report sizes
    local zip_size
    zip_size=$(wc -c < "$zip_file" | tr -d ' ')

    echo ""
    info "Build successful!"
    echo "  DFU: $zip_file ($zip_size bytes)"
    echo "  Version: $ver"
}

# ── Release ────────────────────────────────────────────────────────────────────
do_release() {
    do_compile

    local ver
    ver=$(get_version)
    validate_git_tag "$ver"

    local zip_file="$BUILD_DIR/firmware.zip"
    local release_name="ghost_operator_dfu-v${ver}.zip"
    local release_path="releases/$release_name"

    mkdir -p releases
    cp "$zip_file" "$release_path"

    echo ""
    info "Release artifact created:"
    echo "  $release_path"
}

# ── Flash ──────────────────────────────────────────────────────────────────────
find_port() {
    # Find USB serial port (macOS: tty.usbmodem*, Linux: ttyACM*)
    local p
    for p in /dev/tty.usbmodem* /dev/ttyACM*; do
        if [[ -e "$p" ]]; then
            echo "$p"
            return
        fi
    done
}

do_flash() {
    do_compile

    local zip_file="$BUILD_DIR/firmware.zip"

    if ! command -v adafruit-nrfutil &>/dev/null; then
        die "adafruit-nrfutil not found. Install with: pip3 install adafruit-nrfutil"
    fi

    # Find USB serial port
    local port
    port=$(find_port)
    [[ -n "$port" ]] || die "No USB serial port found (expected /dev/tty.usbmodem*). Is the device connected?"

    # 1200bps touch: open port at 1200 baud then close — triggers Adafruit
    # bootloader to reboot into Serial DFU mode (DTR assert/deassert)
    info "Triggering DFU mode on $port..."
    if ! python3 -c "
import serial, time
s = serial.Serial('$port', 1200)
time.sleep(0.1)
s.close()
"; then
        die "Failed to trigger DFU mode (is pyserial installed?)"
    fi

    # Wait for bootloader to re-enumerate USB
    info "Waiting for bootloader..."
    sleep 2

    # Re-detect port (may change after reboot into bootloader)
    port=$(find_port)
    [[ -n "$port" ]] || die "DFU port not found after reboot. Is the device in bootloader mode?"

    info "Flashing via $port..."
    adafruit-nrfutil dfu serial \
        --package "$zip_file" \
        --port "$port" \
        --baudrate 115200

    echo ""
    info "Flash complete!"
}

# ── Main ───────────────────────────────────────────────────────────────────────
case "${1:-}" in
    --setup)   do_setup   ;;
    --release) do_release ;;
    --flash)   do_flash   ;;
    --help|-h)
        echo "Usage: ./build.sh [--setup|--release|--flash|--help]"
        echo ""
        echo "  (no args)   Compile firmware, report sizes"
        echo "  --setup     Install PlatformIO + adafruit-nrfutil"
        echo "  --release   Compile + create versioned DFU zip in releases/"
        echo "  --flash     Compile + flash via USB serial"
        echo "  --help      Show this help"
        ;;
    "")        do_compile  ;;
    *)         die "Unknown option: $1 (try --help)" ;;
esac
