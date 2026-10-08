"""Build the bounded XZ envelope accepted by the Bank-B loader."""

from __future__ import annotations

import hashlib
import lzma
import struct
import zlib

STAGE_MAGIC = b"SV6STG01"
STAGE_HEADER = struct.Struct("<8s10I32s")
STAGE_LOAD_MIN = 0x4000
STAGE_LOAD_END = 0xF400
STAGE_DICT_SIZE = 8192
STAGE_PACKED_MAX = 65536


def pack_stage(raw: bytes, load_address: int = 0x4000, entry_address: int = 0x4000) -> bytes:
	if load_address < STAGE_LOAD_MIN or load_address >= STAGE_LOAD_END:
		raise ValueError("load address is outside the ILM window")
	if not raw or len(raw) > STAGE_LOAD_END - load_address:
		raise ValueError("RAM agent does not fit in ILM")
	if entry_address < load_address or entry_address >= load_address + len(raw):
		raise ValueError("entry address is outside the RAM agent")
	packed = lzma.compress(
		raw,
		format=lzma.FORMAT_XZ,
		check=lzma.CHECK_CRC32,
		filters=[{"id": lzma.FILTER_LZMA2, "dict_size": STAGE_DICT_SIZE, "preset": 9}],
	)
	if len(packed) > STAGE_PACKED_MAX:
		raise ValueError("compressed RAM agent exceeds the staging buffer")
	return STAGE_HEADER.pack(
		STAGE_MAGIC,
		1,
		STAGE_HEADER.size,
		load_address,
		entry_address,
		len(raw),
		len(packed),
		STAGE_DICT_SIZE,
		1,
		zlib.crc32(raw) & 0xFFFFFFFF,
		zlib.crc32(packed) & 0xFFFFFFFF,
		hashlib.sha256(raw).digest(),
	) + packed
