/*
 * t32.c
 *
 * Minimal T32 execution core.
 *
 * Instruction format:
 *
 *   bits 31..24  opcode
 *   bits 23..20  destination register
 *   bits 19..0   reserved for this first implementation
 *
 * MOVI consumes the following 32-bit word as its immediate value.
 * All words are little-endian in memory.
 */

#include "t32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct t32_machine {
    uint32_t registers[T32_REGISTER_COUNT];
    uint32_t pc;
    t32_flags_t flags;

    uint8_t *memory;
    size_t memory_size;

    t32_state_t state;
    char halt_reason[128];
    uint64_t instruction_count;
};

static bool range_valid(
    const t32_machine_t *machine,
    uint32_t address,
    size_t length
)
{
    size_t start = (size_t)address;

    if (!machine || !machine->memory)
        return false;

    if (start > machine->memory_size)
        return false;

    return length <= machine->memory_size - start;
}

static bool fetch_u32(
    const t32_machine_t *machine,
    uint32_t address,
    uint32_t *value
)
{
    uint8_t bytes[4];

    if (!value || !t32_read_memory(machine, address, bytes, sizeof(bytes)))
        return false;

    *value =
        ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);

    return true;
}

static void set_fault(t32_machine_t *machine, const char *reason)
{
    machine->state = T32_STATE_ERROR;
    snprintf(
        machine->halt_reason,
        sizeof(machine->halt_reason),
        "%s",
        reason ? reason : "execution fault"
    );
}

t32_machine_t *t32_create(size_t memory_size)
{
    t32_machine_t *machine;

    if (memory_size == 0)
        memory_size = T32_DEFAULT_MEMORY_SIZE;

    machine = (t32_machine_t *)calloc(1, sizeof(*machine));
    if (!machine)
        return NULL;

    machine->memory = (uint8_t *)calloc(1, memory_size);
    if (!machine->memory) {
        free(machine);
        return NULL;
    }

    machine->memory_size = memory_size;
    t32_reset(machine);
    return machine;
}

void t32_destroy(t32_machine_t *machine)
{
    if (!machine)
        return;

    free(machine->memory);
    machine->memory = NULL;
    free(machine);
}

void t32_reset(t32_machine_t *machine)
{
    if (!machine)
        return;

    memset(machine->registers, 0, sizeof(machine->registers));
    memset(&machine->flags, 0, sizeof(machine->flags));

    machine->pc = 0;
    machine->state = T32_STATE_STOPPED;
    machine->halt_reason[0] = '\0';
    machine->instruction_count = 0;
}

bool t32_load_file(
    t32_machine_t *machine,
    const char *path,
    uint32_t address
)
{
    FILE *file;
    long file_size;
    uint8_t *buffer;
    size_t read_size;
    bool ok;

    if (!machine || !path)
        return false;

    file = fopen(path, "rb");
    if (!file)
        return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    buffer = (uint8_t *)malloc((size_t)file_size);
    if (!buffer && file_size != 0) {
        fclose(file);
        return false;
    }

    read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(buffer);
        return false;
    }

    ok = t32_write_memory(machine, address, buffer, read_size);
    free(buffer);
    return ok;
}

bool t32_read_memory(
    const t32_machine_t *machine,
    uint32_t address,
    void *buffer,
    size_t length
)
{
    if (!buffer || !range_valid(machine, address, length))
        return false;

    memcpy(buffer, machine->memory + address, length);
    return true;
}

bool t32_write_memory(
    t32_machine_t *machine,
    uint32_t address,
    const void *buffer,
    size_t length
)
{
    if (!buffer || !range_valid(machine, address, length))
        return false;

    memcpy(machine->memory + address, buffer, length);
    return true;
}

bool t32_set_register(
    t32_machine_t *machine,
    unsigned register_number,
    uint32_t value
)
{
    if (!machine || register_number >= T32_REGISTER_COUNT)
        return false;

    machine->registers[register_number] = value;
    return true;
}

uint32_t t32_get_register(
    const t32_machine_t *machine,
    unsigned register_number
)
{
    if (!machine || register_number >= T32_REGISTER_COUNT)
        return 0;

    return machine->registers[register_number];
}

void t32_set_pc(t32_machine_t *machine, uint32_t pc)
{
    if (!machine)
        return;

    machine->pc = pc;
}

uint32_t t32_get_pc(const t32_machine_t *machine)
{
    return machine ? machine->pc : 0;
}

t32_flags_t t32_get_flags(const t32_machine_t *machine)
{
    t32_flags_t empty = {false, false, false, false};
    return machine ? machine->flags : empty;
}

t32_state_t t32_get_state(const t32_machine_t *machine)
{
    return machine ? machine->state : T32_STATE_ERROR;
}

const char *t32_state_name(t32_state_t state)
{
    switch (state) {
    case T32_STATE_STOPPED:
        return "stopped";
    case T32_STATE_RUNNING:
        return "running";
    case T32_STATE_HALTED:
        return "halted";
    case T32_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

const char *t32_get_halt_reason(const t32_machine_t *machine)
{
    if (!machine || !machine->halt_reason[0])
        return "";

    return machine->halt_reason;
}

uint64_t t32_get_instruction_count(const t32_machine_t *machine)
{
    return machine ? machine->instruction_count : 0;
}

void t32_clear_halt(t32_machine_t *machine)
{
    if (!machine)
        return;

    if (machine->state == T32_STATE_HALTED) {
        machine->state = T32_STATE_STOPPED;
        machine->halt_reason[0] = '\0';
    }
}

t32_step_result_t t32_step(t32_machine_t *machine)
{
    uint32_t instruction;
    uint8_t opcode;
    unsigned destination;
    unsigned source_a;
    unsigned source_b;

    if (!machine)
        return T32_STEP_FAULT;

    if (machine->state == T32_STATE_HALTED)
        return T32_STEP_HALTED;

    if (machine->state == T32_STATE_ERROR)
        return T32_STEP_FAULT;

    machine->state = T32_STATE_RUNNING;

    if (!fetch_u32(machine, machine->pc, &instruction)) {
        set_fault(machine, "instruction fetch outside memory");
        return T32_STEP_FAULT;
    }

    machine->pc += 4;
    opcode = (uint8_t)(instruction >> 24);
    destination = (instruction >> 20) & 0x0fu;
    source_a = (instruction >> 16) & 0x0fu;
    source_b = (instruction >> 12) & 0x0fu;
    machine->instruction_count++;

    /*
     * source_b is decoded now even though the first two implemented
     * instructions do not use it. Keeping decode centralized makes the
     * transition to ADD/SUB/LOAD/STORE straightforward.
     */
    (void)source_b;

    switch (opcode) {
		
	case T32_OPCODE_ADD: {
		uint32_t left = machine->registers[source_a];
		uint32_t right = machine->registers[source_b];
		uint32_t result = left + right;

		machine->registers[destination] = result;

		machine->flags.zero = result == 0;
		machine->flags.negative = (result & 0x80000000u) != 0;
		machine->flags.carry =
			((uint64_t)left + (uint64_t)right) > 0xffffffffu;

		machine->flags.overflow =
			((~(left ^ right) & (left ^ result)) & 0x80000000u) != 0;

		machine->state = T32_STATE_STOPPED;
		return T32_STEP_OK;
	}
    case T32_OPCODE_HALT:
        machine->state = T32_STATE_HALTED;
        snprintf(
            machine->halt_reason,
            sizeof(machine->halt_reason),
            "HALT instruction"
        );
        return T32_STEP_HALTED;

    case T32_OPCODE_MOV:
        /*
         * MOV copies one general-purpose register to another.
         *
         * Architectural decision for T32 v0.0.2:
         * MOV does not modify carry, zero, negative, or overflow.
         */
        machine->registers[destination] =
            machine->registers[source_a];
        machine->state = T32_STATE_STOPPED;
        return T32_STEP_OK;

    case T32_OPCODE_MOVI: {
        uint32_t immediate;

        if (!fetch_u32(machine, machine->pc, &immediate)) {
            set_fault(machine, "MOVI immediate outside memory");
            return T32_STEP_FAULT;
        }

        machine->pc += 4;
        machine->registers[destination] = immediate;
        machine->flags.zero = immediate == 0;
        machine->flags.negative = (immediate & 0x80000000u) != 0;
        machine->state = T32_STATE_STOPPED;
        return T32_STEP_OK;
    }

    default: {
        char reason[128];

        snprintf(
            reason,
            sizeof(reason),
            "unknown opcode 0x%02x at 0x%08x",
            opcode,
            machine->pc - 4
        );
        set_fault(machine, reason);
        return T32_STEP_FAULT;
    }
    }
}

t32_step_result_t t32_run(
    t32_machine_t *machine,
    uint64_t instruction_limit
)
{
    uint64_t executed = 0;

    if (!machine)
        return T32_STEP_FAULT;

    for (;;) {
        t32_step_result_t result;

        if (instruction_limit && executed >= instruction_limit) {
            machine->state = T32_STATE_STOPPED;
            snprintf(
                machine->halt_reason,
                sizeof(machine->halt_reason),
                "instruction limit reached"
            );
            return T32_STEP_OK;
        }

        result = t32_step(machine);
        executed++;

        if (result != T32_STEP_OK)
            return result;
    }
}
