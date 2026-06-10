import os
import sys
from datetime import datetime
from pathlib import Path

# Shared configuration
DOCS_DIR = Path(__file__).resolve().parent
REPO_ROOT = DOCS_DIR.parent

sys.path.insert(0, str(REPO_ROOT))

author = "Boomchecker"
current_year = datetime.now().year
copyright = f"{current_year}, {author}"

extensions = [
    "breathe",
    "sphinxcontrib.mermaid",
]

# Projects are built as separate source directories by the GitHub Pages workflow.
# PROJECT selects which small project-specific config file should be merged into
# these shared settings. This avoids sphinx-multiproject, which currently breaks
# under Sphinx 9 during the config-inited event.
multiproject_projects = {
    "root": {},
    "firmware": {},
    "monorepo": {},
    "scripts": {},
}
current_project = os.environ.get("PROJECT", "root")
if current_project not in multiproject_projects:
    known_projects = ", ".join(sorted(multiproject_projects))
    raise RuntimeError(
        f"Unknown PROJECT={current_project!r}. Expected one of: {known_projects}."
    )

# Shared settings
templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"] if (DOCS_DIR / "_static").exists() else []

project_conf = DOCS_DIR / current_project / "conf.py"
if project_conf.exists():
    exec(compile(project_conf.read_text(encoding="utf-8"), str(project_conf), "exec"))

# Doxygen setup for scripts project
if current_project == "scripts":
    breathe_projects = {
        "peak_detector": str(DOCS_DIR / "_doxygen" / "xml"),
    }
    breathe_default_project = "peak_detector"
