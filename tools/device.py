#!/usr/bin/env python3
"""Explicit backup, first-install and same-partition update. Dry-run by default."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def validate_mac(value):
    if not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}',value):raise argparse.ArgumentTypeError('MAC format must be aa:bb:cc:dd:ee:ff')
    return value.lower()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['backup','first-install','update'])
    parser.add_argument('--port',required=True)
    parser.add_argument('--expected-mac',required=True,type=validate_mac)
    parser.add_argument('--python',default=sys.executable,help='Python interpreter containing esptool')
    parser.add_argument('--backup',type=Path,help='Verified full 16MiB backup, required for first-install')
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    if args.mode=='first-install':
        if not args.backup or not args.backup.is_file() or args.backup.stat().st_size!=0x1000000:
            raise RuntimeError('first-install requires your own full 16MiB --backup')
        sidecar=args.backup.with_suffix('.json')
        if not sidecar.is_file():raise RuntimeError('Backup sidecar missing; create backup with this tool')
        record=json.loads(sidecar.read_text())
        if record['mac']!=args.expected_mac or record['sha256']!=sha(args.backup):raise RuntimeError('Backup identity/checksum mismatch')
    build=ROOT/'firmware/build'
    if args.mode!='backup':
        manifest=json.loads((ROOT/'output/build-manifest.json').read_text())
        for item in manifest['files']:
            path=ROOT/item['path']
            if path.stat().st_size!=item['bytes'] or sha(path)!=item['sha256']:raise RuntimeError('Build checksum mismatch: '+item['path'])
    if not args.execute:
        print('Offline validation passed. No serial access or writes; add --execute to run.');return
    def run(command,reset=False):
        result=subprocess.run([args.python,'-m','esptool','--chip','esp32s3','--port',args.port,'--baud','460800',
           '--after','hard_reset' if reset else 'no_reset',*command],check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(result.stdout,flush=True);return result.stdout
    device=run(['flash_id'])
    if args.expected_mac not in device.lower() or not re.search(r'Detected flash size:\s*16MB',device):raise RuntimeError('Device identity/Flash mismatch; no erase or write')
    security=run(['get_security_info'])
    if not all(re.search(p,security) for p in [r'Secure Boot:\s*Disabled',r'Flash Encryption:\s*Disabled']):raise RuntimeError('Unsupported security configuration')
    directory=ROOT/'.local/backups';directory.mkdir(parents=True,exist_ok=True)
    timestamp=datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    def backup(start,size,label):
        path=directory/(label+'-'+timestamp+'.bin');run(['read_flash',hex(start),hex(size),str(path)])
        if path.stat().st_size!=size:raise RuntimeError('Incomplete backup')
        path.with_suffix('.json').write_text(json.dumps({'mac':args.expected_mac,'offset':start,'bytes':size,'sha256':sha(path)},indent=2)+'\n')
        print('Backup:',path);return path
    if args.mode=='backup':backup(0,0x1000000,'full-flash');return
    if args.mode=='update':
        table=build/'partition_table/partition-table.bin'
        with tempfile.TemporaryDirectory() as tmp:
            current=Path(tmp)/'table.bin';run(['read_flash','0x8000',str(table.stat().st_size),str(current)])
            if current.read_bytes()!=table.read_bytes():raise RuntimeError('Partition mismatch; do not update')
        backup(0x230000,0x200000,'progress')
        run(['write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB','0x30000',str(build/'zectrix_word_machine.bin')],True)
    else:
        # Explicit first install replaces existing application/data; full backup already verified.
        run(['erase_region','0x9000','0x21000']);run(['erase_region','0x230000','0x200000'])
        run(['write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB',
           '0x0',str(build/'bootloader/bootloader.bin'),'0x8000',str(build/'partition_table/partition-table.bin'),
           '0x30000',str(build/'zectrix_word_machine.bin')],True)
    print('Write and esptool checksum verification completed. Test real hardware before relying on it.')
if __name__=='__main__':
    try:main()
    except (OSError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        if isinstance(error,subprocess.CalledProcessError):print(error.stdout or '',file=sys.stderr)
        print('Stopped:',error,file=sys.stderr);raise SystemExit(1)
