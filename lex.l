%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/symtable.h"
#include "headers/quad.h"
#include "parser.tab.h"

#define YY_DECL int yylex(void)

typedef struct comment_info {
    int start_line;
    int end_line;
} comment_info_t;

comment_info_t current_comment_info;

int comment_depth = 0;
int nested_comment = 0;
static int token_counter = 1;

char *string_buf = NULL;
int buf_size = 0;
int buf_len = 0;

/* Helper function to add text to buf */
void addToBuff(char input) {
    if (buf_len + 2 > buf_size) {
        buf_size = (buf_size == 0) ? 64 : buf_size * 2;
        string_buf = realloc(string_buf, buf_size);
        if (!string_buf) {
            fprintf(stderr, "realloc failed in addToBuff()\n");
            return;
        }
    }

    string_buf[buf_len++] = input;
    string_buf[buf_len] = '\0';
}

%}

/* Declerations */

%option noyywrap
%option yylineno
%x COMMENT_MODE

ID          [a-zA-Z_][a-zA-Z0-9_]*
ILL_ID      _+[a-zA-Z0-9]*
INT         [0-9]+
REAL        [0-9]+\.[0-9]+
STRING      \"([^\"\\]|\\.)*\"
UNTERMINATED_STRING \"([^\"\\]|\\.)*
WHITESPACE  [\t' '\r '\n]* 

SINGLE_LINE_COMM        "//".*
MULTI_LINE_COMM_BEG     "/*"
MULTI_LINE_COMM_END     "*/"

%%

"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"for"       { return FOR; }
"function"  { return FUNCTION; }
"return"    { return RETURN; }
"break"     { return BREAK; }
"continue"  { return CONTINUE; }
"and"       { return AND; }
"not"       { return NOT; }
"or"        { return OR; }
"local"     { return LOCAL; }
"true"      { return TRUE; }
"false"     { return FALSE; }
"nil"       { return NIL; }

"="         { return ASSIGN; }
"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return MULTIPLY; }
"/"         { return DIVIDE; }
"%"         { return MOD; }
"=="        { return EQUAL; }
"!="        { return NEQUAL; }
"++"        { return INCR; }
"--"        { return DECR; }
">"         { return GREATER; }
"<"         { return LESS; }
">="        { return GREATER_EQUAL; }
"<="        { return LESS_EQUAL; }

{INT}       { yylval.intValue = atoi(yytext); return INTCONST; }
{REAL}      { yylval.realValue = atof(yytext); return REALCONST; }

{STRING} {
    buf_size = 16;
    buf_len = 0;
    string_buf = calloc(buf_size, 1);
    if (!string_buf) {
        fprintf(stderr, "calloc failed in string_buf\n");
        yyterminate();
    }

    int i = 1;
    while ((yytext[i] != '"' && yytext[i] != '\0')) {
        if (yytext[i] == '\\') {
            switch (yytext[++i]) {
                case 'n': addToBuff('\n'); break;
                case 't': addToBuff('\t'); break;
                case '\\': addToBuff('\\'); break;
                case '"': addToBuff('"'); break;
                default:
                    addToBuff('\\');
                    addToBuff(yytext[i]);
                    fprintf(stderr, "Invalid escape character in line %d\n", yylineno);
                    break;
            }
        } else {
            addToBuff(yytext[i]);
        }
        i++;
    }

    yylval.stringValue = string_buf;
    return STRINGCONST;
}

{UNTERMINATED_STRING}   { fprintf(stderr, "Unterminated string found in line: %d\n", yylineno); }

"{"         { return LBRACE; }
"}"         { return RBRACE; }
"["         { return LBRACKET; }
"]"         { return RBRACKET; }
"("         { return LPAREN; }
")"         { return RPAREN; }
";"         { return SEMICOLON; }
","         { return COMMA; }
":"         { return COLON; }
"::"        { return DBL_COLON; }
"."         { return DOT; }
".."        { return DBL_DOT; }

{ILL_ID}    { fprintf(stderr, "Error: Identifiers starting with \'_\' are not allowed. At line %d\n", yylineno); }
{ID}        { yylval.stringValue = strdup(yytext); return IDENTIFIER; }

{WHITESPACE} {}

{SINGLE_LINE_COMM}  {  }

{MULTI_LINE_COMM_BEG} {
    if (comment_depth == 0) current_comment_info.start_line = yylineno;
    comment_depth++;
    BEGIN(COMMENT_MODE);
}

<COMMENT_MODE>{MULTI_LINE_COMM_END} {
    comment_depth--;
    if (comment_depth == 0) BEGIN(INITIAL);
}

<COMMENT_MODE>.|\n { /* skip */ }

<COMMENT_MODE><<EOF>> {
    fprintf(stderr, "Error: EOF reached before finding a closing comment (\"*/\") at line %d\n", current_comment_info.start_line);
    yyterminate();
}

.           { fprintf(stderr, "Line %d: Unrecognized character: %s\n", yylineno, yytext); }

%%