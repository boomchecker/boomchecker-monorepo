/**
 * @file audio_sai_handler.c
 * @brief Implementation of PDM downsampling and real-time processing pipeline.
 */

#include "audio_sai_handler.h"
#include "mfcc_processor.h"
#include "svm_classifier.h"
#include "svm_model_data.h"
#include <string.h>

// FIR low-pass filter coefficients for 48 kHz -> 16 kHz downsampling (decimate by 3)
// Designed as a 15-tap low-pass filter with cutoff at ~7.5 kHz
#define FILTER_TAPS 15
static const float32_t fir_coeffs[FILTER_TAPS] = {
    -0.0034f, -0.0076f, 0.0000f, 0.0354f, 0.0910f, 0.1478f, 0.1874f, 0.1874f,
     0.1478f,  0.0910f, 0.0354f, 0.0000f, -0.0076f, -0.0034f, 0.0000f
};

// CMSIS-DSP FIR Decimate instance and state buffer
static arm_fir_decimate_instance_f32 dec_inst;
static float32_t dec_state[FILTER_TAPS + DMA_HALF_BUF_SIZE - 1];

// Circular buffer for 16 kHz float32 audio samples
// Large enough to hold several frames (each frame is 1024, hop is 512)
#define CIRCULAR_BUF_SIZE 4096
static float32_t circular_audio_buffer[CIRCULAR_BUF_SIZE];
static uint32_t write_idx = 0;
static uint32_t read_idx = 0;
static uint32_t available_samples = 0;

// MFCC frame aggregation buffer
// We accumulate MFCC frames to run SVM classification on them
#define MAX_ACCUMULATED_FRAMES 14
static float32_t accumulated_mfccs[MAX_ACCUMULATED_FRAMES * NUM_MFCC_COEFFS];
static int accumulated_frame_count = 0;

// Squelch RMS threshold matching optimized tests
#define RMS_SQUELCH_THRESHOLD 0.010f

arm_status audio_sai_init(void) {
    // Initialize MFCC processor
    if (mfcc_init() != ARM_MATH_SUCCESS) {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    
    // Initialize SVM classifier
    svm_classifier_init();
    
    // Initialize CMSIS-DSP decimation filter (decimate by 3)
    return arm_fir_decimate_init_f32(
        &dec_inst,
        FILTER_TAPS,
        DECIMATION_FACTOR,
        (float32_t *)fir_coeffs,
        dec_state,
        DMA_HALF_BUF_SIZE
    );
}

void audio_sai_process_half_callback(const int16_t *p_dma_buf) {
    // 1. Convert 16-bit integer PCM samples to float32
    static float32_t input_float[DMA_HALF_BUF_SIZE];
    for (int i = 0; i < DMA_HALF_BUF_SIZE; i++) {
        input_float[i] = (float32_t)p_dma_buf[i] / 32768.0f;
    }
    
    // 2. Downsample (Decimate by 3): 1536 input samples -> 512 decimated samples
    static float32_t decimated_float[DMA_HALF_BUF_SIZE / DECIMATION_FACTOR];
    arm_fir_decimate_f32(
        &dec_inst,
        input_float,
        decimated_float,
        DMA_HALF_BUF_SIZE
    );
    
    // 3. Write 512 decimated samples into the circular buffer
    uint32_t dec_len = DMA_HALF_BUF_SIZE / DECIMATION_FACTOR; // 512
    for (uint32_t i = 0; i < dec_len; i++) {
        circular_audio_buffer[write_idx] = decimated_float[i];
        write_idx = (write_idx + 1) % CIRCULAR_BUF_SIZE;
    }
    
    available_samples += dec_len;
    if (available_samples > CIRCULAR_BUF_SIZE) {
        // Overflow safety: force read pointer forward if buffer is full
        read_idx = write_idx;
        available_samples = CIRCULAR_BUF_SIZE;
    }
}

static void local_aggregate_features(const float32_t *p_mfcc_frames, int num_frames, float32_t *p_out) {
    // Feature vector consists of: [mean0..12, std0..12]
    for (int c = 0; c < NUM_MFCC_COEFFS; c++) {
        float32_t sum = 0.0f;
        for (int f = 0; f < num_frames; f++) {
            sum += p_mfcc_frames[f * NUM_MFCC_COEFFS + c];
        }
        float32_t mean = sum / (float32_t)num_frames;
        p_out[c] = mean;

        float32_t sum_sq_diff = 0.0f;
        for (int f = 0; f < num_frames; f++) {
            float32_t diff = p_mfcc_frames[f * NUM_MFCC_COEFFS + c] - mean;
            sum_sq_diff += diff * diff;
        }
        p_out[c + NUM_MFCC_COEFFS] = sqrtf(sum_sq_diff / (float32_t)num_frames);
    }
}

void audio_sai_pipeline_poll(void) {
    // We need at least 1024 samples (WINDOW_SIZE) to process a frame
    while (available_samples >= PROCESSING_BUF_SIZE) {
        
        // 1. Copy contiguous block of 1024 samples from circular buffer
        static float32_t frame_buffer[PROCESSING_BUF_SIZE];
        uint32_t temp_read_idx = read_idx;
        for (uint32_t i = 0; i < PROCESSING_BUF_SIZE; i++) {
            frame_buffer[i] = circular_audio_buffer[temp_read_idx];
            temp_read_idx = (temp_read_idx + 1) % CIRCULAR_BUF_SIZE;
        }
        
        // 2. Squelch Gate check: compute RMS energy of the frame
        float32_t frame_rms = 0.0f;
        arm_rms_f32(frame_buffer, PROCESSING_BUF_SIZE, &frame_rms);
        
        if (frame_rms < RMS_SQUELCH_THRESHOLD) {
            // Squelch triggered: too quiet. Fast-forward and skip processing
            accumulated_frame_count = 0; // Reset active frames
            
            // Advance circular buffer read pointer by hop size (512)
            read_idx = (read_idx + 512) % CIRCULAR_BUF_SIZE;
            available_samples -= 512;
            continue;
        }
        
        // 3. Process MFCC for the frame
        // Save output directly in the accumulated buffer
        float32_t *p_target_mfcc = &accumulated_mfccs[accumulated_frame_count * NUM_MFCC_COEFFS];
        mfcc_process(frame_buffer, p_target_mfcc);
        accumulated_frame_count++;
        
        // 4. Once we have accumulated enough frames, run SVM inference
        if (accumulated_frame_count >= MAX_ACCUMULATED_FRAMES) {
            static float32_t feature_vector[SVM_NUM_FEATURES]; // 26 elements
            
            // Aggregate mean and standard deviation
            local_aggregate_features(accumulated_mfccs, MAX_ACCUMULATED_FRAMES, feature_vector);
            
            // Get prediction
            int prediction = svm_predict(feature_vector);
            
            if (prediction == 1) {
                // DRONE DETECTED!
                // In firmware: HAL_GPIO_WritePin(GPIO_PORT, GPIO_PIN, GPIO_PIN_SET) to light an LED,
                // or send a message over UART.
            } else {
                // NOISE
                // HAL_GPIO_WritePin(GPIO_PORT, GPIO_PIN, GPIO_PIN_RESET);
            }
            
            // Slide window by resetting frame counter (or slide buffer)
            accumulated_frame_count = 0;
        }
        
        // Advance read pointer by hop size (512) for 50% overlap STFT
        read_idx = (read_idx + 512) % CIRCULAR_BUF_SIZE;
        available_samples -= 512;
    }
}
