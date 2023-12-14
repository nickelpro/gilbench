#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Balm.h"
#include "benchmarks.h"
#include "util.h"

const static char cap_name[] = "RefCountedData Pointer";

void destroy_rd(PyObject* cap) {
  RefCountedData* rd = PyCapsule_GetPointer(cap, cap_name);
  if(!(--rd->ref_count))
    free(rd);
}

static PyObject* read_file(PyObject* self, PyObject* const* args,
    Py_ssize_t nargs, PyObject* kwnames) {

  static const char* _keywords[] = {"file_name", NULL};
  static _PyArg_Parser _parser = {.keywords = _keywords,
      .format = "s:read_file"};

  char* file_name;

  if(!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser, &file_name))
    return NULL;

  int64_t size = file_size(file_name);
  if(size < 0) {
    PyErr_Format(PyExc_RuntimeError, "Stat failed for: %s", file_name);
    return NULL;
  }

  FILE* f = fopen(file_name, "rb");
  if(!f) {
    PyErr_Format(PyExc_RuntimeError, "Error opening file: %s", file_name);
    return NULL;
  }


  RefCountedData* rd = malloc(sizeof(*rd) + size);
  rd->ref_count = 1;

  size_t read = fread(rd->data, 1, size, f);
  if(read < size || fgetc(f) != EOF) {
    free(rd);
    fclose(f);
    PyErr_Format(PyExc_RuntimeError, "Error reading file: %s", file_name);
    return NULL;
  }

  fclose(f);
  return PyCapsule_New(rd, cap_name, destroy_rd);
}

static PyObject* bench_balm(PyObject* self, PyObject* const* args,
    Py_ssize_t nargs, PyObject* kwnames) {
  static const char* _keywords[] = {"rd", NULL};
  static _PyArg_Parser _parser = {.keywords = _keywords,
      .format = "O:bench_balm"};

  PyObject* rd_capsule;

  if(!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser, &rd_capsule))
    return NULL;

  RefCountedData* rd = PyCapsule_GetPointer(rd_capsule, cap_name);
  if(!rd)
    return NULL;

  PyThreadState* state = PyEval_SaveThread();
  PyObject* tpl = run_balm_bench(rd);
  PyEval_RestoreThread(state);
  rd->ref_count += PyTuple_GET_SIZE(tpl);
  return tpl;
}

static PyObject* bench_balmblock(PyObject* self, PyObject* const* args,
    Py_ssize_t nargs, PyObject* kwnames) {
  static const char* _keywords[] = {"rd", NULL};
  static _PyArg_Parser _parser = {.keywords = _keywords,
      .format = "O:bench_balmblock"};

  PyObject* rd_capsule;

  if(!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser, &rd_capsule))
    return NULL;

  RefCountedData* rd = PyCapsule_GetPointer(rd_capsule, cap_name);
  if(!rd)
    return NULL;

  PyThreadState* state = PyEval_SaveThread();
  PyObject* tpl = run_balmblock_bench(rd);
  PyEval_RestoreThread(state);
  rd->ref_count += PyTuple_GET_SIZE(tpl);
  return tpl;
}


static PyObject* bench_cpy(PyObject* self, PyObject* const* args,
    Py_ssize_t nargs, PyObject* kwnames) {
  static const char* _keywords[] = {"rd", NULL};
  static _PyArg_Parser _parser = {.keywords = _keywords,
      .format = "O:bench_cpy"};

  PyObject* rd_capsule;

  if(!_PyArg_ParseStackAndKeywords(args, nargs, kwnames, &_parser, &rd_capsule))
    return NULL;

  RefCountedData* rd = PyCapsule_GetPointer(rd_capsule, cap_name);
  if(!rd)
    return NULL;

  return run_cpy_bench(rd);
}

static PyMethodDef BalmBenchMethods[] = {
    {"read_file", (PyCFunction) read_file, METH_FASTCALL | METH_KEYWORDS},
    {"bench_balm", (PyCFunction) bench_balm, METH_FASTCALL | METH_KEYWORDS},
    {"bench_balmblock", (PyCFunction) bench_balmblock,
        METH_FASTCALL | METH_KEYWORDS},
    {"bench_cpy", (PyCFunction) bench_cpy, METH_FASTCALL | METH_KEYWORDS},
    {0},
};

static struct PyModuleDef BalmBenchModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "balmbench",
    .m_doc = "GIL Balming Benchmark",
    .m_size = -1,
    .m_methods = BalmBenchMethods,
};

PyMODINIT_FUNC PyInit_balmbench(void) {
  balm_init();
  PyObject* mod = PyModule_Create(&BalmBenchModule);
  if(!mod)
    return NULL;
  if(PyModule_AddStringConstant(mod, "__version__", "1.0.0") == -1)
    return NULL;
  return mod;
}
