#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

#define MAX_TOKEN_LEN 2048

static void add_token(Tokenlist *list, Tokentype type, const char *val) {
    if (list->size >= list->capacity) {
        list->capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
        Token *tmp = realloc(list->tokens, (size_t)list->capacity * sizeof(Token));
        if (tmp == NULL) {
            return;
        }
        list->tokens = tmp;
    }
    list->tokens[list->size].type = type;
    list->tokens[list->size].value = (val != NULL) ? strdup(val) : NULL;
    list->size++;
}

void free_token(Tokenlist *list) {
    if (list == NULL) {
        return;
    }
    for (int i = 0; i < list->size; i++) {
        free(list->tokens[i].value);
    }
    free(list->tokens);
    free(list);
}

static int read_word(const char *input, int len, int *k, char *temp, const char **err) {
    int ti = 0;

    while (*k < len) {
        char c = input[*k];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        if (c == '|' || c == '&' || c == ';' || c == '<' || c == '>') break;

        if (ti >= MAX_TOKEN_LEN - 4) {
            *err = "token_too_long";
            return -1;
        }

        if (c == '\\') {
            (*k)++;
            if (*k >= len) {
                *err = "trailing_backslash";
                return -1;
            }
            temp[ti++] = input[(*k)++];
        } else if (c == '\'') {
            (*k)++;
            while (*k < len && input[*k] != '\'') {
                if (ti >= MAX_TOKEN_LEN - 4) { *err = "token_too_long"; return -1; }
                temp[ti++] = input[(*k)++];
            }
            if (*k >= len) {
                *err = "unclosed_quote";
                return -1;
            }
            (*k)++;
        } else if (c == '"') {
            (*k)++;
            while (*k < len && input[*k] != '"') {
                if (ti >= MAX_TOKEN_LEN - 4) { *err = "token_too_long"; return -1; }
                if (input[*k] == '\\') {
                    if (*k + 1 >= len) {
                        *err = "trailing_backslash";
                        return -1;
                    }
                    if (input[*k + 1] == '"' || input[*k + 1] == '\\') {
                        (*k)++;
                        temp[ti++] = input[(*k)++];
                    } else {
                        temp[ti++] = input[(*k)++];
                        temp[ti++] = input[(*k)++];
                    }
                } else {
                    temp[ti++] = input[(*k)++];
                }
            }
            if (*k >= len) {
                *err = "unclosed_quote";
                return -1;
            }
            (*k)++;
        } else {
            temp[ti++] = input[(*k)++];
        }
    }

    temp[ti] = '\0';
    return 0;
}

Tokenlist *token(const char *input) {
    Tokenlist *list = malloc(sizeof(Tokenlist));
    if (list == NULL) {
        return NULL;
    }
    list->tokens = NULL;
    list->size = 0;
    list->capacity = 0;

    int k = 0;
    int len = (int)strlen(input);

    while (k < len) {
        char c = input[k];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            k++;
            continue;
        }
        if (c == '<') {
             add_token(list, token_op_lt,   "<");
              k++;
               continue;
             }
        if (c == '|') {
             add_token(list, token_op_pipe, "|"); 
             k++; 
             continue; 
            }
        if (c == '&') { 
            add_token(list, token_op_amp,  "&");
             k++; 
             continue; 
            }
        if (c == ';') {
             add_token(list, token_op_semi, ";")
             ; 
             k++; 
             continue;
             }
        if (c == '>') {
            if (k + 1 < len && input[k + 1] == '>') {
                add_token(list, token_op_gtgt, ">>");
                k += 2;
            } else {
                add_token(list, token_op_gt, ">");
                k += 1;
            }
            continue;
        }

        char temp[MAX_TOKEN_LEN];
        const char *err = NULL;
        if (read_word(input, len, &k, temp, &err) != 0) {
            add_token(list, token_error, err);
            return list;
        }
        add_token(list, token_word, temp);
    }

    add_token(list, token_eof, "EOF");
    return list;
}