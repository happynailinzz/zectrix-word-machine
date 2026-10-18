#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
files=['firmware/build/zectrix_word_machine.bin','firmware/build/bootloader/bootloader.bin','firmware/build/partition_table/partition-table.bin','firmware/main/generated/glyphs.bin','firmware/main/generated/dashboard_fonts.bin']
manifest={'project':'zectrix-word-machine','version':(ROOT/'VERSION').read_text().strip(),'target':'esp32s3','files':[]}
for name in files:
 p=ROOT/name;manifest['files'].append({'path':name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(ROOT/'output').mkdir(exist_ok=True)
(ROOT/'output/build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('output/build-manifest.json')
