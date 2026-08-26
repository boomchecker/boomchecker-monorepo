/**
 ******************************************************************************
 * @file    config_store_test.c
 * @brief   Tests for boomlink_config_store_load()/_save() (boomlink.md
 *          section 10.1), against a FAKE in-memory boomlink_storage_port_t
 *          rather than real flash - the same "test the logic, not the
 *          hardware" split the link engine gets from its own fake radio
 *          (tests/fake_port.h).
 *
 *          The guarantee under test: "valid -> validate -> apply;
 *          missing/invalid -> load safe defaults" (section 10.1) - every
 *          scenario here is a way the region can fail to be valid (wrong
 *          magic, wrong format_version, a bad CRC, a corrupt or truncated
 *          length) and confirms boomlink_config_store_load() reports it as a
 *          clean `false` rather than a garbage decode, an out-of-bounds
 *          read, or a partially-filled `*out`.
 ******************************************************************************
 */
#include "boomlink_config_store.h"

#include <string.h>

#include "c_test.h"

BOOMLINK_TEST_STATE;

/* Mirrors boomlink_config_store.c's own put_u32_le()/get_u32_le() exactly -
   this test pokes at the wrapper's header bytes directly to simulate
   corruption, and the header is an explicit little-endian byte layout (not
   a struct), so a raw memcpy of a host uint32_t here would silently assume
   a little-endian host instead of exercising the actual documented format. */
static void put_u32_le(uint8_t *buf, uint32_t value) {
  buf[0] = (uint8_t)(value & 0xFFu);
  buf[1] = (uint8_t)((value >> 8) & 0xFFu);
  buf[2] = (uint8_t)((value >> 16) & 0xFFu);
  buf[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t get_u32_le(const uint8_t *buf) {
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

/* --- the fake backend ------------------------------------------------------- */

#define FAKE_REGION_SIZE       256u
#define FAKE_WRITE_GRANULARITY 16u

typedef struct {
  uint8_t bytes[FAKE_REGION_SIZE];
  bool    fail_erase;
  bool    fail_write;
  bool    fail_read;
  size_t  last_write_len; /* what write() was actually called with, for the padding test below */
} fake_flash_t;

static void fake_flash_init(fake_flash_t *f) {
  /* 0xFF is what a real NOR/NAND erase leaves behind - a fresh, never-
     written region must look the same to load() as a genuinely erased one,
     not a lucky all-zero buffer a memset(0) would give for free. */
  memset(f->bytes, 0xFF, sizeof(f->bytes));
  f->fail_erase      = false;
  f->fail_write      = false;
  f->fail_read       = false;
  f->last_write_len  = 0u;
}

static bool fake_erase(void *ctx) {
  fake_flash_t *f = (fake_flash_t *)ctx;
  if (f->fail_erase) {
    return false;
  }
  memset(f->bytes, 0xFF, sizeof(f->bytes));
  return true;
}

static bool fake_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len) {
  fake_flash_t *f = (fake_flash_t *)ctx;
  if (f->fail_write || (size_t)offset + len > sizeof(f->bytes)) {
    return false;
  }
  memcpy(&f->bytes[offset], data, len);
  f->last_write_len = len;
  return true;
}

static bool fake_read(void *ctx, uint32_t offset, uint8_t *out, size_t len) {
  fake_flash_t *f = (fake_flash_t *)ctx;
  if (f->fail_read || (size_t)offset + len > sizeof(f->bytes)) {
    return false;
  }
  memcpy(out, &f->bytes[offset], len);
  return true;
}

static boomlink_storage_port_t make_port(fake_flash_t *f) {
  boomlink_storage_port_t port = {0};
  port.erase                    = fake_erase;
  port.write                    = fake_write;
  port.read                     = fake_read;
  port.region_size              = FAKE_REGION_SIZE;
  port.write_granularity        = FAKE_WRITE_GRANULARITY;
  port.ctx                      = f;
  return port;
}

static boomlink_NodeConfig sample_config(void) {
  boomlink_NodeConfig cfg   = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  cfg.config_version        = 7u;
  cfg.has_general           = true;
  cfg.general.node_id       = 0x11223344u;
  cfg.general.receive_enabled = true;
  cfg.has_telemetry         = true;
  cfg.telemetry.report_interval_s = 30u;
  return cfg;
}

/* --- boomlink_storage_port_t validity (the generic, format-agnostic half) -- */

static void test_is_valid_rejects_every_missing_piece(void) {
  fake_flash_t             f;
  boomlink_storage_port_t good;
  fake_flash_init(&f);
  good = make_port(&f);

  REQUIRE(boomlink_storage_port_is_valid(&good), "the fake port should be valid to begin with");
  CHECK(!boomlink_storage_port_is_valid(NULL), "a NULL port must be rejected");

  boomlink_storage_port_t bad = good;
  bad.erase                    = NULL;
  CHECK(!boomlink_storage_port_is_valid(&bad), "a NULL erase callback must be rejected");

  bad          = good;
  bad.write    = NULL;
  CHECK(!boomlink_storage_port_is_valid(&bad), "a NULL write callback must be rejected");

  bad         = good;
  bad.read    = NULL;
  CHECK(!boomlink_storage_port_is_valid(&bad), "a NULL read callback must be rejected");

  bad                    = good;
  bad.write_granularity  = 0u;
  CHECK(!boomlink_storage_port_is_valid(&bad), "a zero write_granularity must be rejected");

  bad                    = good;
  bad.region_size        = 0u;
  CHECK(!boomlink_storage_port_is_valid(&bad), "a zero region_size must be rejected");

  bad                    = good;
  bad.write_granularity  = 3u; /* 256 is not a multiple of 3 */
  CHECK(!boomlink_storage_port_is_valid(&bad),
        "a write_granularity that does not evenly divide region_size must be rejected");
}

/* --- the load()/save() wrapper format --------------------------------------- */

static void test_a_fresh_erased_region_fails_to_load(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  bool                 ok  = boomlink_config_store_load(&port, &out);

  CHECK(!ok, "an all-0xFF erased region has no valid magic and must not load");
}

static void test_save_then_load_round_trips_exactly(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();

  REQUIRE(boomlink_config_store_save(&port, &in), "save of a valid, small config must succeed");

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  bool                 ok  = boomlink_config_store_load(&port, &out);

  REQUIRE(ok, "load right after a successful save must succeed");
  CHECK(out.config_version == 7u, "config_version must round-trip, got %u", (unsigned)out.config_version);
  CHECK(out.has_general && out.general.node_id == 0x11223344u,
        "GeneralConfig.node_id must round-trip, got 0x%08X", (unsigned)out.general.node_id);
  CHECK(out.general.receive_enabled, "GeneralConfig.receive_enabled must round-trip");
  CHECK(out.has_telemetry && out.telemetry.report_interval_s == 30u,
        "TelemetryConfig.report_interval_s must round-trip");
}

static void test_corrupt_magic_fails_closed(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  f.bytes[0] ^= 0xFFu; /* flip the first byte of the magic */

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out), "a corrupted magic must fail to load");
  CHECK(out.config_version == 0u, "*out must be left untouched on a failed load, not partially filled");
}

static void test_wrong_format_version_fails_closed(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  /* storage_format_version is header bytes [4:8), little-endian. */
  f.bytes[4] = (uint8_t)(BOOMLINK_CONFIG_STORE_FORMAT_VERSION + 1u);

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out),
        "a storage_format_version this file does not recognise must fail to load, not guess");
}

static void test_corrupt_crc_fails_closed(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  /* Corrupt the stored CRC itself (header bytes [12:16)), leaving the blob
     bytes untouched - a flipped blob byte can ALSO make pb_decode() itself
     fail, which would make this test pass for the wrong reason even with
     the CRC check disabled entirely. Corrupting the CRC field instead means
     the blob still decodes fine, so only the CRC comparison can be what
     rejects this. */
  f.bytes[BOOMLINK_CONFIG_STORE_HEADER_SIZE - 1u] ^= 0x01u;

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out),
        "a blob that no longer matches its stored CRC must fail to load");
}

static void test_oversized_protobuf_length_fails_closed_without_oob_read(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  /* protobuf_length is header bytes [8:12), little-endian - claim a length
     larger than boomlink_NodeConfig_size, but still well within the fake's
     own 256-byte physical backing array (so the fake's OWN offset+len bound
     in fake_read() cannot be what rejects this instead of the real check
     under test). AddressSanitizer (this package's whole host build, see
     boomlink_no_nanopb_dependency's own comment on the build's sanitizer
     flags) turns the resulting stack-buffer-overflow read of `blob`
     (declared `uint8_t blob[boomlink_NodeConfig_size]`, 145 bytes) into an
     immediate crash of this test binary if load() ever used this claimed
     length to size that read instead of rejecting it outright first. */
  uint32_t huge = (uint32_t)boomlink_NodeConfig_size + 55u; /* > 145, 16+200=216 fits the fake's 256 */
  put_u32_le(&f.bytes[8], huge);

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out),
        "protobuf_length larger than boomlink_NodeConfig_size must be rejected before it is used "
        "to size any read");
}

static void test_protobuf_length_past_the_region_fails_closed(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  /* The header, blob and CRC written by the save above are all perfectly
     self-consistent - only the REGION this load() is told it has to work
     with shrinks, to one byte less than the declared protobuf_length
     actually needs once the header is added. Deliberately not corrupting
     protobuf_length or the CRC here (test_oversized_protobuf_length_...
     and test_corrupt_crc_fails_closed already cover those): a corrupted
     length usually also fails the CRC check for an unrelated reason (the
     bytes it would checksum no longer match what was written), which would
     let this test pass even with the region bound itself disabled - the
     same masking test_corrupt_crc_fails_closed's own doc comment warns
     about for pb_decode(). Shrinking region_size instead leaves every other
     check satisfied, so only the region-capacity comparison can be what
     rejects this. write_granularity dropped to 1 so the shrunk region_size
     still passes boomlink_storage_port_is_valid()'s evenly-divides check;
     the fake's own physical backing array is untouched, so this only
     changes what load() itself is willing to trust. */
  uint32_t true_blob_len  = get_u32_le(&f.bytes[8]);
  port.region_size        = BOOMLINK_CONFIG_STORE_HEADER_SIZE + true_blob_len - 1u;
  port.write_granularity  = 1u;

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out),
        "protobuf_length that would read past the port's own region must be rejected");
}

static void test_region_too_small_for_even_the_header_fails_closed(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      in  = sample_config();
  /* A REAL, fully valid save first - correct magic, format_version, CRC,
     and an honest protobuf_length - not a freshly-erased or corrupted
     region. A freshly-erased region would fail the magic check regardless
     of whether the region-size guard under test exists at all, which would
     let this test pass for the wrong reason (exactly the masking
     test_corrupt_crc_fails_closed's own doc comment warns about). Only
     the REGION this second port claims to have shrinks; the underlying
     bytes this ctx points at (and everything the header/CRC say about
     them) are left completely genuine. */
  REQUIRE(boomlink_config_store_save(&port, &in), "setup: save must succeed");

  /* write_granularity dropped to 1 so this small region_size still passes
     boomlink_storage_port_is_valid()'s generic evenly-divides check - this
     test is specifically about boomlink_config_store_load()'s OWN
     format-specific minimum. Without it, `port->region_size -
     BOOMLINK_CONFIG_STORE_HEADER_SIZE` (both unsigned) underflows to a huge
     value, which would make the length bound it guards accept ANY
     protobuf_length instead of rejecting one - the real header/blob being
     genuinely valid here means that underflow would let this load()
     actually SUCCEED, not merely fail for an unrelated reason. */
  port.region_size        = BOOMLINK_CONFIG_STORE_HEADER_SIZE - 1u; /* one byte too small */
  port.write_granularity  = 1u;

  boomlink_NodeConfig out = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  CHECK(!boomlink_config_store_load(&port, &out),
        "a region too small to even hold the header must fail to load, not underflow the bounds "
        "check into accepting anything");
}

static void test_save_rejects_a_config_too_large_for_the_region(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  port.region_size              = BOOMLINK_CONFIG_STORE_HEADER_SIZE; /* room for a header, nothing else */
  boomlink_NodeConfig cfg       = sample_config();

  CHECK(!boomlink_config_store_save(&port, &cfg),
        "a config that cannot fit in the port's region must fail save(), not overrun it");
}

static void test_null_arguments_are_rejected(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      cfg = sample_config();
  boomlink_NodeConfig      out;

  CHECK(!boomlink_config_store_load(NULL, &out), "a NULL port must be rejected by load()");
  CHECK(!boomlink_config_store_load(&port, NULL), "a NULL out must be rejected by load()");
  CHECK(!boomlink_config_store_save(NULL, &cfg), "a NULL port must be rejected by save()");
  CHECK(!boomlink_config_store_save(&port, NULL), "a NULL config must be rejected by save()");
}

static void test_save_pads_the_write_to_a_full_granule(void) {
  fake_flash_t             f;
  fake_flash_init(&f);
  boomlink_storage_port_t port = make_port(&f);
  boomlink_NodeConfig      cfg = sample_config();
  /* One more populated group than sample_config() alone, specifically so
     header+blob is NOT already an exact write_granularity multiple - with
     sample_config() alone it happens to encode to exactly 16 bytes, making
     header(16)+blob(16)=32 already a 16-byte multiple, which would let a
     save() that rounded DOWN instead of up pass this test by accident (a
     rounded-down and a rounded-up padded_len coincide whenever there is
     nothing to round in the first place). Checked below with a REQUIRE
     rather than trusted by hand, so a future proto change that happened to
     land back on an exact multiple would fail loudly instead of silently
     stopping to exercise the rounding this test exists for. */
  cfg.has_gnss           = true;
  cfg.gnss.gnss_enabled  = true;

  REQUIRE(boomlink_config_store_save(&port, &cfg), "setup: save must succeed");

  uint32_t protobuf_length = get_u32_le(&f.bytes[8]);
  size_t   total_len       = (size_t)BOOMLINK_CONFIG_STORE_HEADER_SIZE + protobuf_length;
  REQUIRE(total_len % FAKE_WRITE_GRANULARITY != 0u,
          "setup: this test needs a header+blob length that actually requires rounding, got "
          "%zu bytes (already a %u-byte multiple)",
          total_len, (unsigned)FAKE_WRITE_GRANULARITY);

  /* boomlink_storage_port_t's own contract (boomlink_storage_port.h) is that
     write() is only ever called with a write_granularity multiple - checked
     directly against what the fake actually recorded, rather than guessing
     the exact encoded blob size and inspecting padding bytes by hand. */
  CHECK(f.last_write_len % FAKE_WRITE_GRANULARITY == 0u,
        "write() must be called with a write_granularity multiple, got %zu", f.last_write_len);

  /* And the written length must actually cover the real header+blob, not
     round DOWN past it - the case this test is specifically for. */
  CHECK(f.last_write_len >= total_len,
        "the padded write (%zu bytes) must cover the real header+blob (%zu bytes)",
        f.last_write_len, total_len);
}

int main(void) {
  test_is_valid_rejects_every_missing_piece();
  test_a_fresh_erased_region_fails_to_load();
  test_save_then_load_round_trips_exactly();
  test_corrupt_magic_fails_closed();
  test_wrong_format_version_fails_closed();
  test_corrupt_crc_fails_closed();
  test_oversized_protobuf_length_fails_closed_without_oob_read();
  test_protobuf_length_past_the_region_fails_closed();
  test_region_too_small_for_even_the_header_fails_closed();
  test_save_rejects_a_config_too_large_for_the_region();
  test_null_arguments_are_rejected();
  test_save_pads_the_write_to_a_full_granule();
  BOOMLINK_TEST_REPORT("config_store_test", 30);
}
