from tensorflow.keras.models import Sequential # type: ignore
from tensorflow.keras.layers import Input, Conv2D, MaxPooling2D, Flatten, Dense, Dropout # type: ignore

def build_baseline_cnn(input_shape):
    model = Sequential()
    
    # 1. Konvolučná vrstva
    # 32 filtrov hľadá základné tvary (hrany v spektrograme)
    model.add(Input(shape=input_shape))
    model.add(Conv2D(32, kernel_size=(3, 3), activation='relu'))
    model.add(MaxPooling2D(pool_size=(2, 2)))
    
    # 2. Konvolučná vrstva (voliteľná pre tak malé dáta, ale skúsme ju)
    model.add(Conv2D(64, (3, 3), activation='relu'))
    model.add(MaxPooling2D(pool_size=(2, 2)))
    
    # Sploštenie dát pre Dense vrstvy
    model.add(Flatten())
    
    # 3. Plne prepojená vrstva (Dense)
    model.add(Dense(64, activation='relu'))
    
    # Dropout - náhodne vypne 50% neurónov (prevencia bifľovania)
    model.add(Dropout(0.5))
    
    # 4. Výstupná vrstva (Sigmoid pre binárnu klasifikáciu 0 alebo 1)
    model.add(Dense(1, activation='sigmoid'))
    
    # Kompilácia modelu
    model.compile(optimizer='adam',
                  loss='binary_crossentropy',
                  metrics=['accuracy'])
    
    return model