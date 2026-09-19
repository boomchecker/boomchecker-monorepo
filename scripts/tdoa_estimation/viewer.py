import json
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

FILES = [
    os.path.join(BASE_DIR, 'data_cross_corr.jsonl'),
    os.path.join(BASE_DIR, 'data_cross_corr_parabolic.jsonl'),
    os.path.join(BASE_DIR, 'data_pbde_basic.jsonl'),
    os.path.join(BASE_DIR, 'data_pbde_lin_reg.jsonl')
]

saved_geometry = {"cc": None, "pbde": None}

def save_window_geometry(event, window_name):
    """Saves the exact geometry (position and size) of the window when it is closed."""
    try:
        manager = event.canvas.manager
        backend = matplotlib.get_backend().lower()
        if 'tk' in backend:
            saved_geometry[window_name] = manager.window.geometry()
        elif 'qt' in backend:
            saved_geometry[window_name] = manager.window.geometry()
    except Exception:
        pass

def apply_window_geometry(fig, window_name):
    """Applies the saved geometry to the new window before it is displayed."""
    geom = saved_geometry.get(window_name)
    if not geom:
        return
    try:
        manager = fig.canvas.manager
        backend = matplotlib.get_backend().lower()
        if 'tk' in backend:
            manager.window.geometry(geom)
        elif 'qt' in backend:
            manager.window.setGeometry(geom)
    except Exception:
        pass

def load_merged_data():
    lines = [[], [], [], []]
    for i, f in enumerate(FILES):
        if os.path.exists(f):
            with open(f, 'r', encoding='utf-8') as file:
                lines[i] = [line.strip() for line in file if line.strip()]
            print(f"[OK] Found {os.path.basename(f)} ({len(lines[i])} lines)")
        else:
            print(f"[MISSING] Could not find {f}")

    valid_lengths = [len(l) for l in lines if l]
    min_len = min(valid_lengths) if valid_lengths else 0
    
    if min_len == 0:
        return {}

    events_by_angle = {}
    for i in range(min_len):
        try:
            rec_cc = json.loads(lines[0][i]) if lines[0] else {}
            rec_cc_p = json.loads(lines[1][i]) if lines[1] else {}
            rec_pbde_b = json.loads(lines[2][i]) if lines[2] else {}
            rec_pbde_lr = json.loads(lines[3][i]) if lines[3] else {}

            angle = rec_cc.get("true_angle_deg") or rec_pbde_b.get("true_angle_deg") or 0.0

            if angle not in events_by_angle:
                events_by_angle[angle] = []
            events_by_angle[angle].append((rec_cc, rec_cc_p, rec_pbde_b, rec_pbde_lr))
        except Exception as e:
            print(f"[ERROR] Failed to parse JSON on line {i+1}: {e}")
            continue

    return events_by_angle

def plot_cc(rec_cc, rec_cc_p, idx, total, angle):
    fig = plt.figure(f"Cross Correlation (Angle: {angle}°) - [{idx+1}/{total}]", figsize=(12, 8))
    
    fig.canvas.mpl_connect('close_event', lambda event: save_window_geometry(event, "cc"))
    
    ax1 = fig.add_subplot(2, 1, 1)
    ax1.set_title("Raw Audio Signals")
    data_L = rec_cc.get("raw_L", [])
    data_R = rec_cc.get("raw_R", [])
    if data_L and data_R:
        ax1.plot(data_L, label='Left (L)', color='blue', alpha=0.7)
        ax1.plot(data_R, label='Right (R)', color='red', alpha=0.7)
    ax1.set_ylabel("Amplitude")
    ax1.legend()
    ax1.grid(True)

    ax2 = fig.add_subplot(2, 1, 2)
    ax2.set_title("Cross Correlation Function & Peaks")
    lags = rec_cc.get("lags_array", [])
    corr = rec_cc.get("corr_array", [])
    calc_lag_cc = rec_cc.get("calc_lag", 0)
    calc_angle_cc = rec_cc.get("calc_angle", 0)

    if lags and corr:
        ax2.plot(lags, corr, marker='.', color='purple', alpha=0.6, label='Correlation')
        try:
            max_idx = np.argmax(corr)
            ax2.plot(lags[max_idx], corr[max_idx], 'go', markersize=10, 
                     label=f'CC Peak ({calc_angle_cc:.1f}° | Lag: {calc_lag_cc})')
        except: 
            pass

    calc_lag_p = rec_cc_p.get("calc_lag")
    calc_angle_p = rec_cc_p.get("calc_angle")
    parabola_pts = rec_cc_p.get("parabola_pts", [])

    if calc_lag_p is not None:
        ax2.axvline(x=calc_lag_p, color='red', linestyle='--', 
                    label=f'Parabola ({calc_angle_p:.1f}° | Lag: {calc_lag_p:.3f})')
        if len(parabola_pts) == 3 and calc_lag_cc:
            x_pts = [calc_lag_cc - 1, calc_lag_cc, calc_lag_cc + 1]
            ax2.scatter(x_pts, parabola_pts, color='red', zorder=5, s=80, marker='x', label='Parabola Points')

    ax2.set_ylabel("Correlation Value")
    ax2.set_xlabel("Delay [Samples]")
    ax2.legend()
    ax2.grid(True)
    fig.tight_layout()

    apply_window_geometry(fig, "cc")

def plot_pbde(rec_b, rec_lr, idx, total, angle):
    fig = plt.figure(f"PBDE Methods (Angle: {angle}°) - [{idx+1}/{total}]", figsize=(12, 10))
    
    fig.canvas.mpl_connect('close_event', lambda event: save_window_geometry(event, "pbde"))
    
    ax3 = fig.add_subplot(3, 1, 1)
    ax3.set_title("Raw Audio Signals")
    data_L = rec_b.get("raw_L", [])
    data_R = rec_b.get("raw_R", [])
    if data_L and data_R:
        ax3.plot(data_L, label='Left (L)', color='blue', alpha=0.7)
        ax3.plot(data_R, label='Right (R)', color='red', alpha=0.7)
    ax3.set_ylabel("Amplitude")
    ax3.legend()
    ax3.grid(True)

    bins = rec_b.get("bins_used", [])
    delay_b = rec_b.get("delay", [])
    if bins:
        x_bins = np.arange(bins[0], bins[0] + len(delay_b))
    else:
        x_bins = np.arange(len(delay_b)) if delay_b else []

    ax4 = fig.add_subplot(3, 1, 2)
    ax4.set_title("Unwrapped Phase")
    phase_b = rec_b.get("phase", [])
    phase_lr = rec_lr.get("phase", [])
    init_slope = rec_lr.get("init_slope")

    if len(x_bins) > 0:
        if phase_b:
            ax4.plot(x_bins, phase_b, color='black', alpha=0.4, linewidth=4, label='Raw Phase (Basic)')
        if phase_lr:
            ax4.plot(x_bins, phase_lr, color='magenta', linewidth=2, label='Corrected Phase (Lin Reg)')
        if init_slope is not None:
            ax4.plot(x_bins, x_bins * init_slope, color='blue', linestyle='--', alpha=0.5, label='Median Slope')
            
    ax4.set_ylabel("Phase [rad]")
    ax4.legend()
    ax4.grid(True)

    ax5 = fig.add_subplot(3, 1, 3)
    ax5.set_title("Calculated Delay & Amplitude Weights")
    weights = rec_b.get("weights", [])
    delay_lr = rec_lr.get("delay", [])
    angle_b = rec_b.get("calc_angle", 0)
    angle_lr = rec_lr.get("calc_angle", 0)
    delay_ms_b = rec_b.get("calc_delay_ms", 0)
    delay_ms_lr = rec_lr.get("calc_delay_ms", 0)

    if len(x_bins) > 0:
        ax5_w = ax5.twinx()
        if weights:
            ax5_w.bar(x_bins, weights, color='green', alpha=0.2, width=1.0, label='Norm. Weights')
        if delay_b:
            ax5.scatter(x_bins, delay_b, c='gray', s=15, alpha=0.7, 
                        label=f'Delay Basic ({angle_b:.1f}° | {delay_ms_b:.4f} ms)')
        if delay_lr:
            ax5.scatter(x_bins, delay_lr, c='blue', s=15, 
                        label=f'Delay Lin Reg ({angle_lr:.1f}° | {delay_ms_lr:.4f} ms)')

        ax5.set_ylabel("Delay [ms]")
        ax5_w.set_ylabel("Weight [0.0 - 1.0]")
        ax5.set_xlabel("Frequency Bin")
        
        lines_1, labels_1 = ax5.get_legend_handles_labels()
        lines_2, labels_2 = ax5_w.get_legend_handles_labels()
        ax5.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper right')

    ax5.grid(True)
    fig.tight_layout()

    apply_window_geometry(fig, "pbde")

def main():
    print("Loading databases...")
    data_by_angle = load_merged_data()

    if not data_by_angle:
        print("No data found. Make sure JSONL files are in the same directory.")
        return

    while True:
        print("\n=== Acoustic Location Data Viewer ===")
        print("Available modes:")
        print("[1] Cross Correlation only (Compare Basic vs Parabolic)")
        print("[2] PBDE only (Compare Basic vs Lin Reg)")
        print("[3] View All (Both windows simultaneously)")
        print("[0] Exit")

        mode = input("Select mode (0-3): ").strip()
        if mode == "0":
            break
        if mode not in ["1", "2", "3"]:
            continue

        print("\nAvailable angles and record counts:")
        angles = sorted(data_by_angle.keys())
        for a in angles:
            print(f" - Angle {a}° : {len(data_by_angle[a])} records")
        
        angle_in = input("\nEnter angle to view, or type 'all' to loop through EVERYTHING sequentially: ").strip().lower()

        events_to_show = []

        if angle_in == 'all':
            for a in angles:
                for idx, ev in enumerate(data_by_angle[a]):
                    events_to_show.append((a, idx, len(data_by_angle[a]), ev))
        else:
            try:
                selected_angle = float(angle_in.replace(',', '.'))
                if selected_angle not in data_by_angle:
                    print("Angle not found.")
                    continue
            except ValueError:
                print("Invalid input.")
                continue

            total_records = len(data_by_angle[selected_angle])
            idx_in = input(f"Enter index (0 to {total_records - 1}) or type 'all' to view all records for {selected_angle}°: ").strip().lower()

            if idx_in == 'all':
                for idx, ev in enumerate(data_by_angle[selected_angle]):
                    events_to_show.append((selected_angle, idx, total_records, ev))
            else:
                try:
                    idx = int(idx_in)
                    if 0 <= idx < total_records:
                        events_to_show.append((selected_angle, idx, total_records, data_by_angle[selected_angle][idx]))
                    else:
                        print("Index out of range.")
                        continue
                except ValueError:
                    print("Invalid index.")
                    continue

        for (ang, idx, total, ev) in events_to_show:
            print(f"\nDisplaying Angle: {ang}°, Index: {idx}/{total-1}")
            print("Close graph window(s) to proceed.")
            
            rec_cc, rec_cc_p, rec_pbde_b, rec_pbde_lr = ev

            if mode in ["1", "3"]:
                plot_cc(rec_cc, rec_cc_p, idx, total, ang)
            if mode in ["2", "3"]:
                plot_pbde(rec_pbde_b, rec_pbde_lr, idx, total, ang)

            plt.show()

if __name__ == "__main__":
    main()