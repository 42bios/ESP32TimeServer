"""Low-rate real NTPv3/v4 checks. Measures against this Mac, not an atomic reference."""
import argparse
import json
import socket
import struct
import time

EPOCH = 2208988800

def stamp(t):
    sec = int(t)
    return struct.pack('!II', sec + EPOCH, int((t - sec) * 2**32))

def decode(data, pos):
    sec, fraction = struct.unpack_from('!II', data, pos)
    return sec - EPOCH + fraction / 2**32

p = argparse.ArgumentParser()
p.add_argument('host')
p.add_argument('--count', type=int, default=8)
a = p.parse_args()
results = []
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
    s.settimeout(2)
    s.connect((a.host, 123))
    for i in range(a.count):
        version = 3 if i % 2 == 0 else 4
        request = bytearray(48)
        request[0] = version << 3 | 3
        t1 = time.time()
        request[40:48] = stamp(t1)
        s.send(request)
        try:
            reply = s.recv(4096)
            t4 = time.time()
            if len(reply) < 48:
                raise ValueError('truncated reply')
            if reply[24:32] != request[40:48]:
                raise ValueError('originate timestamp mismatch')
            if reply[0] & 7 != 4 or (reply[0] >> 3) & 7 != version:
                raise ValueError('mode/version mismatch')
            t2, t3 = decode(reply, 32), decode(reply, 40)
            result = dict(version=version, stratum=reply[1], leap=reply[0] >> 6,
                          reference=reply[12:16].decode(errors='replace'),
                          offset_ms=round(((t2-t1)+(t3-t4))*500, 3),
                          delay_ms=round(((t4-t1)-(t3-t2))*1000, 3))
        except (TimeoutError, ValueError) as e:
            result = dict(version=version, error=str(e))
        results.append(result)
        print(json.dumps(result), flush=True)
        time.sleep(1)
if any('error' in r or r['stratum'] != 1 or r['leap'] == 3 for r in results):
    raise SystemExit('Not all requests were answered with synchronized stratum 1')
