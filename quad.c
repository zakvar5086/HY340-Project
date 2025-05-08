#include "quad.h"

/* Global variables for quads management */
Quad *quads = NULL;
unsigned total_quads = 0;
unsigned curr_quad = 0;
unsigned int temp_counter = 0;

#define EXPAND_SIZE 1024
#define CURR_SIZE (total_quads * sizeof(Quad))
#define NEW_SIZE (EXPAND_SIZE * sizeof(Quad) + CURR_SIZE)

/* Allocate memory for quads */
void initQuads() {
    quads = malloc(EXPAND_SIZE * sizeof(Quad));
    if(!quads) {
        fprintf(stderr, "Error: Memory allocation failed for quads\n");
        exit(1);
    }
    total_quads = EXPAND_SIZE;
    curr_quad = 0;
    temp_counter = 0;
}

/* Expand quads array when needed */
void expandQuads() {
    if(curr_quad == total_quads) {
        quads = realloc(quads, NEW_SIZE);
        if(!quads) {
            fprintf(stderr, "Error: Memory reallocation failed for quads\n");
            exit(1);
        }
        total_quads += EXPAND_SIZE;
    }
}

/* Get next quad label (current index) */
unsigned nextQuadLabel() { return curr_quad; }

/* Reset temp counter */
void resetTemp() { temp_counter = 0; }

/* Create a new temp variable name */
char *newtempname() {
    char *name = malloc(16 * sizeof(char));
    if(!name) {
        fprintf(stderr, "Error: Memory allocation failed for temp name\n");
        exit(1);
    }
    sprintf(name, "_t%u", temp_counter);
    ++temp_counter;
    return name;
}

/* Create a new temp variable in symbol table */
SymTableEntry *newtemp(SymTable *symTable, unsigned int scope) {
    char *name = newtempname();
    SymTableEntry *sym = SymTable_Insert(symTable, name, scope, 0, LOCAL_VAR);
    free(name);
    return sym;
}

/* Create a new expression */
Expr *newExpr(Expr_t type) {
    Expr *e = malloc(sizeof(Expr));
    if(!e) {
        fprintf(stderr, "Error: Memory allocation failed for expression\n");
        exit(1);
    }
    memset(e, 0, sizeof(Expr));
    e->type = type;
    return e;
}

/* Create a new constant number expression */
Expr *newExpr_constnum(double val) {
    Expr *e = newExpr(constnum_e);
    e->numConst = val;
    return e;
}

/* Create a new constant boolean expression */
Expr *newExpr_constbool(unsigned char val) {
    Expr *e = newExpr(constbool_e);
    e->boolConst = val;
    return e;
}

/* Create a new constant string expression */
Expr *newExpr_conststring(char *val) {
    Expr *e = newExpr(conststring_e);
    e->strConst = strdup(val);
    return e;
}

/* Create a new symbol (variable, function) expression */
Expr *newExpr_id(SymTableEntry *sym) {
    Expr *e = newExpr(var_e);
    e->sym = sym;
    return e;
}

/* Create a nil expression */
Expr *newExpr_nil() { return newExpr(nil_e); }

/* Emit a new quad instruction */
void emit(iopcode op, Expr *arg1, Expr *arg2, Expr *result, unsigned line) {
    expandQuads();
    quads[curr_quad].op = op;
    quads[curr_quad].arg1 = arg1;
    quads[curr_quad].arg2 = arg2;
    quads[curr_quad].result = result;
    quads[curr_quad].line = line;
    quads[curr_quad].label = 0;
    curr_quad++;
}

/* Get string representation of opcode */
const char *getOpcodeName(iopcode op) {
    switch (op) {
        case assign: return "assign";
        case add: return "add";
        case sub: return "sub";
        case mul: return "mul";
        case div_op: return "div";
        case mod_op: return "mod";
        case uminus: return "uminus";
        case and_op: return "and";
        case or_op: return "or";
        case not_op: return "not";
        case if_eq: return "if_eq";
        case if_noteq: return "if_noteq";
        case if_lesseq: return "if_lesseq";
        case if_greatereq: return "if_greatereq";
        case if_less: return "if_less";
        case if_greater: return "if_greater";
        case jump: return "jump";
        case call: return "call";
        case param: return "param";
        case ret: return "ret";
        case getretval: return "getretval";
        case funcstart: return "funcstart";
        case funcend: return "funcend";
        case tablecreate: return "tablecreate";
        case tablegetelem: return "tablegetelem";
        case tablesetelem: return "tablesetelem";
        default: return "unknown";
    }
}

/* Get string representation of expression type */
const char *getExprType(Expr_t type) {
    switch (type) {
        case var_e: return "var";
        case tableitem_e: return "tableitem";
        case programfunc_e: return "programfunc";
        case libraryfunc_e: return "libraryfunc";
        case arithexpr_e: return "arithexpr";
        case boolexpr_e: return "boolexpr";
        case assignexpr_e: return "assignexpr";
        case newtable_e: return "newtable";
        case constnum_e: return "constnum";
        case constbool_e: return "constbool";
        case conststring_e: return "conststring";
        case nil_e: return "nil";
        default: return "unknown";
    }
}

/* Helper function to get string representation of expression for printing */
char *exprToString(Expr *e) {
    static char buffer[256];
    
    if(!e) return "NULL";
    
    switch (e->type) {
        case var_e:
            if(e->sym) return e->sym->name;
            return "var";
            
        case programfunc_e:
            if(e->sym) return e->sym->name;
            return "programfunc";
            
        case libraryfunc_e:
            if(e->sym) return e->sym->name;
            return "libraryfunc";
            
        case arithexpr_e:
        case boolexpr_e:
        case assignexpr_e:
        case newtable_e:
            if(e->sym) return e->sym->name;
            break;

        case tableitem_e:
            if(e->sym) {
                if(e->index && e->index->type == conststring_e) 
                    sprintf(buffer, "%s.%s", e->sym->name, e->index->strConst);
                else if(e->index) 
                    sprintf(buffer, "%s[...]", e->sym->name);
                else 
                    return e->sym->name;
                return buffer;
            }
            break;
        case constnum_e:
            sprintf(buffer, "%g", e->numConst);
            return buffer;
        case constbool_e:
            return e->boolConst ? "true" : "false";
        case conststring_e:
            if(e->strConst) {
                sprintf(buffer, "\"%s\"", e->strConst);
                return buffer;
            }
            return "\"\"";
        case nil_e:
            return "nil";
    }
    
    return getExprType(e->type);
}

/* Print all quads (for debugging) */
void printQuads() {
    printf("%-10s %-15s %-15s %-15s %-15s %-5s\n", 
           "quad#", "opcode", "result", "arg1", "arg2", "line");
    printf("---------------------------------------------------------------------------------\n");
    
    for(unsigned i = 0; i < curr_quad; i++) {
        Quad q = quads[i];
        printf("%-10d %-15s %-15s %-15s %-15s %-5d\n", 
               i, 
               getOpcodeName(q.op), 
               exprToString(q.result),
               exprToString(q.arg1), 
               exprToString(q.arg2), 
               q.line);
    }
    printf("---------------------------------------------------------------------------------\n");
}