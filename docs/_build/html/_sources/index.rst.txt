Wilkins
=======

**Wilkins** is an in situ workflow system that enables heterogeneous task
specification and execution for in situ data processing. It provides a
data-centric API for defining workflow graphs, creating and launching tasks,
and establishing communicators between them.

Wilkins uses `LowFive <https://github.com/diatomic/LowFive>`_ as its data
transport layer, which is based on the `HDF5 <https://www.hdfgroup.org/solutions/hdf5/>`_
data model. Coupled tasks can communicate both in situ using in-memory data
and MPI message passing, and through traditional HDF5 files — with minimal or
no source-code modification for programs that already use HDF5.

Wilkins supports any directed-graph topology of tasks, including pipeline,
fan-in, fan-out, ensembles, and cycles.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   getting-started/installation
   getting-started/quickstart
   getting-started/environment

.. toctree::
   :maxdepth: 2
   :caption: Concepts

   concepts/architecture
   concepts/workflow-graph
   concepts/execution-model
   concepts/data-transport
   concepts/flow-control

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   user-guide/yaml-reference
   user-guide/python-tasks
   user-guide/cpp-tasks
   user-guide/topologies
   user-guide/custom-actions
   user-guide/running

.. toctree::
   :maxdepth: 2
   :caption: Reference

   reference/python-api
   reference/citation
   reference/faq
