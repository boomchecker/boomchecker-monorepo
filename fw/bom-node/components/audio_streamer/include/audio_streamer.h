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
