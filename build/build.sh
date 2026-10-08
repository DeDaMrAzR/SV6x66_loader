#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
	echo "usage: build.sh <reference-app> <toolchain-archive> <output>" >&2
	exit 2
fi

reference_app=$(realpath "$1")
toolchain_archive=$(realpath "$2")
output=$(realpath -m "$3")
source_root=$(cd "$(dirname "$0")/.." && pwd)
work=/tmp/sv6x66_loader_build
app="$work/app"
toolchain="$work/toolchain"

rm -rf "$work"
mkdir -p "$app" "$toolchain" "$output"
for directory in src include platforms libraries; do
	cp -a "$reference_app/$directory" "$app/$directory"
done
ln -s "$reference_app/sdk" "$app/sdk"
cp "$source_root/loader/sv6x66_loader.c" "$app/platforms/SV6X66/sv6x66_loader.c"
cp "$source_root/loader/sv6x66_loader.h" "$app/platforms/SV6X66/sv6x66_loader.h"
python3 "$source_root/build/prepare_reference.py" "$app/platforms/SV6X66/build.py"
tar -xJf "$toolchain_archive" -C "$toolchain"

toolchain_bin="$toolchain/nds32le-elf-mculib-v3/bin"
python3 "$app/platforms/SV6X66/build.py" \
	--toolchain "$toolchain_bin" \
	--build-dir "$work/loader-out" \
	--version SV6X66_LOADER \
	--layout bank-b-loader \
	--sdk-commit 5bb261d980fd51e67b94b41c7f5c9868cc43301d \
	--jobs 4

agent_out="$work/agent-out"
mkdir -p "$agent_out"
"$toolchain_bin/nds32le-elf-gcc" \
	-std=gnu99 -Os -mcpu=n10 -march=v3 -mcmodel=large \
	-fno-builtin -ffreestanding -ffunction-sections -fdata-sections \
	-Wall -Werror -c "$source_root/agent/sv6x66_flash_agent.c" \
	-o "$agent_out/sv6x66_flash_agent.o"
"$toolchain_bin/nds32le-elf-gcc" \
	-nostdlib -nostartfiles -nodefaultlibs -mcpu=n10 -march=v3 -mcmodel=large \
	-Wl,--gc-sections,-Map,"$agent_out/sv6x66_flash_agent.map" \
	-T "$source_root/agent/sv6x66_flash_agent.ld" \
	"$agent_out/sv6x66_flash_agent.o" -o "$agent_out/sv6x66_flash_agent.elf"
"$toolchain_bin/nds32le-elf-objcopy" -O binary \
	"$agent_out/sv6x66_flash_agent.elf" "$agent_out/sv6x66_flash_agent.bin"
"$toolchain_bin/nds32le-elf-objdump" -dr "$agent_out/sv6x66_flash_agent.elf" \
	> "$agent_out/sv6x66_flash_agent.dis"
"$toolchain_bin/nds32le-elf-nm" -n "$agent_out/sv6x66_flash_agent.elf" \
	> "$agent_out/sv6x66_flash_agent.nm"

if grep -Eiq '300[0-9a-f]{5}' "$agent_out/sv6x66_flash_agent.dis"; then
	echo "XIP reference found in RAM agent" >&2
	exit 90
fi

loader=$(find "$app/output/SV6X66_LOADER" -name '*bank-b-loader_uart.bin' -print -quit)
loader_elf=$(find "$app/output/SV6X66_LOADER" -name '*bank-b-loader.elf' -print -quit)
loader_map=$(find "$app/output/SV6X66_LOADER" -name '*bank-b-loader.map' -print -quit)
test -n "$loader"
test -n "$loader_elf"
test -n "$loader_map"
test -s "$loader"
test -s "$agent_out/sv6x66_flash_agent.bin"
cp "$loader" "$output/SV6x66_loader_uart.bin"
cp "$loader_map" "$output/SV6x66_loader.map"
"$toolchain_bin/nds32le-elf-nm" -n "$loader_elf" > "$output/SV6x66_loader.nm"
cp "$agent_out/sv6x66_flash_agent.bin" "$output/SV6x66_flash_agent.bin"
cp "$agent_out/sv6x66_flash_agent.map" "$output/SV6x66_flash_agent.map"
cp "$agent_out/sv6x66_flash_agent.nm" "$output/SV6x66_flash_agent.nm"
PYTHONPATH="$source_root/host" python3 -c \
	'from pathlib import Path; from stage import pack_stage; p=Path("'"$output"'"); (p/"SV6x66_flash_agent.stage").write_bytes(pack_stage((p/"SV6x66_flash_agent.bin").read_bytes()))'
gzip -n -c "$output/SV6x66_loader_uart.bin" > "$output/SV6x66_loader_uart.bin.gz"
gzip -n -c "$output/SV6x66_flash_agent.stage" > "$output/SV6x66_flash_agent.stage.gz"
(
	cd "$output"
	sha256sum SV6x66_loader_uart.bin SV6x66_flash_agent.bin \
		SV6x66_flash_agent.stage > SHA256SUMS
)
cat "$output/SHA256SUMS"
