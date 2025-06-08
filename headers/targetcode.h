#ifndef TARGETCODE_H
#define TARGETCODE_H

#include "quad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum vmopcode {
    assign_v,       add_v,          sub_v, 
    mul_v,          div_v,          mod_v,
    uminus_v,       and_v,          or_v, 
    not_v,          jeq_v,          jne_v, 
    jle_v,          jge_v,          jlt_v, 
    jgt_v,          jump_v,         call_v, 
    pusharg_v,      funcenter_v,    funcexit_v,
    newtable_v,     tablegetelem_v, tablesetelem_v,
    nop_v
} vmopcode;

typedef enum vmarg_t {
    label_a     = 0,
    global_a    = 1,
    formal_a    = 2,
    local_a     = 3,
    number_a    = 4,
    string_a    = 5,
    bool_a      = 6,
    nil_a       = 7,
    userfunc_a  = 8,
    libfunc_a   = 9,
    retval_a    = 10
} vmarg_t;

typedef struct vmarg {
    vmarg_t type;
    unsigned val;
} vmarg;

typedef struct instruction {
    vmopcode opcode;
    vmarg *result;
    vmarg *arg1;
    vmarg *arg2;
    unsigned srcLine;
} instruction;

typedef struct userfunc {
    unsigned address;
    unsigned localSize;
    unsigned saved_index;
    char* id;
} userfunc_t;

typedef void (*generator_func_t)(Quad*);

void generate_ADD (Quad *quad);
void generate_SUB (Quad *quad);
void generate_MUL (Quad *quad);
void generate_DIV (Quad *quad);
void generate_MOD (Quad *quad);
void generate_NEWTABLE (Quad *quad);
void generate_TABLEGETELM (Quad *quad);
void generate_TABLESETELEM (Quad *quad); 
void generate_ASSIGN (Quad *quad);
void generate_UMINUS (Quad *quad);
void generate_NOP (Quad *quad);
void generate_JUMP (Quad *quad); 
void generate_IF_EQ (Quad *quad); 
void generate_IF_NOTEQ (Quad *quad);
void generate_IF_GREATER (Quad *quad);
void generate_IF_GREATEREQ (Quad *quad);
void generate_IF_LESS (Quad *quad);
void generate_IF_LESSEQ (Quad *quad);
void generate_NOT (Quad *quad);
void generate_OR (Quad *quad);
void generate_AND (Quad *quad);
void generate_PARAM (Quad *quad);
void generate_CALL (Quad *quad);
void generate_GETRETVAL (Quad *quad);
void generate_FUNCSTART (Quad *quad);
void generate_RETURN (Quad *quad);
void generate_FUNCEND (Quad *quad);

void make_operand(Expr *e, vmarg *arg);
void make_numOperand(vmarg *arg, double val);
void make_boolOperand(vmarg *arg, unsigned val);
void make_retvalOperand(vmarg *arg);

void generate(void);
unsigned consts_newString(char *s);
unsigned consts_newNum(double n);
unsigned libfuncs_newused(char *s);
unsigned userfuncs_newused(SymTableEntry *s);

void print_instructions();
void print_constants();

#endif