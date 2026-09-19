# Acoustic Event Detection for Gunshot Recognition (gunshot-detection-lib-tool)

This component of the project, designed as a GUI application, focuses on the detection of acoustic events, specifically recognizing gunshot sounds. It significantly facilitates the analysis of peaks, their classification, and storage in the library. This enhancement is a part of the broader gunshot detection topic at the Faculty of Electrical Engineering (FEE), aiming to streamline the user experience in acoustic event detection and analysis.

## Overview

The primary objective is to accurately identify peaks of gunshot sounds from audio recordings (.wav). This involves analyzing acoustic data to distinguish gunshots from other background noises. The detection process leverages advanced signal processing to achieve high accuracy and reliability (e.g. Median Filter, Z-score peak detection etc.).

## Data Storage

The detected events, along with their acoustic data, are stored in a structured library. This library consists of:

- JSON descriptions: Metadata describing each acoustic event, including timestamps, location (if available), and classification results.
- Audio recordings: Individual files containing the audio data for each detected event. These recordings are crucial for further analysis and improvement of the detection algorithm.

## Contribution

For more information on contributing, please see [CONTRIBUTING.md](./CONTRIBUTING.md).


## Getting started

To make it easy for you to get started with GitLab, here's a list of recommended next steps.

### Prerequisites

Make sure you have Python installed on your system. This project requires Python 3.6 or newer. You can download Python from [the official website](https://www.python.org/downloads/).

### Installation

To install the necessary dependencies for this project, open your terminal and run the following command in the root directory of this project:

```bash
python -m pip install -r requirements.txt
```

## License
TBD

## Project status
TBD