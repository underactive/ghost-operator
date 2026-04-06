#ifndef GHOST_COMMON_OTA_H
#define GHOST_COMMON_OTA_H

#if defined(GHOST_PLATFORM_C6) || defined(GHOST_PLATFORM_S3)

#include <cstdint>

// Progress callback: (bytesWritten, totalBytes)
typedef void (*OtaProgressFn)(uint32_t written, uint32_t total);

// Perform OTA firmware update over USB serial.
// This function does NOT return — reboots on success or error.
// Caller must send "+ok:ota\n" and flush before calling.
void performSerialOta(OtaProgressFn onProgress = nullptr);

#endif // GHOST_PLATFORM_C6 || GHOST_PLATFORM_S3
#endif // GHOST_COMMON_OTA_H
