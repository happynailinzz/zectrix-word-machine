#!/usr/bin/env python3
"""Read runtime logs only; no DTR reset, flash erase, or application writes."""
import argparse
from datetime import datetime
from pathlib import Path
import serial
import time
parser=argparse.ArgumentParser()
parser.add_argument('--port',required=True)
parser.add_argument('--seconds',type=float,default=0,help='0持续记录；正数为采集时长')
args=parser.parse_args()
if args.seconds<0:parser.error('--seconds must be nonnegative')
root=Path(__file__).resolve().parents[1]
out=root/'.local/diagnostics';out.mkdir(parents=True,exist_ok=True)
path=out/('note4-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.log')
port=serial.Serial(port=None,baudrate=115200,timeout=0.5)
port.dtr=False;port.rts=False;port.port=args.port;port.open()
print('Logging:',path,'Press keys on NOTE4; Ctrl+C to stop.',flush=True)
print('Diagnostic build should print HOME-INPUT-DIAG-2 and a heartbeat every 5 seconds.',flush=True)
started=time.monotonic()
try:
    with path.open('a') as log:
        while args.seconds==0 or time.monotonic()-started<args.seconds:
            line=port.readline().decode('utf-8',errors='replace')
            if line:print(line,end='',flush=True);log.write(line);log.flush()
except KeyboardInterrupt:pass
finally:port.close()
