# T32 Run

`t32-run` is the reference execution environment for the T32 virtual CPU and
part of the larger Foundry project.

This repository contains:

- `libt32` — reusable T32 CPU and memory execution core
- `t32-run` — interactive and scriptable command-line runner
- canonical opcode definitions shared by the runtime
- ISA smoke tests covering all 37 currently defined instructions

## Version

```text
t32-run 0.0.4
```

Version 0.0.4 adopts the canonical T32 opcode map and provides first-pass
execution behavior for all 37 defined instructions.

## Build

Linux or macOS:

```bash
make
make test
make install
```

The default developer install prefix is:

```text
~/.local
```

Windows with MinGW/GNU Make:

```bat
make
make test
```

## Products

```text
lib/libt32.a
bin/t32-run
```

## Canonical ISA families

```text
00-04  system       HALT NOP TRAP IRET CPUID
08-09  movement     MOV MOVI
16-21  memory       LDB LDH LDW STB STH STW
24-31  arithmetic   ADD ADDI SUB SUBI MUL MULU DIV DIVU
32-38  logic/shift  AND OR XOR NOT SHL SHR SAR
40-44  compare/jump CMP CMPI JMP JZ JNZ
48-51  stack/call   PUSH POP CALL RET
```

The complete numeric map is defined in:

```text
include/t32_opcodes.h
```

## Important first-pass semantics

- `r15` is the downward-growing stack pointer.
- `LDB` and `LDH` zero-extend values into 32-bit registers.
- `CALL` pushes the return address; `RET` pops it.
- `JZ` and `JNZ` test the named register directly.
- Shift counts use the low five bits of the count register.
- Subtraction carry follows the no-borrow convention.
- `TRAP` currently halts cleanly and reports its vector. A vector table and
  operating-system trap dispatch remain future work.
- `IRET` currently restores a PC value from the stack. Full interrupt-frame
  restoration remains future work.
- `CPUID rd` writes `0x54333201` to `rd` in this implementation.

These choices are deliberately simple and may be refined with matching ISA
versioning and regression tests.

## Interactive use

```text
t32-run
t32-run> load test.bin 0x1000
t32-run> set pc 0x1000
t32-run> run
t32-run> regs
t32-run> status
```

## Script use

```bash
t32-run < test.script
```

Supported commands:

```text
help
version
load <binary> <address>
run
step [count]
regs
status
e <start>[-<end>]
e <start> <length>
set pc <value>
set rN <value>
set run steps <count>
logfile <path>
logfile off
do <scriptfile>
clrhalt
reset
quit
```

## Tests

```bash
make test
```

The ISA smoke suite validates decode and basic execution behavior across all 37
instructions. It is a bring-up suite, not yet exhaustive architectural
conformance. Boundary, aliasing, fault, overflow, and flag matrices should be
expanded instruction by instruction.
