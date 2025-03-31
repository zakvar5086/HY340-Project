%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

extern int yylineno;
extern int yylex(void);
void yyerror(const char *msg);

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

