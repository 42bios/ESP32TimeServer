"""Verify keyed NTPv4 replies and rejection cases without exposing the shared key."""
import argparse
import hashlib
import hmac
import json
import socket
import struct
import time
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('host');a=p.parse_args()
r=Path(__file__).resolve().parents[1]
identity=json.loads((r/'local-backups/ntp-auth/identity.json').read_text())
key=identity['secret'].encode('ascii');key_id=identity['key_id']
def stamp():
    now=time.time();sec=int(now)
    return struct.pack('!II',sec+2208988800,int((now-sec)*2**32))
def request(version=4):
    data=bytearray(48);data[0]=version<<3|3;data[40:48]=stamp();return data
def sign(data,k=key,ident=key_id):
    return bytes(data)+struct.pack('!I',ident)+hashlib.sha256(k+data).digest()[:20]
def valid(data):
    return len(data)==72 and struct.unpack('!I',data[48:52])[0]==key_id and hmac.compare_digest(data[52:],hashlib.sha256(key+data[:48]).digest()[:20])
def receive(s,data):
    s.send(data)
    try:return s.recv(4096)
    except socket.timeout:return None
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
    s.settimeout(2);s.connect((a.host,123))
    first=request();reply=receive(s,sign(first))
    assert reply and valid(reply),'authenticated reply MAC validation failed'
    assert reply[0]&7==4 and reply[1]==1 and reply[0]>>6==0,'not synchronized server reply'
    assert reply[24:32]==first[40:48],'originate mismatch'
    print('PASS authenticated NTPv4 reply, MAC and stratum 1')
    altered=bytearray(reply);altered[40]^=1
    assert not valid(altered);print('PASS modified reply fails MAC validation')
    second=request();second[24:32]=reply[32:40];second[32:40]=stamp()
    interleaved=receive(s,sign(second))
    assert interleaved and valid(interleaved),'interleaved reply MAC invalid'
    assert interleaved[24:32]==second[32:40] and interleaved[40:48]==reply[40:48],'interleaved timestamps mismatch'
    print('PASS authenticated interleaved exchange')
for label,packet in [
    ('wrong secret',sign(request(),k=bytes([key[0]^1])+key[1:])),
    ('unknown key ID',sign(request(),ident=65535 if key_id!=65535 else 65534)),
    ('truncated MAC',sign(request())[:-1]),
    ('authenticated NTPv3',sign(request(3))),
]:
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.settimeout(2);s.connect((a.host,123))
        assert receive(s,packet) is None,'unexpected response for '+label
    print('PASS rejected '+label,flush=True)
for version in [3,4]:
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.settimeout(2);s.connect((a.host,123));req=request(version);reply=receive(s,req)
        assert reply and len(reply)==48 and reply[1]==1 and reply[0]>>6==0 and reply[24:32]==req[40:48]
    print('PASS ordinary NTPv'+str(version)+' without a key')
