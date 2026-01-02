import os
import sys
from datetime import datetime
from multiproject.utils import get_project

# Shared configuration
author = "Boomchecker"
current_year = datetime.now().year
copyright = f"{current_year}, {author}"

extensions = [
    "multiproject",
    "breathe",
    "sphinxcontrib.mermaid",
]

# Define projects
multiproject_projects = {
    "root": {
        "use_config_file": True,
    },
    "firmware": {
        "use_config_file": True,
    },
    "monorepo": {
        "use_config_file": True,
    },
    "scripts": {
        "use_config_file": True,
    },
}

current_project = get_project(multiproject_projects)

# Shared settings
templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]

sys.path.insert(0, os.path.abspath(".."))

# Doxygen setup for scripts project
if current_project == "scripts":
    breathe_projects = {
        "peak_detector": os.path.abspath(os.path.join(os.path.dirname(__file__), "_doxygen", "xml")),
    }
    breathe_default_project = "peak_detector"
