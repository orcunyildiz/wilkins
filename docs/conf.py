# Sphinx configuration for Wilkins documentation

project = "Wilkins"
copyright = "2024, Orcun Yildiz, Tom Peterka"
author = "Orcun Yildiz, Tom Peterka"
release = "1.0.0"

# -- General configuration ---------------------------------------------------

extensions = [
    "myst_parser",
]

# MyST-Parser settings
myst_enable_extensions = [
    "colon_fence",
    "deflist",
]
myst_heading_anchors = 3

# File suffixes to treat as source
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

# -- Options for HTML output -------------------------------------------------

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]

html_theme_options = {
    "navigation_depth": 3,
    "collapse_navigation": False,
}
