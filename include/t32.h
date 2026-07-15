#ifndef T32_H
#define T32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "t32_opcodes.h"

#define T32_REGISTER_COUNT 16u
#define T32_DEFAULT_MEMORY_SIZE (1024u * 1024u)

typedef enum {
    T32_STATE_STOPPED = 0,
    T32_STATE_RUNNING,
    T32_STATE_HALTED,
    T32_STATE_ERROR
} t32_state_t;

typedef enum {
    T32_STEP_OK = 0,
    T32_STEP_HALTED,
    T32_STEP_FAULT
} t32_step_result_t;

typedef struct {
    bool carry;
    bool zero;
    bool negative;
    bool overflow;
} t32_flags_t;

typedef struct t32_machine t32_machine_t;

t32_machine_t *t32_create(size_t memory_size);
void t32_destroy(t32_machine_t *machine);
void t32_reset(t32_machine_t *machine);

bool t32_load_file(
    t32_machine_t *machine,
    const char *path,
    uint32_t address
);

bool t32_read_memory(
    const t32_machine_t *machine,
    uint32_t address,
    void *buffer,
    size_t length
);

bool t32_write_memory(
    t32_machine_t *machine,
    uint32_t address,
    const void *buffer,
    size_t length
);

bool t32_set_register(
    t32_machine_t *machine,
    unsigned register_number,
    uint32_t value
);

uint32_t t32_get_register(
    const t32_machine_t *machine,
    unsigned register_number
);

void t32_set_pc(t32_machine_t *machine, uint32_t pc);
uint32_t t32_get_pc(const t32_machine_t *machine);

t32_flags_t t32_get_flags(const t32_machine_t *machine);
t32_state_t t32_get_state(const t32_machine_t *machine);
const char *t32_state_name(t32_state_t state);
const char *t32_get_halt_reason(const t32_machine_t *machine);
uint64_t t32_get_instruction_count(const t32_machine_t *machine);

void t32_clear_halt(t32_machine_t *machine);

t32_step_result_t t32_step(t32_machine_t *machine);
t32_step_result_t t32_run(
    t32_machine_t *machine,
    uint64_t instruction_limit
);

#endif
