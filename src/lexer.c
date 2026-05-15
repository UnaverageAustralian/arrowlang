#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lexer.h"

#define NUM_KEYWORDS TOK_LAST - TOK_MOD
static const char *keywords[] = {
    "mod",     "and",
    "or",      "xor",
    "shl",     "shr",
    "rol",     "ror",
    "not",     "drop",
    "swap",    "lt",
    "lteq",    "gt",
    "gteq",    "eq",
    "if",      "else",

    "dup",     "over",
    "dup2",    "over2",
    "swap2",   "while",
    "loop",    "end",
    "brk",     "continue",
    "macro",   "struct",
    "import",  "neg",
    "abs",

    "i8",      "u8",
    "i16",     "u16",
    "i32",     "u32",
    "i64",     "u64",
    "f32",     "f64",
    "char",    "tof32",
    "tof64",   "toint",

    "Integer", "Real",
    "Number",
};

void init_lexer(Lexer *lexer, const char *src, const char *file_path) {
    lexer->prev = (Token){0};
    lexer->cur = (Token){0};

    lexer->line = 1;
    lexer->pos = 1;

    lexer->src = src;
    lexer->file_path = file_path;

    lexer->current = lexer->src;
    lexer->start = lexer->src;
}

static inline char skip(Lexer *lexer, int count) {
    lexer->pos += count;
    lexer->current += count;
    return *lexer->current;
}

static inline char peek(Lexer *lexer, int count) {
    return lexer->current[count];
}

static inline int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' &&  c <= 'f') || (c >= 'A' && c <= 'F');
}

inline void begin_token(Lexer *lexer) {
    lexer->cur.pos = lexer->pos;
    lexer->cur.line = lexer->line;
}

inline void make_token(Lexer *lexer, Token_Type type) {
    lexer->cur.start = lexer->start;
    lexer->cur.type = type;
    lexer->cur.len = lexer->current - lexer->start;
}

inline void make_err_token(Lexer *lexer, char *msg) {
    lexer->cur.start = msg;
    lexer->cur.type = TOK_ERROR;
    lexer->cur.len = strlen(msg);
}

void parse_exponent(Lexer *lexer, int base, double *num) {
    double sign = 1, exp = 0;

    char c = skip(lexer, 1);
    if ((c < '0' || c > '9') && c != '-' && c != '+') {
        skip(lexer, -1);
        make_token(lexer, TOK_INT_LIT);
        return;
    }

    if (c == '-') {
        sign = -1;
        c = skip(lexer, 1);
    }
    else if (c == '+') {
        c = skip(lexer, 1);
    }

    if (c < '0' || c > '9') {
        skip(lexer, -2);
        make_token(lexer, TOK_INT_LIT);
        return;
    }

    while (c >= '0' && c <= '9') {
        exp = exp * 10 + (c - '0');
        c = skip(lexer, 1);
    }
    *num *= base == 10 ? pow(10, exp * sign) : pow(2, exp * sign);
}

void lex_binary(Lexer *lexer) {
    int had_error = 0;
    int64_t num = 0;

    char c = peek(lexer, 0);
    if (c != '0' && c != '1') {
        skip(lexer, -1);
        make_token(lexer, TOK_INT_LIT);
        lexer->cur.as.integer = 0;
        return;
    }

    while (c == '0' || c == '1') {
        if (num > (INT64_MAX >> 1))
            had_error = 1;
        num = num * 2 + (c - '0');
        c = skip(lexer, 1);
    }

    if (had_error)
        make_err_token(lexer, "Number is too large for 64 bits.\n");
    else
        make_token(lexer, TOK_INT_LIT);

    lexer->cur.as.integer = num;
}

void lex_octal(Lexer *lexer) {
    int had_error = 0;
    int64_t num = 0;

    char c = peek(lexer, 0);
    if (c < '0' || c > '7') {
        skip(lexer, -1);
        make_token(lexer, TOK_INT_LIT);
        lexer->cur.as.integer = 0;
        return;
    }

    while (c >= '0' && c <= '7') {
        if (num > (INT64_MAX >> 3))
            had_error = 1;
        num = num * 8 + (c - '0');
        c = skip(lexer, 1);
    }

    if (had_error)
        make_err_token(lexer, "Number is too large for 64 bits.\n");
    else
        make_token(lexer, TOK_INT_LIT);

    lexer->cur.as.integer = num;
}

void lex_hex(Lexer *lexer) {
    double num = 0;
    int real = 0;

    char c = peek(lexer, 0);
    if (!is_hex(c)) {
        skip(lexer, -1);
        make_token(lexer, TOK_INT_LIT);
        lexer->cur.as.integer = 0;
        return;
    }

    while (is_hex(c)) {
        num = num * 16 + (c > 'F' ? c - 'a' : c > '9' ? c - 'A' : c - '0');
        c = skip(lexer, 1);
    }

    if (c == '.') {
        real = 1;
        c = skip(lexer, 1);

        double dividend = 0, divisor = 1;
        while (is_hex(c)) {
            divisor *= 16;
            dividend = dividend * 16 + (c > 'F' ? c - 'a' : c > '9' ? c - 'A' : c - '0');
            c = skip(lexer, 1);
        }
        num += dividend/divisor;
    }

    if (c == 'p' || c == 'P') {
        real = 1;
        parse_exponent(lexer, 16, &num);
    }

    if (!real && num > INT64_MAX)
        make_err_token(lexer, "Number is too large for 64 bits.\n");
    else
        make_token(lexer, real ? TOK_FLOAT_LIT : TOK_INT_LIT);

    if (real)
        lexer->cur.as.real = num;
    else
        lexer->cur.as.integer = (int64_t)num;
}

void lex_decimal(Lexer *lexer) {
    double num = 0;
    int real = 0;

    char c = peek(lexer, 0);
    while (c >= '0' && c <= '9') {
        num = num * 10 + (c - '0');
        c = skip(lexer, 1);
    }

    if (c == '.') {
        real = 1;
        c = skip(lexer, 1);

        double dividend = 0, divisor = 1;
        while (c >= '0' && c <= '9') {
            divisor *= 10;
            dividend = dividend * 10 + (c - '0');
            c = skip(lexer, 1);
        }
        num += dividend/divisor;
    }

    if (c == 'e' || c == 'E') {
        real = 1;
        parse_exponent(lexer, 10, &num);
    }

    if (!real && num > INT64_MAX)
        make_err_token(lexer, "Number is too large for 64 bits.");
    else
        make_token(lexer, real ? TOK_FLOAT_LIT : TOK_INT_LIT);

    if (real)
        lexer->cur.as.real = num;
    else
        lexer->cur.as.integer = (int64_t)num;
}

void lex_number(Lexer *lexer) {
    char c = peek(lexer, 0);
    if (c == '-' || c == '+')
        c = skip(lexer, 1);

    if (c == '0') {
        c = skip(lexer, 1);
        switch (c) {
        case 'X':
        case 'x':
            skip(lexer, 1);
            lex_hex(lexer);
            return;
        case 'O':
        case 'o':
            skip(lexer, 1);
            lex_octal(lexer);
            return;
        case 'B':
        case 'b':
            skip(lexer, 1);
            lex_binary(lexer);
            return;
        default:
            break;
        }
    }
    lex_decimal(lexer);
}

void lex_word(Lexer *lexer) {
    char buf[256] = {0};

    char c = peek(lexer, 0);
    for (int i = 0; isalnum(c); i++) {
        if (i >= 255) {
            make_err_token(lexer, "Identifier is too long");
            return;
        }
        buf[i] = c;
        c = skip(lexer, 1);
    }

    for (int i = 0; i < NUM_KEYWORDS; i++) {
        if (strcmp(buf, keywords[i]) == 0) {
            make_token(lexer, i + TOK_MOD);
            return;
        }
    }
    make_token(lexer, TOK_WORD);
}

void single_line_comment(Lexer *lexer) {
    char c = skip(lexer, 2);
    while (c && c != '\n')
        c = skip(lexer, 1);
    if (c) skip(lexer, 1);
    lexer->line++;
}

void multi_line_comment(Lexer *lexer) {
    char c = skip(lexer, 2);
    while (c && (c != '*' || peek(lexer, 1) != '/') ) {
        if (c == '\n') lexer->line++;
        c = skip(lexer, 1);
    }
    if (c) skip(lexer, 2);
}

void skip_whitespace(Lexer *lexer) {
    for (; ;) {
        char c = peek(lexer, 0);
        switch (c) {
        case '/':
            c = peek(lexer, 1);
            if (c == '/')
                single_line_comment(lexer);
            else if (c == '*')
                multi_line_comment(lexer);
            else
                return;
            break;
        case ' ':
        case '\t':
        case '\r':
            c = skip(lexer, 1);
            break;
        case '\n':
            c = skip(lexer, 1);
            lexer->pos = 1;
            lexer->line++;
            break;
        default: return;
        }
    }
}

void lexer_next(Lexer *lexer) {
    lexer->prev = lexer->cur;
    lexer->cur = (Token){0};

    skip_whitespace(lexer);
    lexer->start = lexer->current;
    begin_token(lexer);

    char c = peek(lexer, 0);
    switch (c) {
    case '+':
        skip(lexer, 1);
        make_token(lexer, TOK_ADD);
        break;
    case '-':
        skip(lexer, 1);
        make_token(lexer, TOK_SUB);
        break;
    case '*':
        skip(lexer, 1);
        make_token(lexer, TOK_MUL);
        break;
    case '/':
        skip(lexer, 1);
        make_token(lexer, TOK_DIV);
        break;
    case '%':
        skip(lexer, 1);
        make_token(lexer, TOK_MOD);
        break;
    case '&':
        skip(lexer, 1);
        make_token(lexer, TOK_AND);
        break;
    case '|':
        skip(lexer, 1);
        make_token(lexer, TOK_OR);
        break;
    case '^':
        skip(lexer, 1);
        make_token(lexer, TOK_XOR);
        break;
    case '~':
        skip(lexer, 1);
        make_token(lexer, TOK_NOT);
        break;
    case '?':
        skip(lexer, 1);
        make_token(lexer, TOK_IF);
        break;
    case ':':
        skip(lexer, 1);
        make_token(lexer, TOK_ELSE);
        break;
    case ';':
        skip(lexer, 1);
        make_token(lexer, TOK_SEMICOLON);
        break;
    case '=':
        skip(lexer, 1);
        make_token(lexer, TOK_EQ);
        break;
    case '{':
        skip(lexer, 1);
        make_token(lexer, TOK_LBRACE);
        break;
    case '}':
        skip(lexer, 1);
        make_token(lexer, TOK_RBRACE);
        break;
    case '.':
        skip(lexer, 1);
        make_token(lexer, TOK_DROP);
        break;
    case '\\':
        skip(lexer, 1);
        make_token(lexer, TOK_SWAP);
        break;
    case '!':
        skip(lexer, 1);
        make_token(lexer, TOK_LNOT);
        break;
    case '<':
        skip(lexer, 1);
        if (peek(lexer, 0) == '<' && peek(lexer, 1) == '<') {
            skip(lexer, 2);
            make_token(lexer, TOK_ROL);
        }
        else if (peek(lexer, 0) == '<') {
            skip(lexer, 1);
            make_token(lexer, TOK_SHL);
        }
        else if (peek(lexer, 0) == '=') {
            skip(lexer, 1);
            make_token(lexer, TOK_LTEQ);
        }
        else {
            make_token(lexer, TOK_LT);
        }
        break;
    case '>':
        skip(lexer, 1);
        if (peek(lexer, 0) == '>' && peek(lexer, 1) == '>') {
            skip(lexer, 2);
            make_token(lexer, TOK_ROR);
        }
        else if (peek(lexer, 0) == '>') {
            skip(lexer, 1);
            make_token(lexer, TOK_SHR);
        }
        else if (peek(lexer, 0) == '=') {
            skip(lexer, 1);
            make_token(lexer, TOK_GTEQ);
        }
        else {
            make_token(lexer, TOK_GT);
        }
        break;
    case '\0':
        make_token(lexer, TOK_EOF);
        break;
    default:
        if (c >= '0' && c <= '9') {
            lex_number(lexer);
        }
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            lex_word(lexer);
        }
        else {
            skip(lexer, 1);
            make_err_token(lexer, "Unknown character");
        }
        break;
    }
}

char *tok_spelling(Token_Type type) {
    switch (type) {
    case TOK_ADD:       return "ADD";
    case TOK_SUB:       return "SUB";
    case TOK_MUL:       return "MUL";
    case TOK_DIV:       return "DIV";
    case TOK_MOD:       return "MOD";
    case TOK_AND:       return "AND";
    case TOK_OR:        return "OR";
    case TOK_XOR:       return "XOR";
    case TOK_SHL:       return "SHL";
    case TOK_SHR:       return "SHR";
    case TOK_ROL:       return "ROL";
    case TOK_ROR:       return "ROR";
    case TOK_NOT:       return "NOT";
    case TOK_EQ:        return "EQ";
    case TOK_LT:        return "LT";
    case TOK_LTEQ:      return "LTEQ";
    case TOK_GT:        return "GT";
    case TOK_GTEQ:      return "GTEQ";
    case TOK_LNOT:      return "LNOT";
    case TOK_DUP:       return "DUP";
    case TOK_OVER:      return "OVER";
    case TOK_DUP2:      return "DUP2";
    case TOK_DROP:      return "DROP";
    case TOK_SWAP:      return "SWAP";
    case TOK_OVER2:     return "OVER2";
    case TOK_SWAP2:     return "SWAP2";
    case TOK_NEG:       return "NEG";
    case TOK_ABS:       return "ABS";
    case TOK_IF:        return "IF";
    case TOK_ELSE:      return "ELSE";
    case TOK_WHILE:     return "WHILE";
    case TOK_LBRACE:    return "LBRACE";
    case TOK_RBRACE:    return "RBRACE";
    case TOK_LOOP:      return "LOOP";
    case TOK_END:       return "END";
    case TOK_BRK:       return "BRK";
    case TOK_CONTINUE:  return "CONTINUE";
    case TOK_SEMICOLON: return "SEMICOLON";
    case TOK_INT_LIT:   return "INT_LIT";
    case TOK_FLOAT_LIT: return "FLOAT_LIT";
    case TOK_WORD:      return "WORD";
    case TOK_EOF:       return "EOF";
    default:            return "UNKNOWN";
    }
}

void print_token(Token token) {
    printf("%d:%d: ", token.line, token.pos);
    printf("%s", tok_spelling(token.type));
    printf(" \"%.*s\" (%lld/%lf)\n", token.len, token.start, token.as.integer, token.as.real);
}

