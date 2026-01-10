#pragma once

#include "audio_config.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void audio_streamer_init(void);
void audio_streamer_apply_config(const audio_config_t *config);
bool audio_streamer_pull_enabled(void);
bool audio_streamer_pull_claim(void);
void audio_streamer_pull_release(void);
size_t audio_streamer_pull_read(uint8_t *buf, size_t len, TickType_t timeout);
int audio_streamer_sample_rate(void);

typedef struct {
  uint32_t tap_calls;
  uint32_t stream_writes;
  uint32_t send_failed;
  uint32_t read_calls;
  uint32_t read_bytes;
  bool pull_enabled;
} audio_streamer_stats_t;

void audio_streamer_get_stats(audio_streamer_stats_t *stats);
