#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

static bool parse_arg(Tokenlist *list, int *index);
static bool parse_cmd(Tokenlist *list, int *index);
static bool parse_target(Tokenlist *list, int *index);
static bool parse_background(Tokenlist *list, int *index);

static Tokentype peek_type(Tokenlist *list, int index) {
    if (index >= list->size) {
        return token_eof;
    }
    return list->tokens[index].type;
}

static bool parse_line(Tokenlist *list, int *index) {
    Tokentype t = peek_type(list, *index);

    if (t == token_eof) {
        return true;
    }
    if (t == token_word) {
        (*index)++;
        return parse_arg(list, index);
    }
    return false;
}

static bool parse_arg(Tokenlist *list, int *index) {
    Tokentype t = peek_type(list, *index);

    if (t == token_eof) {
        return true;
    }
    if (t == token_word) {
        (*index)++;
        return parse_arg(list, index);
    }
    if (t == token_op_lt || t == token_op_gt || t == token_op_gtgt) {
        (*index)++;
        return parse_target(list, index);
    }
    if (t == token_op_pipe || t == token_op_semi) {
        (*index)++;
        return parse_cmd(list, index);
    }
    if (t == token_op_amp) {
        (*index)++;
        return parse_background(list, index);
    }
    return false;
}

static bool parse_cmd(Tokenlist *list, int *index) {
    if (peek_type(list, *index) == token_word) {
        (*index)++;
        return parse_arg(list, index);
    }
    return false;
}

static bool parse_target(Tokenlist *list, int *index) {
    if (peek_type(list, *index) == token_word) {
        (*index)++;
        return parse_arg(list, index);
    }
    return false;
}

static bool parse_background(Tokenlist *list, int *index) {
    Tokentype t = peek_type(list, *index);

    if (t == token_eof) {
        return true;
    }
    if (t == token_word) {
        (*index)++;
        return parse_arg(list, index);
    }
    return false;
}

bool validate(Tokenlist *list) {
    if (list == NULL || list->size == 0) {
        return false;
    }
    for (int i = 0; i < list->size; i++) {
        if (list->tokens[i].type == token_error) {
            return false;
        }
    }

    int index = 0;
    if (!parse_line(list, &index)) {
        return false;
    }
    return peek_type(list, index) == token_eof;
}