### 1. test_uart.py
    - debug súbor, kde sa skúša UART komunikácie s ESP32
    - testovanie komunikácie UART

### 2. model_eval.py
    - testovanie jediného súboru, na overenie funkčnosti main.cpp

### 3. mfcc_add_nosie_eval.py
    - testovanie na esp32 keď vkladáme šum do MFCC
    (ešte nemáme zašumené .wau)

### 4. audio_add_noise_eval.py
    - testovanie na esp32 keď nevkladáme žiaden šum 
    (už máme zašumené .wau)

### 5. main.cpp
    - UART konfigurácia, kvantizácia, TFLite evaluácia
