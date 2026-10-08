# Validation

## Build evidence

- Date: 2026-10-08
- Target: CKW04 / SV6166F dual-slot staging profile
- OpenBK7231T_App reference: `32269ac9e9e249c337cee136d8514b2ac9b71eb3`
- OpenSV6x66 SDK: `5bb261d980fd51e67b94b41c7f5c9868cc43301d`
- Toolchain: `nds32le-elf-mculib-v3`, GCC 4.9.4
- Clean container build: passed
- Host tests: 4 passed
- RAM-agent XIP-reference scan: passed

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `SV6x66_loader_uart.bin` | 94208 | `5f8c9d2c350ac684ac33d06b9ddca408fae25ec106ab8e9349fe9da1d92ce18e` |
| `SV6x66_flash_agent.bin` | 2352 | `bf386affb0f3a0f779c628bf07f18d2b63e1bc77c75d9b17cd2816bf3f0e984a` |
| `SV6x66_flash_agent.stage` | 1788 | `94cb70e9f77df2048c302438e1874250469b37906a150614c760ea85130c9f65` |

## Device evidence

Hardware: CKW04 module with stock firmware `FWSWES-SV04MATE-3.7.1` and
JEDEC `C8 40 15` 2 MiB flash. Transport used one product/factory UART at
2 Mbaud after a manual reset and 9600-baud factory handshake.

The exact current loader and RAM agent completed two consecutive full reads.
Both produced 2,097,152-byte plain BIN files with SHA-256:

`7c2cf0c6d697fa1c5d93f37795ae52c1d755068702ce4eb1ddc4f3c034fdfaba`

This is device-tested evidence for staging, RAM execution, JEDEC/capacity
reporting, complete flash read, CRC verification, and repeatability.

An earlier writer-agent revision completed a full device clone, complete
readback comparison, and restoration. The exact writer artifact listed above
adds explicit `D`/`W` operation selection and has not yet repeated that
destructive device test. Treat the current write path as build-confirmed, not
device-tested.

## Scope limit

The stock OTA bootstrap overwrites inactive Bank B before reading flash. A UART
backup is therefore a complete post-staging image, not a pristine image of the
previous Bank-B contents. This staging route must not be used on layouts without
the CKW04 inactive bank at `0x0A6000`.
