%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alpha_token.h"
#define YY_DECL int alpha_yylex(void* yylval)

alpha_token_t *head = NULL;
comment_info_t current_comment_info;

int comment_depth = 0;
int nested_comment = 0;
static int token_counter = 1;

char *string_buf = NULL;
int buf_size = 0;
int buf_len = 0;

/* Helper functions to convert enums to strings */
const char* categoryToString(TokenCategory category) {
    static const char* categories[] = {
        "KEYWORD", "OPERATOR", "INTCONST", "REALCONST", "STRINGCONST",
        "PUNCTUATION", "IDENTIFIER", "COMMENT", "COMMENT"
    };
    
    if (category >= 0 && category <= COMMENT_BLOCK) {
        return categories[category];
    }
    return "UNKNOWN_CATEGORY";
}
const char* keywordToString(KeywordType keyword) {
    static const char* keywords[] = {
        "IF", "ELSE", "WHILE", "FOR", "FUNCTION", "RETURN",
        "BREAK", "CONTINUE", "AND", "NOT", "OR", "LOCAL", "TRUE", "FALSE", "NIL"
    };
    
    if (keyword >= 0 && keyword <= NIL) {
        return keywords[keyword];
    }
    return "UNKNOWN_KEYWORD";
}
const char* operatorToString(OperatorType op) {
    static const char* operators[] = {
        "ASSIGN", "PLUS", "MINUS", "MULTIPLY", "DIVIDE", "MOD",
        "EQUAL", "NEQUAL", "INCR", "DECR", "GREATER", "LESS",
        "GREATER_EQUAL", "LESS_EQUAL"
    };
    
    if (op >= 0 && op <= LESS_EQUAL) {
        return operators[op];
    }
    return "UNKNOWN_OPERATOR";
}
const char* punctuationToString(PunctuationType punct) {
    static const char* punctuations[] = {
        "LBRACE", "RBRACE", "LBRACKET", "RBRACKET", "LPAREN", "RPAREN", 
        "SEMICOLON", "COMMA", "COLON", "DBL_COLON", "DOT", "DBL_DOT"
    };
    
    if (punct >= 0 && punct <= DBL_DOT) {
        return punctuations[punct];
    }
    return "UNKNOWN_PUNCTUATION";
}

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

void insertNode(int line_num, TokenCategory category, char* value) {
    alpha_token_t* newnode = calloc(1, sizeof(alpha_token_t));
    alpha_token_t* current = NULL;

    if (!newnode) {
        perror("calloc error in insertNode()");
        return;
    }

    newnode->line_num = line_num;
    newnode->token_num = token_counter++;
    newnode->lexeme = strdup(value);
    if (!newnode->lexeme) {
        perror("strdup error in insertNode()");
        free(newnode);
        return;
    }
    newnode->category = category;
    newnode->next = NULL;

    switch (category) {
        case KEYWORD:
            if (strcmp(value, "if") == 0) newnode->attr.keyword = IF;
            else if (strcmp(value, "else") == 0) newnode->attr.keyword = ELSE;
            else if (strcmp(value, "while") == 0) newnode->attr.keyword = WHILE;
            else if (strcmp(value, "for") == 0) newnode->attr.keyword = FOR;
            else if (strcmp(value, "function") == 0) newnode->attr.keyword = FUNCTION;
            else if (strcmp(value, "return") == 0) newnode->attr.keyword = RETURN;
            else if (strcmp(value, "break") == 0) newnode->attr.keyword = BREAK;
            else if (strcmp(value, "continue") == 0) newnode->attr.keyword = CONTINUE;
            else if (strcmp(value, "and") == 0) newnode->attr.keyword = AND;
            else if (strcmp(value, "not") == 0) newnode->attr.keyword = NOT;
            else if (strcmp(value, "or") == 0) newnode->attr.keyword = OR;
            else if (strcmp(value, "local") == 0) newnode->attr.keyword = LOCAL;
            else if (strcmp(value, "true") == 0) newnode->attr.keyword = TRUE;
            else if (strcmp(value, "false") == 0) newnode->attr.keyword = FALSE;
            else if (strcmp(value, "nil") == 0) newnode->attr.keyword = NIL;
            break;
        case OPERATOR:
            if (strcmp(value, "=") == 0) newnode->attr.op = ASSIGN;
            else if (strcmp(value, "+") == 0) newnode->attr.op = PLUS;
            else if (strcmp(value, "-") == 0) newnode->attr.op = MINUS;
            else if (strcmp(value, "*") == 0) newnode->attr.op = MULTIPLY;
            else if (strcmp(value, "/") == 0) newnode->attr.op = DIVIDE;
            else if (strcmp(value, "%") == 0) newnode->attr.op = MOD;
            else if (strcmp(value, "==") == 0) newnode->attr.op = EQUAL;
            else if (strcmp(value, "!=") == 0) newnode->attr.op = NEQUAL;
            else if (strcmp(value, "++") == 0) newnode->attr.op = INCR;
            else if (strcmp(value, "--") == 0) newnode->attr.op = DECR;
            else if (strcmp(value, ">") == 0) newnode->attr.op = GREATER;
            else if (strcmp(value, "<") == 0) newnode->attr.op = LESS;
            else if (strcmp(value, ">=") == 0) newnode->attr.op = GREATER_EQUAL;
            else if (strcmp(value, "<=") == 0) newnode->attr.op = LESS_EQUAL;
            break;
        case INTCONST:
            newnode->attr.int_value = atoi(value);
            break;
        case REALCONST:
            newnode->attr.real_value = atof(value);
            break;
        case STRINGCONST:
            newnode->attr.string_value = strdup(value);
            if (!newnode->attr.string_value){
                perror("strdup error in insertNode()");
                free(newnode);
                return;
            }
            break;
        case PUNCTUATION:
            if (strcmp(value, "{") == 0) newnode->attr.punctuation = LBRACE;
            else if (strcmp(value, "}") == 0) newnode->attr.punctuation = RBRACE;
            else if (strcmp(value, "[") == 0) newnode->attr.punctuation = LBRACKET;
            else if (strcmp(value, "]") == 0) newnode->attr.punctuation = RBRACKET;
            else if (strcmp(value, "(") == 0) newnode->attr.punctuation = LPAREN;
            else if (strcmp(value, ")") == 0) newnode->attr.punctuation = RPAREN;
            else if (strcmp(value, ";") == 0) newnode->attr.punctuation = SEMICOLON;
            else if (strcmp(value, ",") == 0) newnode->attr.punctuation = COMMA;
            else if (strcmp(value, ":") == 0) newnode->attr.punctuation = COLON;
            else if (strcmp(value, "::") == 0) newnode->attr.punctuation = DBL_COLON;
            else if (strcmp(value, ".") == 0) newnode->attr.punctuation = DOT;
            else if (strcmp(value, "..") == 0) newnode->attr.punctuation = DBL_DOT;
            break;
        case IDENTIFIER:
            newnode->attr.identifier_name = strdup(value);
            if (!newnode->attr.identifier_name) {
                perror("strdup error in insertNode()");
                free(newnode);
                return;
            }
            break;
        case COMMENT_LINE:
            newnode->attr.comment_type = strdup("LINE_COMMENT");
            if (!newnode->attr.comment_type) {
                perror("strdup error in insertNode()");
                free(newnode);
                return;
            }
            break;
        case COMMENT_BLOCK:
            newnode->attr.comment_type = strdup("BLOCK_COMMENT");
            if (!newnode->attr.comment_type) {
                perror("strdup error in insertNode()");
                free(newnode);
                return;
            }
            break;            
        default:
            printf("Unknown token category: %d\n", category);
            free(newnode);
            return;
    }
    if (head == NULL) {
        head = newnode;
    } else {
        current = head;
        while(current->next != NULL) current = current->next;
        current->next = newnode;
    }
}

void printList() {
    alpha_token_t* current = head;
    int token_num = 1;
    
    while (current != NULL) {
        current->token_num = token_num++;
        
        printf("%d:  #%d  \"%s\"  %s  ", 
               current->line_num, 
               current->token_num, 
               current->lexeme, 
               categoryToString(current->category));

        switch (current->category) {
            case KEYWORD:
                printf("%s  <-enumerated\n", keywordToString(current->attr.keyword));
                break;
            case OPERATOR:
                printf("%s  <-enumerated\n", operatorToString(current->attr.op));
                break;
            case PUNCTUATION:
                printf("%s  <-enumerated\n", punctuationToString(current->attr.punctuation));
                break;
            case INTCONST:
                printf("%d  <-integer\n", current->attr.int_value);
                break;
            case REALCONST:
                printf("%f  <-double\n", current->attr.real_value);
                break;
            case STRINGCONST:
                printf("\"%s\"  <-char*\n", current->attr.string_value);
                break;
            case IDENTIFIER:
                printf("\"%s\"  <-char*\n", current->attr.identifier_name);
                break;
            case COMMENT_LINE:
                printf("%s  <-enumerated\n", current->attr.comment_type);
                break;
            case COMMENT_BLOCK:
                printf("%s  <-enumerated\n", current->attr.comment_type);
                break;
            default:
                printf(": Unknown\n");
        }
        
        current = current->next;
    }
}

void freeTokenList() {
    alpha_token_t* current = head;
    alpha_token_t* next;
    
    while (current != NULL) {
        next = current->next;
        
        free(current->lexeme);
        
        if (current->category == STRINGCONST) free(current->attr.string_value);
        else if (current->category == IDENTIFIER) free(current->attr.identifier_name);
        else if (current->category == COMMENT_LINE || current->category == COMMENT_BLOCK) free(current->attr.comment_type);
        
        free(current);
        current = next;
    }
    
    head = NULL;
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
STRING      \"([^\"\\]|\\.)*\"?
WHITESPACE  [\t' '\r '\n]* 

SINGLE_LINE_COMM        "//".*
MULTI_LINE_COMM_BEG     "/*"
MULTI_LINE_COMM_END     "*/"

%%

"if"        { insertNode(yylineno, KEYWORD, yytext); }
"else"      { insertNode(yylineno, KEYWORD, yytext); }
"while"     { insertNode(yylineno, KEYWORD, yytext); }
"for"       { insertNode(yylineno, KEYWORD, yytext); }
"function"  { insertNode(yylineno, KEYWORD, yytext); }
"return"    { insertNode(yylineno, KEYWORD, yytext); }
"break"     { insertNode(yylineno, KEYWORD, yytext); }
"continue"  { insertNode(yylineno, KEYWORD, yytext); }
"and"       { insertNode(yylineno, KEYWORD, yytext); }
"not"       { insertNode(yylineno, KEYWORD, yytext); }
"or"        { insertNode(yylineno, KEYWORD, yytext); }
"local"     { insertNode(yylineno, KEYWORD, yytext); }
"true"      { insertNode(yylineno, KEYWORD, yytext); }
"false"     { insertNode(yylineno, KEYWORD, yytext); }
"nil"       { insertNode(yylineno, KEYWORD, yytext); }

"="         { insertNode(yylineno, OPERATOR, yytext); }
"+"         { insertNode(yylineno, OPERATOR, yytext); }
"-"         { insertNode(yylineno, OPERATOR, yytext); }
"*"         { insertNode(yylineno, OPERATOR, yytext); }
"/"         { insertNode(yylineno, OPERATOR, yytext); }
"%"         { insertNode(yylineno, OPERATOR, yytext); }
"=="        { insertNode(yylineno, OPERATOR, yytext); }
"!="        { insertNode(yylineno, OPERATOR, yytext); }
"++"        { insertNode(yylineno, OPERATOR, yytext); }
"--"        { insertNode(yylineno, OPERATOR, yytext); }
">"         { insertNode(yylineno, OPERATOR, yytext); }
"<"         { insertNode(yylineno, OPERATOR, yytext); }
">="        { insertNode(yylineno, OPERATOR, yytext); }
"<="        { insertNode(yylineno, OPERATOR, yytext); }

{INT}       { insertNode(yylineno, INTCONST, yytext); }
{REAL}      { insertNode(yylineno, REALCONST, yytext); }

{STRING}    {
    buf_size = 16;
    buf_len = 0;
    string_buf = calloc(buf_size, 1);
    if (!string_buf) {
        fprintf(stderr, "calloc failed in string_buf\n");
        yyterminate();
    }

    int len = strlen(yytext);
    if (yytext[len-1] != '"') {
        fprintf(stderr, "Unterminated string at line %d\n", yylineno);

        for (int i = len - 1; i >= 0; i--) unput(yytext[i]);

        free(string_buf);
        string_buf = NULL;
        buf_len = 0;
        buf_size = 0;
        yyterminate();
    }

    int i = 1;
    while ((yytext[i] != '"' && yytext[i] != '\0')) {
        if (yytext[i] == '\\') {
            i++;
            switch (yytext[i]) {
                case 'n':
                    addToBuff('\n');
                    break;
                case 't':
                    addToBuff('\t');
                    break;
                case '\\':
                    addToBuff('\\');
                    break;
                case '"':
                    addToBuff('"');
                    break;
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

    insertNode(yylineno, STRINGCONST, string_buf);
    free(string_buf);
    string_buf = NULL;
    buf_len = 0;
    buf_size = 0;
}

"{"         { insertNode(yylineno, PUNCTUATION, yytext); }
"}"         { insertNode(yylineno, PUNCTUATION, yytext); }
"["         { insertNode(yylineno, PUNCTUATION, yytext); }
"]"         { insertNode(yylineno, PUNCTUATION, yytext); }
"("         { insertNode(yylineno, PUNCTUATION, yytext); }
")"         { insertNode(yylineno, PUNCTUATION, yytext); }
";"         { insertNode(yylineno, PUNCTUATION, yytext); }
","         { insertNode(yylineno, PUNCTUATION, yytext); }
":"         { insertNode(yylineno, PUNCTUATION, yytext); }
"::"        { insertNode(yylineno, PUNCTUATION, yytext); }
"."         { insertNode(yylineno, PUNCTUATION, yytext); }
".."        { insertNode(yylineno, PUNCTUATION, yytext); }

{ILL_ID}    { fprintf(stderr, "Error: Identifiers starting with \'_\' are not allowed. At line %d\n", yylineno); }
{ID}        { insertNode(yylineno, IDENTIFIER, yytext); }
{WHITESPACE} {}

{SINGLE_LINE_COMM}  { insertNode(yylineno, COMMENT_LINE, ""); }

{MULTI_LINE_COMM_BEG}     {
    if (comment_depth == 0) {
        current_comment_info.start_line = yylineno;
        BEGIN(COMMENT_MODE); 
    }
    comment_depth++;
    if (comment_depth >= 1) nested_comment++;
}

<COMMENT_MODE>{MULTI_LINE_COMM_END} { 
    comment_depth--;
    if (comment_depth == 0) {
        current_comment_info.end_line = yylineno;

        char lexeme[32];
        if (current_comment_info.start_line == current_comment_info.end_line)
            snprintf(lexeme, sizeof(lexeme), "%d", current_comment_info.start_line);
        else
            snprintf(lexeme, sizeof(lexeme), "%d - %d", current_comment_info.start_line, current_comment_info.end_line);

        insertNode(current_comment_info.start_line, COMMENT_BLOCK, lexeme);

        /* Brings the lexer out of the COMMENT state back to its initial state */
        BEGIN(INITIAL);
    } else if (nested_comment > 1){
        insertNode(yylineno, COMMENT_BLOCK, "NESTED");
        nested_comment--;
    }
    
}

<COMMENT_MODE>{MULTI_LINE_COMM_BEG} {
    comment_depth++;
}

<COMMENT_MODE>.|\n { /* ignore characters inside comments */ }

<COMMENT_MODE><<EOF>> { 
    fprintf(stderr, "Error: EOF reached before finding a closing comment (\"*/\") in line %d \n", current_comment_info.start_line); 
    BEGIN(INITIAL);
    yyterminate();
}

.           { fprintf(stderr, "Line %d: Unrecognized character: %s\n", yylineno, yytext); }


%%
int main(int argc, char *argv[]) {
    FILE *input_file = stdin;
    FILE *output_file = stdout;

    if (argc > 1 && !(input_file = fopen(argv[1], "r"))) {
        fprintf(stderr, "Cannot read file: %s\n", argv[1]);
        return 1;
    }

    if (argc > 2 && !(output_file = fopen(argv[2], "w"))) {
        fprintf(stderr, "Cannot write file: %s\n", argv[2]);
        return 1;
    }

    yyin = input_file;
    if (output_file != stdout) {
        printf("Output written in file\n");
        stdout = output_file;
    }

    int token;
    while ((token = alpha_yylex(NULL)) != 0) {  }

    printList();
    freeTokenList();

    if (input_file != stdin) fclose(input_file);
    if (output_file != stdout) fclose(output_file);

    return 0;
}
