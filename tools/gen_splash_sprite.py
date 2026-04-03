#!/usr/bin/env python3
"""
Convert GhostFly.png sprite sheet to ARGB8888 C header for LVGL splash screen.

Input:  assets/GhostFly.png (256x64, 4 frames at 64x64 each, RGBA)
Output: src/c6/ghost_splash.h (ARGB8888 byte arrays, BGRA in memory for little-endian)
"""

import os
import sys
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
INPUT_PATH = os.path.join(PROJECT_DIR, "assets", "GhostFly.png")
OUTPUT_PATH = os.path.join(PROJECT_DIR, "src", "c6", "ghost_splash.h")

FRAME_W = 64
FRAME_H = 64
NUM_FRAMES = 4
BYTES_PER_PIXEL = 4  # ARGB8888


def extract_frame(img, index):
    """Extract a single frame from the sprite sheet and return BGRA bytes."""
    x_offset = index * FRAME_W
    frame = img.crop((x_offset, 0, x_offset + FRAME_W, FRAME_H))

    pixels = []
    for y in range(FRAME_H):
        for x in range(FRAME_W):
            r, g, b, a = frame.getpixel((x, y))
            # LVGL ARGB8888 on little-endian: stored as B, G, R, A in memory
            pixels.extend([b, g, r, a])
    return bytes(pixels)


def format_c_array(data, name, per_line=16):
    """Format byte data as a C array with PROGMEM."""
    size = len(data)
    lines = [f"static const uint8_t {name}[{size}] PROGMEM = {{"]
    for i in range(0, size, per_line):
        chunk = data[i:i + per_line]
        hex_vals = ",".join(f"0x{b:02X}" for b in chunk)
        comma = "," if i + per_line < size else ""
        lines.append(f"  {hex_vals}{comma}")
    lines.append("};")
    return "\n".join(lines)


def main():
    if not os.path.exists(INPUT_PATH):
        print(f"Error: {INPUT_PATH} not found", file=sys.stderr)
        sys.exit(1)

    img = Image.open(INPUT_PATH).convert("RGBA")
    w, h = img.size
    print(f"Input: {INPUT_PATH} ({w}x{h})")

    if w != FRAME_W * NUM_FRAMES or h != FRAME_H:
        print(f"Error: expected {FRAME_W * NUM_FRAMES}x{FRAME_H}, got {w}x{h}", file=sys.stderr)
        sys.exit(1)

    header_parts = [
        "#ifndef GHOST_C6_SPLASH_H",
        "#define GHOST_C6_SPLASH_H",
        "",
        "#include <lvgl.h>",
        "",
        f"#define SPLASH_FRAME_W {FRAME_W}",
        f"#define SPLASH_FRAME_H {FRAME_H}",
        f"#define SPLASH_NUM_FRAMES {NUM_FRAMES}",
        "",
    ]

    frame_names = []
    for i in range(NUM_FRAMES):
        name = f"splashFrame{i}"
        frame_names.append(name)
        data = extract_frame(img, i)
        print(f"  Frame {i}: {len(data)} bytes")
        header_parts.append(format_c_array(data, name))
        header_parts.append("")

    # Pointer array
    header_parts.append(f"static const uint8_t* splashFrames[SPLASH_NUM_FRAMES] = {{")
    for name in frame_names:
        header_parts.append(f"  {name},")
    header_parts.append("};")
    header_parts.append("")
    header_parts.append("#endif // GHOST_C6_SPLASH_H")
    header_parts.append("")

    with open(OUTPUT_PATH, "w") as f:
        f.write("\n".join(header_parts))

    print(f"Output: {OUTPUT_PATH}")
    total_kb = (FRAME_W * FRAME_H * BYTES_PER_PIXEL * NUM_FRAMES) / 1024
    print(f"Total: {NUM_FRAMES} frames, {total_kb:.0f} KB")


if __name__ == "__main__":
    main()
