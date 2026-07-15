# T32 Run ISA Status

Version: **0.0.4**  
Date: **2026-07-14**

All 37 canonical instructions have a first-pass runtime implementation.
"Implemented" here means basic decode and representative execution work; it
does not mean exhaustive conformance testing is complete.

| Dec | Hex | Instruction | Basic runtime | Smoke test |
|---:|---:|---|:---:|:---:|
| 0 | 00 | HALT | yes | yes |
| 1 | 01 | NOP | yes | yes |
| 2 | 02 | TRAP | preliminary | yes |
| 3 | 03 | IRET | preliminary | yes |
| 4 | 04 | CPUID | preliminary | yes |
| 8 | 08 | MOV | yes | yes |
| 9 | 09 | MOVI | yes | yes |
| 16 | 10 | LDB | yes | yes |
| 17 | 11 | LDH | yes | yes |
| 18 | 12 | LDW | yes | yes |
| 19 | 13 | STB | yes | yes |
| 20 | 14 | STH | yes | yes |
| 21 | 15 | STW | yes | yes |
| 24 | 18 | ADD | yes | yes |
| 25 | 19 | ADDI | yes | yes |
| 26 | 1A | SUB | yes | yes |
| 27 | 1B | SUBI | yes | yes |
| 28 | 1C | MUL | yes | yes |
| 29 | 1D | MULU | yes | yes |
| 30 | 1E | DIV | yes | yes |
| 31 | 1F | DIVU | yes | yes |
| 32 | 20 | AND | yes | yes |
| 33 | 21 | OR | yes | yes |
| 34 | 22 | XOR | yes | yes |
| 35 | 23 | NOT | yes | yes |
| 36 | 24 | SHL | yes | yes |
| 37 | 25 | SHR | yes | yes |
| 38 | 26 | SAR | yes | yes |
| 40 | 28 | CMP | yes | yes |
| 41 | 29 | CMPI | yes | yes |
| 42 | 2A | JMP | yes | yes |
| 43 | 2B | JZ | yes | yes |
| 44 | 2C | JNZ | yes | yes |
| 48 | 30 | PUSH | yes | yes |
| 49 | 31 | POP | yes | yes |
| 50 | 32 | CALL | yes | yes |
| 51 | 33 | RET | yes | yes |

## Next validation work

- arithmetic flag boundary matrices
- source/destination register aliasing
- divide fault tests
- memory boundary and alignment policy tests
- branch taken/not-taken pairs
- stack underflow/overflow behavior
- formal trap and interrupt-frame design
- CPUID field specification
