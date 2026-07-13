#ifndef T32_CLI_H
#define T32_CLI_H

#include <stdbool.h>
#include <stdio.h>

#include "t32.h"

typedef struct {
    t32_machine_t *machine;
    FILE *input;
    bool interactive;
    uint64_t run_limit;
} t32_cli_t;

void t32_cli_init(
    t32_cli_t *cli,
    t32_machine_t *machine,
    FILE *input,
    bool interactive
);

int t32_cli_eval_line(t32_cli_t *cli, const char *line);
int t32_cli_run(t32_cli_t *cli);

#endif
