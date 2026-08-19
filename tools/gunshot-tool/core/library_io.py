import os
import random
import re
import string
import json
import numpy as np
import pandas as pd
from scipy.io.wavfile import write as wav_write
from typing import List, Optional

from core.event_types import PRIMARY_KEY, METADATA_FIELDS


class AudioRecording:
    def __init__(self, id, filenames, label, samplerate, num_channels, other_params=None):
        self.id = id
        self.filenames = filenames
        self.label = label
        self.samplerate = samplerate
        self.num_channels = num_channels
        self.other_params = other_params or {}

    def to_dict(self):
        return {
            'id': self.id,
            'filenames': self.filenames,
            'label': self.label,
            'samplerate': self.samplerate,
            'channels': self.num_channels,
            'other_params': self.other_params,
        }


def _generate_uid(length=6) -> str:
    chars = string.ascii_lowercase + string.digits
    return ''.join(random.choices(chars, k=length))


_UNSAFE_PATH_CHARS = re.compile(r'[<>:"/\\|?*\x00-\x1f]')


def sanitize_path_part(value, fallback: str = 'unknown') -> str:
    """Make a free-text metadata value safe as a single folder or filename part."""
    cleaned = _UNSAFE_PATH_CHARS.sub('_', str(value))
    cleaned = re.sub(r'\s+', '_', cleaned).strip('. ')
    return cleaned or fallback


def _get_primary_id(event_type: str, metadata: dict) -> str:
    key = PRIMARY_KEY.get(event_type, 'caliber')
    return metadata.get(key, 'unknown') or 'unknown'


def save_peak(
    channels: List[np.ndarray],
    sr: int,
    channel_files: List[str],
    peak_sample: int,
    window_size: int,
    pre_peak_pct: float,
    metadata: dict,
    output_root: str,
) -> str:
    """
    Crop all channels synchronously around peak_sample and save to library.
    Path: {root}/{event_type}/{date}_{location}/{primary_id}/{primary_id}_{label}/
    Returns the generated UID.
    """
    pre_samples = int(window_size * pre_peak_pct / 100)
    post_samples = window_size - pre_samples

    start = max(0, peak_sample - pre_samples)
    end = min(len(channels[0]), peak_sample + post_samples)

    uid = _generate_uid()
    event_type = metadata.get('event_type', 'gunshot')
    # Free-text metadata ends up in paths and filenames, so it has to be sanitized
    label = sanitize_path_part(metadata.get('label', ''), 'unknown')
    date = sanitize_path_part(metadata.get('date', ''), '')
    location = sanitize_path_part(metadata.get('location', ''), '')
    primary_id = sanitize_path_part(_get_primary_id(event_type, metadata), 'unknown')

    dir_name = f"{date}_{location}" if location else date
    output_dir = os.path.join(
        output_root, sanitize_path_part(event_type, 'gunshot'), dir_name, primary_id, f"{primary_id}_{label}"
    )
    os.makedirs(output_dir, exist_ok=True)

    # Build other_params from all type-specific fields + standard fields
    type_fields = {key: metadata.get(key, '') for key, _, _ in METADATA_FIELDS.get(event_type, [])}
    other_params = {
        'event_type': event_type,
        'date': metadata.get('date', ''),
        'window_size': window_size,
        'impulse_position': pre_peak_pct,
        **type_fields,
    }

    filenames = [f"{primary_id}_{label}_ch{i + 1}_uid-{uid}.wav" for i in range(len(channels))]
    record = AudioRecording(
        id=uid,
        filenames=filenames,
        label=label,
        samplerate=sr,
        num_channels=len(channels),
        other_params=other_params,
    )
    parquet_path = os.path.join(output_dir, f"{primary_id}_{label}.parquet")

    # Roll back already-written WAVs if any channel or the parquet append fails,
    # so a failed save never leaves unreferenced audio behind
    written = []
    try:
        for ch_audio, filename in zip(channels, filenames):
            data_int16 = (np.clip(ch_audio[start:end], -1.0, 1.0) * 32767).astype(np.int16)
            wav_path = os.path.join(output_dir, filename)
            wav_write(wav_path, sr, data_int16)
            written.append(wav_path)
        _append_record(parquet_path, record)
    except Exception:
        for wav_path in written:
            try:
                os.remove(wav_path)
            except OSError:
                pass
        raise

    return uid


def _append_record(parquet_path: str, record: AudioRecording):
    if os.path.exists(parquet_path):
        df = pd.read_parquet(parquet_path)
    else:
        df = pd.DataFrame()

    if not df.empty and record.id in df['id'].values:
        raise ValueError(f"Duplicate UID: {record.id}")

    new_row = pd.DataFrame([record.to_dict()])
    df = pd.concat([df, new_row], ignore_index=True)
    df.to_parquet(parquet_path, index=False)


def scan_parquet_files(root: str) -> List[str]:
    """Recursively find all .parquet files under root. Returns absolute paths."""
    found = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            if fname.endswith('.parquet'):
                found.append(os.path.join(dirpath, fname))
    return sorted(found)


def load_parquet(path: str) -> pd.DataFrame:
    df = pd.read_parquet(path)
    # Ensure other_params is always a dict (handle JSON string storage)
    if 'other_params' in df.columns:
        df['other_params'] = df['other_params'].apply(
            lambda x: json.loads(x) if isinstance(x, str) else (x if isinstance(x, dict) else {})
        )
    return df


def save_parquet(df: pd.DataFrame, path: str):
    df.to_parquet(path, index=False)


def merge_parquets(paths: List[str]) -> pd.DataFrame:
    """Merge multiple parquet files, warn on duplicate IDs."""
    frames = [load_parquet(p) for p in paths]
    merged = pd.concat(frames, ignore_index=True)
    duplicates = merged[merged.duplicated(subset=['id'], keep=False)]['id'].unique()
    return merged, list(duplicates)
