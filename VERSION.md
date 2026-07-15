# t32-asm Version History

## 0.0.4

- Adopted the canonical 37-opcode T32 map.
- Added recognition and encoding for all current T32 mnemonics.
- Added `.org ADDRESS`.
- `.org` changes logical addresses without padding the flat output image.
- Restricted `.org` to one occurrence before emitted code or data.
- Retained `.equ`, `.byte`, `.word`, `.ascii`, labels, and flat `bin` output.
