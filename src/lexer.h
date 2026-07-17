#ifndef ARROW_LEXER_H
#define ARROW_LEXER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOK_ERROR,

    // Literals
    TOK_INT_LIT,  TOK_FLOAT_LIT,
    TOK_CHAR_LIT, TOK_STR_LIT,

    TOK_WORD,

    // Symbols
    TOK_ADD,       TOK_SUB,
    TOK_MUL,       TOK_DIV,
    TOK_SEMICOLON, TOK_LNOT,
    TOK_LBRACE,    TOK_RBRACE,
    TOK_AT,        TOK_FUNC,
    TOK_ARROW,     TOK_LPAREN,
    TOK_RPAREN,    TOK_SCOPE,
    TOK_EXT_FUNC,  TOK_C_FUNC,
    TOK_HASH,      TOK_ARROW_HASH,
    TOK_DOT,

    // Symbols/Words
    TOK_MOD,       TOK_AND,
    TOK_OR,        TOK_XOR,
    TOK_SHL,       TOK_SHR,
    TOK_ROL,       TOK_ROR,
    TOK_NOT,       TOK_DROP,
    TOK_SWAP,      TOK_LT,
    TOK_LTEQ,      TOK_GT,
    TOK_GTEQ,      TOK_EQ,
    TOK_NEQ,       TOK_IF,
    TOK_COLON,

    // Words
    TOK_DUP,       TOK_OVER,
    TOK_DUP2,      TOK_OVER2,
    TOK_SWAP2,     TOK_WHILE,
    TOK_LOOP,      TOK_END,
    TOK_BRK,       TOK_CONTINUE,
    TOK_MACRO,     TOK_STRUCT,
    TOK_IMPORT,    TOK_NEG,
    TOK_RET,       TOK_ROT,
    TOK_ROTN,
    // Types
    TOK_I8,        TOK_U8,
    TOK_I16,       TOK_U16,
    TOK_I32,       TOK_U32,
    TOK_I64,       TOK_U64,
    TOK_F32,       TOK_F64,
    TOK_CHAR,      TOK_STR,
    // Generics
    TOK_INTEGER,   TOK_REAL,
    TOK_NUMBER,

    TOK_EOF,
    TOK_LAST = TOK_EOF,
} Token_Type;

typedef struct {
    Token_Type type;
    union {
        int64_t integer;
        double real;
    } as;
    const char *start;
    size_t len;
    int pos, line;
} Token;

typedef struct {
    Token prev, cur;
    const char *file_path;
    const char *src;
    const char *start;
    const char *current;
    int pos, line;
} Lexer;

void init_lexer(Lexer *lexer, const char *src, const char *file_path);
void lexer_next(Lexer *lexer);

char *tok_spelling(Token_Type type);
void print_token(Token token);

#endif // ARROW_LEXER_H
