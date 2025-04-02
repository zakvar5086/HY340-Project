%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symtable.h"
#include "parser.tab.h"

#define YY_DECL int yylex(YYSTYPE *yylval)
extern int yylex(YYSTYPE *yylval);
extern FILE *yyin;

extern int yylineno;
void yyerror(const char *msg);

int isFunc = 0;
unsigned int currentScope = 0;
static int anon_func = 0;
SymTable* symTable;
%}

%pure-parser

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

%type <symEntry> lvalue member call funcdef

%%

program:    stmt_list
            ;

stmt_list:  | stmt stmt_list
            ;

stmt:       expr SEMICOLON
            | ifstmt
            | whilestmt
            | forstmt
            | returnstmt
            | BREAK SEMICOLON
            | CONTINUE SEMICOLON
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

term:       LPAREN expr RPAREN
            | UMINUS expr
            | NOT expr
            | INCR lvalue       { if(isFunc){ fprintf(stderr, "\033[1;31m Illegal action. \033[0mCan't increment func \n"); isFunc = 0; } }
            | lvalue INCR       { if(isFunc){ fprintf(stderr, "\033[1;31m Illegal action. \033[0mCan't increment func \n"); isFunc = 0; } }
            | DECR lvalue       { if(isFunc){ fprintf(stderr, "\033[1;31m Illegal action. \033[0mCan't increment func \n"); isFunc = 0; } }
            | lvalue DECR       { if(isFunc){ fprintf(stderr, "\033[1;31m Illegal action. \033[0mCan't increment func \n"); isFunc = 0; } }
            | primary
            ;

assignexpr: lvalue ASSIGN expr
            ;

primary:    lvalue
            | call
            | objectdef
            | LPAREN funcdef RPAREN
            | const
            ;

lvalue:     IDENTIFIER { 
                SymTableEntry *pCurr = SymTable_LookupAny(symTable, $1);

                /* TODO: Maybe check if instead of pCurr to use $$ */

                if(!pCurr){
                    pCurr = SymTable_Insert(symTable, $1, 0, yylineno, GLOBAL_VAR);
                    fprintf(stdout, "Inserted global variable '%s' at line %d\n", $1, yylineno);
                }
                else if(pCurr->type == USERFUNC || pCurr->type == LIBFUNC){
                    isFunc = 1;
                }
                $$ = pCurr;
            }
            | LOCAL IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);

                if(pCurr){
                    fprintf(stderr, "\033[1;31mError:\033[0m Redeclaration of local '%s' in scope %u (line %d)\n", $2, currentScope, yylineno);
                    $$ = pCurr;
                } else {
                    SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);
                    if(libFunc && libFunc->type == LIBFUNC) {
                        fprintf(stderr, "\033[1;31mError:\033[0m Local shadows library function '%s' (line %d)\n", $2, yylineno);
                        $$ = NULL;
                    } else {
                        SymTableEntry *pNew = SymTable_Insert(symTable, $2, currentScope, yylineno, LOCAL_VAR);
                        fprintf(stdout, "Inserted local variable %s in scope %u at line %d\n", $2, currentScope, yylineno);
                        $$ = pNew;
                    }
                }
            }
            | DBL_COLON IDENTIFIER {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, 0);

                if(!pCurr || !pCurr->isActive){
                    fprintf(stderr, "\033[1;31mError:\033[0m Global symbol '::%s' not found (line %d)\n", $2, yylineno);
                    $$ = NULL;
                } else $$ = pCurr;
            }
            | member {
                $$ = $1;
            }
            ;

member:     lvalue DOT IDENTIFIER
            | lvalue LBRACKET expr RBRACKET
            | call DOT IDENTIFIER
            | call LBRACKET expr RBRACKET
            ;

call:       call LPAREN elist RPAREN
            | lvalue callsuffix
            | LPAREN funcdef RPAREN LPAREN elist RPAREN { $$ = $2; }
            ;

callsuffix: normcall
            | methodcall
            ;

normcall:   LPAREN elist RPAREN
            ;

methodcall: DBL_DOT IDENTIFIER LPAREN elist RPAREN
            ;

elist:      expr elist_expr
            ;

elist_expr: COMMA expr elist_expr
            |
            ;

objectdef:  LBRACKET elist RBRACKET
            | LBRACKET indexed RBRACKET
            ;

indexed:    indexedelem indexedelem_list
            ;

indexedelem_list:   COMMA indexedelem indexedelem_list
                    |
                    ;

indexedelem:        LBRACE expr COLON expr RBRACE
                    ;

block:      LBRACE { ++currentScope; } stmt_list RBRACE { currentScope--; SymTable_Hide(symTable, currentScope); }
            ;

funcdef:    FUNCTION IDENTIFIER LPAREN {
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);
                SymTableEntry *libFunc = SymTable_Lookup(symTable, $2, 0);
                
                if(pCurr) fprintf(stderr, "\033[1;31mError:\033[0m Function '%s' already defined in scope %u(line %d)\n", $2, currentScope, yylineno);
                else if(libFunc && libFunc->type == LIBFUNC) fprintf(stderr, "\033[1;31mError:\033[0m Cannot shadow a library function. '%s' (line %d)\n", $2, yylineno);
                else {
                    SymTable_Insert(symTable, $2, currentScope, yylineno, USERFUNC);
                    ++currentScope;
                }
            } paramlist RPAREN block {
                currentScope--;
                SymTable_Hide(symTable, currentScope);
            }
            | FUNCTION LPAREN paramlist {
                ++anon_func;
                char anon_func_name[16];
                snprintf(anon_func_name, sizeof(anon_func_name), "_anon_func_%d", anon_func);
                
                SymTable_Insert(symTable, anon_func_name, currentScope, yylineno, USERFUNC);
                ++currentScope;
                fprintf(stdout, "Inserted anon func '%s' in scope %u(line %d)\n", anon_func_name, currentScope, yylineno);
            } RPAREN block {
                currentScope--;
                SymTable_Hide(symTable, currentScope);
            }
            ;

paramlist:  | IDENTIFIER { 
                SymTableEntry *pCurr = SymTable_Lookup(symTable, $1, currentScope);
                if(pCurr) fprintf(stderr, "\033[1;31mError:\033[0m Formal '%s' redeclared in scope %u(line %d)\n", $1, currentScope, yylineno);
                else {
                    SymTable_Insert(symTable, $1, currentScope, yylineno, FORMAL);
                    fprintf(stdout, "Inserted formal parameter '%s' in scope %u(line %d)\n", $1, currentScope, yylineno);
                }
            } paramlist_tail
            ;

paramlist_tail:     | COMMA IDENTIFIER { 
                        SymTableEntry *pCurr = SymTable_Lookup(symTable, $2, currentScope);
                        if(pCurr) fprintf(stderr, "\033[1;31mError:\033[0m Formal '%s' redeclared in scope %u(line %d)\n", $2, currentScope, yylineno);
                        else {
                            SymTable_Insert(symTable, $2, currentScope, yylineno, FORMAL);
                            fprintf(stdout, "Inserted formal parameter '%s' in scope %u(line %d)\n", $2, currentScope, yylineno);
                        }
                    } paramlist_tail
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

whilestmt:  WHILE LPAREN expr RPAREN stmt
            ;

forstmt:    FOR LPAREN elist SEMICOLON expr SEMICOLON elist RPAREN stmt
            ;

returnstmt: RETURN expr SEMICOLON
            | RETURN SEMICOLON
            ;

%%

int main(int argc, char **argv) {
    symTable = SymTable_Initialize();

    printf("\033[1;34m[INFO]\033[0m Starting Alpha parser...\n");

    if (argc > 1) {
        FILE *inputFile = fopen(argv[1], "r");
        if (!inputFile) {
            perror("Failed to open input file");
            return 1;
        }
        yyin = inputFile;
    }

    if (yyparse() == 0) {
        printf("\033[1;32m[SUCCESS]\033[0m Parsing completed successfully.\n");
    } else {
        printf("\033[1;31m[FAILURE]\033[0m Parsing failed.\n");
    }

    printf("\nFinal Symbol Table:\n");
    SymTable_Print(symTable);
    SymTable_Free(symTable);
    return 0;
}

void yyerror(const char *msg) {
    fprintf(stderr, "\033[1;31mSyntax Error\033[0m at line %d: %s\n", yylineno, msg);
}