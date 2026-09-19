#include "audio_capture.h"

#include "audio_config.h"
#include "mic_input.h"

void audio_capture_init(void) {
    audio_config_t audio_cfg = audio_config_get();
    mic_config mic_cfg = {
        .sampling_freq = audio_cfg.sampling_rate > 0 ? audio_cfg.sampling_rate
                                                     : MIC_SAMPLING_FREQUENCY,
        .pre_event_ms = MIC_PRE_EVENT_MS,
        .post_event_ms = MIC_POST_EVENT_MS,
        .num_taps = MIC_DEFAULT_NUM_TAPS,
        .tap_size = MIC_DEFAULT_TAP_SIZE,
    };
    mic_init(&mic_cfg);
}

void audio_capture_start(void) {
    mic_start();
}
