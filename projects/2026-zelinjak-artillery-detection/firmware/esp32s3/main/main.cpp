#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model_data.h"

#define UART_PORT_NUM      UART_NUM_0      
#define UART_BAUD_RATE     115200          

#define BUF_SIZE           4096       
#define EXPECTED_BYTES     2784

// --- TFLite Globálne premenné ---
const tflite::Model* model            = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input_tensor            = nullptr;
TfLiteTensor* output_tensor           = nullptr;

constexpr int kTensorArenaSize = 80 * 1024;

uint8_t tensor_arena[kTensorArenaSize];
uint8_t rx_buffer[EXPECTED_BYTES];


// ==========================================
// 1. Funkcia pre inicializáciu UART
// ==========================================
void init_uart() {
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT // Vynúti prispôsobenie hodinám procesora
    };

    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    ESP_LOGI("UART", "UART úspešne inicializovaný na porte %d s rýchlosťou %d", UART_PORT_NUM, UART_BAUD_RATE);
}


// ==========================================
// 2. Funkcia pre inicializáciu TFLite
// ==========================================
bool init_tflite() {
    ESP_LOGI("TFLITE", "Načítavam model...");
    
    // Názov gunshot_model_int8_tflite musí sedieť s názvom poľa v model_data.h
    model = tflite::GetModel(gunshot_model_int8_tflite); 
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE("TFLITE", "Chybná verzia modelu!");
        return false;
    }

    static tflite::MicroMutableOpResolver<10> resolver; 
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddReshape();
    resolver.AddLogistic();
    resolver.AddSoftmax();
    resolver.AddShape();
    resolver.AddStridedSlice();
    resolver.AddPack();

    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Alokácia pamäte pre tenzory
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE("TFLITE", "Alokácia tenzorov zlyhala!");
        return false;
    }

    // Priradenie pointerov na vstupný a výstupný tenzor
    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);

    ESP_LOGI("TFLITE", "Model úspešne inicializovaný.");
    return true;
}


// ==========================================
// 3. Funkcia pre kvantizáciu dát (Float32 -> Int8)
// ==========================================
void quantize_input(float* float_data, int num_elements) {
    // Získame parametre kvantizácie z modelu
    TfLiteQuantizationParams quant_params = input_tensor->params;

    for (int i = 0; i < num_elements; i++) {
        // Prepočet z desatinného čísla na celé číslo v rozsahu int8
        float val     = float_data[i];
        int32_t q_val = round(val / quant_params.scale) + quant_params.zero_point;

        // Orezanie, aby sme nepretiekli mimo rozsah bajtu
        if (q_val >  127) q_val =  127;
        if (q_val < -128) q_val = -128;

        // Zápis priamo do vstupného tenzora ako int8
        input_tensor->data.int8[i] = (int8_t)q_val;
    }
}



extern "C" void app_main(void) {

    // --- 1. Inicializácia UART ---
    init_uart();

    // --- 2. Inicializácia TensorFlow Lite Micro ---
    if (!init_tflite()) { return; }

    // --- 3. Hlavná slučka: Prijímanie dát a Inferencia ---
    int total_received = 0;

    while (1) {
        
        int len = uart_read_bytes(UART_PORT_NUM, rx_buffer + total_received, EXPECTED_BYTES - total_received, 20 / portTICK_PERIOD_MS);

        if (len > 0) {
            total_received += len;

            // Ak sme nazbierali celú maticu...
            if (total_received == EXPECTED_BYTES) {

                // === A) Kvantizácia Float32 -> Int8 ===
                float* float_data = (float*)rx_buffer;
                int num_elements  = EXPECTED_BYTES / sizeof(float); // Malo by byť 696 prvkov

                quantize_input(float_data, num_elements);

                // === B) Inferencia ===
                int64_t start_time         = esp_timer_get_time();
                TfLiteStatus invoke_status = interpreter->Invoke();
                int64_t end_time           = esp_timer_get_time();

                if (invoke_status != kTfLiteOk) {
                    const char* err_msg = "Chyba inferencie!\n";
                    uart_write_bytes(UART_PORT_NUM, err_msg, strlen(err_msg));

                } else {
                    // === C) Dekvantizácia Int8 -> Float32 ===
                    TfLiteQuantizationParams out_quant_params = output_tensor->params;
                    int8_t out_val    = output_tensor->data.int8[0];
                     
                    // Prepočet späť na percentuálnu pravdepodobnosť (0.0 až 1.0)
                    float prediction  = (out_val - out_quant_params.zero_point) * out_quant_params.scale;
                    int infer_time_ms = (end_time - start_time) / 1000;

                    // === D) Odoslanie výsledkov ===
                    char response[100];
                    snprintf(response, sizeof(response), "PREDIKCIA: %.4f (Cas: %d ms)\n", prediction, infer_time_ms);
                    uart_write_bytes (UART_PORT_NUM, response, strlen(response));
                    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
                }

                // Vynulujeme počítadlo pre ďalšiu maticu
                total_received = 0;
            }
        }

        //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}


