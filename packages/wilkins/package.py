
# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)


import os

from spack.package import *


class Wilkins(CMakePackage):
    """A workflow system for triple convergence of HPC, Big Data, and AI applications.

    Wilkins is an in situ workflow system for heterogeneous task specification
    and execution.  The orchestrator is a pure-Python package installed via pip.
    CMake builds the C++ example task libraries (.hx shared libraries loaded by
    Henson at runtime) and registers the CTest test suite.

    Install with tests:  spack install --test=root wilkins
    """

    homepage = "https://github.com/orcunyildiz/wilkins"
    git      = "https://github.com/orcunyildiz/wilkins.git"

    version('master', branch='master')
    version('refactoring', branch='refactoring-python')

    # Core dependencies
    depends_on('mpi')
    depends_on('lowfive')
    depends_on('hdf5+mpi+hl@1.14', type='link')
    depends_on('henson@master+python+mpi-wrappers')

    # Python dependencies (needed for pip install of the orchestrator)
    depends_on('python@3.8:', type=('build', 'run'))
    depends_on('py-setuptools', type='build')
    depends_on('py-pip', type='build')
    depends_on('py-wheel', type='build')
    depends_on('py-mpi4py', type=('build', 'run'))
    depends_on('py-pyyaml', type=('build', 'run'))
    depends_on('py-h5py', type=('build', 'run'))

    def cmake_args(self):
        return [
            self.define('lowfive', True),
        ]

    def _hdf5_test_env(self):
        """Return dict of HDF5/LowFive env vars needed to run the tests."""
        lowfive_prefix = self.spec['lowfive'].prefix
        return {
            'HDF5_PLUGIN_PATH': join_path(lowfive_prefix, 'lib'),
            'HDF5_VOL_CONNECTOR': 'lowfive under_vol=0;under_info={};',
        }

    @run_after('build')
    def install_python_package(self):
        """Install the pure-Python orchestrator via pip."""
        with working_dir(self.stage.source_path):
            pip = which('pip')
            pip('install', '--prefix={0}'.format(self.prefix),
                '--no-deps', '--no-build-isolation', '.')

    def check(self):
        """Run ctest at build time (invoked by ``spack install --test=root``)."""
        with working_dir(self.build_directory):
            for key, val in self._hdf5_test_env().items():
                os.environ[key] = val

            ctest = which('ctest')
            ctest('--output-on-failure')
