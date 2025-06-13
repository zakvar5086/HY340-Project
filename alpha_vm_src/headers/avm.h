#ifndef AVM_H
#define AVM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>

typedef struct instruction instruction;
typedef struct vmarg vmarg;
typedef struct userfunc userfunc_t;

#include "avm_memcell.h"
#include "avm_tables.h"
#include "avm_gc.h"
#include "avm_instr.h"
#include "avm_libFunc.h"

#define AVM_STACKSIZE 4096
#define AVM_STACKENV_SIZE 4
#define AVM_MAX_INSTRUCTIONS (unsigned) nop_v

typedef struct {
    /* Runtime stack */
    avm_memcell *stack;
    unsigned top;
    unsigned topsp;
    
    /* Special registers */
    avm_memcell retval;
    avm_memcell ax, bx, cx;
    
    /* Program execution state */
    instruction *code;
    unsigned pc;
    unsigned codeSize;
    unsigned char executionFinished;
    unsigned currLine;
    
    /* Function call tracking */
    unsigned totalActuals;
    
    /* Global variables */
    unsigned programVarCount;
    
    /* Constant pools */
    char **strings;
    double *numbers;
    char **libfuncs;
    userfunc_t *userfuncs;
    
    unsigned totalStrings;
    unsigned totalNumbers;
    unsigned totalLibfuncs;
    unsigned totalUserfuncs;
} avm_state;

/* Global VM instance */
extern avm_state vm;

/* VM Lifecycle */
void avm_initialize(void);
void avm_run(char *filename);
void avm_cleanup(void);

/* Stack management */
void avm_initstack(void);
void avm_dec_top(void);
void avm_push_envvalue(unsigned val);
void avm_callsaveenvironment(void);

/* Program loading */
void avm_load_program(FILE *file);
void avm_load_constants(FILE *file);
void avm_load_code(FILE *file);

/* Execution cycle */
void execute_cycle(void);

/* Operand translation */
avm_memcell *avm_translate_operand(vmarg *arg, avm_memcell *reg);

/* Binary file format verification */
unsigned avm_get_envvalue(unsigned i);

/* Function management */
userfunc_t *avm_getfuncinfo(unsigned address);
void avm_call_functor(avm_table *functor);

#define AVM_MAGICNUMBER 340200501

void debug_print_stack_state(const char* context);

#endif