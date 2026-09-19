import streamlit as st

st.set_page_config(
    page_title='Gunshot Tool',
    page_icon='🎯',
    layout='wide',
)

pages = {
    'Tools': [
        st.Page('pages/detection.py', title='Detection', icon='🎯'),
        st.Page('pages/library.py', title='Library', icon='📚'),
    ],
}

pg = st.navigation(pages)
pg.run()
