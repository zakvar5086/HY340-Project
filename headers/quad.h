#ifndef QUAD_H
#define QUAD_H

#include "symtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum Expr_t {
    var_e,
    tableitem_e,

    programfunc_e,
    libraryfunc_e,

    arithexpr_e,
    boolexpr_e,
    assignexpr_e,
    newtable_e,

    constnum_e,
    constbool_e,
    conststring_e,
    
    nil_e
} Expr_t;

typedef enum iopcode {
    assign,         add,            sub,
    mul,            div_op,         mod_op,
    uminus,         and_op,         or_op,
    not_op,         if_eq,          if_noteq,
    if_lesseq,      if_greatereq,   if_less,
    if_greater,     jump,           call,
    param,          ret,            getretval,
    funcstart,      funcend,        tablecreate,
    tablegetelem,   tablesetelem
} iopcode;

typedef struct Expr {
    Expr_t type;
    SymTableEntry *sym;
    struct Expr *table;
    struct Expr *index;
    double numConst;
    char *strConst;
    unsigned char boolConst;
    struct Expr *next;

    unsigned truelist;
    unsigned falselist;
} Expr;

typedef struct Quad {
    iopcode op;
    Expr *result;
    Expr *arg1;
    Expr *arg2;
    unsigned label;
    unsigned line;
} Quad;

typedef struct stmt {
    unsigned breaklist;
    unsigned contlist;
    unsigned retlist;
} stmt_t;

void make_stmt(stmt_t **s);

typedef struct forstmt {
    unsigned test;
    unsigned enter;
} forstmt_t;

void emit(iopcode op, Expr *arg1, Expr *arg2, Expr *result, unsigned line);
unsigned nextQuadLabel(void);
void expandQuads(void);
void resetTemp(void);
char *newtempname(void);
SymTableEntry *newtemp(SymTable *symTable, unsigned int scope);
void initQuads(void);
void printQuads(void);

Expr *newExpr(Expr_t type);
Expr *newExpr_constnum(double val);
Expr *newExpr_constbool(unsigned char val);
Expr *newExpr_conststring(char *val);
Expr *newExpr_id(SymTableEntry *sym);
Expr *newExpr_nil(void);

const char *getOpcodeName(iopcode op);
const char *getExprType(Expr_t type);
const char *exprToString(Expr *e);
int isArithExpr(Expr *e);

extern unsigned int temp_counter;

extern Quad *quads;
extern unsigned total_quads;
extern unsigned curr_quad;

unsigned newlist(unsigned i);
unsigned mergelist(unsigned l1, unsigned l2);
void patchlist(unsigned list, unsigned label);
void patchlabel(unsigned quad, unsigned label);
Expr* evaluate(Expr* expr);
Expr* emit_eval(Expr* expr);
Expr* emit_eval_var(Expr* expr);

Expr* emit_iftableitem(Expr* expr);
Expr* member_item(Expr* lvalue, Expr* index);
Expr* handle_tableitem_assignment(Expr* lvalue, Expr* expr);

Expr* create_table();
void add_table_element(Expr* table, unsigned index, Expr* value);
void add_indexed_element(Expr* table, Expr* index, Expr* value);

#endif