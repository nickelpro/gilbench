#include <stddef.h>
#include <stdint.h>

#include <fileapi.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

Py_LOCAL_SYMBOL int64_t file_size(char* file_name) {
  HANDLE f = CreateFileA(file_name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL);

  if(f == INVALID_HANDLE_VALUE)
    return -1;

  int64_t size;
  if(!GetFileSizeEx(f, &size))
    size = -1;

  CloseHandle(f);
  return size;
}
