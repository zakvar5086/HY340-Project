%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "headers/symtable.h"
#include "headers/quad.h"
#include "headers/stack.h"
#include "parser.tab.h"

#define YY_DECL int yylex(void)
extern int yylex(void);
extern FILE *yyin;

extern unsigned int offset;
extern int yylineno;
void yyerror(const char *msg);

unsigned int currentScope = 0;
int isFunctionScopes[MAX_SCOPE] = {0};
int isLoop = 0;
static int anon_func = 0;
SymTable* symTable;

int isFunctionScope(unsigned int scope) {
    if(scope < MAX_SCOPE) return isFunctionScopes[scope];
    return 0;
}

%}

%union {
    char* stringValue;
    int intValue;
    double realValue;
    SymTableEntry *symEntry;
    Expr *exprNode;
    stmt_t *stmtValue;
    forstmt_t *forValue;
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
%type <exprNode> expr term assignexpr primary const call callsuffix normcall methodcall member lvalue
%type <exprNode> elist elist_expr objectdef indexed indexedelem indexedelem_list idlist_tail
%type <intValue> M
%type <stmtValue> stmt stmt_list ifstmt whilestmt forstmt block
%type <intValue> ifprefix jumpandsavepos whilestart whilecond returnstmt
%type <forValue> forprefix

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

expr:       assignexpr { $$ = $1; }
            | expr PLUS expr {
                if($1->type == tableitem_e) $1 = emit_iftableitem($1);
                if($3->type == tableitem_e) $3 = emit_iftableitem($3);
                
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '+' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(add, $1, $3, $$, 0);
            }
            | expr MINUS expr {
                if($1->type == tableitem_e) $1 = emit_iftableitem($1);
                if($3->type == tableitem_e) $3 = emit_iftableitem($3);
                
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '+' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(sub, $1, $3, $$, 0);
            }
            | expr MULTIPLY expr {
                if($1->type == tableitem_e) $1 = emit_iftableitem($1);
                if($3->type == tableitem_e) $3 = emit_iftableitem($3);
                
                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '+' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(mul, $1, $3, $$, 0);
            }
            | expr DIVIDE expr {
                if($1->type == tableitem_e) $1 = emit_iftableitem($1);
                if($3->type == tableitem_e) $3 = emit_iftableitem($3);

                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '/' operator (line %d)\n", yylineno);
                if($3->type == constnum_e && $3->numConst == 0)
                    fprintf(stderr, "\033[1;31mError:\033[0m Division by zero (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(div_op, $1, $3, $$, 0);
            }
            | expr MOD expr {
                if($1->type == tableitem_e) $1 = emit_iftableitem($1);
                if($3->type == tableitem_e) $3 = emit_iftableitem($3);

                if(!isArithExpr($1) || !isArithExpr($3))
                    fprintf(stderr, "\033[1;31mError:\033[0m Invalid operands to '%%' operator (line %d)\n", yylineno);

                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(mod_op, $1, $3, $$, 0);
            }
            | expr GREATER { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
                $$ = newExpr(boolexpr_e);
                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_greater, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr GREATER_EQUAL { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
                $$ = newExpr(boolexpr_e);
                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_greatereq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr LESS { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
                $$ = newExpr(boolexpr_e);
                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_less, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr LESS_EQUAL { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
                $$ = newExpr(boolexpr_e);
                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_lesseq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr EQUAL { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
                $$ = newExpr(boolexpr_e);
                $$->truelist = nextQuadLabel();
                $$->falselist = nextQuadLabel() + 1;

                emit(if_eq, $1, $4, NULL, 0);
                emit(jump, NULL, NULL, NULL, 0);
            }
            | expr NEQUAL  { 
                if($1->type == boolexpr_e) $1 = emit_eval($1);
                else if($1->type == tableitem_e) $1 = emit_iftableitem($1);
            } expr {
                if($4->type == boolexpr_e) $4 = emit_eval($4);
                else if($4->type == tableitem_e) $4 = emit_iftableitem($4);
                
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
            ;

M:          { $$ = nextQuadLabel(); }

term:       LPAREN expr RPAREN { $$ = $2; if($2->type == boolexpr_e) $2 = emit_eval($2); }
            | UMINUS expr %prec UMINUS {
                if($2->type != arithexpr_e) fprintf(stderr, "\033[1;31mError:\033[0m Invalid operand to unary '-' operator (line %d)\n", yylineno);
                $$ = newExpr(arithexpr_e);
                $$->sym = newtemp(symTable, currentScope);
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
                if($2 && ($2->sym->type == USERFUNC || $2->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);

                $$ = newExpr(var_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(add, $2, newExpr_constnum(1), $2, 0);
                emit(assign, $2, NULL, $$, 0);
            }
            | lvalue INCR {
                if($1 && ($1->sym->type == USERFUNC || $1->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);

                $$ = newExpr(var_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(assign, $1, NULL, $$, 0);
                emit(add, $1, newExpr_constnum(1), $1, 0);
            }
            | DECR lvalue {
                if($2 && ($2->sym->type == USERFUNC || $2->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);

                $$ = newExpr(var_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(sub, $2, newExpr_constnum(1), $2, 0);
                emit(assign, $2, NULL, $$, 0);
            }
            | lvalue DECR {
                if($1 && ($1->sym->type == USERFUNC || $1->sym->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);

                $$ = newExpr(var_e);
                $$->sym = newtemp(symTable, currentScope);
                emit(assign, $1, NULL, $$, 0);
                emit(sub, $1, newExpr_constnum(1), $1, 0);
            }
            | primary { $$ = $1; }
            ;

assignexpr: lvalue ASSIGN expr {
                if($1) {
                    if($1->type == tableitem_e) {
                        $$ = handle_tableitem_assignment($1, $3);
                    } else {
                        if($3->type == boolexpr_e) $3 = emit_eval_var($3);

                        $$ = newExpr(assignexpr_e);
                        $$->sym = newtemp(symTable, currentScope);
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
                            SymTableEntry *newSym;
                            if(currentScope == 0) newSym = SymTable_Insert(symTable, $1, currentScope, yylineno, GLOBAL_VAR);
                            else newSym = SymTable_Insert(symTable, $1, currentScope, yylineno, varType);
                            newSym->offset = offset++;
                            $$ = newExpr_id(newSym);
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
                        pNew->offset = offset++;
                        $$ = newExpr_id(pNew);
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
                if($1) {
                    Expr* index = newExpr_conststring($3);
                    $$ = member_item($1, index);
                } else $$ = NULL;
            }
            | lvalue LBRACKET expr RBRACKET {
                if($1) {
                    if($3->type == boolexpr_e) $3 = emit_eval_var($3);
                    $$ = member_item($1, $3);
                } else $$ = NULL;
            }
            | call DOT IDENTIFIER { 
                if($1) {
                    Expr* index = newExpr_conststring($3);
                    $$ = member_item($1, index);
                } else $$ = NULL;
            }
            | call LBRACKET expr RBRACKET { 
                if($1) {
                    if($3->type == boolexpr_e) $3 = emit_eval_var($3);
                    $$ = member_item($1, $3);
                } else $$ = NULL;
            }
            ;

call:       call LPAREN elist RPAREN {
                emit(call, $1, NULL, NULL, 0);
                
                Expr *result = newExpr(var_e);
                result->sym = newtemp(symTable, currentScope);
                emit(getretval, NULL, NULL, result, 0);
                $$ = result;
            }
            | lvalue callsuffix {
                if($1) {
                    Expr *func = NULL;
                    if($1->sym && $1->sym->type == USERFUNC) func = newExpr(programfunc_e);
                    else if($1->sym && $1->sym->type == LIBFUNC) func = newExpr(libraryfunc_e);
                    else func = newExpr(var_e);
                    func->sym = $1->sym;

                    if($2) {
                        Expr *arg = $2;
                        while(arg) {
                            if(arg->type == boolexpr_e) arg = emit_eval_var(arg);
                            emit(param, arg, NULL, NULL, 0);
                            arg = arg->next;
                        }
                    }
                    emit(call, func, NULL, NULL, 0);

                    Expr *result = newExpr(var_e);
                    result->sym = newtemp(symTable, currentScope);
                    emit(getretval, NULL, NULL, result, 0);
                    $$ = result;
                } else $$ = NULL;
            }
            | LPAREN funcdef RPAREN LPAREN elist RPAREN {
                if($2) {
                    Expr *func = newExpr(programfunc_e);
                    func->sym = $2;

                    if($5) {
                        Expr *arg = $5;
                        while(arg) {
                            emit(param, arg, NULL, NULL, 0);
                            arg = arg->next;
                        }
                    }
                    emit(call, func, NULL, NULL, 0);
                    
                    Expr *result = newExpr(var_e);
                    result->sym = newtemp(symTable, currentScope);
                    emit(getretval, NULL, NULL, result, 0);
                    $$ = result;
                } else $$ = NULL;
            }
            ;

callsuffix: normcall { $$ = $1; }
            | methodcall { $$ = $1; }
            ;

normcall:   LPAREN elist RPAREN {
                $$ = $2;
            }
            ;

methodcall: DBL_DOT IDENTIFIER LPAREN elist RPAREN {
                Expr *method = newExpr_conststring($2);
                if($4) method->next = $4;
                $$ = method;
            }
            ;

elist:      expr elist_expr {
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

objectdef:  LBRACKET RBRACKET { $$ = create_table(); }
            | LBRACKET elist RBRACKET {
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

block:      LBRACE { ++currentScope; } stmt_list RBRACE { $$ = $3; SymTable_Hide(symTable, currentScope); --currentScope; }
            ;

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
 
funcdef:    funcprefix M LPAREN {
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
            } idlist RPAREN { --currentScope; } block {
                patchlist($8->retlist, nextQuadLabel());

                if($1) {
                    Expr *funcExpr = newExpr(programfunc_e);
                    funcExpr->sym = $1;
                    emit(funcend, NULL, NULL, funcExpr, 0);
                    patchlabel($2 - 2, nextQuadLabel());
                }
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
                    newEntry->offset = offset++;
                }
            } idlist_tail
            ;

idlist_tail:    { $$ = NULL; }
                | COMMA IDENTIFIER idlist_tail {
                    $$ = newExpr_conststring($2);
                    $$->next = $3;
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
    FILE *output_file = stdout;

    if(argc > 1 && !(input_file = fopen(argv[1], "r"))) {
        fprintf(stderr, "Cannot read file: %s\n", argv[1]);
        return 1;
    }

    if(argc > 2 && !(output_file = fopen(argv[2], "w"))) {
        fprintf(stderr, "Cannot write file: %s\n", argv[2]);
        return 1;
    }

    symTable = SymTable_Initialize();
    initQuads();

    yyin = input_file;
    if(output_file != stdout) {
        printf("Output written in file\n");
        stdout = output_file;
    }

    yyparse();
    printQuads();
    
    SymTable_Free(symTable);    
    if(quads) free(quads);

    if(input_file != stdin) fclose(input_file);
    if(output_file != stdout) fclose(output_file);

    return 0;
}

void yyerror(const char *msg) {
    fprintf(stderr, "\033[1;31mSyntax Error\033[0m at line %d: %s\n", yylineno, msg);
}