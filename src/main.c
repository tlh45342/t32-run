#include "t32.h"
#include "t32_cli.h"
#include "t32_log.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    t32_machine_t *machine;
    t32_cli_t cli;
    bool interactive;
    int result;

    machine = t32_create(T32_DEFAULT_MEMORY_SIZE);
    if (!machine) {
        fprintf(stderr, "t32-run: could not create machine\n");
        return 1;
    }

    if (argc == 3) {
        uint32_t address = (uint32_t)strtoul(argv[2], NULL, 0);

        if (!t32_load_file(machine, argv[1], address)) {
            fprintf(stderr, "t32-run: could not load %s\n", argv[1]);
            t32_destroy(machine);
            return 1;
        }

        t32_set_pc(machine, address);
        t32_run(machine, 0);

        printf("state=%s\n", t32_state_name(t32_get_state(machine)));
        printf("pc=0x%08x\n", t32_get_pc(machine));
        printf("r0=0x%08x\n", t32_get_register(machine, 0));

        if (t32_get_halt_reason(machine)[0])
            printf("reason=%s\n", t32_get_halt_reason(machine));

        {
            int exit_code =
                t32_get_state(machine) == T32_STATE_ERROR ? 1 : 0;
            t32_destroy(machine);
            return exit_code;
        }
    }

    if (argc != 1) {
        fprintf(stderr, "usage: t32-run [binary load-address]\n");
        t32_destroy(machine);
        return 1;
    }

    interactive = isatty(fileno(stdin)) != 0;
    t32_cli_init(&cli, machine, stdin, interactive);
    result = t32_cli_run(&cli);

    t32_log_close();
    t32_destroy(machine);
    return result;
}
