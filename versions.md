# T32 Run Version History

## 0.0.1

- Initial reusable execution core
- Initial scriptable runner
- `HALT` and `MOVI`

## 0.0.2

- Added `MOV`
- Added initial regression framework

## 0.0.3

- Added `ADD`
- Added shared `include/version.h`
- Improved CLI and execution logging

## 0.0.4 — 2026-07-14

- Adopted the canonical 37-instruction opcode map
- Added `include/t32_opcodes.h`
- Implemented first-pass behavior for every defined instruction
- Added byte, halfword, and word memory operations
- Added arithmetic, multiply/divide, logic, shifts, compare, and branches
- Added `r15` stack operations and stack-based `CALL` / `RET`
- Added preliminary `TRAP`, `IRET`, and `CPUID` behavior
- Added an all-opcode ISA smoke suite
- Updated installation to include the canonical opcode header
