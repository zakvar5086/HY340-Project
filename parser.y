%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symtable.h"
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

%type <symEntry> lvalue funcdef term assignexpr

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

expr:       assignexpr
            | expr PLUS expr
            | expr MINUS expr
            | expr MULTIPLY expr
            | expr DIVIDE expr
            | expr MOD expr
            | expr GREATER expr
            | expr GREATER_EQUAL expr
            | expr LESS expr
            | expr LESS_EQUAL expr
            | expr EQUAL expr
            | expr NEQUAL expr
            | expr AND expr
            | expr OR expr
            | term
            ;

term:       LPAREN expr RPAREN { $$ = NULL; }
            | UMINUS expr { $$ = NULL; }
            | NOT expr { $$ = NULL; }
            | INCR lvalue {
                if($2 && ($2->type == USERFUNC || $2->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);
                $$ = $2;
            }
            | lvalue INCR {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '++' to function (line %d)\n", yylineno);
                $$ = $1;
            }
            | DECR lvalue {
                if($2 && ($2->type == USERFUNC || $2->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);
                $$ = $2;
            }
            | lvalue DECR {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '--' to function (line %d)\n", yylineno);
                $$ = $1;
            }
            | primary { $$ = NULL; }
            ;

assignexpr: lvalue ASSIGN expr {
                if($1 && ($1->type == USERFUNC || $1->type == LIBFUNC))
                    fprintf(stderr, "\033[1;31mError:\033[0m Illegal action '=' to function (line %d)\n", yylineno);                
                else $$ = $1
            }
            ;

primary:    lvalue
            | call
            | objectdef
            | LPAREN funcdef RPAREN
            | const
            ;

lvalue:     IDENTIFIER {
                int varType = (currentScope == 0) ? GLOBAL_VAR : LOCAL_VAR;
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $1, currentScope);

                if(pCurr) $$ = pCurr;
                else {
                    int found = 0;
                    int scope;
                    for(scope = currentScope - 1; scope >= 0; scope--) {
                        SymTableEntry *outer = SymTable_Lookup(symTable, $1, scope);
                        if(!outer) continue;

                        found = 1;
                        int parentScope = (currentScope == 0) ? 0 : currentScope - 1;
                        
                        if(isFunctionScope(currentScope) &&
                            isFunctionScope(parentScope) &&
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
                        fprintf(stderr, "\033[1;31mError:\033[0m Symbol shadows library function '%s' (line %d)\n", $2, yylineno);
                        $$ = NULL;
                    } else {
                        SymTableEntry *pNew = SymTable_Insert(symTable, $2, currentScope, yylineno, varType);
                        $$ = pNew;
                    }
                }
            }
            | DBL_COLON IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, 0);

                if(!pCurr || !pCurr->isActive) {
                    fprintf(stderr, "\033[1;31mError:\033[0m Global symbol '::%s' not found (line %d)\n", $2, yylineno);
                    $$ = NULL;
                } else $$ = pCurr;
            }
            | member { $$ = NULL; }
            ;

member:     lvalue DOT IDENTIFIER
            | lvalue LBRACKET expr RBRACKET
            | call DOT IDENTIFIER
            | call LBRACKET expr RBRACKET
            ;

call:       call LPAREN elist RPAREN
            | lvalue callsuffix
            | LPAREN funcdef RPAREN LPAREN elist RPAREN
            ;

callsuffix: normcall
            | methodcall
            ;

normcall:   LPAREN elist RPAREN
            ;

methodcall: DBL_DOT IDENTIFIER LPAREN elist RPAREN
            ;

elist:      
            | expr elist_expr
            ;

elist_expr: 
            | COMMA expr elist_expr
            ;

/* This rules helps with conflict of objectdef */
notempty_elist:     expr elist_expr
                    ;

objectdef:  LBRACKET RBRACKET
            | LBRACKET notempty_elist RBRACKET
            | LBRACKET indexed RBRACKET
            ;

indexed:    indexedelem indexedelem_list
            ;

indexedelem_list:   COMMA indexedelem indexedelem_list
                    |
                    ;

indexedelem:        LBRACE expr COLON expr RBRACE
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
                    else SymTable_Insert(symTable, $2, currentScope, yylineno, USERFUNC);
                }
            } LPAREN {
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
            } idlist RPAREN { --currentScope; } block { $$ = NULL; }
            | FUNCTION {
                ++anon_func;
                char anon_func_name[16];
                snprintf(anon_func_name, sizeof(anon_func_name), "$f%d", anon_func);
                
                SymTable_Insert(symTable, anon_func_name, currentScope, yylineno, USERFUNC);
            } LPAREN {
                ++currentScope;
                isFunctionScopes[currentScope] = 1;
            } idlist RPAREN { --currentScope; } block { $$ = NULL; }
            ;

idlist:
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

const:      INTCONST
            | REALCONST
            | STRINGCONST
            | NIL
            | TRUE
            | FALSE
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

    yyin = input_file;
    if(output_file != stdout) {
        printf("Output written in file\n");
        stdout = output_file;
    }

    if(yyparse() == 0) printf("\033[1;32m[SUCCESS]\033[0m Parsing completed successfully.\n");
    else printf("\033[1;31m[FAILURE]\033[0m Parsing failed.\n");

    SymTable_Print(symTable);
    SymTable_Free(symTable);

    if(input_file != stdin) fclose(input_file);
    if(output_file != stdout) fclose(output_file);

    return 0;
}

void yyerror(const char *msg) {
    fprintf(stderr, "\033[1;31mSyntax Error\033[0m at line %d: %s\n", yylineno, msg);
}