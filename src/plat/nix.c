#include <stddef.h>
#include <stdint.h>

#include <sys/stat.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

Py_LOCAL_SYMBOL int64_t file_size(char* file_name) {
  struct stat s;
  if(stat(file_name, &s))
    return -1;
  return s.st_size;
}
