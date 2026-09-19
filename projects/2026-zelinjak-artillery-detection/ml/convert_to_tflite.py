import re

INPUT_H_FILE = 'model_data.h'                  
OUTPUT_TFLITE_FILE = 'model.tflite' 

def main():
    print(f"Čítam súbor {INPUT_H_FILE}...")
    try:
        with open(INPUT_H_FILE, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Chyba: Súbor {INPUT_H_FILE} sa nenašiel.")
        return

    # Nájdenie všetkého medzi zátvorkami { a } (C-pole)
    match = re.search(r'\{(.*?)\}', content, re.DOTALL)
    if not match:
        print("Chyba: Nenašiel sa blok s dátami.")
        return

    raw_data = match.group(1).split(',')
    byte_array = bytearray()
    
    for item in raw_data:
        item = item.strip()
        if item:
            try:
                byte_array.append(int(item, 16))
            except ValueError:
                pass 

    with open(OUTPUT_TFLITE_FILE, 'wb') as f:
        f.write(byte_array)
        
    print(f"✅ Úspešne zrekonštruované a uložené ako: {OUTPUT_TFLITE_FILE}")

if __name__ == '__main__':
    main()