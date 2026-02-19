#!/usr/bin/env bash
# build.sh — Ghost Operator build & release automation
# Usage:
#   ./build.sh            Compile firmware, report sizes
#   ./build.sh --release  Compile + create versioned DFU zip in releases/
#   ./build.sh --flash    Compile + flash via USB serial (adafruit-nrfutil)
#   ./build.sh --setup    One-time: install arduino-cli, board package, libraries

set -euo pipefail

# ── Constants ──────────────────────────────────────────────────────────────────
FQBN="Seeeduino:nrf52:xiaonRF52840"
BUILD_DIR="build/Seeeduino.nrf52.xiaonRF52840"
SKETCH_NAME="ghost_operator.ino"
BOARD_URL="https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json"

# ── Helpers ────────────────────────────────────────────────────────────────────
die()  { echo "ERROR: $*" >&2; exit 1; }
info() { echo "── $*"; }

# Extract version from config.h
get_version() {
    local ver
    ver=$(grep -E '^#define VERSION "' config.h | sed 's/.*"\(.*\)".*/\1/')
    [[ -n "$ver" ]] || die "Could not extract VERSION from config.h"
    echo "$ver"
}

# If HEAD is an exact git tag, validate it matches config.h version
validate_git_tag() {
    local ver="$1"
    local tag
    tag=$(git describe --exact-match --tags HEAD 2>/dev/null || true)
    if [[ -n "$tag" ]]; then
        local tag_ver="${tag#v}"  # strip v prefix
        if [[ "$tag_ver" != "$ver" ]]; then
            die "Git tag '$tag' (version $tag_ver) does not match config.h version '$ver'"
        fi
        info "Git tag $tag matches config.h version"
    fi
}

# ── Setup ──────────────────────────────────────────────────────────────────────
do_setup() {
    info "Installing arduino-cli via Homebrew..."
    if command -v arduino-cli &>/dev/null; then
        info "arduino-cli already installed: $(arduino-cli version)"
    else
        brew install arduino-cli
    fi

    info "Configuring arduino-cli..."
    arduino-cli config init --overwrite 2>/dev/null || true
    arduino-cli config add board_manager.additional_urls "$BOARD_URL"
    # Use ~/Arduino instead of ~/Documents/Arduino (macOS sandboxes ~/Documents)
    arduino-cli config set directories.user "$HOME/Arduino"

    info "Installing Seeed nRF52 board package..."
    arduino-cli core update-index
    arduino-cli core install Seeeduino:nrf52

    info "Installing required libraries..."
    arduino-cli lib install "Adafruit GFX Library"
    arduino-cli lib install "Adafruit SSD1306"
    arduino-cli lib install "Adafruit BusIO"

    # Check python symlink (Seeed toolchain needs it)
    if ! command -v python &>/dev/null; then
        echo ""
        echo "WARNING: 'python' command not found."
        echo "The Seeed nRF52 toolchain requires it. Fix with:"
        echo "  sudo ln -s \$(which python3) /usr/local/bin/python"
    else
        info "python available: $(python --version 2>&1)"
    fi

    echo ""
    info "Setup complete!"
}

# ── Compile ────────────────────────────────────────────────────────────────────
do_compile() {
    local ver
    ver=$(get_version)
    info "Compiling Ghost Operator v${ver}..."

    arduino-cli compile \
        --fqbn "$FQBN" \
        --build-path "$BUILD_DIR" \
        . \
        --warnings default

    local hex_file="$BUILD_DIR/$SKETCH_NAME.hex"
    local zip_file="$BUILD_DIR/$SKETCH_NAME.zip"

    [[ -f "$hex_file" ]] || die "Expected HEX file not found: $hex_file"
    [[ -f "$zip_file" ]] || die "Expected DFU zip not found: $zip_file"

    # Report sizes
    local hex_size zip_size
    hex_size=$(wc -c < "$hex_file" | tr -d ' ')
    zip_size=$(wc -c < "$zip_file" | tr -d ' ')

    echo ""
    info "Build successful!"
    echo "  HEX: $hex_file ($hex_size bytes)"
    echo "  DFU: $zip_file ($zip_size bytes)"
    echo "  Version: $ver"
}

# ── Release ────────────────────────────────────────────────────────────────────
do_release() {
    do_compile

    local ver
    ver=$(get_version)
    validate_git_tag "$ver"

    local zip_file="$BUILD_DIR/$SKETCH_NAME.zip"
    local release_name="ghost_operator_dfu-v${ver}.zip"
    local release_path="releases/$release_name"

    mkdir -p releases
    cp "$zip_file" "$release_path"

    echo ""
    info "Release artifact created:"
    echo "  $release_path"
}

# ── Flash ──────────────────────────────────────────────────────────────────────
do_flash() {
    do_compile

    local zip_file="$BUILD_DIR/$SKETCH_NAME.zip"

    # Find USB serial port
    local port
    port=$(find /dev -name "tty.usbmodem*" 2>/dev/null | head -1)
    [[ -n "$port" ]] || die "No USB serial port found (expected /dev/tty.usbmodem*). Is the device connected?"

    info "Flashing via $port..."

    if ! command -v adafruit-nrfutil &>/dev/null; then
        die "adafruit-nrfutil not found. Install with: pip install adafruit-nrfutil"
    fi

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
        echo "  --setup     Install arduino-cli, board package, libraries"
        echo "  --release   Compile + create versioned DFU zip in releases/"
        echo "  --flash     Compile + flash via USB serial"
        echo "  --help      Show this help"
        ;;
    "")        do_compile  ;;
    *)         die "Unknown option: $1 (try --help)" ;;
esac
