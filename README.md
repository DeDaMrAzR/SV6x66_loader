# SV6x66 loader

`SV6x66_loader` is an experimental UART bootstrap and RAM-resident SPI-flash
agent for the SV6166F/SV6x66 family. This repository is being published as a
development reference so other developers can inspect and extend the work.

## Supported staging profile

The currently proven bootstrap applies only to the CKW04 dual-slot module with:

- stock firmware `FWSWES-SV04MATE-3.7.1`;
- 2 MiB SPI flash with JEDEC ID `C8 40 15`;
- product/factory UART on GPIO6/GPIO7;
- Bank A at `0x00B000-0x0A5FFF`;
- inactive Bank B at `0x0A6000-0x140FFF`;
- boot-selection flag sector at `0x004000`;
- filesystem beginning at `0x141000`.

**Do not stage this loader on an arbitrary SV6x66 device.** In particular, the
B02-F-A60/RE705 layout has no mapped Bank B: its application extends to
`0x0B9FFF` and its filesystem starts at `0x0BA000`. The CKW04 staging address
`0x0A6000` is inside that application. A different initial bootstrap is required
for that layout.

## Design

The CKW04 stock factory UART starts at 9600 baud. The host verifies the exact
stock firmware, switches the UART to 2 Mbaud, and temporarily installs the
bootstrap through the stock inactive-bank OTA path. The bootstrap arms return
to stock Bank A and authenticates an XZ-compressed RAM agent before executing
it.

The RAM agent disables XIP and provides:

- JEDEC identity and capacity detection;
- complete 2 MiB flash read with CRC32;
- 64 KiB block and 4 KiB sector erase;
- page programming with per-sector verification;
- complete-image CRC and byte-for-byte readback verification.

The raw flash operations are layout-independent after the RAM agent starts. The
current method of reaching that agent is not: it is specific to the CKW04
dual-slot staging profile above.

## Repository contents

- `loader/`: temporary Bank-B bootstrap and authenticated RAM-agent staging.
- `agent/`: RAM-only flash reader/writer and linker script.
- `host/`: protocol constants, stage packer, and validation tests.
- `build/`: reproducible container build entry point.

The build requires the pinned OpenBK7231T_App SV6x66 port, OpenSV6x66 SDK, and
Andes NDS32 toolchain described in `build/README.md`. Those components are not
redistributed here.

## Status

This is a development reference, not a general-purpose release. The current
loader and RAM-agent read path are clean-build and device tested on the named
CKW04 hardware. An earlier writer revision completed a full clone, readback,
and restoration cycle. Device validation of the exact current writer revision
remains pending.
