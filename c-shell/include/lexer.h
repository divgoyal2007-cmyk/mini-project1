#ifndef lexer_h
#define lexer_h
typedef enum{
    token_word,
    token_op_pipe, //|
    token_op_amp,  //&
    token_op_semi, //;
    token_op_lt, //<
    token_op_gt, //>
    token_op_gtgt, //>>
    token_eof, //end
    token_error //error
}Tokentype;

typedef struct{
    Tokentype type;
    char* value;
}Token;
typedef struct{
    Token* tokens;
    int size;
    int capacity;
}Tokenlist;

Tokenlist* token(char* input);
void list_token(Tokenlist* list);
#endif