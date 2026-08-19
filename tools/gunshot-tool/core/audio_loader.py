import os
import re
import numpy as np
import soundfile as sf
import librosa
from typing import List, Tuple


def _check_same_samplerate(rates: List[int], filepaths: List[str]):
    """Channels are treated as synchronous, so a differing rate would misalign them."""
    if len(set(rates)) > 1:
        detail = ', '.join(f'{os.path.basename(p)}: {r} Hz' for p, r in zip(filepaths, rates))
        raise ValueError(f'Selected files have different sample rates ({detail}) — they cannot form one recording.')


def _trim_to_shortest(channels: List[np.ndarray]) -> Tuple[List[np.ndarray], List[str]]:
    """Cut every channel to the shortest length. Returns (channels, warnings)."""
    lengths = [len(ch) for ch in channels]
    if len(set(lengths)) <= 1:
        return channels, []
    shortest = min(lengths)
    warning = (
        f'Channels have different lengths ({", ".join(str(l) for l in lengths)} samples) — '
        f'all were trimmed to {shortest} samples.'
    )
    return [ch[:shortest] for ch in channels], [warning]


def load_separate_wavs(folder: str, pattern: str) -> Tuple[List[np.ndarray], int, List[str]]:
    """
    Load separate per-channel WAV files using a pattern with {n} placeholder.
    Returns (channels, sample_rate, file_paths).
    """
    channels = []
    file_paths = []
    rates = []
    n = 1
    while True:
        filename = pattern.replace('{n}', str(n))
        filepath = os.path.join(folder, filename)
        if not os.path.exists(filepath):
            break
        audio, file_sr = librosa.load(filepath, sr=None, mono=True)
        rates.append(file_sr)
        channels.append(audio)
        file_paths.append(filepath)
        n += 1

    if not channels:
        raise ValueError(f"No files found for pattern '{pattern}' in folder '{folder}'")

    _check_same_samplerate(rates, file_paths)
    channels, _ = _trim_to_shortest(channels)
    return channels, rates[0], file_paths


def detect_channels_from_file(filepath: str) -> Tuple[List[str], str]:
    """
    Given one WAV file, auto-detect sibling channel files.
    Tries replacing each digit sequence in the filename with {n} and scans for n=1,2,3...
    Returns (sorted list of found paths, detected pattern filename).
    """
    folder = os.path.dirname(filepath)
    filename = os.path.basename(filepath)
    stem, ext = os.path.splitext(filename)

    # Try each digit sequence in the stem, right-to-left (channel number is usually last)
    for match in reversed(list(re.finditer(r'\d+', stem))):
        start, end = match.span()
        pattern_stem = stem[:start] + '{n}' + stem[end:]
        pattern = pattern_stem + ext

        found = []
        n = 1
        while True:
            candidate = os.path.join(folder, pattern.replace('{n}', str(n)))
            if os.path.exists(candidate):
                found.append(candidate)
                n += 1
            else:
                break

        if len(found) > 1:
            return found, pattern

    # Fallback: just the selected file as a single channel
    return [filepath], filename


def get_audio_info(filepath: str) -> dict:
    """Return metadata (channels, samplerate, duration) without loading audio data."""
    info = sf.info(filepath)
    return {'channels': info.channels, 'samplerate': info.samplerate, 'duration': info.duration}


def load_audio_files(filepaths: List[str]) -> Tuple[List[np.ndarray], int, List[str], List[str]]:
    """
    Unified loader:
    - Multiple files → each file = one channel (first track if file is multi-channel)
    - Single file   → load all internal channels; if mono, tries pattern auto-detection

    Raises ValueError on mixed sample rates; channels of differing length are trimmed
    to the shortest so they stay sample-aligned, which is reported in the warnings list.
    Returns (channels, sample_rate, file_paths, warnings).
    """
    if len(filepaths) > 1:
        channels, rates = [], []
        for p in filepaths:
            data, file_sr = sf.read(p, always_2d=False)
            if data.ndim > 1:
                data = data[:, 0]
            rates.append(file_sr)
            channels.append(data.astype(np.float32))
        _check_same_samplerate(rates, filepaths)
        channels, warnings = _trim_to_shortest(channels)
        return channels, rates[0], list(filepaths), warnings

    filepath = filepaths[0]
    data, sr = sf.read(filepath, always_2d=False)

    if data.ndim > 1:
        channels = [data[:, i].astype(np.float32) for i in range(data.shape[1])]
        return channels, sr, [filepath] * len(channels), []

    # Mono single file → try auto-detect sibling channel files
    detected_paths, _ = detect_channels_from_file(filepath)
    if len(detected_paths) > 1:
        return load_audio_files(detected_paths)

    return [data.astype(np.float32)], sr, [filepath], []


def load_multichannel_wav(filepath: str) -> Tuple[List[np.ndarray], int, List[str]]:
    """
    Load a multi-channel WAV file. Returns (channels, sample_rate, file_paths).
    file_paths repeats the single filepath for each channel (for naming).
    """
    data, sr = sf.read(filepath, always_2d=False)
    if data.ndim == 1:
        return [data.astype(np.float32)], sr, [filepath]
    # data shape: (samples, channels)
    channels = [data[:, i].astype(np.float32) for i in range(data.shape[1])]
    return channels, sr, [filepath] * len(channels)
