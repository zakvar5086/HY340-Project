#ifndef AVM_GC_H
#define AVM_GC_H

#include "avm_memcell.h"

/* Memory cell cleanup function type */
typedef void (*memclear_func_t)(avm_memcell*);

/* Dispatch table for memory cleanup */
extern memclear_func_t memclearFuncs[];

/* Type-specific cleanup functions */
void memclear_string(avm_memcell *m);
void memclear_table(avm_memcell *m);
void memclear_number(avm_memcell *m);
void memclear_bool(avm_memcell *m);
void memclear_userfunc(avm_memcell *m);
void memclear_libfunc(avm_memcell *m);
void memclear_nil(avm_memcell *m);
void memclear_undef(avm_memcell *m);

/* Initialize garbage collection system */
void avm_initgc(void);

#endif