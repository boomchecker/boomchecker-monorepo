import sys
import os
import io
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import streamlit as st
import numpy as np
import soundfile as sf
import plotly.graph_objs as go

from core.audio_loader import detect_channels_from_file, load_audio_files, get_audio_info
from core.analysis_methods import (
    perform_amplitude_thresholding, perform_median_filtering,
    perform_zscore_detection, perform_energy_analysis, perform_spectral_analysis,
    filter_impulses,
    AMPLITUDE_TRESHOLD, MEDIAN_FILTER_TRESHOLD, ZSCORE_TRESHOLD,
    ENERGY_THRESHOLD, STFT_TRESHOLD, MEDIAN_WINDOW_SIZE, ENERGY_WINDOW_SIZE,
    BASE_WINDOW_SAMPLE_SIZE, BASE_WINDOW_IMPLUSE_POSITION,
)
from core.library_io import save_peak
from core.event_types import EVENT_TYPES, METADATA_FIELDS, OPTIONAL_FIELD_KEYS
from core.ui_helpers import path_input, files_input

METHODS = [
    'Amplitude Thresholding',
    'Median Filtering',
    'Z-Score',
    'Energy Analysis',
    'Spectral Analysis',
]


def _init():
    defaults = {
        'det_channels': None,
        'det_channel_files': [],
        'det_sr': None,
        'det_impulses': None,
        'det_phase': 'setup',   # 'setup' | 'review'
        'det_peak_idx': 0,
        'det_decisions': {},
        'det_metadata': {},
        'det_window_size': BASE_WINDOW_SAMPLE_SIZE,
        'det_pre_pct': BASE_WINDOW_IMPLUSE_POSITION,
        'det_ch_idx': 0,
    }
    for k, v in defaults.items():
        if k not in st.session_state:
            st.session_state[k] = v


def _render_event_fields(event_type: str, key_prefix: str, current: dict):
    """Render type-specific metadata fields. Returns dict of {field_key: value}."""
    fields = METADATA_FIELDS.get(event_type, [])
    values = {}
    n_cols = min(len(fields), 3)
    cols = st.columns(n_cols) if n_cols > 0 else []
    for i, (key, label, default) in enumerate(fields):
        col = cols[i % n_cols]
        with col:
            if isinstance(default, list):
                cur = current.get(key, default[0])
                idx = default.index(cur) if cur in default else 0
                values[key] = st.selectbox(label, default, index=idx, key=f'{key_prefix}_{key}')
            else:
                values[key] = st.text_input(label, value=current.get(key, default), key=f'{key_prefix}_{key}')
    return values


def _render_field(key, label, default, current_meta, key_prefix):
    """Render a single metadata field (text input or selectbox)."""
    if isinstance(default, list):
        cur = current_meta.get(key, default[0])
        idx_v = default.index(cur) if cur in default else 0
        st.selectbox(label, default, index=idx_v, key=f'{key_prefix}_{key}')
    else:
        st.text_input(label, value=current_meta.get(key, default), key=f'{key_prefix}_{key}')


def _channel_preview_chart(channels, sr, highlight_ch=None):
    fig = go.Figure()
    for i, ch in enumerate(channels):
        if len(ch) > 50000:
            step = len(ch) // 50000
            x = np.arange(0, len(ch), step) / sr
            y = ch[::step]
        else:
            x = np.arange(len(ch)) / sr
            y = ch
        fig.add_trace(go.Scatter(
            x=x, y=y, name=f'ch{i + 1}', mode='lines',
            line=dict(width=2 if i == highlight_ch else 1),
        ))
    fig.update_layout(
        xaxis_title='Time [s]', yaxis_title='Amplitude',
        height=300, margin=dict(t=10, b=40, l=40, r=10),
        legend=dict(orientation='h', yanchor='bottom', y=1.02),
    )
    return fig


def _detection_overview_chart(channels, sr, impulses, det_ch_idx):
    """Full-recording chart with × markers at each detected peak."""
    fig = go.Figure()
    for i, ch in enumerate(channels):
        if len(ch) > 50000:
            step = len(ch) // 50000
            x = np.arange(0, len(ch), step) / sr
            y = ch[::step]
        else:
            x = np.arange(len(ch)) / sr
            y = ch
        is_det = i == det_ch_idx
        fig.add_trace(go.Scatter(
            x=x, y=y, name=f'ch{i + 1}', mode='lines',
            line=dict(width=2 if is_det else 1),
            opacity=1.0 if is_det else 0.35,
        ))

    if len(impulses) > 0:
        det_ch = channels[det_ch_idx]
        peak_times = impulses / sr
        peak_amps = np.array([det_ch[min(int(p), len(det_ch) - 1)] for p in impulses])
        fig.add_trace(go.Scatter(
            x=peak_times, y=peak_amps,
            mode='markers',
            marker=dict(symbol='x', size=10, color='red', line=dict(width=2)),
            name=f'peaks ({len(impulses)})',
        ))

    fig.update_layout(
        xaxis_title='Time [s]', yaxis_title='Amplitude',
        height=380, margin=dict(t=10, b=40, l=40, r=10),
        legend=dict(orientation='h', yanchor='bottom', y=1.02),
    )
    return fig


def _peak_chart(channels, sr, peak_sample, window_size, pre_pct, det_ch_idx):
    pre = int(window_size * pre_pct / 100)
    post = window_size - pre
    start = max(0, peak_sample - pre)
    end = min(len(channels[0]), peak_sample + post)
    x = np.arange(start, end) / sr

    fig = go.Figure()
    for i, ch in enumerate(channels):
        fig.add_trace(go.Scatter(
            x=x, y=ch[start:end], name=f'ch{i + 1}', mode='lines',
            line=dict(width=2 if i == det_ch_idx else 1),
        ))

    fig.add_vline(x=peak_sample / sr, line_color='red', line_width=2,
                  annotation_text='peak', annotation_position='top right')
    fig.add_vrect(x0=start / sr, x1=end / sr, fillcolor='gray', opacity=0.08, line_width=0)

    fig.update_layout(
        xaxis_title='Time [s]', yaxis_title='Amplitude',
        height=350, margin=dict(t=10, b=40, l=40, r=10),
        legend=dict(orientation='h', yanchor='bottom', y=1.02),
    )
    return fig


def _get_peak_audio_bytes(channels, sr, peak_sample, window_size, pre_pct, ch_idx) -> io.BytesIO:
    """Crop a single channel around the peak and return it as WAV bytes."""
    pre = int(window_size * pre_pct / 100)
    post = window_size - pre
    start = max(0, peak_sample - pre)
    end = min(len(channels[0]), peak_sample + post)
    audio = channels[ch_idx][start:end]
    buf = io.BytesIO()
    sf.write(buf, audio, sr, format='WAV', subtype='PCM_16')
    buf.seek(0)
    return buf


def _get_cropped_wav_bytes(channels, sr, peak_sample, window_size, pre_pct, ch_indices=None) -> io.BytesIO:
    """Crop selected channels synchronously around the peak and return as WAV bytes."""
    if ch_indices is None:
        ch_indices = list(range(len(channels)))
    pre = int(window_size * pre_pct / 100)
    post = window_size - pre
    start = max(0, peak_sample - pre)
    end = min(len(channels[0]), peak_sample + post)
    selected = [channels[i][start:end] for i in ch_indices]
    data = selected[0] if len(selected) == 1 else np.stack(selected, axis=1)
    buf = io.BytesIO()
    sf.write(buf, data, sr, format='WAV', subtype='PCM_16')
    buf.seek(0)
    return buf


def _get_cropped_zip_bytes(channels, sr, peak_sample, window_size, pre_pct, stem) -> io.BytesIO:
    """Return a ZIP containing one WAV per channel, each cropped around the peak."""
    import zipfile
    pre = int(window_size * pre_pct / 100)
    post = window_size - pre
    start = max(0, peak_sample - pre)
    end = min(len(channels[0]), peak_sample + post)
    zip_buf = io.BytesIO()
    with zipfile.ZipFile(zip_buf, 'w', zipfile.ZIP_DEFLATED) as zf:
        for i, ch in enumerate(channels):
            wav_buf = io.BytesIO()
            sf.write(wav_buf, ch[start:end], sr, format='WAV', subtype='PCM_16')
            zf.writestr(f'{stem}_ch{i + 1}.wav', wav_buf.getvalue())
    zip_buf.seek(0)
    return zip_buf


_TICK_STEPS = [0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.025, 0.05,
               0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0]

def _nice_tick(target: float) -> float:
    for v in _TICK_STEPS:
        if v >= target:
            return v
    return _TICK_STEPS[-1]


def _make_publication_plot(channels, sr, peak_sample, window_size, pre_pct, det_ch_idx, meta, ch_indices=None) -> io.BytesIO:
    """Render a publication-quality matplotlib waveform plot (300 DPI PNG)."""
    if ch_indices is None:
        ch_indices = list(range(len(channels)))
    pre = int(window_size * pre_pct / 100)
    post = window_size - pre
    start = max(0, peak_sample - pre)
    end = min(len(channels[0]), peak_sample + post)
    duration = (end - start) / sr
    t = np.linspace(0.0, duration, end - start)

    plt.rcParams.update({'font.size': 30})
    ch_colors = ['#1f77b4', '#d62728', '#2ca02c', '#ff7f0e', '#9467bd', '#8c564b']

    fig, ax = plt.subplots(figsize=(20, 10))

    for i in ch_indices:
        audio = channels[i][start:end]
        max_a = np.max(np.abs(audio))
        norm = audio / max_a if max_a > 0 else audio
        is_det = (i == det_ch_idx)
        ax.plot(t, norm,
                color=ch_colors[i % len(ch_colors)],
                alpha=0.7 if is_det else 0.35,
                linewidth=3 if is_det else 1.5,
                label=f'ch{i + 1}' + (' (det.)' if is_det else ''))

    ax.set_xlabel('Time (s)', fontsize=35, fontweight='bold')
    ax.set_ylabel('Normalized Amplitude (-)', fontsize=35, fontweight='bold')

    ax.tick_params(axis='both', which='major', labelsize=30)
    ax.tick_params(axis='x', which='major', pad=15)

    ax.set_ylim(-1.05, 1.05)
    ax.yaxis.set_major_locator(mticker.MultipleLocator(0.5))
    ax.yaxis.set_minor_locator(mticker.MultipleLocator(0.25))
    ax.yaxis.set_major_formatter(mticker.FormatStrFormatter('%.1f'))

    ax.set_xlim(0.0, duration)
    x_major = _nice_tick(duration / 5)
    ax.xaxis.set_major_locator(mticker.MultipleLocator(x_major))
    ax.xaxis.set_minor_locator(mticker.MultipleLocator(x_major / 2))
    ax.xaxis.set_major_formatter(mticker.FormatStrFormatter('%.3f'))

    ax.grid(True, which='major', alpha=0.4)
    ax.grid(True, which='minor', linestyle='-', linewidth=0.5, alpha=0.35)

    if len(channels) > 1:
        ax.legend(fontsize=25, loc='upper right')

    plt.tight_layout()
    buf = io.BytesIO()
    fig.savefig(buf, format='png', dpi=300, bbox_inches='tight')
    plt.close(fig)
    buf.seek(0)
    return buf


def _run_detection(audio, sr, method, params):
    if method == 'Amplitude Thresholding':
        impulses = perform_amplitude_thresholding(audio, sr,
            threshold=params['amp_thresh'],
            edge=params['amp_edge'])
    elif method == 'Median Filtering':
        impulses = perform_median_filtering(audio, sr,
            median_threshold=params['med_thresh'],
            median_window_size=params['med_win'])
    elif method == 'Z-Score':
        impulses = perform_zscore_detection(audio, sr, zscore_threshold=params['zscore_thresh'])
    elif method == 'Energy Analysis':
        impulses = perform_energy_analysis(audio, sr,
            energy_threshold=params['energy_thresh'],
            energy_window_size=params['energy_win'])
    elif method == 'Spectral Analysis':
        impulses = perform_spectral_analysis(audio, sr, spectrum_threshold=params['spec_thresh'])
    else:
        return np.array([])

    # Apply global minimum gap between detections (0 = disabled)
    min_gap = params.get('min_gap_samples', 0)
    if min_gap > 0 and len(impulses) > 0:
        impulses = filter_impulses(impulses, min_gap)
    return impulses


# ── SETUP MODE ─────────────────────────────────────────────────────────────────

def _setup_mode():
    st.title('Acoustic Event Detection')

    # ── 1. Event type — most important, at the top ────────────────────────────
    event_type = st.radio(
        '**Event type**',
        options=list(EVENT_TYPES.keys()),
        format_func=lambda x: EVENT_TYPES[x],
        horizontal=True,
        key='s_event_type',
    )

    # ── 2. Recording info + Event parameters ─────────────────────────────────
    fields = METADATA_FIELDS.get(event_type, [])
    optional_keys = OPTIONAL_FIELD_KEYS.get(event_type, set())
    current_meta = {
        k: st.session_state.get(f's_param_{k}', d[0] if isinstance(d, list) else d)
        for k, _, d in fields
    }
    required_fields = [(k, l, d) for k, l, d in fields if k not in optional_keys]
    optional_fields = [(k, l, d) for k, l, d in fields if k in optional_keys]

    col_l, col_r = st.columns(2)
    with col_l:
        with st.container(border=True):
            st.markdown('**Recording**')
            st.text_input('Date', value='240213', key='s_date', placeholder='YYMMDD')
            st.text_input('Location', key='s_location', placeholder='e.g. Boletice')
            path_input('Output folder', 's_output_root', mode='folder')

    with col_r:
        with st.container(border=True):
            st.markdown('**Recording parameters**')
            for key, label, default in required_fields:
                _render_field(key, label, default, current_meta, 's_param')
            if optional_fields:
                with st.expander('Optional parameters'):
                    opt_cols = st.columns(min(len(optional_fields), 2))
                    for i, (key, label, default) in enumerate(optional_fields):
                        with opt_cols[i % 2]:
                            _render_field(key, label, default, current_meta, 's_param')

    st.text_input(
        'Recording label',
        value='unknown',
        key='s_label',
        placeholder='short description, used in filenames',
        help='Part of the folder path and WAV filenames — no spaces',
    )

    st.divider()

    # ── 3. Audio input ────────────────────────────────────────────────────────
    st.subheader('Audio input')
    selected_files, load_clicked = files_input(
        'WAV files — select one or more channels',
        's_wav_files',
        file_types=[('WAV files', '*.wav')],
        action_label='Load audio',
        action_type='primary',
    )

    if selected_files:
        if len(selected_files) == 1:
            try:
                info = get_audio_info(selected_files[0])
                if info['channels'] > 1:
                    st.caption(f'Multi-channel WAV → {info["channels"]} channels, sr={info["samplerate"]} Hz, duration: {info["duration"]:.1f} s')
                else:
                    detected, pattern = detect_channels_from_file(selected_files[0])
                    if len(detected) > 1:
                        st.caption(f'Detected pattern `{pattern}` → {len(detected)} channels')
                    else:
                        st.caption('Mono WAV → 1 channel')
            except Exception:
                pass
        else:
            st.caption(f'{len(selected_files)} files → {len(selected_files)} channels')

    if load_clicked and selected_files:
        try:
            ch, sr_val, paths, load_warnings = load_audio_files(selected_files)
            st.session_state.det_channels = ch
            st.session_state.det_sr = sr_val
            st.session_state.det_channel_files = paths
            st.success(f'Loaded {len(ch)} channels, sr={sr_val} Hz, duration: {len(ch[0]) / sr_val:.1f} s')
            for warning in load_warnings:
                st.warning(warning)
        except Exception as e:
            st.error(f'Error: {e}')

    if st.session_state.det_channels is None:
        return

    channels = st.session_state.det_channels
    sr = st.session_state.det_sr
    n_ch = len(channels)

    st.divider()

    # ── 4. Channel selection + preview ────────────────────────────────────────
    col_ch, col_info = st.columns([2, 3])
    with col_ch:
        det_ch = st.radio(
            '**Detect using channel**',
            options=list(range(n_ch)),
            format_func=lambda x: f'ch{x + 1}',
            horizontal=True,
            key='s_det_ch',
        )
    with col_info:
        st.caption(f'{n_ch} channels · duration {len(channels[0]) / sr:.1f} s · {sr} Hz')

    st.plotly_chart(_channel_preview_chart(channels, sr, highlight_ch=det_ch), use_container_width=True)

    st.divider()

    # ── 5. Detection parameters ───────────────────────────────────────────────
    st.subheader('Detection parameters')

    col_m, col_w, col_p = st.columns(3)
    with col_m:
        method = st.selectbox('Method', METHODS, key='s_method')
    with col_w:
        window_size = st.number_input('Window [samples]', min_value=100, value=BASE_WINDOW_SAMPLE_SIZE, step=1000, key='s_window')
    with col_p:
        pre_pct = st.slider('Pre-peak [%]', 1, 50, BASE_WINDOW_IMPLUSE_POSITION, key='s_pre_pct')

    col_gap, _ = st.columns([1, 2])
    with col_gap:
        min_gap = st.number_input(
            'Min. time between detections [s]',
            min_value=0.0, value=0.0, step=0.05, format='%.2f',
            key='s_min_gap',
            help='Detections closer than this are suppressed — applies to all methods (0 = disabled)',
        )

    with st.expander('Method parameters'):
        if method == 'Amplitude Thresholding':
            st.number_input('Amplitude threshold (0–1, normalized)', value=AMPLITUDE_TRESHOLD,
                            min_value=0.01, max_value=1.0, step=0.01, format='%.2f', key='p_amp')
            st.radio('Edge', ['rising', 'falling', 'both'], horizontal=True, key='p_amp_edge')
        elif method == 'Median Filtering':
            st.number_input('Median threshold', value=MEDIAN_FILTER_TRESHOLD, step=0.01, format='%.3f', key='p_med_t')
            st.number_input('Filter window size (odd)', min_value=3, value=MEDIAN_WINDOW_SIZE, step=2, key='p_med_w')
        elif method == 'Z-Score':
            st.number_input('Z-score threshold', value=float(ZSCORE_TRESHOLD), step=1.0, key='p_zscore')
        elif method == 'Energy Analysis':
            st.number_input('Energy threshold', value=float(ENERGY_THRESHOLD), step=1.0, key='p_e_t')
            st.number_input('Energy window size', min_value=10, value=ENERGY_WINDOW_SIZE, step=10, key='p_e_w')
        elif method == 'Spectral Analysis':
            st.number_input('Spectrum threshold', value=float(STFT_TRESHOLD), step=1.0, key='p_spec')

    if st.button('Detect', type='primary', use_container_width=True):
        audio = channels[det_ch]
        params = {
            'amp_thresh':    st.session_state.get('p_amp',      AMPLITUDE_TRESHOLD),
            'amp_edge':      st.session_state.get('p_amp_edge', 'rising'),
            'med_thresh':    st.session_state.get('p_med_t',    MEDIAN_FILTER_TRESHOLD),
            'med_win':       st.session_state.get('p_med_w',    MEDIAN_WINDOW_SIZE),
            'zscore_thresh': st.session_state.get('p_zscore',   ZSCORE_TRESHOLD),
            'energy_thresh': st.session_state.get('p_e_t',      ENERGY_THRESHOLD),
            'energy_win':    st.session_state.get('p_e_w',      ENERGY_WINDOW_SIZE),
            'spec_thresh':   st.session_state.get('p_spec',     STFT_TRESHOLD),
            'min_gap_samples': int(st.session_state.get('s_min_gap', 0.0) * sr),
        }
        event_fields = {k: st.session_state.get(f's_param_{k}', '') for k, _, _ in fields}
        try:
            with st.spinner('Detecting peaks...'):
                impulses = _run_detection(audio, sr, method, params)
        except ValueError as e:
            st.error(str(e))
            impulses = None

        if impulses is not None:
            st.session_state.det_impulses = impulses
            st.session_state.det_ch_idx = det_ch
            st.session_state.det_window_size = int(window_size)
            st.session_state.det_pre_pct = pre_pct
            st.session_state.det_metadata = {
                'event_type':  event_type,
                'date':        st.session_state.s_date,
                'location':    st.session_state.s_location,
                'label':       st.session_state.s_label,
                'output_root': st.session_state.s_output_root,
                **event_fields,
            }
            st.session_state.det_peak_idx = 0
            st.session_state.det_decisions = {}

    if st.session_state.det_impulses is not None:
        st.divider()
        _detection_results_preview()


def _detection_results_preview():
    """Shown right below the Detect button — re-running Detect with new
    parameters just refreshes this in place, no page navigation needed."""
    impulses = st.session_state.det_impulses
    channels = st.session_state.det_channels
    sr = st.session_state.det_sr
    det_ch = st.session_state.det_ch_idx
    total = len(impulses)
    decisions = st.session_state.det_decisions
    saved = sum(1 for v in decisions.values() if v == 'saved')
    skipped = sum(1 for v in decisions.values() if v == 'skipped')

    peak_s = 's' if total != 1 else ''
    st.subheader(f'{total} peak{peak_s} detected on ch{det_ch + 1}')

    st.plotly_chart(
        _detection_overview_chart(channels, sr, impulses, det_ch),
        use_container_width=True,
    )

    if total == 0:
        st.warning('No peaks found. Adjust detection parameters above and click Detect again.')
        return

    if decisions:
        remaining = total - saved - skipped
        st.caption(f'Progress: {saved} saved · {skipped} skipped · {remaining} remaining')

    col_btn, _ = st.columns([1, 2])
    with col_btn:
        label = 'Resume review →' if decisions else 'Review peaks →'
        if st.button(label, type='primary', use_container_width=True):
            st.session_state.det_phase = 'review'
            st.rerun()


# ── REVIEW MODE ────────────────────────────────────────────────────────────────

def _review_mode():
    impulses = st.session_state.det_impulses
    total = len(impulses)
    idx = st.session_state.det_peak_idx
    decisions = st.session_state.det_decisions
    saved = sum(1 for v in decisions.values() if v == 'saved')
    skipped = sum(1 for v in decisions.values() if v == 'skipped')

    channels = st.session_state.det_channels
    sr = st.session_state.det_sr
    window_size = st.session_state.det_window_size
    pre_pct = st.session_state.det_pre_pct
    det_ch = st.session_state.det_ch_idx
    meta = st.session_state.det_metadata
    event_type = meta.get('event_type', 'gunshot')

    hc1, hc2 = st.columns([3, 1])
    with hc1:
        st.title(f'{EVENT_TYPES.get(event_type, event_type)} — {saved} saved / {skipped} skipped')
    with hc2:
        if st.button('← Overview', type='secondary'):
            st.session_state.det_phase = 'setup'
            st.rerun()

    if idx >= total:
        st.success(f'All peaks processed. Saved: {saved}, Skipped: {skipped}')
        col_a, col_b, _ = st.columns([1, 1, 2])
        with col_a:
            if st.button('← Overview', use_container_width=True):
                st.session_state.det_phase = 'setup'
                st.rerun()
        with col_b:
            if st.button('New session', type='primary', use_container_width=True):
                st.session_state.det_phase = 'setup'
                st.session_state.det_impulses = None
                st.session_state.det_decisions = {}
                st.session_state.det_channels = None
                st.rerun()
        return

    peak_sample = int(impulses[idx])
    decision = decisions.get(idx)

    st.subheader(f'Peak {idx + 1} / {total}  —  time: {peak_sample / sr:.3f} s  —  sample: {peak_sample}')

    if decision:
        color = 'green' if decision == 'saved' else 'orange'
        st.markdown(f'<span style="color:{color};font-weight:bold">● {decision.upper()}</span>', unsafe_allow_html=True)

    with st.expander('Metadata (click to edit)'):
        cc1, cc2, cc3 = st.columns(3)
        with cc1:
            meta['date'] = st.text_input('Date', value=meta.get('date', ''), key=f'r_date_{idx}')
            meta['location'] = st.text_input('Location', value=meta.get('location', ''), key=f'r_loc_{idx}')
        with cc2:
            meta['label'] = st.text_input('Label', value=meta.get('label', ''), key=f'r_label_{idx}')
            meta['output_root'] = path_input('Output folder', f'r_out_{idx}', mode='folder', default=meta.get('output_root', ''))
        with cc3:
            new_type = st.selectbox(
                'Event type', list(EVENT_TYPES.keys()),
                index=list(EVENT_TYPES.keys()).index(event_type),
                format_func=lambda x: EVENT_TYPES[x],
                key=f'r_etype_{idx}',
            )
            meta['event_type'] = new_type

        updated_fields = _render_event_fields(new_type, f'r_param_{idx}', meta)
        meta.update(updated_fields)

    st.plotly_chart(_peak_chart(channels, sr, peak_sample, window_size, pre_pct, det_ch), use_container_width=True)

    # Audio playback
    n_ch = len(channels)
    if n_ch > 1:
        play_ch = st.radio(
            'Play channel', list(range(n_ch)),
            format_func=lambda x: f'ch{x + 1}',
            index=det_ch, horizontal=True,
            key=f'r_play_ch_{idx}',
        )
    else:
        play_ch = 0
    st.audio(_get_peak_audio_bytes(channels, sr, peak_sample, window_size, pre_pct, play_ch), format='audio/wav')

    col_actions, col_dl = st.columns([2, 1])
    with col_actions:
        bc1, bc2, bc3 = st.columns(3)
        with bc1:
            if st.button('← Back', disabled=(idx == 0), use_container_width=True):
                st.session_state.det_peak_idx -= 1
                st.rerun()
        with bc2:
            if decision:
                if st.button('Next →', use_container_width=True):
                    st.session_state.det_peak_idx += 1
                    st.rerun()
            else:
                if st.button('Skip', type='secondary', use_container_width=True):
                    st.session_state.det_decisions[idx] = 'skipped'
                    st.session_state.det_peak_idx += 1
                    st.rerun()
        with bc3:
            if not decision:
                if st.button('Save', type='primary', use_container_width=True):
                    output_root = meta.get('output_root', '')
                    if not output_root:
                        st.error('Set output folder in metadata.')
                    else:
                        try:
                            save_peak(
                                channels=channels,
                                sr=sr,
                                channel_files=st.session_state.det_channel_files,
                                peak_sample=peak_sample,
                                window_size=window_size,
                                pre_peak_pct=pre_pct,
                                metadata=meta,
                                output_root=output_root,
                            )
                            st.session_state.det_decisions[idx] = 'saved'
                            st.session_state.det_peak_idx += 1
                            st.rerun()
                        except Exception as e:
                            st.error(f'Error saving: {e}')

    with col_dl:
        stem = f'{meta.get("event_type", "event")}_{meta.get("label", "peak")}_{idx + 1}'
        with st.popover('Download', use_container_width=True):
            st.caption('PNG')
            st.download_button(
                'PNG — ch1 only',
                data=_make_publication_plot(channels, sr, peak_sample, window_size, pre_pct, det_ch, meta, ch_indices=[0]),
                file_name=f'{stem}_ch1.png',
                mime='image/png',
                use_container_width=True,
                key=f'dl_png1_{idx}',
            )
            st.download_button(
                'PNG — all channels',
                data=_make_publication_plot(channels, sr, peak_sample, window_size, pre_pct, det_ch, meta),
                file_name=f'{stem}_all_ch.png',
                mime='image/png',
                use_container_width=True,
                key=f'dl_pngall_{idx}',
            )
            st.caption('WAV')
            st.download_button(
                'WAV — ch1 only',
                data=_get_cropped_wav_bytes(channels, sr, peak_sample, window_size, pre_pct, ch_indices=[0]),
                file_name=f'{stem}_ch1.wav',
                mime='audio/wav',
                use_container_width=True,
                key=f'dl_wav1_{idx}',
            )
            st.download_button(
                'WAV — all channels (ZIP)',
                data=_get_cropped_zip_bytes(channels, sr, peak_sample, window_size, pre_pct, stem),
                file_name=f'{stem}_all_ch.zip',
                mime='application/zip',
                use_container_width=True,
                key=f'dl_wavall_{idx}',
            )


# ── ENTRY POINT ────────────────────────────────────────────────────────────────

_init()

phase = st.session_state.det_phase
if phase == 'review':
    _review_mode()
else:
    _setup_mode()
