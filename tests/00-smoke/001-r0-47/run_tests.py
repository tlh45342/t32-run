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
BINARY = TEST_DIR / "test.t32"
SCRIPT = TEST_DIR / "test.script"
LOG = TEST_DIR / "test.log"

EXPECTED = {
    "version": "t32-run version 0.0.2",
    "memory": "0x00001000: 00 00 00 02 2f 00 00 00 00 00 00 00",
    "r0": "r0 =0x0000002f",
    "pc": "pc =0x0000100c",
    "state": "state=halted",
    "instructions": "instructions=2",
    "reason": "reason=HALT instruction",
}


def assemble() -> None:
    assembler = shutil.which("t32-asm")

    if assembler:
        subprocess.run(
            [assembler, str(SOURCE), str(BINARY)],
            cwd=TEST_DIR,
            check=True,
        )
        return

    # Known-good encoding for:
    #   movi r0, 47
    #   halt
    BINARY.write_bytes(
        bytes(
            [
                0x00, 0x00, 0x00, 0x02,
                0x2F, 0x00, 0x00, 0x00,
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

    print(f"\nPASS 001-r0-47 ({len(EXPECTED)} checks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
