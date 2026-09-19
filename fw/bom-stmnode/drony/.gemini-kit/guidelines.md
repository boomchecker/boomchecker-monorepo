# Guidelines - Drone Detection System

## Code Style
- **Python:** Follow PEP 8. Use type hints where possible.
- **C:** Use MISRA C guidelines where applicable for firmware. Consistent indentation (4 spaces).
- **Naming:** CamelCase for classes (Python) / Types (C), snake_case for functions and variables.

## Development Workflow
- **Prototype First:** All DSP and ML logic must be validated in Python before porting to C.
- **Testing:** Unit tests for individual DSP blocks (FFT, filters).
- **Documentation:** Keep the `docs/` folder updated with algorithm descriptions and results.

## Communication
- Use technical terms from the provided articles (PSD, MFCC, Spectrogram).
- Focus on performance and memory constraints of the STM32H5.
