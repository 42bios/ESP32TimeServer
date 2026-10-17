"""Low-rate NTP/HTTPS comparison. Measures host/network observations, not UTC accuracy."""
import argparse
import datetime
import hashlib
import hmac
import json
import math
import signal
import socket
import ssl
import statistics
import struct
import threading
import time
import urllib.request
from pathlib import Path

EPOCH=2208988800
PHASES=[('plain_idle',False,False),('plain_https',False,True),('auth_idle',True,False),('auth_https',True,True),('recovery',False,False)]

def stamp(ns):
    sec,part=divmod(ns,10**9)
    return struct.pack('!II',sec+EPOCH,(part<<32)//10**9)
def decode(data,pos):
    sec,part=struct.unpack_from('!II',data,pos)
    return (sec-EPOCH)*10**9+(part*10**9>>32)
def percentile(values,q):
    if not values:return None
    v=sorted(values);i=(len(v)-1)*q;a=int(i);b=min(a+1,len(v)-1)
    return round(v[a]+(v[b]-v[a])*(i-a),3)
def stats(rows):
    valid=[r for r in rows if r.get('ok')]
    stable=[r for r in valid if not r['host_clock_step']]
    result={'samples':len(rows),'valid':len(valid),'errors':len(rows)-len(valid),'host_clock_steps':len(valid)-len(stable)}
    for field in ['rtt_us','delay_us','offset_us','server_interval_us','client_prepare_us','schedule_late_us']:
        data=[r[field] for r in stable]
        result[field]={'min':round(min(data),3) if data else None,'median':percentile(data,.5),'p95':percentile(data,.95),'p99':percentile(data,.99),'max':round(max(data),3) if data else None}
    # Successive offset differences include host clock and network changes.
    changes=[abs(b['offset_us']-a['offset_us']) for a,b in zip(stable,stable[1:]) if b['sequence']==a['sequence']+1]
    result['offset_change_abs_p95_us']=percentile(changes,.95)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('host');p.add_argument('--seconds',type=float,default=60,help='seconds per phase');p.add_argument('--cycles',type=int,default=1);p.add_argument('--rate',type=float,default=2);p.add_argument('--https-interval',type=float,default=1);p.add_argument('--out',type=Path,required=True);p.add_argument('--identity',type=Path);p.add_argument('--cacert',type=Path);p.add_argument('--link-note',default='Unspecified client connection')
    a=p.parse_args()
    if a.seconds<2 or a.cycles<1 or not 0<a.rate<=10 or a.https_interval<.5:p.error('invalid duration, cycles or rate')
    root=Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=False)
    ident=json.loads((a.identity or root/'local-backups/ntp-auth/identity.json').read_text());key=ident['secret'].encode();keyid=ident['key_id']
    context=ssl.create_default_context(cafile=str(a.cacert or root/'local-backups/tls/server-cert.pem'))
    stop=threading.Event();signal.signal(signal.SIGTERM,lambda *_:stop.set());signal.signal(signal.SIGINT,lambda *_:stop.set())
    started=datetime.datetime.now(datetime.timezone.utc).isoformat()
    summary={'started_utc':started,'host':a.host,'seconds_per_phase':a.seconds,'cycles':a.cycles,'rate':a.rate,'https_interval':a.https_interval,'client_connection':a.link_note,'limitations':['Software timestamps on this host; no independent UTC reference.','Offset includes host clock error, path asymmetry and client preparation.','The reported t3-t2 interval uses server timestamp fields, not independent CPU execution timing.','No control of other network clients, browser polling or host clock discipline.','HTTPS phases inject fresh verified TLS connections; not a simulated normal browser session.'],'phases':[],'complete':False}
    def save():
        temp=a.out/'summary.tmp';temp.write_text(json.dumps(summary,indent=2)+'\n');temp.replace(a.out/'summary.json')
    def status():
        try:
            with urllib.request.urlopen('https://'+a.host+'/api/status',context=context,timeout=5) as r:return json.load(r)
        except Exception as e:return {'error':type(e).__name__}
    # Fail immediately for bad certificate/address, rather than collecting meaningless load data.
    with urllib.request.urlopen('https://'+a.host+'/api/status',context=context,timeout=8) as r:
        initial=json.load(r)
    if not initial.get('synchronized'):raise RuntimeError('Device must be synchronized before benchmarking')
    summary['initial_status']=initial;save()
    with (a.out/'samples.jsonl').open('w',buffering=1) as raw,(a.out/'https.jsonl').open('w',buffering=1) as http:
        for cycle in range(a.cycles):
            # Reverse order in alternate cycles to reduce a fixed ordering bias.
            phases=PHASES if cycle%2==0 else [PHASES[0],PHASES[3],PHASES[2],PHASES[1],PHASES[4]]
            for label,auth,load in phases:
                if stop.is_set():break
                phase_stop=threading.Event();http_results=[]
                def https_load():
                    n=0
                    while not stop.is_set() and not phase_stop.is_set():
                        begin=time.monotonic();row={'cycle':cycle,'phase':label}
                        try:
                            path='/' if n%2 else '/api/status'
                            with urllib.request.urlopen('https://'+a.host+path,context=context,timeout=5) as response:
                                body=response.read();row.update(ok=response.status==200,bytes=len(body))
                                if path.endswith('status'):row['status']=json.loads(body)
                        except Exception as e:row.update(ok=False,error=type(e).__name__)
                        row['duration_ms']=round((time.monotonic()-begin)*1000,3);http_results.append(row);http.write(json.dumps(row)+'\n');n+=1
                        phase_stop.wait(max(0,a.https_interval-(time.monotonic()-begin)))
                start_status=status();rows=[]
                print('PHASE',cycle+1,label,flush=True)
                summary['running']={'cycle':cycle+1,'phase':label,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()};save()
                worker=threading.Thread(target=https_load,daemon=True) if load else None
                if worker:worker.start()
                begin=time.monotonic();deadline=begin+a.seconds;next_sample=begin;sequence=0
                with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
                    s.settimeout(min(1.5,max(.25,2/a.rate)));s.connect((a.host,123))
                    while not stop.is_set() and time.monotonic()<deadline:
                        if stop.wait(max(0,next_sample-time.monotonic())):break
                        if time.monotonic()>=deadline:break
                        due=next_sample;next_sample+=1/a.rate
                        if next_sample<time.monotonic():next_sample=time.monotonic()+1/a.rate
                        version=4 if auth or sequence%2 else 3
                        packet=bytearray(48);packet[0]=version<<3|3
                        m1=time.monotonic_ns();t1=time.time_ns();packet[40:48]=stamp(t1)
                        payload=bytes(packet)
                        if auth:payload+=struct.pack('!I',keyid)+hashlib.sha256(key+packet).digest()[:20]
                        prep=time.monotonic_ns()-m1
                        row={'cycle':cycle,'phase':label,'sequence':sequence,'version':version,'authenticated':auth,'elapsed_s':round(time.monotonic()-begin,3),'schedule_late_us':round(max(0,m1/1e9-due)*1e6,3),'client_prepare_us':round(prep/1000,3)}
                        try:
                            s.send(payload)
                            # A late response to an earlier request must not poison the next sample.
                            while True:
                                reply=s.recv(4096);t4=time.time_ns();m4=time.monotonic_ns()
                                if len(reply)<48 or reply[24:32]!=packet[40:48]:
                                    if (m4-m1)>2_000_000_000:raise ValueError('stale reply')
                                    continue
                                break
                            if reply[0]&7!=4 or (reply[0]>>3)&7!=version:raise ValueError('mode/version')
                            if auth:
                                if len(reply)!=72 or struct.unpack('!I',reply[48:52])[0]!=keyid or not hmac.compare_digest(reply[52:],hashlib.sha256(key+reply[:48]).digest()[:20]):raise ValueError('MAC validation')
                            elif len(reply)!=48:raise ValueError('unexpected trailer')
                            t2,t3=decode(reply,32),decode(reply,40)
                            if reply[1]!=1 or reply[0]>>6!=0:raise ValueError('not synchronized')
                            row.update(ok=True,stratum=reply[1],leap=reply[0]>>6,rtt_us=(m4-m1)/1000,delay_us=((t4-t1)-(t3-t2))/1000,offset_us=((t2-t1)+(t3-t4))/2000,server_interval_us=(t3-t2)/1000,host_clock_step=abs((t4-t1)-(m4-m1))>1_000_000)
                        except (OSError,ValueError) as e:row.update(ok=False,error=type(e).__name__+': '+str(e))
                        raw.write(json.dumps(row)+'\n');rows.append(row);sequence+=1
                phase_stop.set()
                if worker:worker.join(timeout=7)
                report={'cycle':cycle,'phase':label,'status_before':start_status,'status_after':status(),'ntp':stats(rows),'https_requests':len(http_results),'https_errors':sum(not x['ok'] for x in http_results)}
                summary['phases'].append(report);save();print(json.dumps(report),flush=True)
            if stop.is_set():break
    summary.pop('running',None);summary['complete']=not stop.is_set();summary['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save()
    print('COMPLETE' if summary['complete'] else 'STOPPED',a.out,flush=True)
if __name__=='__main__':main()
