/**
 ******************************************************************************
 * @file    boomlink_config_store.c
 ******************************************************************************
 */
#include "boomlink_config_store.h"

#include <pb_decode.h>
#include <pb_encode.h>

#include "boomlink_crc32.h"

/* The header is serialized field-by-field as explicit little-endian bytes,
   not overlaid as a struct - see boomlink_config_store.h's own doc on why:
   this format has to survive being read back by a possibly-different
   compiler/build across a firmware update, and struct layout (padding,
   endianness, `int` width) is not part of that contract the way "four
   uint32_t fields, low byte first" is. */
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

bool boomlink_config_store_load(const boomlink_storage_port_t *port, boomlink_NodeConfig *out) {
  /* boomlink_storage_port_is_valid() only checks what is generic to any
     port (see its own doc) - a region too small to even hold this format's
     header is this file's own format-specific minimum to enforce, and must
     be checked before the subtraction below, which would otherwise
     underflow `region_size` into a huge value and defeat the very bounds
     check it guards. */
  if (!boomlink_storage_port_is_valid(port) || out == NULL ||
      port->region_size < BOOMLINK_CONFIG_STORE_HEADER_SIZE) {
    return false;
  }

  uint8_t header[BOOMLINK_CONFIG_STORE_HEADER_SIZE];
  if (!port->read(port->ctx, 0, header, sizeof(header))) {
    return false;
  }

  uint32_t magic           = get_u32_le(&header[0]);
  uint32_t format_version  = get_u32_le(&header[4]);
  uint32_t protobuf_length = get_u32_le(&header[8]);
  uint32_t stored_crc      = get_u32_le(&header[12]);

  if (magic != BOOMLINK_CONFIG_STORE_MAGIC || format_version != BOOMLINK_CONFIG_STORE_FORMAT_VERSION) {
    return false;
  }
  /* Bounds-checked before `protobuf_length` is ever used to size a read - a
     corrupt header claiming an oversized length must fail closed here, not
     become an over-large read into a fixed buffer or past the region. */
  if (protobuf_length > boomlink_NodeConfig_size ||
      protobuf_length > port->region_size - BOOMLINK_CONFIG_STORE_HEADER_SIZE) {
    return false;
  }

  uint8_t blob[boomlink_NodeConfig_size];
  if (!port->read(port->ctx, BOOMLINK_CONFIG_STORE_HEADER_SIZE, blob, protobuf_length)) {
    return false;
  }
  if (boomlink_crc32(blob, protobuf_length) != stored_crc) {
    return false;
  }

  /* Decoded into a local, copied to *out only once fully successful - unlike
     boomlink_decode_envelope()'s "zero *out first, re-zero on failure"
     idiom, this never writes anything into *out at all on any failure path,
     which is simpler to reason about at the one extra cost of a stack copy
     no bigger than boomlink_NodeConfig_size itself. */
  boomlink_NodeConfig decoded = (boomlink_NodeConfig)boomlink_NodeConfig_init_zero;
  pb_istream_t stream            = pb_istream_from_buffer(blob, protobuf_length);
  if (!pb_decode(&stream, boomlink_NodeConfig_fields, &decoded)) {
    return false;
  }

  *out = decoded;
  return true;
}

bool boomlink_config_store_save(const boomlink_storage_port_t *port,
                                const boomlink_NodeConfig *config) {
  /* boomlink_storage_port_is_valid() only checks what is generic to any
     port (see its own doc) - BOOMLINK_CONFIG_STORE_MAX_WRITE_GRANULARITY is
     this wrapper format's own limit, so this file enforces it itself,
     BEFORE the padding arithmetic below that it protects: a
     write_granularity anywhere near SIZE_MAX makes `total_len +
     write_granularity - 1u` overflow and wrap to a small value, which
     would otherwise sail straight past the `sizeof(buf)` guard that
     follows instead of being caught by it. */
  if (!boomlink_storage_port_is_valid(port) || config == NULL ||
      port->write_granularity > BOOMLINK_CONFIG_STORE_MAX_WRITE_GRANULARITY) {
    return false;
  }

  /* Sized for the worst case this file supports: the header, the largest a
     NodeConfig can ever encode to, and up to one full write_granularity of
     padding (BOOMLINK_CONFIG_STORE_MAX_WRITE_GRANULARITY bounds that, and
     the check above already rejected any port claiming more) - no heap, so
     this has to be big enough up front rather than sized from the real
     encoded length after the fact. */
  uint8_t buf[BOOMLINK_CONFIG_STORE_HEADER_SIZE + boomlink_NodeConfig_size +
             BOOMLINK_CONFIG_STORE_MAX_WRITE_GRANULARITY] = {0};

  pb_ostream_t stream = pb_ostream_from_buffer(&buf[BOOMLINK_CONFIG_STORE_HEADER_SIZE],
                                               boomlink_NodeConfig_size);
  if (!pb_encode(&stream, boomlink_NodeConfig_fields, config)) {
    /* Should not happen - boomlink_NodeConfig_size is Nanopb's own ceiling
       for this message - but a config service bug that hands this a value
       Nanopb itself cannot bound is a save failure, not a buffer overrun. */
    return false;
  }
  uint32_t blob_len = (uint32_t)stream.bytes_written;

  put_u32_le(&buf[0], BOOMLINK_CONFIG_STORE_MAGIC);
  put_u32_le(&buf[4], BOOMLINK_CONFIG_STORE_FORMAT_VERSION);
  put_u32_le(&buf[8], blob_len);
  put_u32_le(&buf[12], boomlink_crc32(&buf[BOOMLINK_CONFIG_STORE_HEADER_SIZE], blob_len));

  /* write() may only ever be called with a write_granularity multiple (the
     port's own contract, boomlink_storage_port.h) - round up, not down, so
     every byte actually written (including the header) lands inside the
     write, never left over for a write() call this function never makes.
     No separate "does total_len alone fit region_size" check before this:
     rounding up can only grow a length, never shrink it, so padded_len is
     always >= total_len and the single check below already covers both -
     a second one testing the unrounded length first could never reject
     anything this one would not already catch. */
  size_t total_len  = (size_t)BOOMLINK_CONFIG_STORE_HEADER_SIZE + blob_len;
  size_t padded_len = ((total_len + port->write_granularity - 1u) / port->write_granularity) *
                       port->write_granularity;
  if (padded_len > port->region_size || padded_len > sizeof(buf)) {
    return false;
  }

  if (!port->erase(port->ctx)) {
    return false;
  }
  return port->write(port->ctx, 0, buf, padded_len);
}
