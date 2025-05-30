#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../headers/targetcode.h"
#include "../headers/quad.h"
#include "../headers/symtable.h"
#include "../headers/stack.h"
#include "targetcode.h"

instruction *instructions = NULL;
unsigned currInstructions = 0;
unsigned totalInstructions = 0;
unsigned instructionsSize = 0;

double *numConsts;
unsigned totalNumConsts = 0;
unsigned currNumConst = 0;

char **stringConsts;
unsigned totalStringConsts = 0;
unsigned currStringConst = 0;

char **namedLibfuncs;
unsigned totalNameLibfuncs = 0;
unsigned currLibfunct = 0;

userfunc_t *userFuncs;
unsigned totalUserFuncs = 0;
unsigned currUserfunct = 0;

static Stack *funcStack = NULL;

#define EXPAND_SIZE 1024
#define CURR_SIZE (instructionsSize * sizeof(instruction))
#define NEW_SIZE (EXPAND_SIZE * sizeof(instruction) + CURR_SIZE)

unsigned nextinstructionlabel() { return currInstructions; }

generator_func_t generators[] = {
    generate_ASSIGN,
    generate_ADD,
    generate_SUB,
    generate_MUL,
    generate_DIV,
    generate_MOD,
    generate_NEWTABLE,
    generate_TABLEGETELM,
    generate_TABLESETELEM,
    generate_NOP,
    generate_JUMP,
    generate_IF_EQ,
    generate_IF_NOTEQ,
    generate_IF_GREATER,
    generate_IF_GREATEREQ,
    generate_IF_LESS,
    generate_IF_LESSEQ,
    generate_NOT,
    generate_OR,
    generate_PARAM,
    generate_CALL,
    generate_GETRETVAL,
    generate_FUNCSTART,
    generate_RETURN,
    generate_FUNCEND
};

void initInstructions() {
    instructions = malloc(EXPAND_SIZE * sizeof(instruction));
    assert(instructions);
    memset(instructions, 0, EXPAND_SIZE * sizeof(instruction));
    instructionsSize = EXPAND_SIZE;
    currInstructions = 0;

    numConsts = malloc(EXPAND_SIZE * sizeof(double));
    assert(numConsts);
    totalNumConsts = EXPAND_SIZE;
    currNumConst = 0;

    stringConsts = malloc(EXPAND_SIZE * sizeof(char*));
    assert(stringConsts);
    totalStringConsts = EXPAND_SIZE;
    currStringConst = 0;

    namedLibfuncs = malloc(EXPAND_SIZE * sizeof(char*));
    assert(namedLibfuncs);
    totalNameLibfuncs = EXPAND_SIZE;
    currLibfunct = 0;

    userFuncs = malloc(EXPAND_SIZE * sizeof(userfunc_t));
    assert(userFuncs);
    totalUserFuncs = EXPAND_SIZE;
    currUserfunct = 0;
    funcStack = newStack();
}

void expandNum() {
    assert(totalNumConsts == currNumConst);
    numConsts = realloc(numConsts, (totalNumConsts + EXPAND_SIZE) * sizeof(double));
    if(!numConsts) {
        fprintf(stderr, "Error: Memory allocation failed for numeric constants\n");
        exit(1);
    }
    totalNumConsts += EXPAND_SIZE;
}
void expandString() {
    assert(totalStringConsts == currStringConst);

    stringConsts = realloc(stringConsts, (totalStringConsts + EXPAND_SIZE) * sizeof(char*));
    if(!stringConsts) {
        fprintf(stderr, "Error: Memory allocation failed for string constants\n");
        exit(1);
    }
    totalStringConsts += EXPAND_SIZE;
}
void expandUserfunc() {
    assert(totalUserFuncs == currUserfunct);

    userFuncs = realloc(userFuncs, (totalUserFuncs + EXPAND_SIZE) * sizeof(userfunc_t));
    if(!userFuncs) {
        fprintf(stderr, "Error: Memory allocation failed for user functions\n");
        exit(1);
    }
    totalUserFuncs += EXPAND_SIZE;
}
void expandLibfunc() {
    assert(totalNameLibfuncs == currLibfunct);
    
    namedLibfuncs = realloc(namedLibfuncs, (totalNameLibfuncs + EXPAND_SIZE) * sizeof(char*));
    if(!namedLibfuncs) {
        fprintf(stderr, "Error: Memory allocation failed for library functions\n");
        exit(1);
    }
    totalNameLibfuncs += EXPAND_SIZE;
}
void expandInstructions() {
    if(currInstructions == instructionsSize) {
        instructions = realloc(instructions, NEW_SIZE);
        if(!instructions) {
            fprintf(stderr, "Error: Memory allocation failed for instructions\n");
            exit(1);
        }
        instructionsSize += EXPAND_SIZE;
    }
}

unsigned consts_newString(char *s) {
    if(currStringConst == totalStringConsts) expandString();
    assert(currStringConst < totalStringConsts);

    char *newString = strdup(s);
    assert(newString != NULL);

    stringConsts[currStringConst] = newString;
    unsigned index = currStringConst;
    currStringConst++;
    return index;
}

unsigned consts_newNum(double n) {
    if(currNumConst == totalNumConsts) expandNum();
    assert(currNumConst < totalNumConsts);

    numConsts[currNumConst] = n;
    unsigned index = currNumConst;
    currNumConst++;
    return index;
}

unsigned libfuncs_newused(char *s) {
    if(currLibfunct == totalNameLibfuncs) expandLibfunc();
    assert(currLibfunct < totalNameLibfuncs);

    char *newLibfunc = strdup(s);
    assert(newLibfunc != NULL);

    namedLibfuncs[currLibfunct] = newLibfunc;
    unsigned index = currLibfunct;
    currLibfunct++;
    return index;
}

unsigned userfuncs_newused(userfunc_t *s) {
    if(currUserfunct == totalUserFuncs) expandUserfunc();
    assert(currUserfunct < totalUserFuncs);

    userFuncs[currUserfunct].address = s->address;
    userFuncs[currUserfunct].localSize = s->localSize;
    userFuncs[currUserfunct].saved_index = s->saved_index;
    userFuncs[currUserfunct].id = strdup(s->id);
    assert(userFuncs[currUserfunct].id != NULL);

    unsigned index = currUserfunct;
    currUserfunct++;
    return index;
}

void emit_tcode(instruction *instr) {
    expandInstructions();

    instructions[currInstructions].opcode = instr->opcode;
    instructions[currInstructions].arg1 = instr->arg1;
    instructions[currInstructions].arg2 = instr->arg2;
    instructions[currInstructions].result = instr->result;
    instructions[currInstructions].srcLine = instr->srcLine;
    ++currInstructions;
}

void make_operand(Expr *e, vmarg *arg) {
    if(!e || !arg) {
        fprintf(stderr, "Error: Null expression or argument in make_operand\n");
        exit(EXIT_FAILURE);
    }

    switch (e->type) {
        case var_e:
        case tableitem_e:
        case arithexpr_e:
        case boolexpr_e:
        case newtable_e:
            arg->val = e->sym->offset;
            switch (e->sym->type) {
                case GLOBAL_VAR: arg->type = global_a; break;
                case LOCAL_VAR:  arg->type = local_a;  break;
                case FORMAL:     arg->type = formal_a; break;
            }

        case constbool_e: {
            arg->val = e->boolConst;
            arg->type = bool_a;
            break;
        }
        case conststring_e: {
            arg->val = consts_newString(e->strConst);
            arg->type = string_a;
            break;
        }
        case constnum_e: {
            arg->val = consts_newNum(e->numConst);
            arg->type = number_a;
            break;
        }

        case programfunc_e: {
            arg->type = userfunc_a;
            arg->val = userfuncs_newused(e->sym);
            break;
        }
        case libraryfunc_e: {
            arg->type = libfunc_a;
            arg->val = libfuncs_newused(e->sym->name);
            break;
        }
        default: assert(0);
    }    
}

void make_numOperand(vmarg *arg, double val) {
    arg->val = consts_newNum(val);
    arg->type = number_a;
}
void make_boolOperand(vmarg *arg, unsigned val) {
    arg->val = val;
    arg->type = bool_a;
}
void make_retvalOperand(vmarg *arg) {
    arg->type = retval_a;
}
void reset_operand(vmarg *arg) {
    assert(arg != NULL);
    arg = (vmarg *)0;
}

void generate(void) {
    for(unsigned i = 0; i < totalQuads; ++i) {
        if(quads[i].op < 0 || quads[i].op >= sizeof(generators)/sizeof(generators[0])) {
            fprintf(stderr, "Error: Invalid opcode %d at quad %u\n", quads[i].op, i);
            exit(EXIT_FAILURE);
        }
        generators[quads[i].op](&quads[i]);
    }
}

void generate_op(vmopcode op, Quad *quad) {
    instruction instr;

    instr.opcode = op;
    quad->taddress = nextinstructionlabel();
    instr.srcLine = quad->line;
    
    make_operand(quad->arg1, &instr.arg1);
    make_operand(quad->arg2, &instr.arg2);
    make_operand(quad->result, &instr.result);

    emit_tcode(&instr);
}

void generate_ADD(Quad *quad) { generate_op(add_v, quad); }
void generate_SUB(Quad *quad) { generate_op(sub_v, quad); }
void generate_MUL(Quad *quad) { generate_op(mul_v, quad); }
void generate_DIV(Quad *quad) { generate_op(div_v, quad); }
void generate_MOD(Quad *quad) { generate_op(mod_v, quad); }

void generate_NEWTABLE(Quad *quad)     { generate_op(newtable_v, quad); }
void generate_TABLEGETELM(Quad *quad)  { generate_op(tablegetelem_v, quad); }
void generate_TABLESETELEM(Quad *quad) { generate_op(tablesetelem_v, quad); }
void generate_ASSIGN(Quad *quad)       { generate_op(assign_v, quad); }
void generate_NOP(Quad *quad)          { instruction t; t.opcode = nop_v; emit_tcode(&t); }

void generate_JUMP(Quad *quad)         { generate_op(jump_v, quad); }
void generate_IF_EQ(Quad *quad)        { generate_op(jeq_v, quad); }
void generate_IF_NOTEQ(Quad *quad)     { generate_op(jne_v, quad); }
void generate_IF_GREATER(Quad *quad)   { generate_op(jgt_v, quad); }
void generate_IF_GREATEREQ(Quad *quad) { generate_op(jge_v, quad); }
void generate_IF_LESS(Quad *quad)      { generate_op(jlt_v, quad); }
void generate_IF_LESSEQ(Quad *quad)    { generate_op(jle_v, quad); }

void generate_NOT(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction instr;

    instr.opcode = jeq_v;
    make_operand(quad->arg1, &instr.arg1);
    make_boolOperand(&instr.arg2, 0);
    instr.result->type = label_a;
    instr.result->val = nextinstructionlabel() + 3;
    emit_tcode(&instr);

    instr.opcode = assign_v;
    make_boolOperand(&instr.arg1, 0);
    reset_operand(&instr.arg2);
    make_operand(quad->result, &instr.result);
    emit_tcode(&instr);

    instr.opcode = jump_v;
    reset_operand(&instr.arg1); reset_operand(&instr.arg2);
    instr.result->type = label_a;
    instr.result->val = nextinstructionlabel() + 2;
    emit_tcode(&instr);

    instr.opcode = assign_v;
    make_boolOperand(&instr.arg1, 1);
    reset_operand(&instr.arg2);
    make_operand(quad->result, &instr.result);
    emit_tcode(&instr);
}

void generate_OR(Quad *quad) {
    assert(0 && "generate_OR should not be called: short-circuit front-end handles AND");  
}

void generate_AND(Quad *quad) {
    assert(0 && "generate_AND should not be called: short-circuit front-end handles AND");
}

void generate_PARAM(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction instr;
    instr.opcode = pusharg_v;
    make_operand(quad->arg1, &instr.arg1);
    emit_tcode(&instr);
}
void generate_CALL(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction instr;
    instr.opcode = call_v;
    make_operand(quad->arg1, &instr.arg1);
    emit_tcode(&instr);
}
void generate_GETRETVAL(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction instr;
    instr.opcode = assign_v;
    make_operand(quad->result, &instr.result);
    make_retvalOperand(&instr.arg1);
    emit_tcode(&instr);
}

void generate_FUNCSTART(Quad *quad) {
    SymTableEntry *func = quad->result->sym;
    func->taddress = nextinstructionlabel();
    quad->taddress = nextinstructionlabel();

    userfunc_t userFunc = {
        .address = func->taddress,
        .localSize = func->localCount,
        .saved_index = func->offset,
        .id = strdup(func->name)
    };
    userfuncs_newused(&userFunc);
    pushStack(funcStack, func);

    instruction instr;
    instr.opcode = funcenter_v;
    make_operand(quad->result, &instr.result);
    emit_tcode(&instr);
}

void generate_RETURN(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction instr;
    instr.opcode = assign_v;
    make_retvalOperand(&instr.result);
    make_operand(quad->arg1, &instr.arg1);
    emit_tcode(&instr);
    
    instr.opcode = jump_v;
    reset_operand(&instr.arg1); reset_operand(&instr.arg2);
    instr.result->type = label_a;
    emit_tcode(&instr);
}

void generate_FUNCEND(Quad *quad) {
    SymTableEntry *func = popStack(funcStack);

    quad->taddress = nextinstructionlabel();
    instruction instr;
    instr.opcode = funcexit_v;
    make_operand(quad->result, &instr.result);
    emit_tcode(&instr);
}