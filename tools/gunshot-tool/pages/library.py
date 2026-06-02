import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

import streamlit as st
import pandas as pd
import numpy as np
import soundfile as sf
import plotly.graph_objs as go

from core.library_io import scan_parquet_files, load_parquet, save_parquet, merge_parquets
from core.ui_helpers import path_input


def _init():
    defaults = {
        'lib_root': '',
        'lib_parquet_files': [],
        'lib_selected': None,
        'lib_df': None,
        'lib_df_path': None,
    }
    for k, v in defaults.items():
        if k not in st.session_state:
            st.session_state[k] = v


def _audio_chart(audio_folder: str, filenames: list, record_id: str):
    fig = go.Figure()
    for fname in filenames:
        fpath = os.path.join(audio_folder, fname)
        if not os.path.exists(fpath):
            st.warning(f'File not found: {fpath}')
            continue
        try:
            data, sr = sf.read(fpath)
            if data.ndim > 1:
                data = data[:, 0]
            t = np.linspace(0, len(data) / sr, num=len(data))
            fig.add_trace(go.Scatter(x=t, y=data, mode='lines', name=fname))
        except Exception as e:
            st.error(f'Error reading {fname}: {e}')

    fig.update_layout(
        title=f'Audio for UID: {record_id}',
        xaxis_title='Time [s]', yaxis_title='Amplitude',
        height=300, margin=dict(t=40, b=40, l=40, r=10),
    )
    return fig


def _expand_other_params(df: pd.DataFrame) -> pd.DataFrame:
    """Expand other_params dict into individual columns for editing."""
    if 'other_params' not in df.columns:
        return df
    param_keys = ['guntype', 'caliber', 'distance', 'suppressor', 'angle', 'date', 'window_size', 'impulse_position']
    for key in param_keys:
        df[f'p_{key}'] = df['other_params'].apply(
            lambda x: x.get(key, '') if isinstance(x, dict) else ''
        )
    return df


def _collapse_other_params(df: pd.DataFrame) -> pd.DataFrame:
    """Reconstruct other_params dict from expanded columns."""
    param_keys = ['guntype', 'caliber', 'distance', 'suppressor', 'angle', 'date', 'window_size', 'impulse_position']
    col_names = [f'p_{k}' for k in param_keys]
    existing_cols = [c for c in col_names if c in df.columns]

    def _rebuild(row):
        d = {}
        for key in param_keys:
            col = f'p_{key}'
            if col in row.index:
                d[key] = row[col]
        return d

    df['other_params'] = df.apply(_rebuild, axis=1)
    df = df.drop(columns=[c for c in existing_cols], errors='ignore')
    return df


def _browser_tab(parquet_files):
    st.subheader('Browser')

    if not parquet_files:
        st.info('Scan a library folder first.')
        return

    rel_paths = [os.path.relpath(p, st.session_state.lib_root) for p in parquet_files]
    selected_rel = st.selectbox('Select Parquet file', rel_paths, key='lib_browser_sel')
    selected_abs = os.path.join(st.session_state.lib_root, selected_rel)

    if st.button('Load', key='lib_load_btn'):
        try:
            df = load_parquet(selected_abs)
            st.session_state.lib_df = df
            st.session_state.lib_df_path = selected_abs
            st.success(f'Loaded {len(df)} records.')
        except Exception as e:
            st.error(f'Error: {e}')

    if st.session_state.lib_df is None or st.session_state.lib_df_path != selected_abs:
        return

    df = st.session_state.lib_df
    audio_folder = path_input(
        'Audio files folder (for graph display)',
        'lib_audio_folder', mode='folder',
        default=os.path.dirname(selected_abs),
    )

    for idx, row in df.iterrows():
        fnames = row.get('filenames', [])
        fnames_str = ', '.join(fnames) if isinstance(fnames, list) else str(fnames)
        label = row.get('label', '')
        uid = row.get('id', idx)

        with st.expander(f'{idx + 1}. UID: {uid}  |  Label: {label}'):
            st.write(f'**Files:** {fnames_str}')
            st.write(f'**Sample rate:** {row.get("samplerate", "?")}')
            st.write(f'**Channels:** {row.get("channels", "?")}')
            params = row.get('other_params', {})
            if isinstance(params, dict):
                for k, v in params.items():
                    st.write(f'**{k}:** {v}')

            if audio_folder and st.button('Show plot', key=f'plot_{idx}'):
                fnames_list = fnames if isinstance(fnames, list) else [fnames]
                st.plotly_chart(
                    _audio_chart(audio_folder, fnames_list, uid),
                    use_container_width=True,
                )


def _editor_tab(parquet_files):
    st.subheader('Editor')

    if not parquet_files:
        st.info('Scan a library folder first.')
        return

    rel_paths = [os.path.relpath(p, st.session_state.lib_root) for p in parquet_files]
    selected_rel = st.selectbox('Select Parquet file to edit', rel_paths, key='lib_editor_sel')
    selected_abs = os.path.join(st.session_state.lib_root, selected_rel)

    if st.button('Load for editing', key='lib_edit_load'):
        try:
            df = load_parquet(selected_abs)
            df_expanded = _expand_other_params(df.copy())
            st.session_state['lib_edit_df'] = df_expanded
            st.session_state['lib_edit_path'] = selected_abs
        except Exception as e:
            st.error(f'Error: {e}')

    if 'lib_edit_df' not in st.session_state or st.session_state.get('lib_edit_path') != selected_abs:
        return

    df_edit = st.session_state['lib_edit_df']

    editable_cols = ['label'] + [c for c in df_edit.columns if c.startswith('p_')]
    display_cols = ['id'] + editable_cols

    edited = st.data_editor(
        df_edit[display_cols],
        use_container_width=True,
        num_rows='fixed',
        key='lib_data_editor',
    )

    if st.button('Save changes', type='primary', key='lib_save_edits'):
        try:
            df_updated = df_edit.copy()
            for col in editable_cols:
                if col in edited.columns:
                    df_updated[col] = edited[col].values
            df_final = _collapse_other_params(df_updated)
            save_parquet(df_final, selected_abs)
            st.success('Saved.')
            st.session_state['lib_edit_df'] = _expand_other_params(df_final.copy())
        except Exception as e:
            st.error(f'Error: {e}')


def _merge_tab(parquet_files):
    st.subheader('Merge')

    if not parquet_files:
        st.info('Scan a library folder first.')
        return

    rel_paths = [os.path.relpath(p, st.session_state.lib_root) for p in parquet_files]
    selected_rels = st.multiselect('Select Parquet files to merge', rel_paths, key='lib_merge_sel')

    if len(selected_rels) < 2:
        st.info('Select at least 2 files.')
        return

    output_name = st.text_input('Output filename', value='merged.parquet', key='lib_merge_out')
    output_path = path_input(
        'Output folder', 'lib_merge_dir', mode='folder',
        default=st.session_state.lib_root,
    )

    if st.button('Merge & Save', type='primary', key='lib_merge_btn'):
        selected_abs = [os.path.join(st.session_state.lib_root, r) for r in selected_rels]
        try:
            merged_df, duplicates = merge_parquets(selected_abs)
            if duplicates:
                st.warning(f'Duplicate UIDs ({len(duplicates)}): {", ".join(duplicates[:10])}')
            dest = os.path.join(output_path, output_name)
            save_parquet(merged_df, dest)
            st.success(f'Merged {len(merged_df)} records → {dest}')
        except Exception as e:
            st.error(f'Error: {e}')


# ── ENTRY POINT ────────────────────────────────────────────────────────────────

_init()

st.title('Shot Library')

root = path_input('Library root folder', 'lib_root_input', mode='folder', default=st.session_state.lib_root)

if st.button('Scan', key='lib_scan_btn'):
    if not os.path.isdir(root):
        st.error('Folder does not exist.')
    else:
        files = scan_parquet_files(root)
        st.session_state.lib_root = root
        st.session_state.lib_parquet_files = files
        if files:
            st.success(f'Found {len(files)} Parquet files.')
        else:
            st.warning('No Parquet files found.')

parquet_files = st.session_state.lib_parquet_files

tab_browser, tab_editor, tab_merge = st.tabs(['Browser', 'Editor', 'Merge'])

with tab_browser:
    _browser_tab(parquet_files)

with tab_editor:
    _editor_tab(parquet_files)

with tab_merge:
    _merge_tab(parquet_files)
