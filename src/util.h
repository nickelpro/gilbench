#ifndef BALM_UTIL_H
#define BALM_UTIL_H

#include <stdint.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

Py_LOCAL_SYMBOL int64_t file_size(char* filename);

#endif // BALM_UTIL_H
