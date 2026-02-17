"""Setuptools shim for editable installs (pip install -e .)."""

from setuptools import setup, find_packages

setup(
    name="wilkins",
    version="1.0.0",
    description="Wilkins: an in situ workflow system for heterogeneous task specification and execution",
    packages=find_packages(where="bindings/python"),
    package_dir={"": "bindings/python"},
    python_requires=">=3.8",
    install_requires=[
        "mpi4py",
        "pyyaml",
    ],
    entry_points={
        "console_scripts": [
            "wilkins-master=wilkins.master:main",
        ],
    },
)
