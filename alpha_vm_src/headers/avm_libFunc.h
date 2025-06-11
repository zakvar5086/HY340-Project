#ifndef AVM_LIBRARY_H
#define AVM_LIBRARY_H

#include "avm_memcell.h"

/* Library function type */
typedef void (*library_func_t)(void);

/* Library function management */
void avm_registerlibfunc(char *id, library_func_t addr);
library_func_t avm_getlibraryfunc(char *id);
void avm_calllibfunc(char *funcName);

/* Initialize library functions */
void avm_initlibraryfuncs(void);

/* Standard library functions */
void libfunc_print(void);
void libfunc_input(void);
void libfunc_objectmemberkeys(void);
void libfunc_objecttotalmembers(void);
void libfunc_objectcopy(void);
void libfunc_totalarguments(void);
void libfunc_argument(void);
void libfunc_typeof(void);
void libfunc_strtonum(void);
void libfunc_sqrt(void);
void libfunc_cos(void);
void libfunc_sin(void);

/* Utility functions for library function execution */
unsigned avm_totalactuals(void);
avm_memcell *avm_getactual(unsigned i);

/* Stack environment offsets for function calls */
#define AVM_NUMACTUALS_OFFSET    +4
#define AVM_SAVEDPC_OFFSET       +3
#define AVM_SAVEDTOP_OFFSET      +2
#define AVM_SAVEDTOPSP_OFFSET    +1

#endif