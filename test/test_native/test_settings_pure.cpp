#include <unity.h>
#include "settings_pure.h"

void setUp() {}
void tearDown() {}

void test_ghost_clamp_u32() {
  TEST_ASSERT_EQUAL_UINT32(0u, ghost_clamp_u32(0u, 0u, 10u));
  TEST_ASSERT_EQUAL_UINT32(10u, ghost_clamp_u32(20u, 0u, 10u));
  TEST_ASSERT_EQUAL_UINT32(10u, ghost_clamp_u32(5u, 10u, 100u));
  TEST_ASSERT_EQUAL_UINT32(100u, ghost_clamp_u32(200u, 10u, 100u));
}

void test_ghost_xor_checksum_bytes() {
  uint8_t b[] = {0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_UINT8(0u, ghost_xor_checksum_bytes(b, 0));
  TEST_ASSERT_EQUAL_UINT8(0x01u, ghost_xor_checksum_bytes(b, 1));
  TEST_ASSERT_EQUAL_UINT8(0x03u, ghost_xor_checksum_bytes(b, 2));
  TEST_ASSERT_EQUAL_UINT8(0x00u, ghost_xor_checksum_bytes(b, 3));
  uint8_t c[] = {0xFF, 0xFF};
  TEST_ASSERT_EQUAL_UINT8(0u, ghost_xor_checksum_bytes(c, 2));
}
