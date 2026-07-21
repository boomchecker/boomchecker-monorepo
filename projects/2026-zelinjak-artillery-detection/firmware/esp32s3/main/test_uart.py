import serial
import time
import threading

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

def read_from_esp(ser):
    """Nekonečná slučka bežiaca na pozadí, ktorá počúva ESP32."""
    while True:
        try:
            if ser.in_waiting > 0:
                # Prečítame riadok a odstránime prebytočné medzery/entery
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Vypíšeme aj aktuálny čas, aby sme videli, kedy to prišlo
                    print(f"\n[{time.strftime('%H:%M:%S')}] ESP32 hovorí: {line}")
        except Exception as e:
            print(f"Chyba pri čítaní: {e}")
            break
        # Krátka pauza, aby sme úplne nezaťažili procesor PC
        time.sleep(0.01) 

def main():
    print(f"Pripájam sa na port {SERIAL_PORT} s rýchlosťou {BAUD_RATE}...")

    try:
        # 1. Otvorenie portu (toto spôsobí reštart ESP32)
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print("Pripojenie úspešné!")

        # 2. Počkáme 3 sekundy na prebudenie ESP32 po reštarte
        print("Čakám 3 sekundy na naštartovanie ESP32...")
        time.sleep(3)

        # 3. Spustíme vlákno na počúvanie prichádzajúcich správ
        read_thread = threading.Thread(target=read_from_esp, args=(ser,), daemon=True)
        read_thread.start()

        # 4. Pošleme prvú úvodnú správu do ESP32
        print("Odosielam úvodnú správu do ESP32...")
        ser.write("Ahoj ESP32, tu je tvoj Python!\n".encode('utf-8'))
        ser.flush() # Vynúti okamžité odoslanie dát z buffera PC do kábla

        print("\n" + "="*40)
        print(" KOMUNIKÁCIA BEŽÍ ".center(40, "="))
        print("Teraz môžeš písať správy a stlačiť Enter.")
        print("Pre ukončenie programu napíš 'exit'.")
        print("="*40 + "\n")

        # 5. Hlavná slučka na tvoje písanie správ
        while True:
            user_input = input()
            if user_input.lower() == 'exit':
                break

            # Ku každej správe musíme pridať '\n' (nový riadok), inak by ESP32 nevedelo, že správa skončila
            msg_to_send = user_input + '\n'
            ser.write(msg_to_send.encode('utf-8'))
            ser.flush()
            print(f"-> Odoslané do ESP32: {user_input}")

    except serial.SerialException as e:
        print(f"\nCHYBA PORTU: {e}")
        print("Nezabudni zavrieť 'idf.py monitor' v C++ termináli, ak ti náhodou beží!")
    except KeyboardInterrupt:
        print("\nProgram ukončený cez Ctrl+C.")
    finally:
        # Vždy bezpečne zatvoríme port pri odchode
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Sériový port bol bezpečne zatvorený.")

if __name__ == '__main__':
    main()