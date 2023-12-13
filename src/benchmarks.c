#include <stdint.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Balm.h"
#include "benchmarks.h"


Py_LOCAL_SYMBOL PyObject* run_balm_bench(RefCountedData* rd) {

  uint16_t* header_cur = (uint16_t*) rd->data;
  uint16_t entries = *header_cur;
  uint16_t* end = ++header_cur + entries;
  char* data_cur = (char*) end;

  BalmTuple* tpl = New_BalmTuple(entries);

  for(Py_ssize_t i = 0; header_cur < end; ++header_cur, ++i) {
    uint16_t len = *header_cur;
    BalmString* str = New_BalmStringView(rd, data_cur, len);
    str->ob_base.ob_refcnt = 1;
    PyTuple_SET_ITEM(tpl, i, str);
    data_cur += len;
  }

  return (PyObject*) tpl;
}

Py_LOCAL_SYMBOL PyObject* run_cpy_bench(RefCountedData* rd) {

  uint16_t* header_cur = (uint16_t*) rd->data;
  uint16_t entries = *header_cur;
  uint16_t* end = ++header_cur + entries;
  char* data_cur = (char*) end;

  PyObject* tpl = PyTuple_New(entries);
  if(!tpl)
    return NULL;

  for(Py_ssize_t i = 0; header_cur < end; ++header_cur, ++i) {
    uint16_t len = *header_cur;
    PyObject* str = PyUnicode_New(len, 127);
    memcpy(PyUnicode_1BYTE_DATA(str), data_cur, len);
    PyTuple_SET_ITEM(tpl, i, str);
    data_cur += len;
  }

  return tpl;
}
