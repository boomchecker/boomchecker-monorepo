import os
import sys
from datetime import datetime

project = "Peak Detector"
author = "Boomchecker"
current_year = datetime.now().year
copyright = f"{current_year}, {author}"

extensions = [
    "breathe",
    "sphinxcontrib.mermaid",
]

breathe_projects = {
    "peak_detector": os.path.abspath(os.path.join(os.path.dirname(__file__), "_doxygen", "xml")),
}
breathe_default_project = "peak_detector"

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]

# Keep relative links working when docs are built from repo root
sys.path.insert(0, os.path.abspath(".."))
