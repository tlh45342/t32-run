# T32

T32 is a small, deterministic 32-bit virtual machine project and part of the
larger Foundry experiment.

This repository contains:

- `libt32` — the reusable execution core
- `t32-run` — a local interactive and scriptable runner
- executable smoke tests and execution-trace verification

Version 0.0.3 remains intentionally narrow. It supports the instructions
needed to prove immediate loading, register copying, and deterministic halt:

```asm
movi r0, 47
mov  r1, r0
halt
```

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

Ensure the executable directory is on your path:

```bash
export PATH="$HOME/.local/bin:$PATH"
echo "$PATH"
```

For a system-wide installation:

```bash
sudo make PREFIX=/usr/local install
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

## Direct execution

```bash
t32-run tests/00-smoke/001-r0-47/test.t32 0x1000
```

Expected:

```text
state=halted
pc=0x0000100c
r0=0x0000002f
reason=HALT instruction
```

## Interactive use

```text
t32-run
t32-run> load test.t32 0x1000
t32-run> e 0x1000-0x100b
t32-run> set pc 0x1000
t32-run> run
t32-run> regs
t32-run> status
```

## Script use

```bash
t32-run < test.script
```

or interactively:

```text
t32-run> do test.script
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

## First test

```text
tests/00-smoke/001-r0-47/
```

`run_tests.py` assembles the source with `t32-asm` when available. If the
assembler is not installed, it writes the known-good 12-byte binary so the
execution core can still be tested independently.

The test verifies:

- instruction bytes in memory
- final `r0`
- final `pc`
- halted state
- instruction count
- halt reason
- execution log creation

## Current instruction encoding

T32 instructions are fixed 32-bit little-endian words:

```text
bits 31..24  opcode
bits 23..20  destination register
bits 19..0   reserved
```

`MOVI` consumes a second 32-bit word:

```text
word 0: opcode=MOVI, destination register
word 1: immediate value
```

Current opcodes:

```text
0x00 HALT
0x01 MOV
0x02 MOVI
```

`MOV` copies one register to another and does not modify flags.

## Versioning

The shared public version is defined in:

```text
include/version.h
```

Use `T32_VERSION` rather than a generic `VERSION` macro so future T32 tools can
include the header without colliding with unrelated projects.

The next instructions should be added only with matching scripted tests.

-TLH
