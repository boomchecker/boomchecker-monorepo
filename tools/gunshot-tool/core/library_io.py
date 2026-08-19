import os
import random
import re
import string
import time
import json
import numpy as np
import pandas as pd
from scipy.io.wavfile import write as wav_write
from typing import List, Tuple

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


class _file_lock:
    """
    Exclusive cross-process lock for one parquet file. Without it two sessions
    appending at the same time both read before either writes, and the first
    session's record is silently overwritten.
    """

    def __init__(self, target_path: str, timeout: float = 10.0, stale_after: float = 60.0):
        self.lock_path = f'{target_path}.lock'
        self.timeout = timeout
        self.stale_after = stale_after

    def _is_stale(self) -> bool:
        try:
            return time.time() - os.path.getmtime(self.lock_path) > self.stale_after
        except OSError:
            return False

    def __enter__(self):
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                fd = os.open(self.lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(fd, str(os.getpid()).encode())
                os.close(fd)
                return self
            except FileExistsError:
                if self._is_stale():
                    # Left behind by a crashed session
                    try:
                        os.remove(self.lock_path)
                    except OSError:
                        pass
                    continue
                if time.monotonic() >= deadline:
                    raise TimeoutError(
                        f'Another session is writing {os.path.basename(self.lock_path)[:-5]} — try again.'
                    )
                time.sleep(0.05)

    def __exit__(self, *exc_info):
        try:
            os.remove(self.lock_path)
        except OSError:
            pass
        return False


def _serialize_other_params(df: pd.DataFrame) -> pd.DataFrame:
    """
    Store other_params as JSON. Written as dicts, pyarrow infers one struct schema for
    the whole column and back-fills None for every key a record does not have, which
    corrupts metadata as soon as records of different event types share a file.
    """
    if 'other_params' not in df.columns:
        return df
    df = df.copy()
    df['other_params'] = df['other_params'].apply(
        lambda x: x if isinstance(x, str) else json.dumps(x if isinstance(x, dict) else {}, default=str)
    )
    return df


def _write_parquet_atomic(df: pd.DataFrame, path: str):
    """Write via a temp file so an interrupted write cannot truncate the library index."""
    tmp_path = f'{path}.tmp-{os.getpid()}'
    try:
        _serialize_other_params(df).to_parquet(tmp_path, index=False)
        os.replace(tmp_path, path)
    except Exception:
        try:
            os.remove(tmp_path)
        except OSError:
            pass
        raise


def _append_record(parquet_path: str, record: AudioRecording):
    with _file_lock(parquet_path):
        if os.path.exists(parquet_path):
            df = load_parquet(parquet_path)
        else:
            df = pd.DataFrame()

        if not df.empty and record.id in df['id'].values:
            raise ValueError(f"Duplicate UID: {record.id}")

        new_row = pd.DataFrame([record.to_dict()])
        df = pd.concat([df, new_row], ignore_index=True)
        _write_parquet_atomic(df, parquet_path)


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
    with _file_lock(path):
        _write_parquet_atomic(df, path)


def merge_parquets(paths: List[str]) -> Tuple[pd.DataFrame, List[str], List[str]]:
    """
    Merge multiple parquet files.
    Returns (merged, duplicate_ids, event_types) — event_types lets the caller flag a
    merge that mixes record schemas.
    """
    frames = [load_parquet(p) for p in paths]
    merged = pd.concat(frames, ignore_index=True)
    duplicates = merged[merged.duplicated(subset=['id'], keep=False)]['id'].unique()

    event_types = {
        params.get('event_type') for params in merged.get('other_params', [])
        if isinstance(params, dict) and params.get('event_type')
    }
    return merged, list(duplicates), sorted(event_types)
