/*
 * t32.c
 *
 * Reference execution core for the canonical T32 ISA.
 *
 * Instruction word:
 *   bits 31..24  opcode
 *   bits 23..20  destination register (rd)
 *   bits 19..16  source/address register A (ra)
 *   bits 15..12  source/value register B (rb)
 *   bits 11..0   reserved
 *
 * Immediate and target forms consume one additional little-endian word.
 */

#include "t32.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T32_STACK_POINTER 15u
#define T32_CPUID_VALUE 0x54333201u /* "T32", implementation revision 1 */

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

static bool range_valid(const t32_machine_t *machine, uint32_t address, size_t length)
{
    size_t start = (size_t)address;

    if (!machine || !machine->memory)
        return false;
    if (start > machine->memory_size)
        return false;
    return length <= machine->memory_size - start;
}

static bool fetch_u8(const t32_machine_t *machine, uint32_t address, uint8_t *value)
{
    return value && t32_read_memory(machine, address, value, 1);
}

static bool fetch_u16(const t32_machine_t *machine, uint32_t address, uint16_t *value)
{
    uint8_t bytes[2];

    if (!value || !t32_read_memory(machine, address, bytes, sizeof(bytes)))
        return false;
    *value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
    return true;
}

static bool fetch_u32(const t32_machine_t *machine, uint32_t address, uint32_t *value)
{
    uint8_t bytes[4];

    if (!value || !t32_read_memory(machine, address, bytes, sizeof(bytes)))
        return false;
    *value = ((uint32_t)bytes[0]) |
             ((uint32_t)bytes[1] << 8) |
             ((uint32_t)bytes[2] << 16) |
             ((uint32_t)bytes[3] << 24);
    return true;
}

static bool store_u8(t32_machine_t *machine, uint32_t address, uint8_t value)
{
    return t32_write_memory(machine, address, &value, 1);
}

static bool store_u16(t32_machine_t *machine, uint32_t address, uint16_t value)
{
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    return t32_write_memory(machine, address, bytes, sizeof(bytes));
}

static bool store_u32(t32_machine_t *machine, uint32_t address, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24)
    };
    return t32_write_memory(machine, address, bytes, sizeof(bytes));
}

static void set_fault(t32_machine_t *machine, const char *reason)
{
    machine->state = T32_STATE_ERROR;
    snprintf(machine->halt_reason, sizeof(machine->halt_reason), "%s",
             reason ? reason : "execution fault");
}

static t32_step_result_t memory_fault(t32_machine_t *machine, const char *operation,
                                      uint32_t address)
{
    char reason[128];
    snprintf(reason, sizeof(reason), "%s outside memory at 0x%08x",
             operation, address);
    set_fault(machine, reason);
    return T32_STEP_FAULT;
}

static bool fetch_extension(t32_machine_t *machine, uint32_t *value,
                            const char *instruction_name)
{
    char reason[128];

    if (fetch_u32(machine, machine->pc, value)) {
        machine->pc += 4;
        return true;
    }

    snprintf(reason, sizeof(reason), "%s extension outside memory",
             instruction_name);
    set_fault(machine, reason);
    return false;
}

static void set_zn(t32_machine_t *machine, uint32_t result)
{
    machine->flags.zero = result == 0;
    machine->flags.negative = (result & 0x80000000u) != 0;
}

static uint32_t alu_add(t32_machine_t *machine, uint32_t left, uint32_t right)
{
    uint32_t result = left + right;
    uint64_t wide = (uint64_t)left + (uint64_t)right;

    set_zn(machine, result);
    machine->flags.carry = wide > UINT32_MAX;
    machine->flags.overflow =
        ((~(left ^ right) & (left ^ result)) & 0x80000000u) != 0;
    return result;
}

static uint32_t alu_sub(t32_machine_t *machine, uint32_t left, uint32_t right)
{
    uint32_t result = left - right;

    set_zn(machine, result);
    /* T32 follows the common no-borrow convention. */
    machine->flags.carry = left >= right;
    machine->flags.overflow =
        (((left ^ right) & (left ^ result)) & 0x80000000u) != 0;
    return result;
}

static bool stack_push(t32_machine_t *machine, uint32_t value)
{
    uint32_t sp = machine->registers[T32_STACK_POINTER];

    if (sp < 4)
        return false;
    sp -= 4;
    if (!store_u32(machine, sp, value))
        return false;
    machine->registers[T32_STACK_POINTER] = sp;
    return true;
}

static bool stack_pop(t32_machine_t *machine, uint32_t *value)
{
    uint32_t sp = machine->registers[T32_STACK_POINTER];

    if (!fetch_u32(machine, sp, value))
        return false;
    if (sp > UINT32_MAX - 4)
        return false;
    machine->registers[T32_STACK_POINTER] = sp + 4;
    return true;
}

const char *t32_opcode_name(uint8_t opcode)
{
    switch (opcode) {
    case T32_OPCODE_HALT: return "HALT";
    case T32_OPCODE_NOP: return "NOP";
    case T32_OPCODE_TRAP: return "TRAP";
    case T32_OPCODE_IRET: return "IRET";
    case T32_OPCODE_CPUID: return "CPUID";
    case T32_OPCODE_MOV: return "MOV";
    case T32_OPCODE_MOVI: return "MOVI";
    case T32_OPCODE_LDB: return "LDB";
    case T32_OPCODE_LDH: return "LDH";
    case T32_OPCODE_LDW: return "LDW";
    case T32_OPCODE_STB: return "STB";
    case T32_OPCODE_STH: return "STH";
    case T32_OPCODE_STW: return "STW";
    case T32_OPCODE_ADD: return "ADD";
    case T32_OPCODE_ADDI: return "ADDI";
    case T32_OPCODE_SUB: return "SUB";
    case T32_OPCODE_SUBI: return "SUBI";
    case T32_OPCODE_MUL: return "MUL";
    case T32_OPCODE_MULU: return "MULU";
    case T32_OPCODE_DIV: return "DIV";
    case T32_OPCODE_DIVU: return "DIVU";
    case T32_OPCODE_AND: return "AND";
    case T32_OPCODE_OR: return "OR";
    case T32_OPCODE_XOR: return "XOR";
    case T32_OPCODE_NOT: return "NOT";
    case T32_OPCODE_SHL: return "SHL";
    case T32_OPCODE_SHR: return "SHR";
    case T32_OPCODE_SAR: return "SAR";
    case T32_OPCODE_CMP: return "CMP";
    case T32_OPCODE_CMPI: return "CMPI";
    case T32_OPCODE_JMP: return "JMP";
    case T32_OPCODE_JZ: return "JZ";
    case T32_OPCODE_JNZ: return "JNZ";
    case T32_OPCODE_PUSH: return "PUSH";
    case T32_OPCODE_POP: return "POP";
    case T32_OPCODE_CALL: return "CALL";
    case T32_OPCODE_RET: return "RET";
    default: return "UNKNOWN";
    }
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

bool t32_load_file(t32_machine_t *machine, const char *path, uint32_t address)
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
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
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

bool t32_read_memory(const t32_machine_t *machine, uint32_t address,
                     void *buffer, size_t length)
{
    if (!buffer || !range_valid(machine, address, length))
        return false;
    memcpy(buffer, machine->memory + address, length);
    return true;
}

bool t32_write_memory(t32_machine_t *machine, uint32_t address,
                      const void *buffer, size_t length)
{
    if (!buffer || !range_valid(machine, address, length))
        return false;
    memcpy(machine->memory + address, buffer, length);
    return true;
}

bool t32_set_register(t32_machine_t *machine, unsigned register_number,
                      uint32_t value)
{
    if (!machine || register_number >= T32_REGISTER_COUNT)
        return false;
    machine->registers[register_number] = value;
    return true;
}

uint32_t t32_get_register(const t32_machine_t *machine, unsigned register_number)
{
    if (!machine || register_number >= T32_REGISTER_COUNT)
        return 0;
    return machine->registers[register_number];
}

void t32_set_pc(t32_machine_t *machine, uint32_t pc)
{
    if (machine)
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
    case T32_STATE_STOPPED: return "stopped";
    case T32_STATE_RUNNING: return "running";
    case T32_STATE_HALTED: return "halted";
    case T32_STATE_ERROR: return "error";
    default: return "unknown";
    }
}

const char *t32_get_halt_reason(const t32_machine_t *machine)
{
    return (!machine || !machine->halt_reason[0]) ? "" : machine->halt_reason;
}

uint64_t t32_get_instruction_count(const t32_machine_t *machine)
{
    return machine ? machine->instruction_count : 0;
}

void t32_clear_halt(t32_machine_t *machine)
{
    if (machine && machine->state == T32_STATE_HALTED) {
        machine->state = T32_STATE_STOPPED;
        machine->halt_reason[0] = '\0';
    }
}

t32_step_result_t t32_step(t32_machine_t *machine)
{
    uint32_t instruction;
    uint32_t extension;
    uint32_t left;
    uint32_t right;
    uint32_t result;
    uint32_t address;
    uint8_t opcode;
    unsigned rd;
    unsigned ra;
    unsigned rb;

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
    rd = (instruction >> 20) & 0x0fu;
    ra = (instruction >> 16) & 0x0fu;
    rb = (instruction >> 12) & 0x0fu;
    machine->instruction_count++;

    switch (opcode) {
    case T32_OPCODE_HALT:
        machine->state = T32_STATE_HALTED;
        snprintf(machine->halt_reason, sizeof(machine->halt_reason),
                 "HALT instruction");
        return T32_STEP_HALTED;

    case T32_OPCODE_NOP:
        break;

    case T32_OPCODE_TRAP:
        if (!fetch_extension(machine, &extension, "TRAP"))
            return T32_STEP_FAULT;
        machine->state = T32_STATE_HALTED;
        snprintf(machine->halt_reason, sizeof(machine->halt_reason),
                 "TRAP 0x%08x", extension);
        return T32_STEP_HALTED;

    case T32_OPCODE_IRET:
        if (!stack_pop(machine, &machine->pc))
            return memory_fault(machine, "IRET stack read",
                                machine->registers[T32_STACK_POINTER]);
        break;

    case T32_OPCODE_CPUID:
        machine->registers[rd] = T32_CPUID_VALUE;
        break;

    case T32_OPCODE_MOV:
        machine->registers[rd] = machine->registers[ra];
        break;

    case T32_OPCODE_MOVI:
        if (!fetch_extension(machine, &extension, "MOVI"))
            return T32_STEP_FAULT;
        machine->registers[rd] = extension;
        set_zn(machine, extension);
        break;

    case T32_OPCODE_LDB: {
        uint8_t value;
        address = machine->registers[ra];
        if (!fetch_u8(machine, address, &value))
            return memory_fault(machine, "LDB read", address);
        machine->registers[rd] = value;
        break;
    }

    case T32_OPCODE_LDH: {
        uint16_t value;
        address = machine->registers[ra];
        if (!fetch_u16(machine, address, &value))
            return memory_fault(machine, "LDH read", address);
        machine->registers[rd] = value;
        break;
    }

    case T32_OPCODE_LDW:
        address = machine->registers[ra];
        if (!fetch_u32(machine, address, &machine->registers[rd]))
            return memory_fault(machine, "LDW read", address);
        break;

    case T32_OPCODE_STB:
        address = machine->registers[ra];
        if (!store_u8(machine, address, (uint8_t)machine->registers[rb]))
            return memory_fault(machine, "STB write", address);
        break;

    case T32_OPCODE_STH:
        address = machine->registers[ra];
        if (!store_u16(machine, address, (uint16_t)machine->registers[rb]))
            return memory_fault(machine, "STH write", address);
        break;

    case T32_OPCODE_STW:
        address = machine->registers[ra];
        if (!store_u32(machine, address, machine->registers[rb]))
            return memory_fault(machine, "STW write", address);
        break;

    case T32_OPCODE_ADD:
        machine->registers[rd] =
            alu_add(machine, machine->registers[ra], machine->registers[rb]);
        break;

    case T32_OPCODE_ADDI:
        if (!fetch_extension(machine, &extension, "ADDI"))
            return T32_STEP_FAULT;
        machine->registers[rd] =
            alu_add(machine, machine->registers[ra], extension);
        break;

    case T32_OPCODE_SUB:
        machine->registers[rd] =
            alu_sub(machine, machine->registers[ra], machine->registers[rb]);
        break;

    case T32_OPCODE_SUBI:
        if (!fetch_extension(machine, &extension, "SUBI"))
            return T32_STEP_FAULT;
        machine->registers[rd] =
            alu_sub(machine, machine->registers[ra], extension);
        break;

    case T32_OPCODE_MUL: {
        int64_t product = (int64_t)(int32_t)machine->registers[ra] *
                          (int64_t)(int32_t)machine->registers[rb];
        result = (uint32_t)product;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = product > INT32_MAX || product < INT32_MIN;
        break;
    }

    case T32_OPCODE_MULU: {
        uint64_t product = (uint64_t)machine->registers[ra] *
                           (uint64_t)machine->registers[rb];
        result = (uint32_t)product;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = (product >> 32) != 0;
        machine->flags.overflow = false;
        break;
    }

    case T32_OPCODE_DIV: {
        int32_t dividend = (int32_t)machine->registers[ra];
        int32_t divisor = (int32_t)machine->registers[rb];
        if (divisor == 0) {
            set_fault(machine, "DIV by zero");
            return T32_STEP_FAULT;
        }
        if (dividend == INT32_MIN && divisor == -1) {
            set_fault(machine, "DIV signed overflow");
            return T32_STEP_FAULT;
        }
        result = (uint32_t)(dividend / divisor);
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;
    }

    case T32_OPCODE_DIVU:
        right = machine->registers[rb];
        if (right == 0) {
            set_fault(machine, "DIVU by zero");
            return T32_STEP_FAULT;
        }
        result = machine->registers[ra] / right;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;

    case T32_OPCODE_AND:
        result = machine->registers[ra] & machine->registers[rb];
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;

    case T32_OPCODE_OR:
        result = machine->registers[ra] | machine->registers[rb];
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;

    case T32_OPCODE_XOR:
        result = machine->registers[ra] ^ machine->registers[rb];
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;

    case T32_OPCODE_NOT:
        result = ~machine->registers[ra];
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = false;
        machine->flags.overflow = false;
        break;

    case T32_OPCODE_SHL: {
        unsigned count = machine->registers[rb] & 31u;
        left = machine->registers[ra];
        result = count ? left << count : left;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = count ? ((left >> (32u - count)) & 1u) != 0 : false;
        machine->flags.overflow = false;
        break;
    }

    case T32_OPCODE_SHR: {
        unsigned count = machine->registers[rb] & 31u;
        left = machine->registers[ra];
        result = count ? left >> count : left;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = count ? ((left >> (count - 1u)) & 1u) != 0 : false;
        machine->flags.overflow = false;
        break;
    }

    case T32_OPCODE_SAR: {
        unsigned count = machine->registers[rb] & 31u;
        left = machine->registers[ra];
        result = count ? (uint32_t)((int32_t)left >> count) : left;
        machine->registers[rd] = result;
        set_zn(machine, result);
        machine->flags.carry = count ? ((left >> (count - 1u)) & 1u) != 0 : false;
        machine->flags.overflow = false;
        break;
    }

    case T32_OPCODE_CMP:
        (void)alu_sub(machine, machine->registers[ra], machine->registers[rb]);
        break;

    case T32_OPCODE_CMPI:
        if (!fetch_extension(machine, &extension, "CMPI"))
            return T32_STEP_FAULT;
        (void)alu_sub(machine, machine->registers[ra], extension);
        break;

    case T32_OPCODE_JMP:
        if (!fetch_extension(machine, &extension, "JMP"))
            return T32_STEP_FAULT;
        machine->pc = extension;
        break;

    case T32_OPCODE_JZ:
        if (!fetch_extension(machine, &extension, "JZ"))
            return T32_STEP_FAULT;
        if (machine->registers[ra] == 0)
            machine->pc = extension;
        break;

    case T32_OPCODE_JNZ:
        if (!fetch_extension(machine, &extension, "JNZ"))
            return T32_STEP_FAULT;
        if (machine->registers[ra] != 0)
            machine->pc = extension;
        break;

    case T32_OPCODE_PUSH:
        if (!stack_push(machine, machine->registers[ra]))
            return memory_fault(machine, "PUSH stack write",
                                machine->registers[T32_STACK_POINTER]);
        break;

    case T32_OPCODE_POP:
        if (!stack_pop(machine, &machine->registers[rd]))
            return memory_fault(machine, "POP stack read",
                                machine->registers[T32_STACK_POINTER]);
        break;

    case T32_OPCODE_CALL:
        if (!fetch_extension(machine, &extension, "CALL"))
            return T32_STEP_FAULT;
        if (!stack_push(machine, machine->pc))
            return memory_fault(machine, "CALL stack write",
                                machine->registers[T32_STACK_POINTER]);
        machine->pc = extension;
        break;

    case T32_OPCODE_RET:
        if (!stack_pop(machine, &machine->pc))
            return memory_fault(machine, "RET stack read",
                                machine->registers[T32_STACK_POINTER]);
        break;

    default: {
        char reason[128];
        snprintf(reason, sizeof(reason),
                 "unknown opcode 0x%02x at 0x%08x",
                 opcode, machine->pc - 4);
        set_fault(machine, reason);
        return T32_STEP_FAULT;
    }
    }

    machine->state = T32_STATE_STOPPED;
    return T32_STEP_OK;
}

t32_step_result_t t32_run(t32_machine_t *machine, uint64_t instruction_limit)
{
    uint64_t executed = 0;

    if (!machine)
        return T32_STEP_FAULT;

    for (;;) {
        t32_step_result_t step_result;

        if (instruction_limit && executed >= instruction_limit) {
            machine->state = T32_STATE_STOPPED;
            snprintf(machine->halt_reason, sizeof(machine->halt_reason),
                     "instruction limit reached");
            return T32_STEP_OK;
        }

        step_result = t32_step(machine);
        executed++;
        if (step_result != T32_STEP_OK)
            return step_result;
    }
}
