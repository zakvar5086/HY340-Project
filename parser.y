%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symtable.h"
#include "quad.h"
#include "parser.tab.h"

#define YY_DECL int yylex(void)
extern int yylex(void);
extern FILE *yyin;

extern int yylineno;
void yyerror(const char *msg);

unsigned int currentScope = 0;
int isFunctionScopes[MAX_SCOPE] = {0};
int isLoop = 0;
static int anon_func = 0;
SymTable* symTable;

int isFunctionScope(unsigned int scope) {
    if (scope < MAX_SCOPE) return isFunctionScopes[scope];
    return 0;
}

%}

%union {
    char* stringValue;
    int intValue;
    double realValue;
    SymTableEntry *symEntry;
    Expr *exprNode;
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

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

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

%type <symEntry> lvalue funcdef
%type <exprNode> expr term assignexpr primary const call callsuffix normcall methodcall member
%type <exprNode> elist elist_expr notempty_elist objectdef indexed indexedelem indexedelem_list

%%

program:    stmt_list
            ;

stmt_list:  
            | stmt stmt_list
            ;

stmt:       expr SEMICOLON
            | ifstmt
            | whilestmt
            | forstmt
            | returnstmt
            | BREAK SEMICOLON {
                if(isLoop == 0) fprintf(stderr, "\033[1;31mError:\033[0m 'break' statement outside of a loop (line %d)\n", yylineno);
            }
            | CONTINUE SEMICOLON {
                if(isLoop == 0) fprintf(stderr, "\033[1;31mError:\033[0m 'continue' statement outside of a loop (line %d)\n", yylineno);
            }
            | block
            | funcdef
            | SEMICOLON
            ;

expr:       assignexpr { $$ = $1; }
            | expr PLUS expr {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(add, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr MINUS expr {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(sub, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr MULTIPLY expr {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(mul, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr DIVIDE expr {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(div_op, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr MOD expr {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(mod_op, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr GREATER expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_greater, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr GREATER_EQUAL expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_greatereq, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr LESS expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_less, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr LESS_EQUAL expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_lesseq, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr EQUAL expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_eq, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr NEQUAL expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(if_noteq, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr AND expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(and_op, $1, $3, result, yylineno);
                $$ = result;
            }
            | expr OR expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(or_op, $1, $3, result, yylineno);
                $$ = result;
            }
            | term { $$ = $1; }
            ;

term:       LPAREN expr RPAREN { $$ = $2; }
            | UMINUS expr %prec UMINUS {
                Expr *result = newExpr(arithexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(uminus, $2, NULL, result, yylineno);
                $$ = result;
            }
            | NOT expr {
                Expr *result = newExpr(boolexpr_e);
                result->sym = newtemp(symTable, currentScope);
                emit(not_op, $2, NULL, result, yylineno);
                $$ = result;
            }
            | INCR lvalue {
                if($2 && ($2->type == USERFUNC || $2->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);
                
                Expr *result = newExpr(var_e);
                result->sym = $2;
                Expr *one = newExpr_constnum(1);
                Expr *temp = newExpr(arithexpr_e);
                temp->sym = newtemp(symTable, currentScope);
                
                emit(add, result, one, temp, yylineno);
                emit(assign, temp, NULL, result, yylineno);
                $$ = temp;
            }
            | lvalue INCR {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);
                
                Expr *result = newExpr(var_e);
                result->sym = $1;
                Expr *copyExpr = newExpr(var_e);
                copyExpr->sym = $1;
                Expr *one = newExpr_constnum(1);
                Expr *temp = newExpr(arithexpr_e);
                temp->sym = newtemp(symTable, currentScope);
                
                emit(assign, result, NULL, temp, yylineno);
                emit(add, result, one, result, yylineno);
                $$ = temp;
            }
            | DECR lvalue {
                if($2 && ($2->type == USERFUNC || $2->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);
                
                Expr *result = newExpr(var_e);
                result->sym = $2;
                Expr *one = newExpr_constnum(1);
                Expr *temp = newExpr(arithexpr_e);
                temp->sym = newtemp(symTable, currentScope);
                
                emit(sub, result, one, temp, yylineno);
                emit(assign, temp, NULL, result, yylineno);
                $$ = temp;
            }
            | lvalue DECR {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);
                
                Expr *result = newExpr(var_e);
                result->sym = $1;
                Expr *copyExpr = newExpr(var_e);
                copyExpr->sym = $1;
                Expr *one = newExpr_constnum(1);
                Expr *temp = newExpr(arithexpr_e);
                temp->sym = newtemp(symTable, currentScope);
                
                emit(assign, result, NULL, temp, yylineno);
                emit(sub, result, one, result, yylineno);
                $$ = temp;
            }
            | primary { $$ = $1; }
            ;

assignexpr: lvalue ASSIGN expr {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '=' to function (line %d)\n", yylineno);
                
                Expr *result = newExpr_id($1);
                emit(assign, $3, NULL, result, yylineno);
                $$ = $3;
            }
            ;

primary:    lvalue { 
                if($1) $$ = newExpr_id($1);
                else $$ = NULL;
            }
            | call { $$ = $1; }
            | objectdef { $$ = $1; }
            | LPAREN funcdef RPAREN { $$ = NULL; }
            | const { $$ = $1; }
            ;

lvalue:     IDENTIFIER {
                int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $1, currentScope);

                if(pCurr) $$ = pCurr;
                else {
                    int found = 0;
                    int scope;
                    for(scope = currentScope - 1; scope > 0; scope--) {
                        SymTableEntry *outer = SymTable_Lookup(symTable, $1, scope);
                        if(!outer) continue;

                        found = 1;
                        if(isFunctionScope(currentScope) &&
                            (outer->type == LOCAL_VAR || outer->type == FORMAL) &&
                            isLoop == 0 &&
                            outer->isActive) {
                            fprintf(stderr, "\033[1;31mError:\033[0m Cannot access symbol '%s' at line %d.\n", $1, yylineno);
                            $$ = NULL;
                        } else $$ = outer;
                        break;
                    }

                    if(!found) {
                        pCurr = SymTable_Lookup(symTable, $1, 0);
                        if(pCurr && (pCurr->type == LIBFUNC || pCurr->type == USERFUNC || pCurr->type == GLOBAL_VAR))
                            $$ = pCurr;
                        else if(currentScope == 0) $$ = SymTable_Insert(symTable, $1, currentScope, yylineno, GLOBAL_VAR);
                        else $$ = SymTable_Insert(symTable, $1, currentScope, yylineno, varType);

                    }
                }
            }
            | LOCAL IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);

                if(pCurr) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Redeclaration of symbol '%s' in scope %u (line %d)\n", $2, currentScope, yylineno);
                    $$ = pCurr;
                } else {
                    int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                    SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);

                    if(libFunc && libFunc->type == LIBFUNC) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' shadows library function (line %d)\n", $2, yylineno);
                        $$ = NULL;
                    } else {
                        SymTableEntry *pNew = SymTable_Insert(symTable, $2, currentScope, yylineno, varType);
                        $$ = pNew;
                    }
                }
            }
            | DBL_COLON IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, 0);

                if(pCurr) $$ = pCurr;
                else {
                    fprintf(stderr, "\033[1;31mError:\033[0m Global symbol '::%s' not found (line %d)\n", $2, yylineno);
                    $$ = NULL;
                }
            }
            | member { $$ = NULL; }
            ;

member:     lvalue DOT IDENTIFIER {
                Expr *result = newExpr(tableitem_e);
                result->sym = $1;
                result->index = newExpr_conststring($3);
                $$ = result;
            }
            | lvalue LBRACKET expr RBRACKET {
                Expr *result = newExpr(tableitem_e);
                result->sym = $1;
                result->index = $3;
                $$ = result;
            }
            | call DOT IDENTIFIER {
                $$ = NULL;
            }
            | call LBRACKET expr RBRACKET {
                $$ = NULL;
            }
            ;

call:       call LPAREN elist RPAREN {
                $$ = NULL;
            }
            | lvalue callsuffix {
                $$ = NULL;
            }
            | LPAREN funcdef RPAREN LPAREN elist RPAREN {
                $$ = NULL;
            }
            ;

callsuffix: normcall { $$ = $1; }
            | methodcall { $$ = $1; }
            ;

normcall:   LPAREN elist RPAREN {
                $$ = NULL;
            }
            ;

methodcall: DBL_DOT IDENTIFIER LPAREN elist RPAREN {
                $$ = NULL;
            }
            ;

elist:      { $$ = NULL; }
            | expr elist_expr {
                $$ = $1;
                if ($2) $1->next = $2;
            }
            ;

elist_expr: { $$ = NULL; }
            | COMMA expr elist_expr {
                $$ = $2;
                if ($3) $2->next = $3;
            }
            ;

/* This rule helps with conflict of objectdef */
notempty_elist: expr elist_expr {
                $$ = $1;
                if ($2) $1->next = $2;
            }
            ;

objectdef:  LBRACKET RBRACKET {
                Expr *result = newExpr(newtable_e);
                result->sym = newtemp(symTable, currentScope);
                emit(tablecreate, NULL, NULL, result, yylineno);
                $$ = result;
            }
            | LBRACKET notempty_elist RBRACKET {
                Expr *result = newExpr(newtable_e);
                result->sym = newtemp(symTable, currentScope);
                emit(tablecreate, NULL, NULL, result, yylineno);
                $$ = result;
            }
            | LBRACKET indexed RBRACKET {
                Expr *result = newExpr(newtable_e);
                result->sym = newtemp(symTable, currentScope);
                emit(tablecreate, NULL, NULL, result, yylineno);
                $$ = result;
            }
            ;

indexed:    indexedelem indexedelem_list {
                $$ = $1;
                if ($2) $1->next = $2;
            }
            ;

indexedelem_list: { $$ = NULL; }
                | COMMA indexedelem indexedelem_list {
                    $$ = $2;
                    if ($3) $2->next = $3;
                }
                ;

indexedelem: LBRACE expr COLON expr RBRACE {
                $$ = NULL;
            }
            ;

block:      LBRACE { ++currentScope; } stmt_list RBRACE { SymTable_Hide(symTable, currentScope); --currentScope; }
            ;

funcdef:    FUNCTION IDENTIFIER {
                SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);

                if(libFunc && libFunc->type == LIBFUNC) fprintf(stderr, "\033[1;31mError:\033[0m Cannot shadow a library function '%s' (line %d)\n", $2, yylineno);
                else {
                    SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);
                    if(pCurr) {
                        if(pCurr->type != GLOBAL_VAR) fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' already defined in scope %u (line %d)\n", $2, currentScope, yylineno);
                        else fprintf(stderr, "\033[1;31mError:\033[0m Symbol '%s' declared in scope %u (line %d)\n", $2, currentScope, yylineno);
                    }
                    else $$ = SymTable_Insert(symTable, $2, currentScope, yylineno, USERFUNC);
                }
            } LPAREN {
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
            } idlist RPAREN { --currentScope; } block { $$ = NULL; }
            | FUNCTION {
                ++anon_func;
                char anon_func_name[16];
                snprintf(anon_func_name, sizeof(anon_func_name), "$f%d", anon_func);
                
                $$ = SymTable_Insert(symTable, anon_func_name, currentScope, yylineno, USERFUNC);
            } LPAREN {
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
            } idlist RPAREN { --currentScope; } block { $$ = NULL; }
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
                    SymTable_Insert(symTable, $1, currentScope, yylineno, FORMAL);
                }
            } idlist_tail
            ;

idlist_tail:
            | COMMA IDENTIFIER { 
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
                    SymTable_Insert(symTable, $2, currentScope, yylineno, FORMAL);
                }
            } idlist_tail
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

ifstmt:     IF LPAREN expr RPAREN stmt ELSE stmt
            | IF LPAREN expr RPAREN stmt %prec LOWER_THAN_ELSE
            ;

whilestmt:  WHILE LPAREN expr RPAREN { ++isLoop; } stmt { --isLoop; }
            ;

forstmt:    FOR LPAREN elist SEMICOLON expr SEMICOLON elist RPAREN { ++isLoop; } stmt { --isLoop; }
            ;

returnstmt: RETURN expr SEMICOLON {
                if(!isFunctionScope(currentScope)) fprintf(stderr, "\033[1;31mError:\033[0m 'return' statement outside of a function (line %d)\n", yylineno);
            }
            | RETURN SEMICOLON {
                if(!isFunctionScope(currentScope)) fprintf(stderr, "\033[1;31mError:\033[0m 'return' statement outside of a function (line %d)\n", yylineno);
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