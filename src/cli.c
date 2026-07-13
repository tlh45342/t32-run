/*
 * cli.c
 *
 * Scriptable and interactive front end for one local T32 machine.
 *
 * The same evaluator is used for:
 *
 *   t32-run
 *   t32-run < test.script
 *   do test.script
 */

#include "t32_cli.h"
#include "t32_log.h"
#include "version.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ARGS 32

typedef int (*command_fn)(t32_cli_t *, int, char **);

typedef struct {
    const char *name;
    command_fn function;
    const char *help;
} command_t;

static int command_help(t32_cli_t *, int, char **);
static int command_version(t32_cli_t *, int, char **);
static int command_load(t32_cli_t *, int, char **);
static int command_run(t32_cli_t *, int, char **);
static int command_step(t32_cli_t *, int, char **);
static int command_regs(t32_cli_t *, int, char **);
static int command_status(t32_cli_t *, int, char **);
static int command_examine(t32_cli_t *, int, char **);
static int command_set(t32_cli_t *, int, char **);
static int command_logfile(t32_cli_t *, int, char **);
static int command_do(t32_cli_t *, int, char **);
static int command_clrhalt(t32_cli_t *, int, char **);
static int command_reset(t32_cli_t *, int, char **);
static int command_quit(t32_cli_t *, int, char **);

static const command_t COMMANDS[] = {
    {"help",    command_help,    "show command help"},
    {"version", command_version, "show t32-run version"},
    {"load",    command_load,    "load <binary> <address>"},
    {"run",     command_run,     "run until halt or configured limit"},
    {"step",    command_step,    "step [count]"},
    {"regs",    command_regs,    "dump r0-r15 and pc"},
    {"status",  command_status,  "dump state, flags, and halt reason"},
    {"e",       command_examine, "e <start>[-<end>] | e <start> <length>"},
    {"set",     command_set,     "set pc|rN|run steps <value>"},
    {"logfile", command_logfile, "logfile <path> | logfile off"},
    {"do",      command_do,      "do <scriptfile>"},
    {"clrhalt", command_clrhalt, "clear halted state"},
    {"reset",   command_reset,   "reset registers, flags, pc, and state"},
    {"quit",    command_quit,    "exit"},
    {"exit",    command_quit,    "exit"}
};

static int equals_ignore_case(const char *left, const char *right)
{
    while (*left && *right) {
        if (tolower((unsigned char)*left) !=
            tolower((unsigned char)*right))
            return 0;

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static void trim(char *text)
{
    char *start = text;
    size_t length;

    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != text)
        memmove(text, start, strlen(start) + 1);

    length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static void strip_comments(char *text)
{
    char *cursor;

    for (cursor = text; *cursor; ++cursor) {
        if (*cursor == '#' || *cursor == ';' ||
            (*cursor == '/' && cursor[1] == '/')) {
            *cursor = '\0';
            break;
        }
    }

    trim(text);
}

static int tokenize(char *line, char **argv, int maximum)
{
    int argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token && argc < maximum) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    return argc;
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (!text || !*text || !value)
        return 0;

    parsed = strtoul(text, &end, 0);

    if (end == text || *end != '\0' || parsed > 0xfffffffful)
        return 0;

    *value = (uint32_t)parsed;
    return 1;
}

static int parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (!text || !*text || !value)
        return 0;

    parsed = strtoull(text, &end, 0);

    if (end == text || *end != '\0')
        return 0;

    *value = (uint64_t)parsed;
    return 1;
}

void t32_cli_init(
    t32_cli_t *cli,
    t32_machine_t *machine,
    FILE *input,
    bool interactive
)
{
    cli->machine = machine;
    cli->input = input;
    cli->interactive = interactive;
    cli->run_limit = 0;
}

int t32_cli_eval_line(t32_cli_t *cli, const char *line)
{
    char buffer[MAX_LINE];
    char *argv[MAX_ARGS];
    int argc;
    size_t index;

    snprintf(buffer, sizeof(buffer), "%s", line);
    strip_comments(buffer);

    if (!buffer[0])
        return 0;

    argc = tokenize(buffer, argv, MAX_ARGS);
    if (!argc)
        return 0;

    for (index = 0; index < sizeof(COMMANDS) / sizeof(COMMANDS[0]); ++index) {
        if (equals_ignore_case(argv[0], COMMANDS[index].name))
            return COMMANDS[index].function(cli, argc, argv);
    }

    t32_log_printf("[ERROR] unknown command: %s\n", argv[0]);
    return -1;
}

int t32_cli_run(t32_cli_t *cli)
{
    char line[MAX_LINE];

    for (;;) {
        int result;

        if (cli->interactive) {
            fputs("t32-run> ", stdout);
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), cli->input))
            break;

        result = t32_cli_eval_line(cli, line);
        if (result == 1)
            break;
    }

    return 0;
}

static int command_help(t32_cli_t *cli, int argc, char **argv)
{
    size_t index;

    (void)cli;
    (void)argc;
    (void)argv;

    t32_log_printf("t32-run commands:\n");

    for (index = 0; index < sizeof(COMMANDS) / sizeof(COMMANDS[0]); ++index) {
        t32_log_printf(
            "  %-8s %s\n",
            COMMANDS[index].name,
            COMMANDS[index].help
        );
    }

    return 0;
}

static int command_version(t32_cli_t *cli, int argc, char **argv)
{
    (void)cli;
    (void)argc;
    (void)argv;

    t32_log_printf("%s version %s\n", T32_RUN_PRODUCT_NAME, T32_VERSION);
    return 0;
}

static int command_load(t32_cli_t *cli, int argc, char **argv)
{
    uint32_t address;

    if (argc != 3 || !parse_u32(argv[2], &address)) {
        t32_log_printf("usage: load <binary> <address>\n");
        return -1;
    }

    if (!t32_load_file(cli->machine, argv[1], address)) {
        t32_log_printf(
            "[ERROR] could not load %s at 0x%08x\n",
            argv[1],
            address
        );
        return -1;
    }

    t32_log_printf(
        "loaded %s at 0x%08x\n",
        argv[1],
        address
    );
    return 0;
}

static int command_run(t32_cli_t *cli, int argc, char **argv)
{
    t32_step_result_t result;

    (void)argc;
    (void)argv;

    result = t32_run(cli->machine, cli->run_limit);

    if (result == T32_STEP_FAULT) {
        t32_log_printf(
            "[ERROR] run failed: %s\n",
            t32_get_halt_reason(cli->machine)
        );
        return -1;
    }

    return 0;
}

static int command_step(t32_cli_t *cli, int argc, char **argv)
{
    uint64_t count = 1;
    uint64_t index;

    if (argc > 2 || (argc == 2 && !parse_u64(argv[1], &count))) {
        t32_log_printf("usage: step [count]\n");
        return -1;
    }

    for (index = 0; index < count; ++index) {
        t32_step_result_t result = t32_step(cli->machine);

        if (result != T32_STEP_OK)
            break;
    }

    return 0;
}

static int command_regs(t32_cli_t *cli, int argc, char **argv)
{
    unsigned index;

    (void)argc;
    (void)argv;

    for (index = 0; index < T32_REGISTER_COUNT; ++index) {
        t32_log_printf(
            "r%-2u=0x%08x%s",
            index,
            t32_get_register(cli->machine, index),
            (index % 4u == 3u) ? "\n" : "  "
        );
    }

    t32_log_printf("pc =0x%08x\n", t32_get_pc(cli->machine));
    return 0;
}

static int command_status(t32_cli_t *cli, int argc, char **argv)
{
    t32_flags_t flags = t32_get_flags(cli->machine);
    const char *reason = t32_get_halt_reason(cli->machine);

    (void)argc;
    (void)argv;

    t32_log_printf(
        "state=%s\n",
        t32_state_name(t32_get_state(cli->machine))
    );
    t32_log_printf(
        "instructions=%llu\n",
        (unsigned long long)t32_get_instruction_count(cli->machine)
    );
    t32_log_printf("carry=%u\n", flags.carry ? 1u : 0u);
    t32_log_printf("zero=%u\n", flags.zero ? 1u : 0u);
    t32_log_printf("negative=%u\n", flags.negative ? 1u : 0u);
    t32_log_printf("overflow=%u\n", flags.overflow ? 1u : 0u);

    if (reason[0])
        t32_log_printf("reason=%s\n", reason);

    return 0;
}

static int command_examine(t32_cli_t *cli, int argc, char **argv)
{
    uint32_t start;
    uint32_t end;
    uint32_t address;

    if (argc < 2 || argc > 3) {
        t32_log_printf(
            "usage: e <start>[-<end>] | e <start> <length>\n"
        );
        return -1;
    }

    if (argc == 2) {
        char range[128];
        char *dash;

        snprintf(range, sizeof(range), "%s", argv[1]);
        dash = strchr(range, '-');

        if (dash) {
            *dash = '\0';

            if (!parse_u32(range, &start) ||
                !parse_u32(dash + 1, &end)) {
                t32_log_printf("[ERROR] invalid memory range\n");
                return -1;
            }
        } else {
            if (!parse_u32(range, &start)) {
                t32_log_printf("[ERROR] invalid memory address\n");
                return -1;
            }

            end = start;
        }
    } else {
        uint32_t length;

        if (!parse_u32(argv[1], &start) ||
            !parse_u32(argv[2], &length) ||
            length == 0) {
            t32_log_printf("[ERROR] invalid memory address or length\n");
            return -1;
        }

        end = start + length - 1;
        if (end < start) {
            t32_log_printf("[ERROR] memory range overflow\n");
            return -1;
        }
    }

    address = start;

    while (address <= end) {
        unsigned column;

        t32_log_printf("0x%08x:", address);

        for (column = 0; column < 16 && address <= end; ++column) {
            uint8_t byte;

            if (t32_read_memory(cli->machine, address, &byte, 1))
                t32_log_printf(" %02x", byte);
            else
                t32_log_printf(" ??");

            ++address;
        }

        t32_log_printf("\n");
    }

    return 0;
}

static int command_set(t32_cli_t *cli, int argc, char **argv)
{
    uint32_t value;

    if (argc == 3 && equals_ignore_case(argv[1], "pc")) {
        if (!parse_u32(argv[2], &value)) {
            t32_log_printf("[ERROR] invalid pc value\n");
            return -1;
        }

        t32_set_pc(cli->machine, value);
        t32_log_printf("pc <= 0x%08x\n", value);
        return 0;
    }

    if (argc == 4 &&
        equals_ignore_case(argv[1], "run") &&
        equals_ignore_case(argv[2], "steps")) {
        uint64_t limit;

        if (!parse_u64(argv[3], &limit)) {
            t32_log_printf("[ERROR] invalid run step limit\n");
            return -1;
        }

        cli->run_limit = limit;
        t32_log_printf(
            "run steps <= %llu\n",
            (unsigned long long)limit
        );
        return 0;
    }

    if (argc == 3 &&
        (argv[1][0] == 'r' || argv[1][0] == 'R') &&
        isdigit((unsigned char)argv[1][1])) {
        char *end = NULL;
        long register_number = strtol(argv[1] + 1, &end, 10);

        if (*end != '\0' ||
            register_number < 0 ||
            register_number >= (long)T32_REGISTER_COUNT ||
            !parse_u32(argv[2], &value)) {
            t32_log_printf("[ERROR] invalid register assignment\n");
            return -1;
        }

        t32_set_register(cli->machine, (unsigned)register_number, value);
        t32_log_printf(
            "r%ld <= 0x%08x\n",
            register_number,
            value
        );
        return 0;
    }

    t32_log_printf(
        "usage: set pc <value> | set rN <value> | set run steps <count>\n"
    );
    return -1;
}

static int command_logfile(t32_cli_t *cli, int argc, char **argv)
{
    (void)cli;

    if (argc != 2) {
        t32_log_printf("usage: logfile <path> | logfile off\n");
        return -1;
    }

    if (equals_ignore_case(argv[1], "off")) {
        t32_log_printf("logging disabled\n");
        t32_log_close();
        return 0;
    }

    if (!t32_log_open(argv[1])) {
        t32_log_printf("[ERROR] cannot open logfile %s\n", argv[1]);
        return -1;
    }

    t32_log_printf("logging to %s\n", argv[1]);
    return 0;
}

static int command_do(t32_cli_t *cli, int argc, char **argv)
{
    FILE *script;
    t32_cli_t nested;
    int result;

    if (argc != 2) {
        t32_log_printf("usage: do <scriptfile>\n");
        return -1;
    }

    script = fopen(argv[1], "r");
    if (!script) {
        t32_log_printf("[ERROR] cannot open %s\n", argv[1]);
        return -1;
    }

    t32_cli_init(&nested, cli->machine, script, false);
    nested.run_limit = cli->run_limit;

    result = t32_cli_run(&nested);
    cli->run_limit = nested.run_limit;

    fclose(script);
    return result;
}

static int command_clrhalt(t32_cli_t *cli, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    t32_clear_halt(cli->machine);
    t32_log_printf("halt cleared\n");
    return 0;
}

static int command_reset(t32_cli_t *cli, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    t32_reset(cli->machine);
    t32_log_printf("machine reset\n");
    return 0;
}

static int command_quit(t32_cli_t *cli, int argc, char **argv)
{
    (void)cli;
    (void)argc;
    (void)argv;
    return 1;
}
