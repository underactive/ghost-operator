#!/bin/bash
# screenshot-c6.sh — Capture a screenshot from the ESP32-C6 Ghost Operator
# Sends 'p' over serial, captures base64 BMP output, decodes to PNG
#
# Usage: ./screenshot-c6.sh [output.png] [port]
#   output.png  — output filename (default: screenshot_YYYYMMDD_HHMMSS.png)
#   port        — serial port (default: auto-detect /dev/cu.usbmodem*)

set -e

OUTPUT="${1:-}"
PORT="${2:-}"
BAUD=115200
WAIT_SECS=30

# Auto-detect port
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
  if [ -z "$PORT" ]; then
    echo "Error: No USB serial port found. Is the device connected?"
    exit 1
  fi
fi

# Check port is not busy
if lsof "$PORT" >/dev/null 2>&1; then
  echo "Error: $PORT is busy. Close PlatformIO monitor or other serial apps first."
  lsof "$PORT" 2>/dev/null
  exit 1
fi

# Default output filename with timestamp
if [ -z "$OUTPUT" ]; then
  OUTPUT="screenshot_$(date +%Y%m%d_%H%M%S).png"
fi

TMPRAW="/tmp/ghost_c6_serial_$$.txt"
TMPBMP="/tmp/ghost_c6_screenshot_$$.bmp"

echo "Port: $PORT"
echo "Output: $OUTPUT"
echo "Capturing screenshot (${WAIT_SECS}s timeout)..."

# Configure serial port
stty -f "$PORT" $BAUD raw -echo 2>/dev/null

# Start capturing serial output
cat "$PORT" > "$TMPRAW" &
CAT_PID=$!
sleep 0.5

# Send 'p' command
echo -n "p" > "$PORT"

# Wait for data (BMP is ~220KB base64 at 115200 baud = ~20s)
sleep $WAIT_SECS

# Stop capture
kill $CAT_PID 2>/dev/null
wait $CAT_PID 2>/dev/null || true

# Check we got data
SIZE=$(wc -c < "$TMPRAW" | tr -d ' ')
if [ "$SIZE" -lt 1000 ]; then
  echo "Error: Only received ${SIZE} bytes. Device may not be responding."
  echo "Make sure USB CDC is enabled and the device is running."
  rm -f "$TMPRAW"
  exit 1
fi

echo "Received ${SIZE} bytes"

# Extract base64 between markers
B64=$(sed -n '/--- BMP START ---/,/--- BMP END ---/p' "$TMPRAW" | grep -v "^---" | tr -d '\r\n')

if [ -z "$B64" ]; then
  echo "Error: No BMP data found in serial output."
  echo "Raw output (first 500 chars):"
  head -c 500 "$TMPRAW"
  rm -f "$TMPRAW"
  exit 1
fi

# Decode base64 to BMP
echo "$B64" | base64 -d > "$TMPBMP" 2>/dev/null

# Verify BMP
if ! file "$TMPBMP" | grep -q "bitmap"; then
  echo "Error: Decoded file is not a valid BMP."
  rm -f "$TMPRAW" "$TMPBMP"
  exit 1
fi

# Convert to PNG
if command -v sips >/dev/null 2>&1; then
  sips -s format png "$TMPBMP" --out "$OUTPUT" >/dev/null 2>&1
elif command -v convert >/dev/null 2>&1; then
  convert "$TMPBMP" "$OUTPUT"
else
  # No converter available, just keep the BMP
  OUTPUT="${OUTPUT%.png}.bmp"
  cp "$TMPBMP" "$OUTPUT"
  echo "Warning: No PNG converter found, saved as BMP"
fi

# Cleanup
rm -f "$TMPRAW" "$TMPBMP"

echo "Screenshot saved: $OUTPUT ($(wc -c < "$OUTPUT" | tr -d ' ') bytes)"

# Open in Preview on macOS
if [ "$(uname)" = "Darwin" ]; then
  open "$OUTPUT"
fi
