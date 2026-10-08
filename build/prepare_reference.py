"""Remove development-only symbol gates from the isolated reference build."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")
old_required = """                    'flash_init', 'flash_page_program', 'flash_sector_erase',
                    'SV6X66_RamTestEnter', 'SV6X66_RamTestMain', 'SV6X66_RamUartWrite',
                    'SV6X66_RamUartRead', 'SV6X66_RamCrc32']"""
new_required = """                    'flash_init', 'flash_page_program', 'flash_sector_erase']"""
old_ram = """        if args.layout == 'bank-b-loader':
            ram_required += ['SV6X66_RamTestEnter', 'SV6X66_RamTestMain',
                             'SV6X66_RamUartWrite', 'SV6X66_RamUartRead', 'SV6X66_RamCrc32']
"""
if old_required not in text or old_ram not in text:
	raise SystemExit("reference build.py does not match the pinned source")
text = text.replace(old_required, new_required).replace(old_ram, "")
path.write_text(text, encoding="utf-8")

