#ifndef BALM_BENCHMARKS_H
#define BALM_BENCHMARKS_H

#include <stdint.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Balm.h"

Py_LOCAL_SYMBOL PyObject* run_balm_bench(RefCountedData* rd);
Py_LOCAL_SYMBOL PyObject* run_cpy_bench(RefCountedData* rd);

#endif // BALM_BENCHMARKS_H
