#ifndef INPUT_H
#define INPUT_H

typedef enum{
    INPUT_OK,
    INPUT_INTERRUPTED,
    INPUT_EOF
}InputResult;
InputResult read_input(char *line, int max_len);

#endif