
#include <stdint.h>

typedef struct {
  int sampling_freq; // [Hz]
  int pre_event_ms;  // [ms]
  int post_event_ms; // [ms]
} mic_config;

void mic_init(const mic_config *mic_cnfg);
void mic_start_reading(void);
void mic_save_event(int *out_left_mic, int *out_right_mic);
