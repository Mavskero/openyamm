"""Read the MMVI/MMVIII single-directory LOD layouts without executing game code."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct
import zlib


class ImportFormatError(ValueError):
    pass


MAX_MEMBER_BYTES = 128 * 1024 * 1024


@dataclass(frozen=True)
class LodEntry:
    name: str
    offset: int
    size: int


def checked_name(raw: bytes) -> str:
    try:
        name = raw.split(b"\0", 1)[0].decode("ascii")
    except UnicodeDecodeError as error:
        raise ImportFormatError("Non-ASCII archive member name") from error
    if not name or name in (".", "..") or any(c in name for c in "/\\:"):
        raise ImportFormatError(f"Unsafe archive member name: {name!r}")
    if any(ord(c) < 32 for c in name):
        raise ImportFormatError(f"Control byte in archive member name: {name!r}")
    return name


def inflate_exact(data: bytes, expected: int) -> bytes:
    if expected <= 0 or expected > MAX_MEMBER_BYTES:
        raise ImportFormatError(f"Invalid expanded member size: {expected}")
    decoder = zlib.decompressobj()
    try:
        result = decoder.decompress(data, expected + 1)
    except zlib.error as error:
        raise ImportFormatError(f"Invalid compressed member: {error}") from error
    if len(result) != expected or not decoder.eof or decoder.unconsumed_tail or decoder.unused_data:
        raise ImportFormatError("Compressed member length or stream boundary mismatch")
    return result


class LodArchive:
    def __init__(self, path: Path):
        self.path = path
        self.size = path.stat().st_size
        with path.open("rb") as stream:
            header = stream.read(288)
            if len(header) != 288 or header[:4] != b"LOD\0":
                raise ImportFormatError("Not a supported LOD archive")
            self.version = header[4:84].split(b"\0", 1)[0].decode("ascii", errors="strict")
            if self.version not in ("MMVI", "GameMMVI", "MMVIII"):
                raise ImportFormatError(f"Unsupported LOD version: {self.version}")
            base, directory_size, _, count = struct.unpack_from("<IIII", header, 272)
            stride = 76 if self.version == "MMVIII" else 32
            name_size = 64 if self.version == "MMVIII" else 16
            if count > 100000 or base < 288 or base + directory_size > self.size:
                raise ImportFormatError("Invalid LOD directory bounds")
            index_size = count * stride
            if index_size > directory_size:
                raise ImportFormatError("LOD index extends beyond directory")
            stream.seek(base)
            index = stream.read(index_size)
            if len(index) != index_size:
                raise ImportFormatError("Truncated LOD directory")
        entries: list[LodEntry] = []
        names: set[str] = set()
        for i in range(count):
            start = i * stride
            name = checked_name(index[start:start + name_size])
            offset, size = struct.unpack_from("<II", index, start + name_size)
            if offset < index_size or offset + size > directory_size or size > MAX_MEMBER_BYTES:
                raise ImportFormatError(f"Invalid member bounds: {name}")
            if name.casefold() in names:
                raise ImportFormatError(f"Duplicate member name: {name}")
            names.add(name.casefold())
            entries.append(LodEntry(name, base + offset, size))
        previous_end = base + index_size
        for entry in sorted(entries, key=lambda item: item.offset):
            if entry.offset < previous_end:
                raise ImportFormatError(f"Overlapping member: {entry.name}")
            previous_end = entry.offset + entry.size
        self.entries = entries

    def read(self, entry: LodEntry) -> bytes:
        with self.path.open("rb") as stream:
            stream.seek(entry.offset)
            data = stream.read(entry.size)
        if len(data) != entry.size:
            raise ImportFormatError(f"Truncated member: {entry.name}")
        return data

    def read_text_payload(self, entry: LodEntry) -> bytes:
        """Remove a language resource wrapper; leave STR NUL separators intact."""
        data = self.read(entry)
        header_size, name_size = (96, 64) if self.version == "MMVIII" else (48, 16)
        if len(data) < header_size:
            raise ImportFormatError(f"Truncated language resource: {entry.name}")
        if checked_name(data[:name_size]).casefold() != entry.name.casefold():
            raise ImportFormatError(f"Unexpected language resource header: {entry.name}")
        stored_size = struct.unpack_from("<I", data, name_size + 4)[0]
        expanded_size = struct.unpack_from("<I", data, name_size + 24)[0]
        if header_size + stored_size != len(data):
            raise ImportFormatError(f"Unexpected language resource length: {entry.name}")
        payload = data[header_size:]
        return inflate_exact(payload, expanded_size) if expanded_size else payload

    def read_map_payload(self, entry: LodEntry) -> bytes:
        data = self.read(entry)
        if self.version != "GameMMVI" or len(data) < 16 or data[4:8] != b"mvii":
            raise ImportFormatError(f"Unsupported map wrapper: {entry.name}")
        compressed, expanded = struct.unpack_from("<II", data, 8)
        if len(data) != compressed + 16:
            raise ImportFormatError(f"Unexpected map resource length: {entry.name}")
        return inflate_exact(data[16:], expanded)
