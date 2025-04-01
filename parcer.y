%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

extern int yylineno;
extern int yylex(void);
void yyerror(const char *msg);

int isFunc = 0;

unsigned int currentScope = 0;
SymTable* symTable;    

%}

%union {
    char* stringValue;
    int intValue;
    double realValue;
    SymbolTableEntry* symEntry;
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

%type <symEntry> lvalue

%%

program:    stmt_list
            ;

stmt_list:  stmt stmt_list
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

lvalue:     IDENTIFIER
            | LOCAL IDENTIFIER
            | DBL_COLON IDENTIFIER
            | member
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

elist:      expr elist_expr
            ;

elist_expr: COMMA expr elist_expr
            ;

objectdef:  LBRACKET elist RBRACKET
            | LBRACKET indexed RBRACKET
            ;

indexed:    indexedelem indexedelem_list
            ;

indexedelem_list:   COMMA indexedelem indexedelem_list
                    ;

indexedelem:        LBRACE expr COLON expr RBRACE
                    ;

block:      LBRACE stmt_list RBRACE
            | LBRACE RBRACE
            ;

funcdef:    FUNCTION IDENTIFIER LPAREN id_list RPAREN block
            | LPAREN id_list RPAREN block
            ;

const:      INTCONST
            | REALCONST
            | STRINGCONST
            | NIL
            | TRUE
            | FALSE
            ;

id_list:    IDENTIFIER id_list_list
            ;
id_list_list:       COMMA IDENTIFIER id_list_list
                    ;

ifstmt:     IF LPAREN expr RPAREN stmt ELSE stmt
            | IF LPAREN expr RPAREN stmt
            ;

whilestmt:  WHILE LPAREN expr RPAREN stmt
            ;

forstmt:    FOR LPAREN elist SEMICOLON expr SEMICOLON elist RPAREN stmt
            ;

returnstmt: RETURN expr SEMICOLON
            | RETURN SEMICOLON
            ;
