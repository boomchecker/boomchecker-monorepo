# Model Exploration & Performance Report

This report compares different signal preprocessing (normalization), feature scaling, and SVM kernel options to optimize the acoustic drone detection system for STM32H5.

## Experiment Results Table

| # | Experiment Configuration | Accuracy | F1 Drone | F1 Noise | Support Vectors | Est. Flash Memory |
|---|--------------------------|----------|----------|----------|-----------------|-------------------|
| 1 | 1. Max Norm, No Scaling, RBF (Current) | 0.8757 | 0.8853 | 0.8645 | 2783 | 293.52 KB |
| 2 | 2. RMS Norm, No Scaling, RBF | 0.8770 | 0.8857 | 0.8669 | 2734 | 288.35 KB |
| 3 | 3. No Norm, No Scaling, RBF | 0.8816 | 0.8893 | 0.8727 | 2693 | 284.03 KB |
| 4 | 4. RMS Norm, Std Scaler, RBF | 0.8980 | 0.9038 | 0.8914 | 2117 | 223.28 KB |
| 5 | 5. RMS Norm, MinMax Scaler, RBF | 0.8795 | 0.8881 | 0.8695 | 2740 | 288.98 KB |
| 6 | 6. RMS Norm, No Scaling, Linear | 0.8724 | 0.8819 | 0.8612 | 2657 | 0.11 KB |
| 7 | 7. RMS Norm, No Scaling, Cubic (Poly d=3) | 0.8023 | 0.8337 | 0.7561 | 4973 | 524.50 KB |
| 8 | 8. RMS Norm, Std Scaler, Linear | 0.8732 | 0.8828 | 0.8620 | 2650 | 0.11 KB |
| 9 | 9. RMS Norm, Std Scaler, Cubic (Poly d=3) | 0.8753 | 0.8838 | 0.8656 | 3712 | 391.50 KB |

## Analysis and Key Findings

### 1. Preprocessing (Normalization)
- **RMS Normalization** vs **Max Amplitude**:
  Evaluating how dividing by the Root Mean Square (RMS) compares to scaling by peak amplitude.
- **No Normalization**:
  Keeps absolute energy levels, which may help distinguish low energy ambient noises from closer drone noises.

### 2. Feature Scaling
- **StandardScaler** (Standardization) and **MinMaxScaler**:
  Normalizing feature vectors to prevent large variance features (like first coefficients) from dominating decision functions.

### 3. SVM Kernels & Memory Efficiency on MCU
- **Linear Kernel**:
  Reduces Flash storage size to just 108 bytes, as we only need to store a single weight vector of 26 parameters and 1 bias.
- **Cubic Kernel** (Polynomial degree 3):
  A non-linear boundary that can capture more complex feature boundaries compared to Linear.
- **RBF Kernel**:
  Standard kernel, but requires storing a massive amount of support vectors (very high Flash consumption).
