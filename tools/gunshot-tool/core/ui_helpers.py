import os
import streamlit as st
from typing import List, Tuple


def _tk_root():
    import tkinter as tk
    root = tk.Tk()
    root.withdraw()
    root.wm_attributes('-topmost', 1)
    return root


def _open_dialog(mode: str, file_types=None) -> str:
    from tkinter import filedialog
    root = _tk_root()
    if mode == 'folder':
        result = filedialog.askdirectory(parent=root)
    else:
        types = file_types or [('WAV files', '*.wav'), ('All files', '*.*')]
        result = filedialog.askopenfilename(parent=root, filetypes=types)
    root.destroy()
    return result or ''


def _open_files_dialog(file_types=None) -> List[str]:
    from tkinter import filedialog
    root = _tk_root()
    types = file_types or [('WAV files', '*.wav'), ('All files', '*.*')]
    result = filedialog.askopenfilenames(parent=root, filetypes=types)
    root.destroy()
    return list(result)


def path_input(label: str, key: str, mode: str = 'folder', file_types=None, default: str = '') -> str:
    """Text input with an inline Browse button. mode='folder' or 'file'."""
    pending_key = f'_pending_{key}'
    if pending_key in st.session_state:
        st.session_state[key] = st.session_state[pending_key]
        del st.session_state[pending_key]
    elif key not in st.session_state:
        st.session_state[key] = default

    st.markdown(
        '<style>button[kind="secondary"]{white-space:nowrap}</style>',
        unsafe_allow_html=True,
    )
    col1, col2 = st.columns([5, 1], vertical_alignment='bottom')
    with col1:
        val = st.text_input(label, key=key)
    with col2:
        browse_clicked = st.button('Browse', key=f'_browse_{key}')

    if browse_clicked:
        selected = _open_dialog(mode, file_types)
        if selected:
            st.session_state[pending_key] = selected
            st.rerun()

    return val


def files_input(
    label: str,
    key: str,
    file_types=None,
    action_label: str = None,
    action_type: str = 'secondary',
) -> Tuple[List[str], bool]:
    """
    Multi-file picker with a Browse button.
    If action_label is given, a second button appears next to Browse (only when files are selected).
    Returns (files, action_clicked).
    """
    pending_key = f'_pending_{key}'
    if pending_key in st.session_state:
        st.session_state[key] = st.session_state[pending_key]
        del st.session_state[pending_key]
    elif key not in st.session_state:
        st.session_state[key] = []

    files: List[str] = st.session_state.get(key, [])

    st.markdown(
        '<style>button[kind="secondary"]{white-space:nowrap}</style>',
        unsafe_allow_html=True,
    )

    n = len(files)
    if n:
        plural = 's' if n != 1 else ''
        st.markdown(f'**{label}** — {n} file{plural}')
        for f in files:
            try:
                size_str = f'{os.path.getsize(f) / 1024:.0f} kB'
            except OSError:
                size_str = ''
            st.caption(f'`{os.path.basename(f)}`  {size_str}')
    else:
        st.markdown(f'**{label}**')
        st.caption('No files selected — click Browse')

    # Browse and optional action button on the same row
    if action_label and files:
        bc1, bc2 = st.columns(2)
        with bc1:
            browse_clicked = st.button('Browse', key=f'_browse_{key}', use_container_width=True)
        with bc2:
            action_clicked = st.button(action_label, type=action_type, key=f'_action_{key}', use_container_width=True)
    else:
        browse_clicked = st.button('Browse', key=f'_browse_{key}')
        action_clicked = False

    if browse_clicked:
        selected = _open_files_dialog(file_types)
        if selected:
            st.session_state[pending_key] = selected
            st.rerun()

    return files, action_clicked
