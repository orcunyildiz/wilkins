
# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)


from spack import *


class Wilkins(CMakePackage):
    """A workflow system for triple convergence of HPC, Big Data, and AI applications."""

    homepage = "https://github.com/orcunyildiz/wilkins"
    url      = "https://github.com/orcunyildiz/wilkins.git"
    git      = "https://github.com/orcunyildiz/wilkins.git"

    #NB: Use the local copy if there are problems with private GitHub repo authentication.
    #homepage = "/Users/oyildiz/Work/software/wilkins"
    #url      = "/Users/oyildiz/Work/software/wilkins"
    #git      = "/Users/oyildiz/Work/software/wilkins"

    version('master', branch='master')

    depends_on('mpich') #TODO: keeping it mpich for now as L5 does so, but should switch to mpi later
    depends_on('lowfive@master')
    depends_on('hdf5+mpi+hl@1.14 ^mpich', type='link')
    depends_on('henson@master+python+mpi-wrappers')

    extends("python")
    depends_on("py-mpi4py", type=("build", "run"))

    def cmake_args(self):
        args = ['-DCMAKE_C_COMPILER=%s' % self.spec['mpi'].mpicc,
                '-DCMAKE_CXX_COMPILER=%s' % self.spec['mpi'].mpicxx,
                self.define("PYTHON_EXECUTABLE", self.spec["python"].command.path)]

        return args

