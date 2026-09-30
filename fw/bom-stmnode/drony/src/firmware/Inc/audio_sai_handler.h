/**
 * @file audio_sai_handler.h
 * @brief SAI PDM microphone downsampling and pipeline integration interface.
 */

#ifndef AUDIO_SAI_HANDLER_H
#define AUDIO_SAI_HANDLER_H

#include "arm_math.h"

// Define audio parameters
#define SAI_AUDIO_FREQ_48K   48000
#define PIPELINE_FREQ_16K    16000
#define DECIMATION_FACTOR    3

// DMA buffer sizes
// For 48 kHz stereo (or mono) we capture in blocks.
// Let's assume SAI receives mono 16-bit PDM decoded data.
// We configure DMA in circular mode with a buffer size of 3072 samples (1536 per half-buffer).
// 1536 samples at 48 kHz decimate by 3 to exactly 512 samples at 16 kHz (which is our hop size!).
#define DMA_HALF_BUF_SIZE    1536
#define DMA_FULL_BUF_SIZE    (DMA_HALF_BUF_SIZE * 2)

// Queue size for 16 kHz audio frames
// The pipeline needs 1024 samples (WINDOW_SIZE) to process a frame.
// Each half-buffer processing yields 512 samples.
// We store them in a ping-pong processing buffer of 1024 samples.
#define PROCESSING_BUF_SIZE  1024

/**
 * @brief Initializes the SAI audio processing structures (FIR decimation, state buffers).
 * @return arm_status CMSIS-DSP initialization status.
 */
arm_status audio_sai_init(void);

/**
 * @brief Callback function to be called from the SAI DMA half-transfer callback.
 * @param p_dma_buf Pointer to the half of the DMA buffer that was filled.
 */
void audio_sai_process_half_callback(const int16_t *p_dma_buf);

/**
 * @brief Periodically polled in the main loop to process accumulated audio.
 * @details Extracts MFCCs and runs SVM prediction when 1024 samples are ready.
 */
void audio_sai_pipeline_poll(void);

#endif /* AUDIO_SAI_HANDLER_H */
