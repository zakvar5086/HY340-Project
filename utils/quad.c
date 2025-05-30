#include "../headers/quad.h"

extern unsigned yylineno;
extern SymTable *symTable;
extern unsigned int currentScope;

Quad *quads = NULL;
unsigned quadsSize = 0;
unsigned curr_quad = 0;
unsigned int temp_counter = 0;
unsigned int offset;
char labelStr[256] = "";

#define EXPAND_SIZE 1024
#define CURR_SIZE (quadsSize * sizeof(Quad))
#define NEW_SIZE (EXPAND_SIZE * sizeof(Quad) + CURR_SIZE)

void initQuads() {
    quads = malloc(EXPAND_SIZE * sizeof(Quad));
    memset(quads, 0, EXPAND_SIZE * sizeof(Quad));
    if(!quads) {
        fprintf(stderr, "Error: Memory allocation failed for quads\n");
        exit(1);
    }
    quadsSize = EXPAND_SIZE;
    curr_quad = 1;
    temp_counter = 0;
}

void expandQuads() {
    if(curr_quad == quadsSize) {
        quads = realloc(quads, NEW_SIZE);
        if(!quads) {
            fprintf(stderr, "Error: Memory reallocation failed for quads\n");
            exit(1);
        }
        quadsSize += EXPAND_SIZE;
    }
}

unsigned nextQuadLabel() { return curr_quad; }

void resetTemp() { temp_counter = 0; }

unsigned int istempname(SymTableEntry *s) {
    if (!s || !s->name) return 0;
    return *(s->name) == '_';
}

unsigned int istempexpr(Expr* e) {
    if (!e || !e->sym) return 0;
    if (e->type != var_e) return 0;
    return istempname(e->sym);
}

char *newtempname() {
    char *name = malloc(16 * sizeof(char));
    memset(name, 0, 16 * sizeof(char));
    if(!name) {
        fprintf(stderr, "Error: Memory allocation failed for temp name\n");
        exit(1);
    }
    sprintf(name, "_t%u", temp_counter);
    ++temp_counter;
    return name;
}

SymTableEntry *newtemp() {
    char *name = newtempname();
    SymTableEntry *tmp = SymTable_LookupAny(symTable, name);
    if(!tmp){
        tmp = SymTable_Insert(symTable, name, currentScope, 0, LOCAL_VAR);
        tmp->offset = offset++;
    }
    free(name);
    return tmp;
}

Expr *newExpr(Expr_t type) {
    Expr *e = malloc(sizeof(Expr));
    if(!e) {
        fprintf(stderr, "Error: Memory allocation failed for expression\n");
        exit(1);
    }
    memset(e, 0, sizeof(Expr));
    e->type = type;

    if(type == boolexpr_e){
        e->truelist = 0;
        e->falselist = 0;
    }
    return e;
}

Expr *newExpr_constnum(double val) {
    Expr *e = newExpr(constnum_e);
    e->numConst = val;
    return e;
}

Expr *newExpr_constbool(unsigned char val) {
    Expr *e = newExpr(constbool_e);
    e->boolConst = val;
    return e;
}

Expr *newExpr_conststring(char *val) {
    Expr *e = newExpr(conststring_e);
    e->strConst = strdup(val);
    return e;
}

Expr *newExpr_id(SymTableEntry *sym) {
    Expr *e = newExpr(var_e);
    e->sym = sym;
    return e;
}

Expr *newExpr_nil() { return newExpr(nil_e); }

void emit(iopcode op, Expr *arg1, Expr *arg2, Expr *result, unsigned label) {
    expandQuads();
    quads[curr_quad].op = op;
    quads[curr_quad].arg1 = arg1;
    quads[curr_quad].arg2 = arg2;
    quads[curr_quad].result = result;
    quads[curr_quad].line = yylineno;
    quads[curr_quad].label = label;
    curr_quad++;
}

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

const char *exprToString(Expr *e) {
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

int isArithExpr(Expr *e) {
    if( e->type == constbool_e ||
        e->type == conststring_e ||
        e->type == nil_e ||
        e->type == newtable_e ||
        e->type == programfunc_e ||
        e->type == libraryfunc_e ||
        e->type == boolexpr_e)
        return 0;
    
    return 1;
}

void printQuads() {
    printf("%-10s %-15s %-15s %-15s %-15s %-5s\n", 
           "quad#", "opcode", "result", "arg1", "arg2", "label");
    printf("---------------------------------------------------------------------------------\n");
    
    for(unsigned i = 1; i < curr_quad; i++) {
        Quad q = quads[i];
        
        const char *opStr = getOpcodeName(q.op);
        char resultStr[256] = "";
        char arg1Str[256] = "";
        char arg2Str[256] = "";
        
        if(q.result) {
            const char *temp = exprToString(q.result);
            if(temp && strcmp(temp, "NULL") != 0) {
                strncpy(resultStr, temp, 255);
                resultStr[255] = '\0';
            }
        }
        
        if(q.arg1) {
            const char *temp = exprToString(q.arg1);
            if(temp && strcmp(temp, "NULL") != 0) {
                strncpy(arg1Str, temp, 255);
                arg1Str[255] = '\0';
            }
        }
        
        if(q.arg2) {
            const char *temp = exprToString(q.arg2);
            if(temp && strcmp(temp, "NULL") != 0) {
                strncpy(arg2Str, temp, 255);
                arg2Str[255] = '\0';
            }
        }
        
        if(q.label != 0) sprintf(labelStr, "%d", q.label);
        else sprintf(labelStr, " ");
        
        printf("%-10d %-15s %-15s %-15s %-15s %-5s\n", 
               i, opStr, resultStr, arg1Str, arg2Str, labelStr);
    }
    printf("---------------------------------------------------------------------------------\n");
}

void make_stmt(stmt_t **s) {
    *s = malloc(sizeof(stmt_t));
    memset(*s, 0, sizeof(stmt_t));
    (*s)->breaklist = 0;
    (*s)->contlist = 0;
    (*s)->retlist = 0;
}

unsigned newlist(unsigned i) {
    
    if(i >= curr_quad) {
        printf("ERROR: Invalid quad index %u (max is %u)\n", i, curr_quad-1);
        return 0;
    }
    
    quads[i].label = 0;
    return i;
}
 
void patchlist(unsigned list, unsigned label) {
    while(list) {
        if(list >= curr_quad) {
            printf("ERROR: Invalid quad index %u\n", list);
            break;
        }
        
        unsigned next = quads[list].label;
        quads[list].label = label;
        
        if(next == list) {
            printf("ERROR: Self-referential list at quad #%u\n", list);
            break;
        }
        
        list = next;
    }
}

void patchlabel(unsigned quad, unsigned label) {
    if(quads[quad].label != 0) {
        printf("ERROR: Quad #%u already patched\n", quad);
        return;
    }
    
    quads[quad].label = label;
}

unsigned mergelist(unsigned l1, unsigned l2) {
    if(!l1) return l2;
    if(!l2) return l1;

    if(l1 >= curr_quad || l2 >= curr_quad) {
        printf("ERROR: Invalid quad index %u or %u\n", l1, l2);
        return 0;
    }

    unsigned i = l1;
    while(i != 0 && quads[i].label != 0) {
        if(quads[i].label == i) {
            printf("ERROR: Self-referential list at quad #%u\n", i);
            break;
        }
        i = quads[i].label;
    }

    if(i == 0) {
        printf("WARNING: First list has a zero entry\n");
        return l2;
    }

    quads[i].label = l2;
    return l1;
}
 
Expr* evaluate(Expr* expr) {
    if(expr->type == boolexpr_e) return expr;
    
    Expr* result = newExpr(boolexpr_e);
    result->truelist = nextQuadLabel();
    result->falselist = nextQuadLabel() + 1;

    emit(if_eq, expr, newExpr_constbool(1), NULL, 0);
    emit(jump, NULL, NULL, NULL, 0);

    return result;
}
 
Expr* emit_eval(Expr* expr) {
    Expr *res = expr;

    if(expr->type != boolexpr_e) return expr;

    res = newExpr(boolexpr_e);
    res->sym = newtemp();

    patchlist(expr->truelist, nextQuadLabel());
    patchlist(expr->falselist, nextQuadLabel() + 2);

    emit(assign, newExpr_constbool(1), NULL, res, 0);
    emit(jump, NULL, NULL, NULL, nextQuadLabel() + 2);
    emit(assign, newExpr_constbool(0), NULL, res, 0);

    return res;
}

Expr* emit_eval_var(Expr* expr) {
    Expr *res = expr;

    if(expr->type != boolexpr_e) return expr;

    res = newExpr(var_e);
    res->sym = newtemp();

    patchlist(expr->truelist, nextQuadLabel());
    patchlist(expr->falselist, nextQuadLabel() + 2);

    emit(assign, newExpr_constbool(1), NULL, res, 0);
    emit(jump, NULL, NULL, NULL, nextQuadLabel() + 2);
    emit(assign, newExpr_constbool(0), NULL, res, 0);

    return res;
}

Expr* emit_iftableitem(Expr* expr) {
    if(expr->type != tableitem_e) return expr;
    
    Expr* result = newExpr(var_e);
    result->sym = newtemp();
    result->strConst = (char *)result->sym->name;
    
    Expr* table = emit_iftableitem(expr->table);
    emit(tablegetelem, table, expr->index, result, 0);
    
    return result;
}

Expr* member_item(Expr* lvalue, Expr* index) {
    if(!lvalue) return NULL;
    
    lvalue = emit_iftableitem(lvalue);
    
    Expr* item = newExpr(tableitem_e);
    item->table = lvalue;
    item->index = index;
    
    return item;
}

Expr* make_call(Expr *lvalue, Expr *elist) {
    Expr *func = emit_iftableitem(lvalue);

    Expr *reversed_elist = NULL;
    while(elist) {
        Expr *next = elist->next;
        elist->next = reversed_elist;
        reversed_elist = elist;
        elist = next;
    }

    while(reversed_elist) {
        emit(param, reversed_elist, NULL, NULL, 0);
        reversed_elist = reversed_elist->next;
    }
    emit(call, func, NULL, NULL, 0);

    Expr *result = newExpr(var_e);
    result->sym = istempexpr(lvalue) ? lvalue->sym : newtemp();
    emit(getretval, NULL, NULL, result, 0);
    return result;
}

Expr* handle_method_call(Expr* lvalue, FunctCont_t* callsuffix) {
    lvalue = emit_iftableitem(lvalue);
    Expr *elist = callsuffix->elist;

    if(callsuffix->method) {
        Expr *tempExpr = lvalue;
        lvalue = emit_iftableitem(member_item(lvalue, newExpr_conststring(callsuffix->name)));

        while(elist && elist->next) elist = elist->next;
        if(elist) elist->next = tempExpr;
    }
    return make_call(lvalue, elist);
}

Expr* handle_tableitem_assignment(Expr* lvalue, Expr* expr) {
    if(!lvalue || lvalue->type != tableitem_e) return NULL;
    
    Expr* table = emit_iftableitem(lvalue->table);
    
    if(expr->type == boolexpr_e) expr = emit_eval_var(expr);
    emit(tablesetelem, lvalue->index, expr, table, 0);
    
    Expr* result = newExpr(var_e);
    result->sym = newtemp();
    emit(tablegetelem, table, lvalue->index, result, 0);
    
    return result;
}

Expr* create_table() {
    Expr* table = newExpr(newtable_e);
    table->sym = newtemp();
    emit(tablecreate, NULL, NULL, table, 0);
    return table;
}

void add_table_element(Expr* table, unsigned index, Expr* value) {
    Expr* indexExpr = newExpr_constnum((double)index);
    emit(tablesetelem, value, indexExpr, table, 0);
}

void add_indexed_element(Expr* table, Expr* index, Expr* value) {
    emit(tablesetelem, index, value, table, 0);
}