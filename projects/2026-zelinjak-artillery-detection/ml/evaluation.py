import numpy as np

def evaluate_with_noise(model, X, y, snr_db):
    if snr_db is None:
        results = model.evaluate(X, y, verbose=0)
        # Bezpečné vytiahnutie Accuracy z výsledkov
        acc = results[1] if isinstance(results, list) else results
        return acc, X
        
    # Ak máme šum (napr. 0, 5, 10 dB), vypočítame ho
    signal_power = np.mean(X ** 2)
    noise_power = signal_power / (10 ** (snr_db / 10))
    noise = np.random.normal(0, np.sqrt(noise_power), X.shape)
    X_noisy = X + noise
    
    results = model.evaluate(X_noisy, y, verbose=0)
    acc = results[1] if isinstance(results, list) else results
    
    return acc, X_noisy