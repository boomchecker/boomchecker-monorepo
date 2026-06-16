import joblib
import numpy as np
import os

def export_model_to_c(model_path, output_path):
    print(f"Exportování modelu z: {model_path}")
    
    if not os.path.exists(model_path):
        print(f"Chyba: Model {model_path} nebyl nalezen. Nejdříve spusť 'task train'.")
        return

    clf = joblib.load(model_path)
    
    # SVM parametry
    dual_coef = clf.dual_coef_.ravel()
    support_vectors = clf.support_vectors_
    intercept = clf.intercept_[0]
    gamma = clf._gamma
    
    n_support = len(clf.support_)
    n_features = support_vectors.shape[1]
    
    with open(output_path, 'w') as f:
        f.write("#ifndef SVM_MODEL_DATA_H\n")
        f.write("#define SVM_MODEL_DATA_H\n\n")
        
        f.write(f"// Automaticky generovaný soubor z {model_path}\n\n")
        
        f.write(f"#define SVM_NUM_SUPPORT_VECTORS {n_support}\n")
        f.write(f"#define SVM_NUM_FEATURES {n_features}\n")
        f.write(f"#define SVM_GAMMA {gamma}f\n")
        f.write(f"#define SVM_INTERCEPT {intercept}f\n\n")
        
        # Support Vectors
        f.write("static const float svm_support_vectors[SVM_NUM_SUPPORT_VECTORS][SVM_NUM_FEATURES] = {\n")
        for i in range(n_support):
            f.write("    {")
            f.write(", ".join([f"{v}f" for v in support_vectors[i]]))
            f.write("}" + ("," if i < n_support - 1 else "") + "\n")
        f.write("};\n\n")
        
        # Dual Coefficients
        f.write("static const float svm_dual_coefficients[SVM_NUM_SUPPORT_VECTORS] = {\n")
        f.write("    " + ", ".join([f"{c}f" for c in dual_coef]) + "\n")
        f.write("};\n\n")
        
        f.write("#endif // SVM_MODEL_DATA_H\n")
        
    print(f"Model byl úspěšně exportován do: {output_path}")
    print(f"Počet support vektorů: {n_support}")
    print(f"Počet příznaků: {n_features}")

if __name__ == "__main__":
    model_file = 'models/drone_detector_svm.pkl'
    header_file = 'src/firmware/Inc/svm_model_data.h'
    
    # Zajistíme, že cílový adresář existuje
    os.makedirs(os.path.dirname(header_file), exist_ok=True)
    
    export_model_to_c(model_file, header_file)
