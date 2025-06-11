%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symtable.h"
#include "quad.h"
#include "stack.h"
#include "targetcode.h"
#include "parser.tab.h"

#define YY_DECL int yylex(void)
extern int yylex(void);
extern FILE *yyin;

extern int yylineno;
void yyerror(const char *msg);

unsigned int currentScope = 0;
int isFunctionScopes[MAX_SCOPE] = {0};
int isFunctionScope(unsigned int scope);
int isLoop = 0;

static void *offsetStack;
static int currentOffset = 0;
void initOffsetStack();

static int anon_func = 0;
SymTable* symTable;

static SymTableEntry *currFunc = NULL;
static unsigned functionBlockScope = 0;

void printEverything(int symbols, int quads, int instructions, int constants);
%}

%union {
    char* stringValue;
    int intValue;
    double realValue;
    SymTableEntry *symEntry;
    Expr *exprNode;
    stmt_t *stmtValue;
    forstmt_t *forValue;
    FunctCont_t *functcont;
}

%token <stringValue> IDENTIFIER STRINGCONST
%token <intValue> INTCONST
%token <realValue> REALCONST
%token IF ELSE WHILE FOR FUNCTION RETURN BREAK CONTINUE
%token AND NOT OR LOCAL TRUE FALSE NIL
%token ASSIGN PLUS MINUS MULTIPLY DIVIDE MOD
%token EQUAL NEQUAL INCR DECR GREATER LESS GREATER_EQUAL LESS_EQUAL
%token LBRACE RBRACE LBRACKET RBRACKET LPAREN RPAREN
%token SEMICOLON COMMA COLON DBL_COLON DOT DBL_DOT

%right ASSIGN
%left OR
%left AND
%nonassoc EQUAL NEQUAL
%nonassoc GREATER GREATER_EQUAL LESS LESS_EQUAL
%left PLUS MINUS
%left MULTIPLY DIVIDE MOD
%right NOT INCR DECR UMINUS
%left DOT DBL_DOT
%left LBRACKET RBRACKET
%left LPAREN RPAREN

%type <symEntry> funcdef funcprefix
%type <exprNode> expr term assignexpr primary const call member lvalue
%type <exprNode> elist elist_expr notempty_elist objectdef indexed indexedelem indexedelem_list idlist_tail
%type <intValue> M
%type <stmtValue> stmt stmt_list ifstmt whilestmt forstmt block
%type <intValue> ifprefix jumpandsavepos whilestart whilecond returnstmt
%type <forValue> forprefix
%type <functcont> methodcall normcall callsuffix

%expect 1

%%

program:    stmt_list
            ;

stmt_list:  { make_stmt(&$$); }
            | stmt stmt_list {
                $$ = $1;
                $$->breaklist = mergelist($1->breaklist, $2->breaklist);
                $$->contlist = mergelist($1->contlist, $2->contlist);
                $$->retlist = mergelist($1->retlist, $2->retlist);
            }
            ;

stmt:       expr SEMICOLON {
                $1 = emit_eval_var($1);
                resetTemp();
                make_stmt(&$$);
            }
            | ifstmt
            | whilestmt
            | forstmt
            | returnstmt {
                make_stmt(&$$);
                $$->retlist = newlist($1);
            }
            | BREAK SEMICOLON {
                make_stmt(&$$);
                emit(jump, NULL, NULL, NULL, 0);
                $$->breaklist = newlist(nextQuadLabel() - 1);

                if(!isLoop)
                    yyerror("Break statement outside of loop");
            }
            | CONTINUE SEMICOLON {
                make_stmt(&$$);
                emit(jump, NULL, NULL, NULL, 0);
                $$->contlist = newlist(nextQuadLabel() - 1);

                if(!isLoop)
                    yyerror("Continue statement outside of loop");
            }
            | block
            | funcdef { make_stmt(&$$); }
            | SEMICOLON { make_stmt(&$$); }
            ;

expr:       expr PLUS expr {
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '+' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($1) ? $1->sym : newtemp();;
                emit(add, $1, $3, $$, 0);
            }
            | expr MINUS expr {
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '-' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($1) ? $1->sym : newtemp();;
                emit(sub, $1, $3, $$, 0);
            }
            | expr MULTIPLY expr {
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '*' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($1) ? $1->sym : newtemp();;
                emit(mul, $1, $3, $$, 0);
            }
            | expr DIVIDE expr {
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '/' operator (line %d)\n", yylineno);
                if($3->type == constnum_e && $3->numConst == 0)
                    fprintf(stderr, "\033[1;31mError:\033[0m Division by zero (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($1) ? $1->sym : newtemp();;
                emit(div_op, $1, $3, $$, 0);
            }
            | expr MOD expr {
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '%%' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($1) ? $1->sym : newtemp();;
                emit(mod_op, $1, $3, $$, 0);
            }
            | expr GREATER { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_greater, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr GREATER_EQUAL { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_greatereq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr LESS { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_less, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr LESS_EQUAL { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_lesseq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr EQUAL { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_eq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr NEQUAL  { if($1->type == boolexpr_e) $1 = emit_eval($1) } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                $$ = newExpr(boolexpr_e);

                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_noteq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr AND { if($1->type!=boolexpr_e) $1 = evaluate($1); } M expr {
                if($5->type != boolexpr_e) $5 = evaluate($5);
                patchlist($1->truelist, $4);

                $$ = newExpr(boolexpr_e);
                $$->truelist = $5->truelist;
                $$->falselist = mergelist($1->falselist, $5->falselist);
            }
            | expr OR { if($1->type!=boolexpr_e) $1 = evaluate($1); } M expr {
                if($5->type != boolexpr_e) $5 = evaluate($5);
                patchlist($1->falselist, $4);

                $$ = newExpr(boolexpr_e);
                $$->truelist = mergelist($1->truelist, $5->truelist);
                $$->falselist = $5->falselist;
            }
            | term { $$ = $1; }
            | assignexpr { $$ = $1; }
            ;

M:          { $$ = nextQuadLabel(); }

term:       LPAREN expr RPAREN { $$ = $2; if($2->type == boolexpr_e) $2 = emit_eval($2); }
            | MINUS expr %prec UMINUS {
                $$ = newExpr(arithexpr_e);
                $$->sym = istempexpr($2) ? $2->sym : newtemp();
                emit(uminus, $2, NULL, $$, 0);
            }
            | NOT expr {
                if($2->type != boolexpr_e) $2 = evaluate($2);
                
                unsigned temp1 = $2->truelist;
                unsigned temp2 = $2->falselist;

                $$ = $2;
                $$->truelist = temp2;
                $$->falselist = temp1;
            }
            | INCR lvalue {
                if($2 && $2->sym && ($2->sym->type == USERFUNC || $2->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);

                if($2->type == tableitem_e) {
                    Expr *tmp = emit_iftableitem($2);

                    emit(add, tmp, newExpr_constnum(1), tmp, 0);
                    emit(tablesetelem, $2->index, tmp, $2->table, 0);
                    $$ = tmp;
                } else {
                    emit(add, $2, newExpr_constnum(1), $2, 0);
                    $$ = newExpr(arithexpr_e);
                    $$->sym = newtemp();
                    emit(assign, $2, NULL, $$, 0);
                }
            }
            | lvalue INCR {
                if($1 && $1->sym && ($1->sym->type == USERFUNC || $1->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp();

                if($1->type == tableitem_e) {
                    Expr *tmp = emit_iftableitem($1);

                    emit(assign, tmp, NULL, $$, 0);
                    emit(add, tmp, newExpr_constnum(1), tmp, 0);
                    emit(tablesetelem, $1->index, tmp, $1->table, 0);
                    $$ = tmp;
                } else {
                    emit(assign, $1, NULL, $$, 0);
                    emit(add, $1, newExpr_constnum(1), $1, 0);
                }
            }
            | DECR lvalue {
                if($2 && $2->sym && ($2->sym->type == USERFUNC || $2->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);

                if($2->type == tableitem_e) {
                    Expr *tmp = emit_iftableitem($2);

                    emit(sub, tmp, newExpr_constnum(1), tmp, 0);
                    emit(tablesetelem, $2->index, tmp, $2->table, 0);
                    $$ = tmp;
                } else {
                    emit(sub, $2, newExpr_constnum(1), $2, 0);
                    $$ = newExpr(arithexpr_e);
                    $$->sym = newtemp();
                    emit(assign, $2, NULL, $$, 0);
                }
            }
            | lvalue DECR {
                if($1 && $1->sym && ($1->sym->type == USERFUNC || $1->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp();

                if($1->type == tableitem_e) {
                    Expr *tmp = emit_iftableitem($1);

                    emit(assign, tmp, NULL, $$, 0);
                    emit(sub, tmp, newExpr_constnum(1), tmp, 0);
                    emit(tablesetelem, $1->index, tmp, $1->table, 0);
                    $$ = tmp;
                } else {
                    emit(assign, $1, NULL, $$, 0);
                    emit(sub, $1, newExpr_constnum(1), $1, 0);
                }
            }
            | primary { $$ = $1; }
            ;

assignexpr: lvalue ASSIGN expr {
                if($1) {
                    if($1->type == tableitem_e)
                        $$ = handle_tableitem_assignment($1, $3);
                    else {
                        if($3->type == boolexpr_e) $3 = emit_eval_var($3);

                        $$ = newExpr(var_e);
                        $$->sym = newtemp();
                        emit(assign, $3, NULL, $1, 0);
                        emit(assign, $1, NULL, $$, 0);
                    }
                } else {
                    $$ = NULL;
                }
            }
            ;

primary:    lvalue {
                if($1) {
                    if($1->type == tableitem_e) {
                        $$ = emit_iftableitem($1);
                    } else if($1->sym && $1->sym->type == USERFUNC) {
                        $$ = newExpr(programfunc_e);
                        $$->sym = $1->sym;
                    } else if($1->sym && $1->sym->type == LIBFUNC) {
                        $$ = newExpr(libraryfunc_e);
                        $$->sym = $1->sym;
                    } else $$ = $1;
                } else $$ = NULL;
            }
            | call { $$ = $1; }
            | objectdef { $$ = $1; }
            | LPAREN funcdef RPAREN { 
                if($2) {
                    $$ = newExpr(programfunc_e);
                    $$->sym = $2;
                } else $$ = NULL;
            }
            | const { $$ = $1; }
            ;

lvalue:     IDENTIFIER {
                int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $1, currentScope);

                if(pCurr) {
                    $$ = newExpr_id(pCurr);
                } else {
                    int found = 0;
                    int scope;
                    for(scope = currentScope - 1; scope >= 0; scope--) {
                        SymTableEntry *outer = SymTable_Lookup(symTable, $1, scope);
                        if(!outer) continue;

                        found = 1;
                        if(isFunctionScope(currentScope) &&
                           (outer->type == LOCAL_VAR || outer->type == FORMAL) &&
                           isLoop == 0 &&
                           outer->isActive) {
                            fprintf(stderr, "\033[1;31mError:\033[0m Cannot access symbol '%s' at line %d.\n", $1, yylineno);
                            $$ = NULL;
                        } else {
                            $$ = newExpr_id(outer);
                        }
                        break;
                    }

                    if(!found) {
                        pCurr = SymTable_Lookup(symTable, $1, 0);
                        if(pCurr && (pCurr->type == LIBFUNC || 
                                    pCurr->type == USERFUNC || 
                                    pCurr->type == GLOBAL_VAR)) {
                            $$ = newExpr_id(pCurr);
                        } else {
                            SymTableEntry *newSym = SymTable_Insert(symTable, $1, currentScope, yylineno, varType);
                            if(newSym) {
                                newSym->offset = currentOffset++;
                                $$ = newExpr_id(newSym);
                            }
                            else $$ = NULL;
                        }
                    }
                }
            }
            | LOCAL IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);

                if(pCurr) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Redeclaration of symbol '%s' in scope %u (line %d)\n", $2, currentScope, yylineno);
                    $$ = newExpr_id(pCurr);
                } else {
                    int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                    SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);

                    if(libFunc && libFunc->type == LIBFUNC) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' shadows library function (line %d)\n", $2, yylineno);
                        $$ = NULL;
                    } else {
                        SymTableEntry *pNew = SymTable_Insert(symTable, $2, currentScope, yylineno, varType);
                        if(pNew){
                            pNew->offset = currentOffset++;
                            $$ = newExpr_id(pNew);
                        } else $$ = NULL;
                    }
                }
            }
            | DBL_COLON IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, 0);

                if(pCurr) $$ = newExpr_id(pCurr);
                else {
                    fprintf(stderr, "\033[1;31mError:\033[0m Global symbol '::%s' not found (line %d)\n", $2, yylineno);
                    $$ = NULL;
                }
            }
            | member {
                $$ = $1;
            }
            ;

member:     lvalue DOT IDENTIFIER {
                if($1->type == var_e) {
                    int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                    SymTableEntry *tmp = SymTable_LookupAny(symTable, $1->sym->name);
                    if(!tmp) $1->sym = SymTable_Insert(symTable, $1->sym->name, currentScope, yylineno, varType);
                }
                $$ = member_item($1, newExpr_conststring($3));
            }
            | lvalue LBRACKET expr RBRACKET {
                if($1->type == var_e) {
                    int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                    SymTableEntry *tmp = SymTable_LookupAny(symTable, $1->sym->name);
                    if(!tmp) $1->sym = SymTable_Insert(symTable, $1->sym->name, currentScope, yylineno, varType);
                }
                $$ = member_item($1, $3);
            }
            | call DOT IDENTIFIER { 
                $$ = $1;
            }
            | call LBRACKET expr RBRACKET { 
                $$ = $1;
            }
            ;

call:       call LPAREN elist RPAREN {
                $$ = make_call($1, $3);
            }
            | lvalue callsuffix {
                if(!$1) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid lvalue in call (line %d)\n", yylineno);
                    $$ = newExpr(nil_e);
                } 
                else if($1->type == tableitem_e) $$ = handle_method_call($1, $2);
                else if(!$1->sym) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Symbol table entry is NULL (line %d)\n", yylineno);
                    $$ = newExpr(nil_e);
                }
                else {
                    SymTableEntry *tmp;
                    if($1->sym->name && !istempname($1->sym)) tmp = SymTable_LookupAny(symTable, $1->sym->name);
                    else tmp = $1->sym;

                    if(!tmp) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Symbol not found (line %d)\n", yylineno);
                        $$ = newExpr(nil_e);
                    }
                    else if(tmp->type == LOCAL && tmp->scope > currentScope) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Cannot access local variable '%s' in outer scope %u (line %d)\n", (tmp->name ? tmp->name : "unknown"), tmp->scope, yylineno);
                        $$ = newExpr(nil_e);
                    }
                    else {
                        $1->sym = tmp;
                        $$ = handle_method_call($1, $2);
                        
                        if($$->type == nil_e && $1->sym && $1->sym->name) fprintf(stderr, "\033[1;31mError:\033[0m Call to non-function '%s' (line %d)\n", $1->sym->name, yylineno);
                    }
                }
            }
            | LPAREN funcdef RPAREN LPAREN elist RPAREN {
                if($2) {
                    Expr *func = newExpr(programfunc_e);
                    func->sym = $2;

                    $$ = make_call(func, $5);
                } else $$ = NULL;
            }
            ;

callsuffix: normcall { $$ = $1; }
            | methodcall { $$ = $1; }
            ;

normcall:   LPAREN elist RPAREN {
                $$ = malloc(sizeof(FunctCont_t));
                $$->elist = $2;
                $$->method = 0;
                $$->name = NULL;
            }
            ;

methodcall: DBL_DOT IDENTIFIER LPAREN elist RPAREN {
                $$ = malloc(sizeof(FunctCont_t));
                $$->elist = $4;
                $$->method = 1;
                $$->name = $2;
            }
            ;

elist:      { $$ = NULL; }
            | expr elist_expr {
                if($1->type == boolexpr_e) $1 = emit_eval_var($1);
                $$ = $1;
                if($2) $1->next = $2;                
            }
            ;

elist_expr: { $$ = NULL; }
            | COMMA expr elist_expr {
                $$ = $2;
                if($3) $2->next = $3;
            }
            ;

/* This rule helps with conflict of objectdef */
notempty_elist: expr elist_expr {
                if($1->type == boolexpr_e) $1 = emit_eval_var($1);
                $$ = $1;
                if($2) $1->next = $2;
            }
            ;

objectdef:  LBRACKET RBRACKET { $$ = create_table(); }
            | LBRACKET notempty_elist RBRACKET {
                $$ = create_table();
                
                Expr* expr = $2;
                unsigned index = 0;
                
                while(expr) {
                    add_table_element($$, index, expr);
                    index++;
                    expr = expr->next;
                }
            }
            | LBRACKET indexed RBRACKET {
                Expr* table = create_table();

                Expr* indexedElem = $2;

                while(indexedElem) {
                    if(!indexedElem) break;
                    Expr* index = indexedElem->index;
                    Expr* value = indexedElem;

                    add_indexed_element(table, index, value);
                    indexedElem = indexedElem->next;
                }
                $$ = table;
            }
            ;

indexed:    indexedelem indexedelem_list {
                $$ = $1;
                if($2) $$->next = $2;
            }
            ;

indexedelem_list: { $$ = NULL; }
                | COMMA indexedelem indexedelem_list {
                    $$ = $2;
                    if($3) $2->next = $3;
                }
                ;

indexedelem: LBRACE expr COLON expr RBRACE {
                if($4->type == boolexpr_e) $4 = emit_eval_var($4);
                $$ = $4;
                $$->index = $2;
            }
            ;

block:      LBRACE {
                ++currentScope;
                if(currFunc && functionBlockScope == 0) functionBlockScope = currentScope;
                int *saved = malloc(sizeof *saved);
                *saved = currentOffset;
                pushStack(offsetStack, saved);
                currentOffset = 0;
            } stmt_list RBRACE {
                if(currentScope == functionBlockScope) currFunc->localCount = currentOffset;
                int *rest = popStack(offsetStack);
                currentOffset = *rest;
                free(rest);
                $$ = $3;
                SymTable_Hide(symTable, currentScope);
                --currentScope;
            }

funcprefix: FUNCTION IDENTIFIER {
                SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);

                if(libFunc && libFunc->type == LIBFUNC) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Cannot shadow a library function '%s' (line %d)\n", $2, yylineno);
                    $$ = NULL;
                } else {
                    SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);
                    if(pCurr) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' already defined in scope %u (line %d)\n", $2, currentScope, yylineno);
                        $$ = NULL;
                    } else {
                        $$ = SymTable_Insert(symTable, $2, currentScope, yylineno, USERFUNC);
                        currFunc = $$;
                        $$->taddress = nextQuadLabel();

                        Expr *funcExpr = newExpr(programfunc_e);
                        funcExpr->sym = $$;
                        emit(jump, NULL, NULL, NULL, 0);
                        emit(funcstart, NULL, NULL, funcExpr, 0);
                    }
                }
            }
            | FUNCTION {
                char *name = malloc(16 * sizeof(char));
                sprintf(name, "$f%d", anon_func++);

                $$ = SymTable_Insert(symTable, name, currentScope, yylineno, USERFUNC);
                currFunc = $$;
                $$->taddress = nextQuadLabel();

                if(!$$) fprintf(stderr, "\033[1;31mError:\033[0m Failed to create anonymous function '%s' (line %d)\n", name, yylineno);
                else {
                    Expr *funcExpr = newExpr(programfunc_e);
                    funcExpr->sym = $$;
                    emit(jump, NULL, NULL, NULL, 0);
                    emit(funcstart, NULL, NULL, funcExpr, 0);
                }
                free(name);
            }
            ;

funcargs:   LPAREN { 
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
                int *saved = malloc(sizeof *saved);
                *saved = currentOffset;
                pushStack(offsetStack, saved);
                currentOffset = 0;
            } idlist RPAREN { 
                --currentScope;
            }
            ;

funcdef:    funcprefix M funcargs block {
                patchlist($4->retlist, nextQuadLabel());

                if($1) {
                    Expr *funcExpr = newExpr(programfunc_e);
                    funcExpr->sym = $1;
                    emit(funcend, NULL, NULL, funcExpr, 0);
                    patchlabel($2 - 2, nextQuadLabel());
                    
                    int *rest = popStack(offsetStack);
                    currentOffset = *rest;
                    free(rest);
                }
                currFunc = NULL;
                functionBlockScope = 0;
                $$ = $1;
            }
            ;

idlist:     {  }
            | IDENTIFIER { 
                SymTableEntry *libFunc = SymTable_Lookup(symTable, $1, 0);

                if(libFunc && libFunc->type == LIBFUNC) fprintf(stderr, "\033[1;31mError:\033[0m Cannot shadow a library function. '%s' (line %d)\n", $1, yylineno);
                else {
                    int scope;
                    for(scope = currentScope; scope >= 0; scope--) {
                        SymTableEntry *pCurr = SymTable_Lookup(symTable, $1, scope);

                        if(pCurr) {
                            if(scope == currentScope)
                                fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' declared in scope %u (line %d)\n", $1, currentScope, yylineno);
                            else if(pCurr->isActive && 
                                      !(pCurr->type == LIBFUNC || 
                                        pCurr->type == USERFUNC || 
                                        pCurr->type == GLOBAL_VAR))
                                fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' shadows existing symbol in outer scope %u (line %d)\n", $1, scope, yylineno);
                            else if(scope == 0) {
                                if(scope < currentScope) break;
                                else  fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' declared in scope %u (line %d)\n", $1, scope, yylineno);
                            }
                            break;
                        }
                    }
                    SymTableEntry *newEntry = SymTable_Insert(symTable, $1, currentScope, yylineno, FORMAL);
                    if(newEntry) newEntry->offset = currentOffset++;

                }
            } idlist_tail
            ;

idlist_tail:    { $$ = NULL; }
                | COMMA IDENTIFIER idlist_tail {
                    SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);

                    if(libFunc && libFunc->type == LIBFUNC) fprintf(stderr, "\033[1;31mError:\033[0m Cannot shadow a library function. '%s' (line %d)\n", $2, yylineno);
                    else {
                        int scope;
                        for(scope = currentScope; scope >= 0; scope--) {
                            SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, scope);

                            if(pCurr) {
                                if(scope == currentScope)
                                    fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' declared in scope %u (line %d)\n", $2, currentScope, yylineno);
                                else if(pCurr->isActive && 
                                          !(pCurr->type == LIBFUNC || 
                                            pCurr->type == USERFUNC || 
                                            pCurr->type == GLOBAL_VAR))
                                    fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' shadows existing symbol in outer scope %u (line %d)\n", $2, scope, yylineno);
                                else if(scope == 0) {
                                    if(scope < currentScope) break;
                                    else  fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' declared in scope %u (line %d)\n", $2, scope, yylineno);
                                }
                                break;
                            }
                        }
                        SymTableEntry *newEntry = SymTable_Insert(symTable, $2, currentScope, yylineno, FORMAL);
                        if(newEntry) newEntry->offset = currentOffset++;

                    }
                    $$ = $3;
                }
                ;

const:      INTCONST {
                $$ = newExpr_constnum((double)$1);
            }
            | REALCONST {
                $$ = newExpr_constnum($1);
            }
            | STRINGCONST {
                $$ = newExpr_conststring($1);
            }
            | NIL {
                $$ = newExpr_nil();
            }
            | TRUE {
                $$ = newExpr_constbool(1);
            }
            | FALSE {
                $$ = newExpr_constbool(0);
            }
            ;

ifprefix:   IF LPAREN expr RPAREN {
                Expr* evaluated_expr = emit_eval_var($3);
                emit(if_eq, evaluated_expr, newExpr_constbool(1), NULL, nextQuadLabel() + 2);
                $$ = nextQuadLabel();
                emit(jump, NULL, NULL, NULL, 0);
            }
            ;

ifstmt:     ifprefix stmt {
                patchlabel($1, nextQuadLabel());
                $$ = $2;
            }
            | ifprefix stmt ELSE jumpandsavepos stmt {
                patchlabel($1, $4 + 1);
                patchlabel($4, nextQuadLabel()); 
                make_stmt(&$$);
                $$->breaklist = mergelist($2->breaklist, $5->breaklist);
                $$->contlist = mergelist($2->contlist, $5->contlist);
                $$->retlist = mergelist($2->retlist, $5->retlist);
            }
            ;

jumpandsavepos: {
                    $$ = nextQuadLabel();
                    emit(jump, NULL, NULL, NULL, 0);
                }
                ;

whilestart: WHILE { $$ = nextQuadLabel(); }
            ;

whilecond:  LPAREN expr RPAREN {
                Expr* evaluated_expr = emit_eval_var($2);
                emit(if_eq, evaluated_expr, newExpr_constbool(1), NULL, nextQuadLabel() + 2);
                $$ = nextQuadLabel();
                emit(jump, NULL, NULL, NULL, 0);
            }
            ;

whilestmt:  whilestart { ++isLoop; } whilecond stmt {--isLoop;} {
                emit(jump, NULL, NULL, NULL, $1);
                patchlabel($3, nextQuadLabel());

                make_stmt(&$$);
                patchlist($4->breaklist, nextQuadLabel());
                patchlist($4->contlist, $1);
                
                $$->breaklist = 0;
                $$->contlist = 0;
                $$->retlist = $4->retlist;
            }
            ;

forprefix:  FOR { ++isLoop; } LPAREN elist M SEMICOLON expr SEMICOLON {
                Expr* evaluated_expr = emit_eval_var($7);
                
                $$ = malloc(sizeof(forstmt_t));
                $$->test = $5;
                $$->enter = nextQuadLabel();

                emit(if_eq, evaluated_expr, newExpr_constbool(1), NULL, 0);
            }
            ;

forstmt:    forprefix jumpandsavepos elist RPAREN jumpandsavepos stmt jumpandsavepos { --isLoop; } {
                patchlabel($1->enter, $5 + 1);
                patchlabel($7, $2 + 1);
                patchlabel($2, nextQuadLabel());
                patchlabel($5, $1->test);

                make_stmt(&$$);
                patchlist($6->breaklist, nextQuadLabel());
                patchlist($6->contlist, $2 + 1);
                
                $$->breaklist = 0;
                $$->contlist = 0;
                $$->retlist = $6->retlist;
            }
            ;

returnstmt: RETURN SEMICOLON {
                if(!isFunctionScope(currentScope))
                    yyerror("Return statement outside of function");

                emit(ret, NULL, NULL, NULL, 0);
                $$ = nextQuadLabel();
                emit(jump, NULL, NULL, NULL, 0);
            }
            | RETURN expr SEMICOLON {
                if(!isFunctionScope(currentScope))
                    yyerror("Return statement outside of function");

                if($2->type == boolexpr_e)
                    $2 = emit_eval_var($2);

                emit(ret, $2, NULL, NULL, 0);
                $$ = nextQuadLabel();
                emit(jump, NULL, NULL, NULL, 0);
            }
            ;

%%

int main(int argc, char **argv) {
    FILE *input_file = stdin;
    FILE *output_file = NULL;
    char *output_filename = NULL;
    int debug_mode = 0;

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <input.alpha> [output.abc] [--debug]\n", argv[0]);
        return 1;
    }

    for(int i = 1; i < argc; i++) if(strcmp(argv[i], "--debug") == 0) debug_mode = 1;

    if(!(input_file = fopen(argv[1], "r"))) {
        fprintf(stderr, "Cannot read file: %s\n", argv[1]);
        return 1;
    }

    // Determine output filename
    if(argc > 2 && strcmp(argv[2], "--debug") != 0) output_filename = argv[2];
    else {
        char *dot = strrchr(argv[1], '.');
        if(dot) {
            int len = dot - argv[1];
            output_filename = malloc(len + 5);
            strncpy(output_filename, argv[1], len);
            strcpy(output_filename + len, ".abc");
        } else {
            output_filename = malloc(strlen(argv[1]) + 5);
            strcpy(output_filename, argv[1]);
            strcat(output_filename, ".abc");
        }
    }

    symTable = SymTable_Initialize();
    initQuads();
    initOffsetStack();

    yyin = input_file;
    
    if(yyparse() != 0) {
        fprintf(stderr, "Parsing failed\n");
        fclose(input_file);
        return 1;
    }
    
    generate();
    
    if(!(output_file = fopen(output_filename, "wb"))) {
        fprintf(stderr, "Cannot write file: %s\n", output_filename);
        fclose(input_file);
        return 1;
    }
    
    write_binary_file(output_file);
    fclose(output_file);
    
    printf("Compilation successful: %s -> %s\n", argv[1], output_filename);
    
    if(debug_mode) printEverything(1, 1, 1, 1);

    SymTable_Free(symTable);    
    if(quads) free(quads);
    fclose(input_file);
    
    if(argc <= 2 || strcmp(argv[2], "--debug") == 0) free(output_filename);

    return 0;
}

void yyerror(const char *msg) {
    fprintf(stderr, "\033[1;31mSyntax Error\033[0m at line %d: %s\n", yylineno, msg);
}

int isFunctionScope(unsigned int scope) {
    if(scope < MAX_SCOPE) return isFunctionScopes[scope];
    return 0;
}

void initOffsetStack() {
    offsetStack = newStack();
    int *initOff = malloc(sizeof *initOff);
    *initOff = 0;
    pushStack(offsetStack, initOff);
    currentOffset = 0;
}

void printEverything(int symbols, int quads, int instructions, int constants) {
    int flags[] = {symbols, quads, instructions, constants};
    const char* headers[] = {
        "========== SYMBOL TABLE ==========",
        "========== INTERMEDIATE CODE ==========", 
        "========== TARGET CODE ==========",
        "========== CONSTANTS =========="
    };
    
    for(int i = 0; i < 4; i++) {
        if(flags[i]) {
            printf("\n%s\n", headers[i]);
            switch(i) {
                case 0: SymTable_Print(symTable); break;
                case 1: printQuads(); break;
                case 2: print_instructions(); break;
                case 3: print_constants(); break;
            }
        }
    }
}