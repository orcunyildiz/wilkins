#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
namespace py = pybind11;

#include <mpi.h>
#include <mpi4py/mpi4py.h>

#include <wilkins/wilkins.hpp>
#include <wilkins/context.h>

using namespace wilkins;
using namespace std;

using communicator = MPI_Comm;

template <typename To, typename From>
To container_cast(From && from) {
    using std::begin; using std::end;
    return To(begin(from), end(from));
}

//Exchanging communicators btw Python and C++. Adapted from a SO post, see the second answer.
//https://stackoverflow.com/questions/49259704/pybind11-possible-to-use-mpi4py
struct mpi4py_comm {
  mpi4py_comm() = default;
  mpi4py_comm(communicator value) : value(value) {}
  operator MPI_Comm () { return value; }

  MPI_Comm value;
};

namespace pybind11 { namespace detail {
  template <> struct type_caster<mpi4py_comm> {
    public:
      PYBIND11_TYPE_CASTER(mpi4py_comm, _("mpi4py_comm"));

      // Python -> C++
      bool load(handle src, bool) {
        PyObject *py_src = src.ptr();

        // Check that we have been passed an mpi4py communicator
        if (PyObject_TypeCheck(py_src, &PyMPIComm_Type)) {
          // Convert to regular MPI communicator
          value.value = *PyMPIComm_Get(py_src);
        } else {
          return false;
        }

        return !PyErr_Occurred();
      }

      // C++ -> Python
      static handle cast(mpi4py_comm src,
                         return_value_policy /* policy */,
                         handle /* parent */)
      {
        // Create an mpi4py handle
        return PyMPIComm_New(src.value);
      }
  };
}} // namespace pybind11::detail

mpi4py_comm get_local_comm(Wilkins* w)
{
  return w->local_comm_handle();
}


std::vector<mpi4py_comm> get_intercomms(Wilkins* w)
{
   std::vector<communicator> communicators = w->build_intercomms();
   return container_cast<std::vector<mpi4py_comm>>(communicators);
}

PYBIND11_MODULE(pywilkins, m)
{
    using namespace pybind11::literals;

    m.doc() = "Wilkins python bindings";

    // import the mpi4py API
    if (import_mpi4py() < 0) {
        throw std::runtime_error("Could not load mpi4py API.");
    }


    py::class_<Wilkins>(m, "Wilkins")
        .def(py::init([](long comm_, string& config_file)
                         {
                             MPI_Comm comm = *static_cast<MPI_Comm*>(reinterpret_cast<void*>(comm_));
                             return new Wilkins(comm,config_file);
                         }))
        .def("local_comm_size", &Wilkins::local_comm_size, "returns the size of the task")
        .def("local_comm_rank", &Wilkins::local_comm_rank, "returns the rank within the task")
        .def("workflow_comm_size", &Wilkins::workflow_comm_size, "returns the size of the workflow")
        .def("workflow_comm_rank", &Wilkins::workflow_comm_rank, "returns the rank within the workflow")
        .def("wait", &Wilkins::wait, "consumer waits until the data is ready")
        .def("commit", &Wilkins::commit, "producer signals that the data is ready to use")
    ;

    m.def("get_local_comm", &get_local_comm, "returns the communicator of the local task.");
    m.def("get_intercomms", &get_intercomms, "returns the intercommunicators for this task.");

}
