#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../headers/targetcode.h"
#include "../headers/quad.h"
#include "../headers/symtable.h"
#include "../headers/stack.h"

extern unsigned int programVarCount;

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

char **libFuncs;
unsigned totalNameLibfuncs = 0;
unsigned currLibfunct = 0;

userfunc_t *userFuncs;
unsigned totalUserFuncs = 0;
unsigned currUserfunct = 0;

static Stack *funcStack = NULL;

#define EXPAND_SIZE 1024
#define CURR_SIZE (instructionsSize * sizeof(instruction))
#define NEW_SIZE (EXPAND_SIZE * sizeof(instruction) + CURR_SIZE)

unsigned nextinstructionlabel() {
    return currInstructions;
}

generator_func_t generators[] = {
    generate_ASSIGN,
    generate_ADD,
    generate_SUB,
    generate_MUL,
    generate_DIV,
    generate_MOD,
    generate_UMINUS,
    generate_AND,
    generate_OR,
    generate_NOT,
    generate_IF_EQ,
    generate_IF_NOTEQ,
    generate_IF_LESSEQ,
    generate_IF_GREATEREQ,
    generate_IF_LESS,
    generate_IF_GREATER,
    generate_JUMP,
    generate_CALL,
    generate_PARAM,
    generate_RETURN,
    generate_GETRETVAL,
    generate_FUNCSTART,
    generate_FUNCEND,
    generate_NEWTABLE,
    generate_TABLEGETELM,
    generate_TABLESETELEM
};

instruction *mallocInstr() {
    instruction *instr = malloc(sizeof(instruction));
    assert(instr != NULL);

    instr->arg1 = malloc(sizeof(vmarg));
    instr->arg2 = malloc(sizeof(vmarg));
    instr->result = malloc(sizeof(vmarg));
    assert(instr->arg1 != NULL && instr->arg2 != NULL && instr->result != NULL);

    instr->arg1->type = nil_a;
    instr->arg1->val = 0;
    instr->arg2->type = nil_a;
    instr->arg2->val = 0;
    instr->result->type = nil_a;
    instr->result->val = 0;

    return instr;
}
void freeInstr(instruction *instr) {
    if(instr->arg1) free(instr->arg1);
    if(instr->arg2) free(instr->arg2);
    if(instr->result) free(instr->result);
    free(instr);
}

void initInstructions() {
    instructions = malloc(EXPAND_SIZE * sizeof(instruction));
    assert(instructions);
    memset(instructions, 0, EXPAND_SIZE * sizeof(instruction));
    instructionsSize = EXPAND_SIZE;
    currInstructions = 1;

    numConsts = malloc(EXPAND_SIZE * sizeof(double));
    assert(numConsts);
    totalNumConsts = EXPAND_SIZE;
    currNumConst = 0;

    stringConsts = malloc(EXPAND_SIZE * sizeof(char*));
    assert(stringConsts);
    totalStringConsts = EXPAND_SIZE;
    currStringConst = 0;

    libFuncs = malloc(EXPAND_SIZE * sizeof(char*));
    assert(libFuncs);
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

    libFuncs = realloc(libFuncs, (totalNameLibfuncs + EXPAND_SIZE) * sizeof(char*));
    if(!libFuncs) {
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
    for(unsigned i = 0; i < currStringConst; i++) {
        if(strcmp(stringConsts[i], s) == 0) {
            return i;
        }
    }

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
    for(unsigned i = 0; i < currNumConst; i++) {
        if(numConsts[i] == n) {
            return i;
        }
    }

    if(currNumConst == totalNumConsts) expandNum();
    assert(currNumConst < totalNumConsts);

    numConsts[currNumConst] = n;
    unsigned index = currNumConst;
    currNumConst++;
    return index;
}
unsigned libfuncs_newused(char *s) {
    if(!s) return 0;

    for(unsigned i = 0; i < currLibfunct; i++)
        if(libFuncs[i] && strcmp(libFuncs[i], s) == 0) return i;

    if(currLibfunct == totalNameLibfuncs) expandLibfunc();
    assert(currLibfunct < totalNameLibfuncs);

    char *newLibfunc = strdup(s);
    assert(newLibfunc != NULL);
    libFuncs[currLibfunct] = newLibfunc;

    unsigned index = currLibfunct;
    currLibfunct++;
    return index;
}
unsigned userfuncs_newused(SymTableEntry *s) {
    if(!s) return 0;

    for(unsigned i = 0; i < currUserfunct; i++) {
        if(userFuncs[i].address == s->taddress &&
                userFuncs[i].id && strcmp(userFuncs[i].id, s->name) == 0) {
            return i;
        }
    }

    if(currUserfunct == totalUserFuncs) expandUserfunc();
    assert(currUserfunct < totalUserFuncs);

    userFuncs[currUserfunct].address = s->taddress;
    userFuncs[currUserfunct].localSize = s->localCount;
    userFuncs[currUserfunct].saved_index = s->offset;
    userFuncs[currUserfunct].id = strdup(s->name);
    assert(userFuncs[currUserfunct].id != NULL);

    unsigned index = currUserfunct;
    currUserfunct++;
    return index;
}

void emit_tcode(instruction *instr) {
    expandInstructions();

    instructions[currInstructions].opcode = instr->opcode;
    instructions[currInstructions].srcLine = instr->srcLine;

    instructions[currInstructions].arg1 = malloc(sizeof(vmarg));
    instructions[currInstructions].arg2 = malloc(sizeof(vmarg));
    instructions[currInstructions].result = malloc(sizeof(vmarg));

    assert(instructions[currInstructions].arg1 != NULL);
    assert(instructions[currInstructions].arg2 != NULL);
    assert(instructions[currInstructions].result != NULL);

    *(instructions[currInstructions].arg1) = *(instr->arg1);
    *(instructions[currInstructions].arg2) = *(instr->arg2);
    *(instructions[currInstructions].result) = *(instr->result);

    ++currInstructions;
}

void make_operand(Expr *e, vmarg *arg) {

    assert(e != NULL || arg != NULL);

    switch(e->type) {
    case var_e:
    case tableitem_e:
    case arithexpr_e:
    case boolexpr_e:
    case newtable_e: {
        if(!e->sym) {
            arg->type = nil_a;
            arg->val = 0;
            return;
        }

        arg->val = e->sym->offset;

        switch(e->sym->space) {
        case 1:
            arg->type = global_a;
            break;
        case 2:
            arg->type = formal_a;
            break;
        case 3:
            arg->type = local_a;
            break;
        case 0:
        default:
            switch(e->sym->type) {
            case GLOBAL_VAR:
                arg->type = global_a;
                break;
            case LOCAL_VAR:
                arg->type = local_a;
                break;
            case FORMAL:
                arg->type = formal_a;
                break;
            default:
                assert(0 && "Invalid symbol type for variable operand");
                arg->type = nil_a;
                break;
            }
            break;
        }
        break;
    }
    case constbool_e: {
        arg->val = e->boolConst;
        arg->type = bool_a;
        break;
    }
    case conststring_e: {
        if(!e->strConst) {
            arg->type = nil_a;
            arg->val = 0;
            return;
        }
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
        if(!e->sym) {
            arg->type = nil_a;
            arg->val = 0;
            return;
        }
        arg->type = userfunc_a;
        arg->val = userfuncs_newused(e->sym);
        break;
    }
    case libraryfunc_e: {
        if(!e->sym || !e->sym->name) {
            arg->type = nil_a;
            arg->val = 0;
            return;
        }
        arg->type = libfunc_a;
        arg->val = libfuncs_newused(e->sym->name);
        break;
    }
    default:
        arg->type = nil_a;
        arg->val = 0;
        break;
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
    arg->type = nil_a;
    arg->val = 0;
}

void generate(void) {
    initInstructions();
    for(unsigned i = 1; i < curr_quad; ++i) {
        if(quads[i].op < 0 || quads[i].op >= sizeof(generators) / sizeof(generators[0])) {
            fprintf(stderr, "Error: Invalid opcode %d at quad %u\n", quads[i].op, i);
            exit(EXIT_FAILURE);
        }
        (*generators[quads[i].op])(quads + i);
    }
}

void generate_op(vmopcode op, Quad *quad) {
    instruction *instr = mallocInstr();

    instr->opcode = op;
    quad->taddress = nextinstructionlabel();
    instr->srcLine = quad->line;

    if(quad->arg1) make_operand(quad->arg1, instr->arg1);
    if(quad->arg2) make_operand(quad->arg2, instr->arg2);
    if(quad->result) make_operand(quad->result, instr->result);

    emit_tcode(instr);
    freeInstr(instr);
}

void generate_relational(vmopcode op, Quad *quad) {
    instruction *instr = mallocInstr();
    instr->opcode = op;
    quad->taddress = nextinstructionlabel();
    instr->srcLine = quad->line;

    if(quad->arg1) make_operand(quad->arg1, instr->arg1);
    if(quad->arg2) make_operand(quad->arg2, instr->arg2);
    instr->result->type = label_a;
    instr->result->val = quad->label;

    emit_tcode(instr);
    freeInstr(instr);
}

void generate_ADD(Quad *quad) {
    generate_op(add_v, quad);
}
void generate_SUB(Quad *quad) {
    generate_op(sub_v, quad);
}
void generate_MUL(Quad *quad) {
    generate_op(mul_v, quad);
}
void generate_DIV(Quad *quad) {
    generate_op(div_v, quad);
}
void generate_MOD(Quad *quad) {
    generate_op(mod_v, quad);
}

void generate_UMINUS(Quad *quad) {
    instruction *instr = mallocInstr();
    instr->opcode = mul_v;
    quad->taddress = nextinstructionlabel();
    instr->srcLine = quad->line;
    make_operand(quad->arg1, instr->arg1);
    make_numOperand(instr->arg2, -1.0);
    make_operand(quad->result, instr->result);
    emit_tcode(instr);
    freeInstr(instr);
}

void generate_NEWTABLE(Quad *quad)     {
    generate_op(newtable_v, quad);
}
void generate_TABLEGETELM(Quad *quad)  {
    generate_op(tablegetelem_v, quad);
}
void generate_TABLESETELEM(Quad *quad) {
    generate_op(tablesetelem_v, quad);
}
void generate_ASSIGN(Quad *quad)       {
    generate_op(assign_v, quad);
}
void generate_NOP(Quad *quad)          {
    instruction t;
    t.opcode = nop_v;
    emit_tcode(&t);
}
void generate_JUMP(Quad *quad)         {
    generate_relational(jump_v, quad);
}
void generate_IF_EQ(Quad *quad)        {
    generate_relational(jeq_v, quad);
}
void generate_IF_NOTEQ(Quad *quad)     {
    generate_relational(jne_v, quad);
}
void generate_IF_GREATER(Quad *quad)   {
    generate_relational(jgt_v, quad);
}
void generate_IF_GREATEREQ(Quad *quad) {
    generate_relational(jge_v, quad);
}
void generate_IF_LESS(Quad *quad)      {
    generate_relational(jlt_v, quad);
}
void generate_IF_LESSEQ(Quad *quad)    {
    generate_relational(jle_v, quad);
}

void generate_NOT(Quad *quad) {
    assert(0 && "generate_NOT should not be called: short-circuit front-end handles NOT");
}

void generate_OR(Quad *quad) {
    assert(0 && "generate_OR should not be called: short-circuit front-end handles OR");
}

void generate_AND(Quad *quad) {
    assert(0 && "generate_AND should not be called: short-circuit front-end handles AND");
}

void generate_PARAM(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction *instr = mallocInstr();
    instr->opcode = pusharg_v;
    make_operand(quad->arg1, instr->arg1);
    emit_tcode(instr);
    freeInstr(instr);
}
void generate_CALL(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction *instr = mallocInstr();
    instr->opcode = call_v;
    make_operand(quad->arg1, instr->arg1);
    emit_tcode(instr);
    freeInstr(instr);
}
void generate_GETRETVAL(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction *instr = mallocInstr();
    instr->opcode = assign_v;
    make_operand(quad->result, instr->result);
    make_retvalOperand(instr->arg1);
    emit_tcode(instr);
    freeInstr(instr);
}

void generate_FUNCSTART(Quad *quad) {
    SymTableEntry *func = quad->result->sym;
    func->taddress = nextinstructionlabel();
    quad->taddress = nextinstructionlabel();

    userfuncs_newused(func);
    pushStack(funcStack, func);

    instruction *instr = mallocInstr();
    instr->opcode = funcenter_v;
    make_operand(quad->result, instr->result);
    emit_tcode(instr);
    freeInstr(instr);
}

void generate_RETURN(Quad *quad) {
    quad->taddress = nextinstructionlabel();
    instruction *assignInstr = mallocInstr();
    assignInstr->opcode = assign_v;
    assignInstr->srcLine = quad->line;
    make_retvalOperand(assignInstr->result);

    if(quad->arg1) make_operand(quad->arg1, assignInstr->arg1);
    else {
        assignInstr->arg1->type = nil_a;
        assignInstr->arg1->val = 0;
    }

    emit_tcode(assignInstr);
    freeInstr(assignInstr);
}

void generate_FUNCEND(Quad *quad) {
    popStack(funcStack);

    quad->taddress = nextinstructionlabel();
    instruction *instr = mallocInstr();
    instr->opcode = funcexit_v;
    make_operand(quad->result, instr->result);
    emit_tcode(instr);
    freeInstr(instr);
}

void print_instructions() {
    printf("%-8s %-15s %-20s %-20s %-20s\n",
           "instr#", "opcode", "result", "arg1", "arg2");
    printf("--------------------------------------------------------------------------------\n");

    const char *opcodes[] = {
        "assign", "add", "sub", "mul", "div", "mod", "uminus", "and", "or", "not",
        "jeq", "jne", "jle", "jge", "jlt", "jgt", "jump", "call", "pusharg",
        "funcenter", "funcexit", "newtable", "tablegetelem", "tablesetelem", "nop"
    };

    const char *arg_types[] = {
        "label", "global", "formal", "local", "number", "string",
        "bool", "nil", "userfunc", "libfunc", "retval"
    };

    for(unsigned i = 1; i < currInstructions; i++) {
        instruction *instr = &instructions[i];
        char result_buf[64] = "";
        char arg1_buf[64] = "";
        char arg2_buf[64] = "";

        printf("%-8d %-15s ", i,
               instr->opcode < sizeof(opcodes) / sizeof(opcodes[0]) ?
               opcodes[instr->opcode] : "unknown");

        // Format result
        if(instr->result && instr->result->type != nil_a &&
                instr->result->type < sizeof(arg_types) / sizeof(arg_types[0])) {
            snprintf(result_buf, sizeof(result_buf), "%s:%d",
                     arg_types[instr->result->type], instr->result->val);
        }

        // Format arg1
        if(instr->arg1 && instr->arg1->type != nil_a &&
                instr->arg1->type < sizeof(arg_types) / sizeof(arg_types[0])) {
            snprintf(arg1_buf, sizeof(arg1_buf), "%s:%d",
                     arg_types[instr->arg1->type], instr->arg1->val);
        }

        // Format arg2
        if(instr->arg2 && instr->arg2->type != nil_a &&
                instr->arg2->type < sizeof(arg_types) / sizeof(arg_types[0])) {
            snprintf(arg2_buf, sizeof(arg2_buf), "%s:%d",
                     arg_types[instr->arg2->type], instr->arg2->val);
        }

        printf("%-20s %-20s %-20s\n", result_buf, arg1_buf, arg2_buf);
    }
    printf("--------------------------------------------------------------------------------\n");
}

void print_constants() {
    printf("String Constants:\n");
    for(unsigned i = 0; i < currStringConst; i++) {
        printf("  [%d]: \"%s\"\n", i, stringConsts[i]);
    }

    printf("Numeric Constants:\n");
    for(unsigned i = 0; i < currNumConst; i++) {
        printf("  [%d]: %g\n", i, numConsts[i]);
    }

    printf("Library Functions:\n");
    for(unsigned i = 0; i < currLibfunct; i++) {
        printf("  [%d]: %s\n", i, libFuncs[i]);
    }

    printf("User Functions:\n");
    for(unsigned i = 0; i < currUserfunct; i++) {
        printf("  [%d]: %s (addr: %d, locals: %d)\n",
               i, userFuncs[i].id, userFuncs[i].address, userFuncs[i].localSize);
    }
    printf("===============================\n");
}

void write_binary_file(FILE *file) {
    unsigned magic = AVM_MAGICNUMBER;
    fwrite(&magic, sizeof(unsigned), 1, file);

    fwrite(&programVarCount, sizeof(unsigned), 1, file);

    // Write string constants
    fwrite(&currStringConst, sizeof(unsigned), 1, file);
    for(unsigned i = 0; i < currStringConst; i++) {
        unsigned len = strlen(stringConsts[i]);
        fwrite(&len, sizeof(unsigned), 1, file);
        fwrite(stringConsts[i], sizeof(char), len, file);
    }

    // Write numeric constants
    fwrite(&currNumConst, sizeof(unsigned), 1, file);
    if(currNumConst > 0) {
        fwrite(numConsts, sizeof(double), currNumConst, file);
    }

    // Write library functions
    fwrite(&currLibfunct, sizeof(unsigned), 1, file);
    for(unsigned i = 0; i < currLibfunct; i++) {
        unsigned len = strlen(libFuncs[i]);
        fwrite(&len, sizeof(unsigned), 1, file);
        fwrite(libFuncs[i], sizeof(char), len, file);
    }

    // Write user functions
    fwrite(&currUserfunct, sizeof(unsigned), 1, file);
    for(unsigned i = 0; i < currUserfunct; i++) {
        fwrite(&userFuncs[i].address, sizeof(unsigned), 1, file);
        fwrite(&userFuncs[i].localSize, sizeof(unsigned), 1, file);
        fwrite(&userFuncs[i].saved_index, sizeof(unsigned), 1, file);

        unsigned len = strlen(userFuncs[i].id);
        fwrite(&len, sizeof(unsigned), 1, file);
        fwrite(userFuncs[i].id, sizeof(char), len, file);
    }

    // Write instructions
    unsigned instrCount = currInstructions - 1;
    fwrite(&instrCount, sizeof(unsigned), 1, file);

    for(unsigned i = 1; i < currInstructions; i++) {
        instruction *instr = &instructions[i];
        fwrite(&instr->opcode, sizeof(vmopcode), 1, file);
        fwrite(&instr->srcLine, sizeof(unsigned), 1, file);
        fwrite(instr->result, sizeof(vmarg), 1, file);
        fwrite(instr->arg1, sizeof(vmarg), 1, file);
        fwrite(instr->arg2, sizeof(vmarg), 1, file);
    }
}