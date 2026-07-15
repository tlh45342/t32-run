# t32-asm 0.0.4

T32 flat-binary assembler using the canonical 37-opcode ISA map.

## Build

```text
make
```

The executable is created in the repository root:

```text
t32-asm
t32-asm.exe
```

## `.org`

The assembler accepts:

```asm
.org 0x1000

start:
    movi r0, 42
    halt
```

`.org` changes the logical address used for labels and branch targets. It does
**not** add leading zero bytes to the flat binary.

The current intentionally simple rules are:

- `.org` may appear once.
- It must appear before code or data.
- The loader must load the resulting binary at the same address.

Example:

```text
t32-asm -f bin program.s -o program.bin
```

Runner script:

```text
load program.bin 0x1000
set pc 0x1000
```

## Supported directives

```text
.org
.equ
.byte
.word
.ascii
```

## Test

```text
make test
```

The example verifies that a label following `.org 0x1000` is encoded as an
absolute address in the `0x1000` range while the output remains a compact flat
binary.
