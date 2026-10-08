# Staging protocol version 1

The stock factory UART starts at 9600 8N1. After `factory!`, the host sends
`AT+BAUD=2000000`, stages the persistent loader with the stock OTA protocol,
and activates it once. The loader clears the Bank-B selection flag before
accepting commands, so the next reset returns to stock Bank A.

At 2 Mbaud the loader reports `SV6LDR02 READY A_ARMED ...` and accepts:

- `S`: receive an authenticated XZ-compressed RAM agent and transfer control to it.

The version-2 RAM agent reports its JEDEC ID, decoded capacity, erase geometry,
and XIP state before accepting an operation:

- `D`: stream the complete 2 MiB flash followed by CRC32.
- `W`: accept a 24-byte write-session header and an exact 2 MiB image.

Writes are refused unless JEDEC capacity detection also reports 2 MiB. Each
requested 4 KiB sector carries CRC32 and is verified after programming. The
complete image is then CRC-checked and streamed back for byte-for-byte host
verification.
