#ifndef LEXER_H
#define LEXER_H

typedef enum {
    token_word,
    token_op_lt,
    token_op_gt,
    token_op_gtgt,
    token_op_pipe,
    token_op_semi,
    token_op_amp,
    token_eof,
    token_error
} Tokentype;

typedef struct {
    Tokentype type;
    char *value;
} Token;

typedef struct {
    Token *tokens;
    int size;
    int capacity;
} Tokenlist;

Tokenlist *token(const char *input);
void free_token(Tokenlist *list);

#endif