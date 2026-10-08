"""Wire constants and geometry for SV6x66 loader protocol version 2."""

from __future__ import annotations

import struct
import zlib

FLASH_BYTES = 0x200000
BLOCK_BYTES = 0x10000
SECTOR_BYTES = 0x1000
PAGE_BYTES = 0x100
LOADER_BAUD = 2_000_000
SESSION_MAGIC = b"SV6FWR02"
READ_COMMAND = b"D"
WRITE_COMMAND = b"W"
SESSION_HEADER = struct.Struct("<8sIIII")
SECTOR_REQUEST = struct.Struct("<cHIB")


def build_session_header(image: bytes) -> tuple[bytes, int]:
	if len(image) != FLASH_BYTES:
		raise ValueError(f"full image must be exactly {FLASH_BYTES} bytes")
	image_crc = zlib.crc32(image) & 0xFFFFFFFF
	prefix = struct.pack("<8sIII", SESSION_MAGIC, 2, len(image), image_crc)
	return prefix + struct.pack("<I", zlib.crc32(prefix) & 0xFFFFFFFF), image_crc


def expected_schedule() -> list[tuple[int, int, str]]:
	result: list[tuple[int, int, str]] = []
	sequence = 0
	for block in range(1, FLASH_BYTES // BLOCK_BYTES):
		for sector in range(BLOCK_BYTES // SECTOR_BYTES):
			result.append((sequence, block * BLOCK_BYTES + sector * SECTOR_BYTES, "B"))
			sequence += 1
	for sector in range(1, BLOCK_BYTES // SECTOR_BYTES):
		result.append((sequence, sector * SECTOR_BYTES, "S"))
		sequence += 1
	result.append((sequence, 0, "S"))
	return result
