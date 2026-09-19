import serial
import matplotlib.pyplot as plt
import numpy as np
import math
import json
import datetime

# settings
SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

# sampling rate
FS = 44100.0 

# output filenames
DB_FILE_CC = 'data_cross_corr.jsonl'
DB_FILE_CC_PARABOLIC = 'data_cross_corr_parabolic.jsonl'
DB_FILE_PBDE_BASIC = 'data_pbde_basic.jsonl'
DB_FILE_PBDE_LIN_REG = 'data_pbde_lin_reg.jsonl'

# physical constants
MIC_DISTANCE_METERS = 0.186
SPEED_OF_SOUND_MPS = 343.0

def parse_csv_line(line):
    """Parses a line formatted as 'NAME:1.0,2.0,3.0,' into an array of floats."""
    try:
        data_str = line.split(':', 1)[1]
        return [float(x) for x in data_str.split(',') if x.strip()]
    except Exception as e:
        print(f"Parse error '{line}': {e}")
        return []

def calculate_aoa(delay_ms):
    """Calculates Angle of Arrival (AoA) from TDOA in degrees."""
    delay_sec = delay_ms / 1000.0
    argument = (SPEED_OF_SOUND_MPS * delay_sec) / MIC_DISTANCE_METERS
    
    if argument > 1.0: argument = 1.0
    elif argument < -1.0: argument = -1.0
        
    return math.degrees(math.asin(argument))

def save_method_data(filename, method_name, true_angle, data_dict):
    """Saves a record to the specified JSONL file."""
    record = {
        "timestamp": datetime.datetime.now().isoformat(),
        "method": method_name,
        "true_angle_deg": true_angle,
        **data_dict
    }
    with open(filename, 'a', encoding='utf-8') as f:
        f.write(json.dumps(record) + '\n')
    print(f"[{method_name}] saved -> {filename}")

def plot_and_calculate(data_L, data_R, 
                       cc_lags, cc_corr, cc_p_pts, cc_p_final_lag,
                       pbde_b_bins, pbde_b_delay, pbde_b_weights, pbde_b_phase,
                       pbde_lr_delay, pbde_lr_phase, pbde_lr_pstep, lr_init_slope):
    """Calculates angles and displays 2 separate windows with plots."""
    
    cc_max_idx = np.argmax(cc_corr) if cc_corr else 0
    cc_best_lag = cc_lags[cc_max_idx] if cc_lags else 0
    cc_max_val = cc_corr[cc_max_idx] if cc_corr else 0
    cc_delay_ms = (cc_best_lag / FS) * 1000.0
    cc_angle = calculate_aoa(cc_delay_ms)
    
    cc_p_lag = cc_p_final_lag if cc_p_final_lag is not None else 0
    cc_p_delay_ms = (cc_p_lag / FS) * 1000.0
    cc_p_angle = calculate_aoa(cc_p_delay_ms)

    w_sum_b = sum(pbde_b_weights) if pbde_b_weights else 0
    pbde_b_delay_ms = sum(d * w for d, w in zip(pbde_b_delay, pbde_b_weights)) / w_sum_b if w_sum_b > 0 else 0
    pbde_b_angle = calculate_aoa(pbde_b_delay_ms)

    pbde_lr_delay_ms = sum(d * w for d, w in zip(pbde_lr_delay, pbde_b_weights)) / w_sum_b if w_sum_b > 0 else 0
    pbde_lr_angle = calculate_aoa(pbde_lr_delay_ms)

    print("\nPlotting graphs... Close BOTH windows to continue.")

    fig1 = plt.figure("Cross Correlation Methods", figsize=(12, 8))
    
    ax1 = fig1.add_subplot(2, 1, 1)
    ax1.set_title("Raw Audio Signals (Time Domain)")
    if data_L and data_R:
        ax1.plot(data_L, label='Left Channel (L)', color='blue', alpha=0.7)
        ax1.plot(data_R, label='Right Channel (R)', color='red', alpha=0.7)
    ax1.set_ylabel("Amplitude")
    ax1.legend()
    ax1.grid(True)

    ax2 = fig1.add_subplot(2, 1, 2)
    ax2.set_title("Cross Correlation Function & Peak Estimates")
    if cc_lags and cc_corr:
        ax2.plot(cc_lags, cc_corr, marker='.', color='purple', alpha=0.6, label='Correlation Function')
        
        ax2.plot(cc_best_lag, cc_max_val, 'go', markersize=10, 
                 label=f'CC Peak ({cc_angle:.1f}° | Lag: {cc_best_lag})')
        
        if cc_p_final_lag is not None:
            ax2.axvline(x=cc_p_final_lag, color='red', linestyle='--', 
                        label=f'Parabola Sub-peak ({cc_p_angle:.1f}° | Lag: {cc_p_final_lag:.3f})')
            if len(cc_p_pts) == 3:
                x_pts = [cc_best_lag - 1, cc_best_lag, cc_best_lag + 1]
                ax2.scatter(x_pts, cc_p_pts, color='red', zorder=5, s=80, marker='x', label='Parabola Points')

    ax2.set_ylabel("Correlation Value")
    ax2.set_xlabel("Delay [Samples]")
    ax2.legend()
    ax2.grid(True)
    fig1.tight_layout()

    fig2 = plt.figure("PBDE (Phase Based Delay Estimation) Methods", figsize=(12, 10))
    
    ax3 = fig2.add_subplot(3, 1, 1)
    ax3.set_title("Raw Audio Signals (Time Domain)")
    if data_L and data_R:
        ax3.plot(data_L, label='Left Channel (L)', color='blue', alpha=0.7)
        ax3.plot(data_R, label='Right Channel (R)', color='red', alpha=0.7)
    ax3.set_ylabel("Amplitude")
    ax3.legend()
    ax3.grid(True)

    x_bins = np.arange(pbde_b_bins[0], pbde_b_bins[0] + len(pbde_b_delay)) if pbde_b_bins else []

    ax4 = fig2.add_subplot(3, 1, 2)
    ax4.set_title("Unwrapped Phase")
    if len(x_bins) > 0:
        if pbde_b_phase:
            ax4.plot(x_bins, pbde_b_phase, color='black', alpha=0.4, linewidth=4, label='Raw Phase (Basic)')
        if pbde_lr_phase:
            ax4.plot(x_bins, pbde_lr_phase, color='magenta', linewidth=2, label='Corrected Phase (Lin Reg)')
        if lr_init_slope is not None:
            ax4.plot(x_bins, x_bins * lr_init_slope, color='blue', linestyle='--', alpha=0.5, label='Median Slope')
            
    ax4.set_ylabel("Phase [rad]")
    ax4.legend()
    ax4.grid(True)

    ax5 = fig2.add_subplot(3, 1, 3)
    ax5.set_title("Calculated Delay & Amplitude Weights")
    if len(x_bins) > 0:
        ax5_weights = ax5.twinx()
        
        if pbde_b_weights:
            ax5_weights.bar(x_bins, pbde_b_weights, color='green', alpha=0.2, width=1.0, label='Norm. Weights')
        
        if pbde_b_delay:
            ax5.scatter(x_bins, pbde_b_delay, c='gray', s=15, alpha=0.7, 
                        label=f'Delay Basic ({pbde_b_angle:.1f}° | {pbde_b_delay_ms:.4f} ms)')
        
        if pbde_lr_delay:
            ax5.scatter(x_bins, pbde_lr_delay, c='blue', s=15, 
                        label=f'Delay Lin Reg ({pbde_lr_angle:.1f}° | {pbde_lr_delay_ms:.4f} ms)')

        ax5.set_ylabel("Delay [ms]")
        ax5_weights.set_ylabel("Weight [0.0 - 1.0]")
        ax5.set_xlabel("Frequency Bin")
        
        lines_1, labels_1 = ax5.get_legend_handles_labels()
        lines_2, labels_2 = ax5_weights.get_legend_handles_labels()
        ax5.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper right')

    ax5.grid(True)
    fig2.tight_layout()

    plt.show()

    return {
        "cc_lag": cc_best_lag, "cc_angle": cc_angle,
        "cc_p_lag": cc_p_lag, "cc_p_angle": cc_p_angle,
        "pbde_b_delay_ms": pbde_b_delay_ms, "pbde_b_angle": pbde_b_angle,
        "pbde_lr_delay_ms": pbde_lr_delay_ms, "pbde_lr_angle": pbde_lr_angle
    }

def main():
    print(f"Connecting to {SERIAL_PORT} @ {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except Exception as e:
        print(f"Connection error: {e}")
        return

    print("\nListening to UART... (Press Ctrl+C to exit)")
    
    in_data = in_cc = in_cc_p = in_pbde_b = in_pbde_lr = False
    
    data_L, data_R = [], []
    cc_lags, cc_corr = [], []
    cc_p_pts, cc_p_final_lag = [], None
    pbde_b_bins, pbde_b_delay, pbde_b_weights, pbde_b_phase = [], [], [], []
    pbde_lr_delay, pbde_lr_phase, pbde_lr_pstep = [], [], []
    lr_init_slope = None

    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line: continue

                if "Method 1 (Initial slope - median):" in line:
                    try: lr_init_slope = float(line.split(":")[-1].strip())
                    except: pass

                if line == "---DATA_END---":
                    in_data = False
                    
                    results = plot_and_calculate(
                        data_L, data_R, 
                        cc_lags, cc_corr, cc_p_pts, cc_p_final_lag,
                        pbde_b_bins, pbde_b_delay, pbde_b_weights, pbde_b_phase,
                        pbde_lr_delay, pbde_lr_phase, pbde_lr_pstep, lr_init_slope
                    )
                    
                    print("\n" + "="*60)
                    save_choice = input("Save this record? (y/n): ").strip().lower()
                    
                    if save_choice == 'y':
                        true_angle_str = input("Enter true angle in degrees (e.g., 45.5): ").replace(',', '.')
                        try:
                            true_angle = float(true_angle_str)
                            
                            save_method_data(DB_FILE_CC, "Cross_Corr", true_angle, {
                                "raw_L": data_L, "raw_R": data_R,
                                "calc_lag": results["cc_lag"], "calc_angle": results["cc_angle"],
                                "lags_array": cc_lags, "corr_array": cc_corr
                            })
                            
                            save_method_data(DB_FILE_CC_PARABOLIC, "Cross_Corr_Parabolic", true_angle, {
                                "raw_L": data_L, "raw_R": data_R,
                                "calc_lag": results["cc_p_lag"], "calc_angle": results["cc_p_angle"],
                                "parabola_pts": cc_p_pts
                            })
                            
                            save_method_data(DB_FILE_PBDE_BASIC, "PBDE_Basic", true_angle, {
                                "raw_L": data_L, "raw_R": data_R,
                                "calc_delay_ms": results["pbde_b_delay_ms"], "calc_angle": results["pbde_b_angle"],
                                "bins_used": pbde_b_bins, "delay": pbde_b_delay, "weights": pbde_b_weights, "phase": pbde_b_phase
                            })
                            
                            save_method_data(DB_FILE_PBDE_LIN_REG, "PBDE_Lin_Reg", true_angle, {
                                "raw_L": data_L, "raw_R": data_R,
                                "calc_delay_ms": results["pbde_lr_delay_ms"], "calc_angle": results["pbde_lr_angle"],
                                "init_slope": lr_init_slope, "delay": pbde_lr_delay, "weights": pbde_b_weights, 
                                "phase": pbde_lr_phase, "phase_step": pbde_lr_pstep
                            })
                            print("All data exported successfully!")
                        except ValueError:
                            print("Invalid angle format. Save aborted.")
                    else:
                        print("Record discarded.")
                    print("="*60)
                    
                    data_L, data_R, cc_lags, cc_corr, cc_p_pts, cc_p_final_lag = [], [], [], [], [], None
                    pbde_b_bins, pbde_b_delay, pbde_b_weights, pbde_b_phase = [], [], [], []
                    pbde_lr_delay, pbde_lr_phase, pbde_lr_pstep = [], [], []
                    lr_init_slope = None
                    ser.reset_input_buffer()
                    print("\nWaiting for next impulse... (Ctrl+C to exit)")

                elif line == "---DATA_START---": in_data = True
                elif line == "---CROSS_CORR_START---": in_cc = True
                elif line == "---CROSS_CORR_END---": in_cc = False
                elif line == "---CROSS_CORR_PARABOLIC_START---": in_cc_p = True
                elif line == "---CROSS_CORR_PARABOLIC_END---": in_cc_p = False
                elif line == "---PBDE_BASIC_START---": in_pbde_b = True
                elif line == "---PBDE_BASIC_END---": in_pbde_b = False
                elif line == "---PBDE_LIN_REG_START---": in_pbde_lr = True
                elif line == "---PBDE_LIN_REG_END---": in_pbde_lr = False

                elif in_data:
                    if line.startswith("L:"): data_L = parse_csv_line(line)
                    elif line.startswith("R:"): data_R = parse_csv_line(line)
                
                elif in_cc:
                    if line.startswith("LAGS:"): cc_lags = parse_csv_line(line)
                    elif line.startswith("CORR:"): cc_corr = parse_csv_line(line)
                
                elif in_cc_p:
                    if line.startswith("PARABOLA_PTS:"): cc_p_pts = parse_csv_line(line)
                    elif line.startswith("FINAL_LAG:"): 
                        try: cc_p_final_lag = float(line.split(':')[1])
                        except: pass
                
                elif in_pbde_b:
                    if line.startswith("BINS_USED:"): pbde_b_bins = [int(x) for x in line.split(':')[1].split(',') if x.strip()]
                    elif line.startswith("DELAY:"): pbde_b_delay = parse_csv_line(line)
                    elif line.startswith("WEIGHTS:"): pbde_b_weights = parse_csv_line(line)
                    elif line.startswith("PHASE:"): pbde_b_phase = parse_csv_line(line)
                
                elif in_pbde_lr:
                    if line.startswith("DELAY:"): pbde_lr_delay = parse_csv_line(line)
                    elif line.startswith("PHASE:"): pbde_lr_phase = parse_csv_line(line)
                    elif line.startswith("PHASE_STEP:"): pbde_lr_pstep = parse_csv_line(line)

    except KeyboardInterrupt:
        print("\nTerminated by user.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()