#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"

static Command *new_command(void) {
    return calloc(1, sizeof(Command));
}

static void add_arg(Command *c, const char *val) {
    if (c->argc >= MAX_ARGS - 1) return;
    c->argv[c->argc] = strdup(val);
    c->argc++;
    c->argv[c->argc] = NULL;
    if (c->name == NULL) c->name = c->argv[0];
}

static void add_input(Command *c, const char *file) {
    if (c->in_count >= MAX_REDIR) return;
    c->in_files[c->in_count++] = strdup(file);
}

static void add_output(Command *c, const char *file, bool append) {
    if (c->out_count >= MAX_REDIR) return;
    c->out_append[c->out_count] = append;
    c->out_files[c->out_count++] = strdup(file);
}

static bool is_empty_stage(const Command *c) {
    return c->argc == 0 && c->in_count == 0 && c->out_count == 0;
}

static void free_command(Command *c) {
    if (c == NULL) return;
    for (int i = 0; i < c->argc; i++) free(c->argv[i]);
    for (int i = 0; i < c->in_count; i++) free(c->in_files[i]);
    for (int i = 0; i < c->out_count; i++) free(c->out_files[i]);
    free(c);
}

static void free_pipeline(Pipeline *p) {
    if (p == NULL) return;
    for (int i = 0; i < p->nstages; i++) free_command(p->stages[i]);
    free(p);
}

// Parses one '|'-chain starting at *i. Stops at the first ';', '&', or
// EOF, advancing *i past that terminator (except EOF, where *i is left
// pointing at the EOF token). Returns which terminator was seen.
static Tokentype parse_group(Tokenlist *list, int *i, Pipeline **out) {
    Pipeline *p = calloc(1, sizeof(Pipeline));
    p->stages[0] = new_command();
    p->nstages = 1;
    Command *cur = p->stages[0];

    Tokentype terminator = token_eof;

    while (*i < list->size) {
        Tokentype type = list->tokens[*i].type;
        char *val = list->tokens[*i].value;

        if (type == token_eof || type == token_error) {
            terminator = token_eof;
            break;
        }
        if (type == token_op_semi) {
            terminator = token_op_semi;
            (*i)++;
            break;
        }
        if (type == token_op_amp) {
            terminator = token_op_amp;
            (*i)++;
            break;
        }

        if (type == token_word) {
            if (val != NULL) add_arg(cur, val);
            (*i)++;
        } else if (type == token_op_lt || type == token_op_gt || type == token_op_gtgt) {
            if (*i + 1 < list->size && list->tokens[*i + 1].type == token_word
                && list->tokens[*i + 1].value != NULL) {
                (*i)++;
                if (type == token_op_lt) {
                    add_input(cur, list->tokens[*i].value);
                } else {
                    add_output(cur, list->tokens[*i].value, type == token_op_gtgt);
                }
            }
            (*i)++;
        } else if (type == token_op_pipe) {
            if (p->nstages < MAX_STAGES) {
                cur = new_command();
                p->stages[p->nstages++] = cur;
            }
            (*i)++;
        } else {
            (*i)++;
        }
    }

    if (p->nstages == 1 && is_empty_stage(p->stages[0])) {
        free_command(p->stages[0]);
        p->stages[0] = NULL;
        p->nstages = 0;
    }
    *out = p;
    return terminator;
}

CommandLine *extract_line(Tokenlist *list) {
    CommandLine *cl = calloc(1, sizeof(CommandLine));
    if (cl == NULL || list == NULL) return cl;

    int i = 0;
    while (i < list->size && list->tokens[i].type != token_eof
           && cl->ngroups < MAX_GROUPS) {
        Pipeline *p;
        Tokentype term = parse_group(list, &i, &p);

        if (p->nstages == 0) {
            // Nothing there (e.g. a stray trailing separator). Grammar
            // validation upstream should already reject genuinely
            // malformed input, so just stop cleanly.
            free_pipeline(p);
            break;
        }

        p->background = (term == token_op_amp);
        cl->groups[cl->ngroups++] = p;
    }

    return cl;
}

void free_command_line(CommandLine *cl) {
    if (cl == NULL) return;
    for (int i = 0; i < cl->ngroups; i++) {
        free_pipeline(cl->groups[i]);
    }
    free(cl);
}