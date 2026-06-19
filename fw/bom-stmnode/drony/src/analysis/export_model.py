import joblib
import numpy as np
import os

def export_linear_model_to_c(model_path, scaler_path, output_path):
    print(f"Exporting model from: {model_path} and scaler from: {scaler_path}")
    
    if not os.path.exists(model_path):
        print(f"Error: Model {model_path} not found. Run 'python src/analysis/train_svm.py' first.")
        return
        
    if not os.path.exists(scaler_path):
        print(f"Error: Scaler {scaler_path} not found.")
        return

    # Load model and scaler
    clf = joblib.load(model_path)
    scaler = joblib.load(scaler_path)
    
    # 1. Verify it's a linear SVM
    if clf.kernel != 'linear':
        print(f"Error: Model is not a linear SVM (kernel is {clf.kernel}).")
        return
        
    # 2. Get weights and bias
    # Scikit-learn Linear SVM coef_ has shape (1, n_features) for binary classification
    weights = clf.coef_[0]
    intercept = clf.intercept_[0]
    
    # 3. Get scaler parameters
    means = scaler.mean_
    stds = scaler.scale_
    
    n_features = len(weights)
    
    # We pre-calculate 1.0 / stds to optimize execution on MCU (floating point division is slow)
    inv_stds = 1.0 / (stds + 1e-8)
    
    with open(output_path, 'w') as f:
        f.write("#ifndef SVM_MODEL_DATA_H\n")
        f.write("#define SVM_MODEL_DATA_H\n\n")
        
        f.write(f"// Automatically generated Linear SVM model weights and StandardScaler parameters\n\n")
        
        f.write(f"#define SVM_NUM_FEATURES {n_features}\n")
        f.write(f"#define SVM_BIAS {intercept:.8e}f\n\n")
        
        # Scaler Mean
        f.write("static const float svm_scaler_mean[SVM_NUM_FEATURES] = {\n")
        f.write("    " + ", ".join([f"{v:.8e}f" for v in means]) + "\n")
        f.write("};\n\n")
        
        # Scaler Inverse Standard Deviation (multiplication is faster than division on MCU)
        f.write("static const float svm_scaler_inv_std[SVM_NUM_FEATURES] = {\n")
        f.write("    " + ", ".join([f"{v:.8e}f" for v in inv_stds]) + "\n")
        f.write("};\n\n")
        
        # Linear Weights
        f.write("static const float svm_weights[SVM_NUM_FEATURES] = {\n")
        f.write("    " + ", ".join([f"{v:.8e}f" for v in weights]) + "\n")
        f.write("};\n\n")
        
        f.write("#endif // SVM_MODEL_DATA_H\n")
        
    print(f"Linear model successfully exported to: {output_path}")
    print(f"Number of features: {n_features}")
    print(f"Bias: {intercept:.4f}")

if __name__ == "__main__":
    model_file = 'models/drone_detector_svm.pkl'
    scaler_file = 'models/scaler.pkl'
    header_file = 'src/firmware/Inc/svm_model_data.h'
    
    os.makedirs(os.path.dirname(header_file), exist_ok=True)
    export_linear_model_to_c(model_file, scaler_file, header_file)
