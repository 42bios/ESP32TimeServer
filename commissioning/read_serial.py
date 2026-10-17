import argparse
import time
from pathlib import Path
import serial
p = argparse.ArgumentParser()
p.add_argument('--seconds', type=float, default=30)
p.add_argument('--port', required=True)
p.add_argument('--log', default='/tmp/ntp-final-serial.log')
a = p.parse_args()
with serial.Serial(a.port, 115200, timeout=1) as s, Path(a.log).open('a') as log:
    end = time.monotonic() + a.seconds
    while time.monotonic() < end:
        line = s.readline().decode(errors='replace')
        if line:
            log.write(line)
            log.flush()
            print(line.rstrip(), flush=True)
