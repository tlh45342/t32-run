#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parents[2]
T32_RUN = REPO_ROOT / "bin" / ("t32-run.exe" if sys.platform == "win32" else "t32-run")

SOURCE = TEST_DIR / "test.asm"
BINARY = TEST_DIR / "mov.t32"
SCRIPT = TEST_DIR / "test.script"
LOG = TEST_DIR / "mov.log"

EXPECTED = {
    "version": "t32-run version 0.0.2",
    "r0 source preserved": "r0 =0x0000002a",
    "r1 copied": "r1 =0x0000002a",
    "pc": "pc =0x00001010",
    "state": "state=halted",
    "instructions": "instructions=3",
    "carry unchanged": "carry=0",
    "zero unchanged": "zero=0",
    "negative unchanged": "negative=0",
    "overflow unchanged": "overflow=0",
}


def assemble() -> None:
    assembler = shutil.which("t32-asm")

    if assembler:
        subprocess.run(
            [assembler, "-f", "bin", str(SOURCE), "-o", str(BINARY)],
            cwd=TEST_DIR,
            check=True,
        )
        return

    # Known-good encoding:
    #   movi r0, 42
    #   mov  r1, r0
    #   halt
    BINARY.write_bytes(
        bytes(
            [
                0x00, 0x00, 0x00, 0x02,
                0x2A, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x10, 0x01,
                0x00, 0x00, 0x00, 0x00,
            ]
        )
    )


def main() -> int:
    if not T32_RUN.exists():
        print(f"FAIL: missing runner: {T32_RUN}")
        return 1

    if LOG.exists():
        LOG.unlink()

    assemble()

    with SCRIPT.open("r", encoding="utf-8") as script:
        completed = subprocess.run(
            [str(T32_RUN)],
            cwd=TEST_DIR,
            stdin=script,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )

    if completed.returncode != 0:
        print(completed.stdout)
        print(f"FAIL: t32-run exited with {completed.returncode}")
        return 1

    if not LOG.exists():
        print(completed.stdout)
        print(f"FAIL: missing execution log: {LOG}")
        return 1

    log_text = LOG.read_text(encoding="utf-8")
    failures = 0

    for label, expected in EXPECTED.items():
        if expected in log_text:
            print(f"PASS {label}")
        else:
            print(f"FAIL {label}: missing {expected!r}")
            failures += 1

    if failures:
        print("\n--- execution log ---")
        print(log_text)
        return 1

    print(f"\nPASS 002-mov ({len(EXPECTED)} checks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
