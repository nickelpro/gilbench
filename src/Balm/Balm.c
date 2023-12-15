#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "Balm.h"
#include "balm_threads.h"

static PyTypeObject BalmString_Type;
static PyTypeObject BalmStringCompact_Type;
static PyTypeObject BalmStringBlock_Type;

static PyTypeObject BalmTuple_Type;
static PyTypeObject BalmTupleBig_Type;

typedef struct {
  mtx_t lock;
  BalmStringNode* head;
} StrPool;

typedef struct {
  mtx_t lock;
  BalmTupleNode* head;
} TuplePool;

typedef struct {
  mtx_t lock;
  BalmBlockNode* head;
} BlockPool;

static struct {
  StrPool bigviews;
  StrPool compacts;
  TuplePool tuples;
  BlockPool blocks;
} pools;

static BalmStringNode* compactbalmstr_block_alloc(size_t len) {
  size_t sz = BALM_COMPACT_ALLOCATION * len;
  BalmStringNode* base = malloc(sz);
  char* cur = (char*) base;
  char* end = cur + sz;
  BalmStringNode* prev = NULL;

  for(BalmStringNode* v = base; cur < end;
      v = (BalmStringNode*) (cur += BALM_COMPACT_ALLOCATION)) {
    v->next = prev;
    prev = v;
    // clang-format off
    v->str = (BalmString) {
        .ob_base.ob_type = &BalmStringCompact_Type,
        .state = {
            .kind = PyUnicode_1BYTE_KIND,
            .compact = 1,
            .ascii = 1,
            .balm = BALM_STRING_COMPACT,
        },
    };
    // clang-format on
  }

  return prev;
}

static BalmStringNode* balmstr_block_alloc(size_t len) {
  BalmStringNode* base = malloc(sizeof(*base) * len);
  BalmStringNode* prev = NULL;
  for(BalmStringNode* v = base; v < base + len; ++v) {
    v->next = prev;
    prev = v;
    // clang-format off
    v->str = (BalmString) {
        .ob_base.ob_type = &BalmString_Type,
        .state = {
            .kind = PyUnicode_1BYTE_KIND,
            .ascii = 1,
        },
    };
    // clang-format on
  }
  return prev;
}

static BalmBlockNode* balmblock_alloc() {
  BalmBlockNode* node = malloc(sizeof(*node) +
      BALM_STRING_ALLOCATION_BLOCK_SIZE * sizeof(*node->blk.strs));
  for(size_t i = 0; i < BALM_STRING_ALLOCATION_BLOCK_SIZE; ++i)
    node->blk.strs[i] = (BalmString) {
        .ob_base = {.ob_type = &BalmStringBlock_Type},
        .state =
            {
                .kind = PyUnicode_1BYTE_KIND,
                .ascii = 1,
                .balm = BALM_STRING_BLOCK,
                .balm_offset = i,
            },
    };
  return node;
}

static BalmTupleNode* balmtpl_block_alloc(size_t len) {
  size_t unit_sz = sizeof(BalmTupleNode);
  unit_sz += sizeof(PyObject*) * (BALM_TUPLE_MAX_SIZE - 1);
  size_t total_sz = unit_sz * len;
  BalmTupleNode* v = malloc(total_sz);
  BalmTupleNode* prev = NULL;

  char* cur = (char*) v;
  char* end = cur + total_sz;

  for(; cur < end; v = (BalmTupleNode*) (cur += unit_sz)) {
    v->next = prev;
    prev = v;
    v->tpl.ob_base.ob_base.ob_type = &BalmTuple_Type;
  }

  return prev;
}

static void balmstr_push(StrPool* pool, PyObject* str) {
  BalmStringNode* node = GET_STRNODE_PYOBJ(str);
  mtx_lock(&pool->lock);
  node->next = pool->head;
  pool->head = node;
  mtx_unlock(&pool->lock);
}

static void balmtpl_push(TuplePool* pool, PyObject* tpl) {
  BalmTupleNode* node = GET_TPLNODE_PYOBJ(tpl);
  mtx_lock(&pool->lock);
  node->next = pool->head;
  pool->head = node;
  mtx_unlock(&pool->lock);
}

static void balmblock_push(BlockPool* pool, BalmBlockNode* node) {
  mtx_lock(&pool->lock);
  node->next = pool->head;
  pool->head = node;
  mtx_unlock(&pool->lock);
}

static BalmString* balmstr_pop(StrPool* pool, BalmStringNode* (*alloc)(size_t),
    size_t alloc_len) {
  mtx_lock(&pool->lock);
  if(!pool->head)
    pool->head = alloc(alloc_len);
  BalmStringNode* node = pool->head;
  pool->head = node->next;
  mtx_unlock(&pool->lock);
  return &node->str;
}

static BalmTuple* balmtpl_pop(TuplePool* pool, BalmTupleNode* (*alloc)(size_t),
    size_t alloc_len) {
  mtx_lock(&pool->lock);
  if(!pool->head)
    pool->head = alloc(alloc_len);
  BalmTupleNode* node = pool->head;
  pool->head = node->next;
  mtx_unlock(&pool->lock);
  return &node->tpl;
}

static BalmStrBlock* balmblock_pop(BlockPool* pool) {
  mtx_lock(&pool->lock);
  if(!pool->head) {
    mtx_unlock(&pool->lock);
    return &balmblock_alloc()->blk;
  }
  BalmBlockNode* node = pool->head;
  pool->head = node->next;
  mtx_unlock(&pool->lock);
  return &node->blk;
}

static void balmstr_dealloc(PyObject* str) {
  BalmString* balm = (BalmString*) str;
  RefCountedData* rd = balm->rd;
  if(!(--rd->ref_count))
    free(rd);
  balmstr_push(&pools.bigviews, str);
}

static void balmstrcompact_dealloc(PyObject* str) {
  balmstr_push(&pools.compacts, str);
}

static void balmstrblock_dealloc(PyObject* str) {
  BalmString* strs = (BalmString*) str;
  RefCountedData* rd = strs->rd;

  strs -= strs->state.balm_offset;
  BalmStrBlock* blk = GET_BLK_STRS(strs);

  if(!(--rd->ref_count))
    free(rd);

  BalmBlockNode* node = GET_BLKNODE_BLK(blk);

  if(!(--blk->ref_count))
    balmblock_push(&pools.blocks, node);
}

static void balmtpl_dealloc(PyObject* tpl) {
  BalmTuple* t = (BalmTuple*) tpl;
  for(Py_ssize_t i = 0; i < t->ob_base.ob_size; ++i)
    Py_DECREF(t->ob_item[i]);
  balmtpl_push(&pools.tuples, tpl);
}

static void balmtplbig_dealloc(PyObject* tpl) {
  BalmTuple* t = (BalmTuple*) tpl;
  for(Py_ssize_t i = 0; i < t->ob_base.ob_size; ++i)
    Py_DECREF(t->ob_item[i]);
  free(tpl);
}

static void string_view(BalmString* str, RefCountedData* rd, char* data,
    size_t len) {
  str->rd = rd;
  str->uc.data.any = data;
  str->uc._base.utf8 = data;
  str->uc._base.utf8_length = len;
  str->length = len;
}

Py_LOCAL_SYMBOL BalmString* New_BalmString(size_t len) {

  if(len <= BALM_COMPACT_MAX_STR) {
    BalmString* str = balmstr_pop(&pools.compacts, compactbalmstr_block_alloc,
        BALM_STRING_ALLOCATION_BLOCK_SIZE);
    str->length = len;
    return str;
  }

  RefCountedData* rd = malloc(sizeof(*rd) + len);
  rd->ref_count = 1;
  BalmString* str = balmstr_pop(&pools.bigviews, balmstr_block_alloc,
      BALM_STRING_ALLOCATION_BLOCK_SIZE);
  string_view(str, rd, rd->data, len);
  str->state.balm = BALM_STRING_BIG;
  return str;
}

Py_LOCAL_SYMBOL BalmString* New_BalmStringView(RefCountedData* rd, char* data,
    size_t len) {
  BalmString* str = balmstr_pop(&pools.bigviews, balmstr_block_alloc,
      BALM_STRING_ALLOCATION_BLOCK_SIZE);
  string_view(str, rd, data, len);
  str->state.balm = BALM_STRING_VIEW;
  return str;
}

Py_LOCAL_SYMBOL BalmStrBlock* New_BalmStringBlock(size_t len) {
  // ToDo: use allocator for this
  if(len > BALM_STRING_ALLOCATION_BLOCK_SIZE)
    return NULL;

  BalmStrBlock* blk = balmblock_pop(&pools.blocks);
  blk->ref_count = len;
  return blk;
}

Py_LOCAL_SYMBOL BalmString* Get_BalmStringFromBlock(BalmStrBlock* blk,
    size_t index, RefCountedData* rd, char* data, size_t len) {
  BalmString* str = &blk->strs[index];
  string_view(str, rd, data, len);
  return str;
};

Py_LOCAL_SYMBOL char* GetData_BalmString(BalmString* str) {
  if(str->state.balm == BALM_STRING_COMPACT)
    return str->data;
  return str->uc.data.any;
}

Py_LOCAL_SYMBOL BalmTuple* New_BalmTuple(size_t len) {
  if(len <= BALM_TUPLE_MAX_SIZE) {
    BalmTuple* tpl = balmtpl_pop(&pools.tuples, balmtpl_block_alloc,
        BALM_TUPLE_ALLOCATION_BLOCK_SIZE);
    tpl->ob_base.ob_size = len;
    tpl->ob_base.ob_base.ob_refcnt = 1;
    return tpl;
  }

  size_t sz = sizeof(BalmTuple) + (len - 1) * sizeof(PyObject*);
  BalmTuple* tpl = malloc(sz);
  tpl->ob_base.ob_size = len;
  tpl->ob_base.ob_base.ob_refcnt = 1;
  tpl->ob_base.ob_base.ob_type = &BalmTupleBig_Type;
  return tpl;
}

static void init_strpool(StrPool* pool, BalmStringNode* (*alloc)(size_t)) {
  mtx_init(&pool->lock, mtx_plain);
  pool->head = alloc(BALM_STRING_ALLOCATION_BLOCK_SIZE);
}

static void init_tplpool(TuplePool* pool, BalmTupleNode* (*alloc)(size_t)) {
  mtx_init(&pool->lock, mtx_plain);
  pool->head = alloc(BALM_TUPLE_ALLOCATION_BLOCK_SIZE);
}

static void init_blkpool(BlockPool* pool) {
  mtx_init(&pool->lock, mtx_plain);
  pool->head = NULL;
}

Py_LOCAL_SYMBOL void balm_init() {
  BalmString_Type = PyUnicode_Type;
  BalmString_Type.tp_new = NULL;
  BalmString_Type.tp_free = NULL;
  BalmString_Type.tp_dealloc = balmstr_dealloc;

  BalmStringCompact_Type = BalmString_Type;
  BalmStringCompact_Type.tp_dealloc = balmstrcompact_dealloc;

  BalmStringBlock_Type = BalmString_Type;
  BalmStringBlock_Type.tp_dealloc = balmstrblock_dealloc;

  BalmTuple_Type = PyTuple_Type;
  BalmTuple_Type.tp_new = NULL;
  BalmTuple_Type.tp_free = NULL;
  BalmTuple_Type.tp_dealloc = balmtpl_dealloc;

  BalmTupleBig_Type = BalmTuple_Type;
  BalmTupleBig_Type.tp_dealloc = balmtplbig_dealloc;

  init_strpool(&pools.bigviews, balmstr_block_alloc);
  init_strpool(&pools.compacts, compactbalmstr_block_alloc);
  init_tplpool(&pools.tuples, balmtpl_block_alloc);
  init_blkpool(&pools.blocks);
}
