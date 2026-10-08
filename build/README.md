# Build requirements

The release build is intentionally isolated from the development tree. It
copies a pinned reference checkout into a temporary directory, overlays the
clean loader source, and compiles the RAM agent independently.

Required inputs:

- OpenBK7231T_App SV6x66 port based on commit
  `32269ac9e9e249c337cee136d8514b2ac9b71eb3`.
- OpenSV6x66 SDK commit `5bb261d980fd51e67b94b41c7f5c9868cc43301d`,
  available as `sdk` inside the pinned reference checkout.
- `nds32le-elf-mculib-v3` toolchain archive.

The local validation uses the `openbk_builder` container and invokes
`build/build.sh` with those three mounted paths. Release hashes are written to
`artifacts/SHA256SUMS`.
