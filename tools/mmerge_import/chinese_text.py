"""Convert the supplied ZHCN package's legacy glyph runs into Unicode text."""
from __future__ import annotations

import re


# This package surrounds each CJK glyph with SO/BEL, and ends a run with SI.
# Match complete runs only: unrelated engine colour/control codes are preserved.
GLYPH = re.compile(r"\x0e([^\x00-\x1f])\x07")
RUN = re.compile(r"\x0e ?((?:\x0e[^\x00-\x1f]\x07 ?)+)\x0f")


def decode_legacy_text(data: bytes, encoding: str) -> tuple[str, int]:
    text = data.decode(encoding, errors="strict")
    converted = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal converted
        glyphs = GLYPH.findall(match.group(1))
        converted += len(glyphs)
        return "".join(glyphs)

    return RUN.sub(replace, text).replace("\r\n", "\n"), converted


def remaining_controls(text: str) -> dict[str, int]:
    return {f"0x{code:02x}": text.count(chr(code)) for code in range(1, 32)
            if code not in (9, 10, 13) and chr(code) in text}
