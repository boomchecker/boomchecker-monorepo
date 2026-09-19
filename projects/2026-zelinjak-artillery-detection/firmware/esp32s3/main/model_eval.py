import serial
import time
import numpy as np
import threading
import struct

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

# TU ZADAJ CESTU K TVOJMU TESTOVACIEMU .npy SÚBORU
TEST_FILE = '241008_1_uid-7r8oyd_mfcc.npy' 

def read_from_esp(ser):
    """Nekonečná slučka bežiaca na pozadí, ktorá počúva ESP32."""
    while True:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Ak to je naša predikcia, pekne ju zvýrazníme
                    if "PREDIKCIA" in line:
                        print("\n" + "="*50)
                        print(f" VÝSLEDOK Z ESP32: {line}")
                        print("="*50 + "\n")
                    else:
                        print(line) # Vypíše boot logy aj prípadné errory
        except Exception as e:
            # Ak sa port zatvorí, vlákno potichu skončí
            break
        time.sleep(0.01)

def main():
    print(f"Pripájam sa na port {SERIAL_PORT} s rýchlosťou {BAUD_RATE}...")
   
    try:
        # 1. Otvorenie portu (toto spôsobí reštart ESP32)
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(3)

        print("Pripojenie úspešné!")
                
        # 2. Príprava dát
        mfcc_data = np.load(TEST_FILE)
        mfcc_flat = mfcc_data.flatten()

        if len(mfcc_flat) != 696:
            print(f"CHYBA: Zlý rozmer matice. Súbor má {len(mfcc_flat)} hodnôt.")
            return

        byte_data = struct.pack(f'<{len(mfcc_flat)}f', *mfcc_flat)
        print(f"Dáta pripravené. Veľkosť: {len(byte_data)} bajtov.")

        # 3. Spustíme vlákno na počúvanie
        read_thread = threading.Thread(target=read_from_esp, args=(ser,), daemon=True)
        read_thread.start()

        # 4. Odosielame maticu MFCC do ESP32 
        ser.write(byte_data)
        ser.flush()

        print("\nVšetky dáta odoslané. Čakám na výpočet z umelej inteligencie...")
        print("(Program ukončíš stlačením Ctrl+C)\n")
        
        # 5. Udržíme hlavný program nažive, kým ty nestlačíš Ctrl+C
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("\nProgram ukončený cez Ctrl+C.")
    except Exception as e:
        print(f"\nCHYBA: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == '__main__':
    main()