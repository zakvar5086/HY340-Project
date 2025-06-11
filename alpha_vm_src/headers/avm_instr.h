
#ifndef AVM_INSTRUCTION_H
#define AVM_INSTRUCTION_H

#include "../../alpha_parser_src/headers/targetcode.h"
#include "avm_memcell.h"

/* Instruction execution function type */
typedef void (*execute_func_t)(instruction*);

/* Dispatch table for instruction execution */
extern execute_func_t executeFuncs[];

/* Arithmetic operations */
void execute_assign(instruction *instr);
void execute_add(instruction *instr);
void execute_sub(instruction *instr);
void execute_mul(instruction *instr);
void execute_div(instruction *instr);
void execute_mod(instruction *instr);
void execute_uminus(instruction *instr);

/* Logical operations */
void execute_and(instruction *instr);
void execute_or(instruction *instr);
void execute_not(instruction *instr);

/* Comparison and jump operations */
void execute_jeq(instruction *instr);
void execute_jne(instruction *instr);
void execute_jle(instruction *instr);
void execute_jge(instruction *instr);
void execute_jlt(instruction *instr);
void execute_jgt(instruction *instr);
void execute_jump(instruction *instr);

/* Function call operations */
void execute_call(instruction *instr);
void execute_pusharg(instruction *instr);
void execute_funcenter(instruction *instr);
void execute_funcexit(instruction *instr);

/* Table operations */
void execute_newtable(instruction *instr);
void execute_tablegetelem(instruction *instr);
void execute_tablesetelem(instruction *instr);

/* No operation */
void execute_nop(instruction *instr);

/* Arithmetic operation function types and dispatch tables */
typedef double (*arithmetic_func_t)(double x, double y);
typedef unsigned char (*comparison_func_t)(double x, double y);

extern arithmetic_func_t arithmeticFuncs[];
extern comparison_func_t comparisonFuncs[];

/* Arithmetic implementation functions */
double add_impl(double x, double y);
double sub_impl(double x, double y);
double mul_impl(double x, double y);
double div_impl(double x, double y);
double mod_impl(double x, double y);

/* Comparison implementation functions */
unsigned char jle_impl(double x, double y);
unsigned char jge_impl(double x, double y);
unsigned char jlt_impl(double x, double y);
unsigned char jgt_impl(double x, double y);

/* Initialize instruction execution system */
void avm_initinstructions(void);

#endif