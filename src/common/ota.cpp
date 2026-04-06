#include "ota.h"

#if defined(GHOST_PLATFORM_C6) || defined(GHOST_PLATFORM_S3)

#include <Arduino.h>
#include <Update.h>

static const size_t OTA_CHUNK_SIZE = 4096;
static const unsigned long HEADER_TIMEOUT_MS = 10000;
static const unsigned long CHUNK_TIMEOUT_MS  = 5000;

static void otaFail(const char* reason) {
  Update.abort();
  Serial.print("-err:");
  Serial.println(reason);
  Serial.flush();
  delay(200);
  ESP.restart();
}

void performSerialOta(OtaProgressFn onProgress) {
  // Drain any pending serial input
  while (Serial.available()) Serial.read();

  // --- Read 4-byte LE uint32 firmware size ---
  uint8_t header[4];
  size_t headerRead = 0;
  unsigned long deadline = millis() + HEADER_TIMEOUT_MS;

  while (headerRead < 4) {
    if (millis() > deadline) {
      otaFail("header timeout");
    }
    if (Serial.available()) {
      header[headerRead++] = Serial.read();
      deadline = millis() + HEADER_TIMEOUT_MS; // reset on progress
    }
  }

  uint32_t firmwareSize = (uint32_t)header[0]
                        | ((uint32_t)header[1] << 8)
                        | ((uint32_t)header[2] << 16)
                        | ((uint32_t)header[3] << 24);

  if (firmwareSize == 0) {
    otaFail("zero size");
  }

  // --- Begin OTA update ---
  if (!Update.begin(firmwareSize)) {
    otaFail("begin failed");
  }

  Serial.println("+ready");
  Serial.flush();

  // --- Receive firmware data in chunks ---
  uint8_t buf[OTA_CHUNK_SIZE];
  uint32_t remaining = firmwareSize;
  uint32_t written = 0;

  while (remaining > 0) {
    size_t chunkLen = (remaining > OTA_CHUNK_SIZE) ? OTA_CHUNK_SIZE : remaining;

    // Read chunk with timeout
    Serial.setTimeout(CHUNK_TIMEOUT_MS);
    size_t received = Serial.readBytes(buf, chunkLen);

    if (received != chunkLen) {
      otaFail("chunk timeout");
    }

    // Write chunk to OTA partition
    size_t wr = Update.write(buf, chunkLen);
    if (wr != chunkLen) {
      otaFail("write failed");
    }

    remaining -= chunkLen;
    written += chunkLen;

    Serial.println("+ok");
    Serial.flush();

    if (onProgress) {
      onProgress(written, firmwareSize);
    }
  }

  // --- Finalize ---
  if (!Update.end(true)) {
    otaFail("validation failed");
  }

  Serial.println("+done");
  Serial.flush();
  delay(500);
  ESP.restart();
}

#endif // GHOST_PLATFORM_C6 || GHOST_PLATFORM_S3
