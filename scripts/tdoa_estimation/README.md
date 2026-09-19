# Acoustic Angle of Arrival (AoA) Analysis Tools

This directory contains Python scripts and dataset files for capturing, analyzing, and visualizing acoustic signal data to determine the Angle of Arrival (AoA) using various Time Difference of Arrival (TDOA) methods.

## Files in this Directory

### Python Scripts
* **`analyzer_live.py`**
    Real-time data acquisition and analysis script. It connects to a serial port (default: `COM3`), reads incoming audio data (Left and Right channels), and calculates the AoA using four different methods:
    1.  Standard Cross-Correlation
    2.  Cross-Correlation with Parabolic Interpolation
    3.  Phase-Based Delay Estimation (PBDE)
    4.  PBDE with Linear Regression
    
    It plots the signals and algorithmic results interactively, allowing the user to manually enter the true angle and save the data to local databases.

* **`viewer.py`**
    An offline visualization tool used to explore the saved data. It reads the `.jsonl` databases and provides an interactive menu to compare how different algorithms performed for specific recorded events and angles. It replicates the matplotlib windows used in the live analyzer.

### Data Files
The scripts generate and utilize the following JSON Lines (`.jsonl`) database files to store raw signals, algorithmic variables, and calculation results:
* **`data_cross_corr.jsonl`**: Stores data for the standard cross-correlation method.
* **`data_cross_corr_parabolic.jsonl`**: Stores sub-sample parabolic interpolation data.
* **`data_pbde_basic.jsonl`**: Stores standard unwrapped phase delay estimation data.
* **`data_pbde_lin_reg.jsonl`**: Stores phase data corrected using linear regression.

## Requirements
To run the scripts, you need Python 3 and the following dependencies:
```bash
pip install pyserial matplotlib numpy
```

## Usage
**Live Analysis & Recording:**
Ensure your hardware is connected to the correct COM port and baud rate (editable in `analyzer_live.py`).
```bash
python analyzer_live.py
```

**Viewing Saved Data:**
Ensure the `.jsonl` files are located in the same directory as the script.
```bash
python viewer.py
```
