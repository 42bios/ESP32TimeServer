"""Real device commissioning; preserves saved values, never prints credentials/tokens."""
import argparse
import copy
import http.client
import urllib.parse
import json
import secrets
import ssl
import time
import urllib.error
import urllib.request
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('host');p.add_argument('--ota',action='store_true');p.add_argument('--cacert',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
password=(root/'local-backups/WEB-ACCESS.txt').read_text().split('Password: ',1)[1].splitlines()[0]
base=a.host.rstrip('/') if '://' in a.host else 'https://'+a.host
context=ssl.create_default_context(cafile=str(a.cacert or root/'local-backups/tls/server-cert.pem')) if base.startswith('https://') else None
token='';results=[]
def call(path,data=None,origin=None,auth=True,raw=False,timeout=10):
    headers={}
    if auth and token:headers['X-Admin-Token']=token
    if data is not None:
        headers['Origin']=origin or base
        headers['Content-Type']='application/octet-stream' if raw else 'application/json'
    payload=data if raw else json.dumps(data).encode() if data is not None else None
    req=urllib.request.Request(base+'/api/'+path,data=payload,headers=headers)
    try:
        with urllib.request.urlopen(req,timeout=timeout,context=context) as r:return r.status,json.load(r)
    except urllib.error.HTTPError as e:return e.code,json.load(e)
def check(ok,label):
    assert ok,label
    results.append(label);print('PASS '+label,flush=True)
def login(pw):
    global token
    time.sleep(1.1)
    code,j=call('login',{'password':pw},auth=False);assert code==200,'login failed';token=j['token']
def draft(c):
    out=copy.deepcopy(c);out.update(password='');return out
def wait_ready():
    end=time.monotonic()+100
    while time.monotonic()<end:
        try:
            code,s=call('status',auth=False,timeout=3)
            if code==200 and s.get('available'):return s
        except OSError:pass
        time.sleep(2)
    raise RuntimeError('device did not return')
if base.startswith('https://'):
    parsed=urllib.parse.urlsplit(base)
    for path in ['/', '/settings', '/api/status']:
        c=http.client.HTTPConnection(parsed.hostname,80,timeout=5)
        c.request('GET',path);r=c.getresponse();status=r.status;location=r.getheader('Location');r.read();c.close()
        check(status==302 and location==base+path,'HTTP redirects '+path+' to HTTPS')
    for path in ['/api/login','/api/update']:
        c=http.client.HTTPConnection(parsed.hostname,80,timeout=5)
        c.request('POST',path,body=b'{}');r=c.getresponse();status=r.status;r.read();c.close()
        check(status in (404,405),'plaintext endpoint unavailable: '+path)
check(call('config',auth=False)[0]==401,'configuration requires login')
check(call('reboot',{},auth=False)[0]==401,'restart requires login')
check(call('update',b'x'*300,auth=False,raw=True)[0]==401,'firmware upload requires login')
check(call('login',{'password':password},origin='http://untrusted.invalid',auth=False)[0]==403,'cross-origin login rejected')
login(password)
code,original=call('config');check(code==200,'authenticated settings readable')
check(not any('mqtt' in key for key in original) and 'password_hash' not in original and 'salt' not in original,'passwords/hashes absent from API')
c=draft(original)
check(call('config',c,origin='http://untrusted.invalid')[0]==403,'cross-origin settings rejected')
bad=draft(original);bad.update(dhcp=False,ip='999.1.2.3',mask='255.255.255.0',gateway='192.168.30.1')
check(call('config',bad)[0]==400,'invalid network rejected')
bad=draft(original);bad['dim_brightness']=256
check(call('config',bad)[0]==400,'invalid brightness rejected')
bad=draft(original);bad['password']='short'
check(call('config',bad)[0]==400,'short new password rejected')
check(call('config')[1]==original,'invalid saves leave settings unchanged')
c=draft(original);c['password']=secrets.token_urlsafe(18);temporary=c['password']
check(call('config',c)[0]==200,'password change saved')
check(call('config')[0]==401,'password change invalidates session')
login(temporary);c=draft(original);c['password']=password
check(call('config',c)[0]==200,'original password restored')
login(password)
c=draft(original);c['brightness']=max(1,original['brightness']-1);c['dim_brightness']=min(c['brightness'],c['dim_brightness'])
check(call('config',c)[0]==200,'display configuration saved')
check(call('reboot',{})[0]==200,'restart accepted')
time.sleep(6);wait_ready();login(password)
check(call('config')[1]['brightness']==c['brightness'],'settings persisted across reboot')
check(call('config',draft(original))[0]==200,'original display values restored')
check(call('update',b'x'*300,raw=True)[0]==400,'invalid firmware rejected')
check(call('status',auth=False)[0]==200,'status survives rejected firmware')
if a.ota:
    # Allow current OTA image health confirmation before starting another update.
    time.sleep(32)
    image=(root/'build/ESP32TimeServer.bin').read_bytes()
    code,j=call('update',image,raw=True,timeout=180)
    check(code==200 and j.get('updated'),'firmware upload validated')
    time.sleep(8);wait_ready();time.sleep(32);login(password)
    check(call('config')[0]==200,'web management returns after OTA reboot')
check(call('logout',{})[0]==200,'logout accepted')
check(call('config')[0]==401,'logout invalidates session')
(root/'commissioning/admin_http_result.json').write_text(json.dumps({'host':a.host,'passed':results},indent=2)+'\n')
