#include "audio_wav.h"

#include <string.h>

static void write_le16(uint8_t *dst, uint16_t val) {
    dst[0] = (uint8_t)(val & 0xff);
    dst[1] = (uint8_t)((val >> 8) & 0xff);
}

static void write_le32(uint8_t *dst, uint32_t val) {
    dst[0] = (uint8_t)(val & 0xff);
    dst[1] = (uint8_t)((val >> 8) & 0xff);
    dst[2] = (uint8_t)((val >> 16) & 0xff);
    dst[3] = (uint8_t)((val >> 24) & 0xff);
}

void audio_wav_build_header(uint8_t *out, int sample_rate) {
    const uint16_t num_channels = 2;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const uint16_t block_align = num_channels * bits_per_sample / 8;
    const uint32_t data_size = 0xffffffff;
    const uint32_t riff_size = data_size + 36;

    memcpy(out, "RIFF", 4);
    write_le32(out + 4, riff_size);
    memcpy(out + 8, "WAVE", 4);
    memcpy(out + 12, "fmt ", 4);
    write_le32(out + 16, 16);
    write_le16(out + 20, 1);
    write_le16(out + 22, num_channels);
    write_le32(out + 24, (uint32_t)sample_rate);
    write_le32(out + 28, byte_rate);
    write_le16(out + 32, block_align);
    write_le16(out + 34, bits_per_sample);
    memcpy(out + 36, "data", 4);
    write_le32(out + 40, data_size);
}
