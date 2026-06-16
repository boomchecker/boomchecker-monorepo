import os
import urllib.request
import zipfile
import shutil

def download_cmsis_dsp():
    # Cesty
    base_dir = "src/firmware/Drivers/CMSIS"
    dsp_dir = os.path.join(base_dir, "DSP")
    core_dir = os.path.join(base_dir, "Core")
    temp_zip = "cmsis_dsp.zip"
    
    # Vytvoření adresářů
    os.makedirs(dsp_dir, exist_ok=True)
    os.makedirs(core_dir, exist_ok=True)
    
    print("Stahuji CMSIS-DSP z GitHubu...")
    # Použijeme release verzi pro stabilitu
    url = "https://github.com/ARM-software/CMSIS-DSP/archive/refs/tags/v1.15.0.zip"
    
    try:
        urllib.request.urlretrieve(url, temp_zip)
        print("Rozbaluji...")
        with zipfile.ZipFile(temp_zip, 'r') as zip_ref:
            zip_ref.extractall("temp_cmsis")
            
        # Důležité: CMSIS-DSP repository má strukturu CMSIS-DSP-1.15.0/
        root_dir = "temp_cmsis/CMSIS-DSP-1.15.0"
        
        # Kopírování Include
        src_inc = os.path.join(root_dir, "Include")
        dst_inc = os.path.join(dsp_dir, "Include")
        if os.path.exists(dst_inc): shutil.rmtree(dst_inc)
        shutil.copytree(src_inc, dst_inc)
        
        # Kopírování Source
        src_src = os.path.join(root_dir, "Source")
        dst_src = os.path.join(dsp_dir, "Source")
        if os.path.exists(dst_src): shutil.rmtree(dst_src)
        shutil.copytree(src_src, dst_src)

        print("Úklid...")
        # Zkusíme smazat s malým zpožděním, pokud by OS soubor ještě držel
        import time
        time.sleep(1)
        shutil.rmtree("temp_cmsis", ignore_errors=True)
        if os.path.exists(temp_zip): os.remove(temp_zip)
        
        print(f"Hotovo. CMSIS-DSP je v: {base_dir}")
        
    except Exception as e:
        print(f"Chyba při stahování: {e}")

if __name__ == "__main__":
    download_cmsis_dsp()
