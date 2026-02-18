#include "screenshot.h"
#include "state.h"

// CRC32 bit-by-bit (polynomial 0xEDB88320, no lookup table needed)
static uint32_t pngCrc32(const uint8_t* data, size_t len, uint32_t crc) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
  }
  return crc;
}

// Base64 encoder state (file-static)
static const char PROGMEM b64Chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8_t b64Buf[57];  // 57 raw bytes = 76 base64 chars per line
static uint8_t b64Pos;

static void b64FlushLine() {
  if (b64Pos == 0) return;
  char out[4];
  for (uint8_t i = 0; i < b64Pos; i += 3) {
    uint8_t remaining = b64Pos - i;
    uint32_t triplet = ((uint32_t)b64Buf[i] << 16);
    if (remaining > 1) triplet |= ((uint32_t)b64Buf[i+1] << 8);
    if (remaining > 2) triplet |= b64Buf[i+2];

    out[0] = pgm_read_byte(&b64Chars[(triplet >> 18) & 0x3F]);
    out[1] = pgm_read_byte(&b64Chars[(triplet >> 12) & 0x3F]);
    out[2] = (remaining > 1) ? pgm_read_byte(&b64Chars[(triplet >> 6) & 0x3F]) : '=';
    out[3] = (remaining > 2) ? pgm_read_byte(&b64Chars[triplet & 0x3F]) : '=';
    Serial.write((uint8_t*)out, 4);
  }
  Serial.println();
  b64Pos = 0;
}

static void b64Write(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    b64Buf[b64Pos++] = data[i];
    if (b64Pos == 57) b64FlushLine();
  }
}

static void b64WriteByte(uint8_t val) {
  b64Buf[b64Pos++] = val;
  if (b64Pos == 57) b64FlushLine();
}

// Write 4 bytes big-endian to base64 stream
static void b64WriteU32(uint32_t val) {
  uint8_t buf[4] = {
    (uint8_t)(val >> 24), (uint8_t)(val >> 16),
    (uint8_t)(val >> 8),  (uint8_t)(val)
  };
  b64Write(buf, 4);
}

// Convert one display row from SSD1306 page format to PNG row-major
static void convertSsdRow(const uint8_t* ssdBuf, uint8_t y, uint8_t* out16) {
  uint8_t page = y >> 3;
  uint8_t bit  = y & 7;
  const uint8_t* pageBase = ssdBuf + page * 128;
  for (uint8_t byteIdx = 0; byteIdx < 16; byteIdx++) {
    uint8_t packed = 0;
    uint8_t x = byteIdx << 3;
    for (uint8_t bitPos = 0; bitPos < 8; bitPos++) {
      if (pageBase[x + bitPos] & (1 << bit)) {
        packed |= (0x80 >> bitPos);
      }
    }
    out16[byteIdx] = packed;
  }
}

// PNG screenshot -- outputs base64-encoded PNG of current display over serial
void serialScreenshot() {
  if (!displayInitialized) {
    Serial.println("[ERR] Display not initialized");
    return;
  }

  const uint8_t* ssdBuf = display.getBuffer();
  uint8_t rowBuf[16];

  Serial.println("\n--- PNG START ---");
  b64Pos = 0;

  // --- PNG Signature ---
  static const uint8_t PROGMEM pngSig[] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
  uint8_t sig[8];
  memcpy_P(sig, pngSig, 8);
  b64Write(sig, 8);

  // --- IHDR Chunk ---
  // Length(4) + "IHDR"(4) + data(13) + CRC(4) = 25 bytes
  static const uint8_t PROGMEM ihdr[] = {
    0x00,0x00,0x00,0x0D,              // length = 13
    0x49,0x48,0x44,0x52,              // "IHDR"
    0x00,0x00,0x00,0x80,              // width = 128
    0x00,0x00,0x00,0x40,              // height = 64
    0x01,                             // bit depth = 1
    0x00,                             // color type = 0 (grayscale)
    0x00,                             // compression = 0
    0x00,                             // filter = 0
    0x00,                             // interlace = 0
  };
  uint8_t ihdrBuf[21];
  memcpy_P(ihdrBuf, ihdr, 21);
  b64Write(ihdrBuf, 21);
  // CRC over type+data (bytes 4..20)
  uint32_t ihdrCrc = pngCrc32(ihdrBuf + 4, 17, 0xFFFFFFFF) ^ 0xFFFFFFFF;
  b64WriteU32(ihdrCrc);

  // --- IDAT Chunk ---
  // Scanline data: 64 rows * (1 filter + 16 pixels) = 1088 bytes
  // Zlib: header(2) + deflate stored(1+2+2) + data(1088) + adler32(4) = 1099 bytes
  static const uint16_t IDAT_DATA_LEN = 1099;
  static const uint16_t RAW_LEN = 1088;  // 64 * 17

  // IDAT length
  uint8_t idatLen[4] = {
    (uint8_t)(IDAT_DATA_LEN >> 24), (uint8_t)(IDAT_DATA_LEN >> 16),
    (uint8_t)(IDAT_DATA_LEN >> 8),  (uint8_t)(IDAT_DATA_LEN)
  };
  b64Write(idatLen, 4);

  // "IDAT" type -- start CRC accumulator
  static const uint8_t idatType[] = {'I','D','A','T'};
  b64Write(idatType, 4);
  uint32_t idatCrc = pngCrc32(idatType, 4, 0xFFFFFFFF);

  // Zlib header
  uint8_t zlibHdr[] = {0x78, 0x01};
  b64Write(zlibHdr, 2);
  idatCrc = pngCrc32(zlibHdr, 2, idatCrc);

  // Deflate stored block header: BFINAL=1, BTYPE=00, LEN=1088, NLEN=~1088
  uint8_t deflateHdr[] = {0x01, (uint8_t)(RAW_LEN & 0xFF), (uint8_t)(RAW_LEN >> 8),
                          (uint8_t)(~RAW_LEN & 0xFF), (uint8_t)((~RAW_LEN >> 8) & 0xFF)};
  b64Write(deflateHdr, 5);
  idatCrc = pngCrc32(deflateHdr, 5, idatCrc);

  // Scanline data + Adler32 accumulation
  uint32_t adlerA = 1, adlerB = 0;

  for (uint8_t y = 0; y < 64; y++) {
    // Filter byte (None = 0x00)
    uint8_t filterByte = 0x00;
    b64WriteByte(filterByte);
    idatCrc = pngCrc32(&filterByte, 1, idatCrc);
    adlerA = (adlerA + filterByte) % 65521;
    adlerB = (adlerB + adlerA) % 65521;

    // Convert and emit 16 pixel bytes
    convertSsdRow(ssdBuf, y, rowBuf);
    b64Write(rowBuf, 16);
    idatCrc = pngCrc32(rowBuf, 16, idatCrc);
    for (uint8_t i = 0; i < 16; i++) {
      adlerA = (adlerA + rowBuf[i]) % 65521;
      adlerB = (adlerB + adlerA) % 65521;
    }
  }

  // Adler32 checksum (big-endian)
  uint32_t adler = (adlerB << 16) | adlerA;
  uint8_t adlerBytes[4] = {
    (uint8_t)(adler >> 24), (uint8_t)(adler >> 16),
    (uint8_t)(adler >> 8),  (uint8_t)(adler)
  };
  b64Write(adlerBytes, 4);
  idatCrc = pngCrc32(adlerBytes, 4, idatCrc);

  // IDAT CRC
  idatCrc ^= 0xFFFFFFFF;
  b64WriteU32(idatCrc);

  // --- IEND Chunk ---
  static const uint8_t PROGMEM iend[] = {
    0x00,0x00,0x00,0x00,              // length = 0
    0x49,0x45,0x4E,0x44,              // "IEND"
    0xAE,0x42,0x60,0x82               // CRC (well-known constant)
  };
  uint8_t iendBuf[12];
  memcpy_P(iendBuf, iend, 12);
  b64Write(iendBuf, 12);

  // Flush remaining base64
  b64FlushLine();

  Serial.println("--- PNG END ---");
}
