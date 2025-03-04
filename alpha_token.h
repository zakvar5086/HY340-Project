#ifndef ALPHA_TOKEN_H
#define ALPHA_TOKEN_H
/* test reverse */
typedef enum {
    IF, ELSE, WHILE, FOR, FUNCTION, RETURN, BREAK, CONTINUE,
    AND, NOT, OR, LOCAL, TRUE, FALSE, NIL
} KeywordType;

typedef enum {
    ASSIGN, PLUS, MINUS, MULTIPLY, DIVIDE, MOD, EQUAL, 
    NEQUAL, INCR, DECR, GREATER, LESS, GREATER_EQUAL, 
    LESS_EQUAL
} OperatorType;

typedef enum {
    LBRACE, RBRACE, LBRACKET, RBRACKET, LPAREN, RPAREN, 
    SEMICOLON, COMMA, COLON, DBL_COLON, DOT, DBL_DOT
} PunctuationType;

typedef enum {
    KEYWORD,
    OPERATOR,
    INTCONST,
    REALCONST,
    STRINGCONST,
    PUNCTUATION,
    IDENTIFIER,
    COMMENT_LINE,
    COMMENT_BLOCK,
    WHITESPACE
} TokenCategory;

typedef struct comment_info {
    int start_line;
    int end_line;
} comment_info_t;

typedef struct alpha_token {
    int line_num;
    int token_num;
    char* lexeme;
    TokenCategory category;
    union {
        KeywordType keyword;
        OperatorType op;
        PunctuationType punctuation;
        int int_value;
        double real_value;
        char* string_value;
        char* identifier_name;
        char* comment_type;
    } attr;
    struct alpha_token* next;
} alpha_token_t;

void insertNode(int line_num, TokenCategory category, char* value);
void printList();
void freeTokenList();
const char* categoryToString(TokenCategory category);
const char* keywordToString(KeywordType keyword);
const char* operatorToString(OperatorType op);
const char* punctuationToString(PunctuationType punct);
void addToBuff(char input);

#endif
