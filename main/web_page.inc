R"PAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Time Server · Status</title>
<style>)PAGE"
#include "web_theme.inc"
R"PAGE(</style></head><body><main>
<header>)PAGE"
#include "web_brand.inc"
R"PAGE(<div class="header-side"><div class="edition">P4 OLED RACK EDITION</div><nav aria-label="Main navigation"><a href="/" aria-current="page">Status</a><a href="/settings">Settings</a></nav></div></header>
<section class="clock" aria-label="UTC time"><div class="clock-top"><div class="eyebrow">COORDINATED UNIVERSAL TIME · UTC</div><div id="badge" role="status" aria-live="polite">Connecting</div></div><div class="time" id="utc">--:--:--</div><small>Latest ESP sample · refreshed every 5 seconds</small></section>
<div id="message" role="status"></div>
<section class="grid" aria-label="Server status">
<div class="card"><div class="label">NTP stratum</div><div class="value" id="stratum">—</div><div class="hint" id="reference">Waiting for status</div></div>
<div class="card"><div class="label">GNSS time reference</div><div class="value" id="gnss">—</div><div class="hint" id="satellites">Satellites: —</div></div>
<div class="card"><div class="label">PPS signal</div><div class="value" id="pps">—</div><div class="hint">Receiver pulse per second</div></div>
<div class="card"><div class="label">IPv4 address</div><div class="value" id="ip">—</div><div class="hint" id="link">Address assigned by DHCP</div></div>
<div class="card"><div class="label">NTP requests</div><div class="value" id="requests">—</div><div class="hint">Since the last restart</div></div>
<div class="card"><div class="label">Uptime</div><div class="value" id="uptime">—</div><div class="hint" id="serving">—</div></div>
</section>
<footer><a href="/settings">Settings</a><span id="updated">No status samples received yet</span><a href="/api/status">Status as JSON</a></footer>
<p class="hint footer-note">Local status view. PPS activity and stratum describe operating states; they do not prove absolute time accuracy.</p>
</main><script>
const $=id=>document.getElementById(id);let lastSuccess=0,timer;
function unknown(message){$('badge').textContent='Status unknown';$('badge').className='warn';$('message').textContent=message;for(const id of ['stratum','gnss','pps','ip','requests','uptime','serving'])$(id).textContent='—';$('utc').textContent='--:--:--';$('reference').textContent='No current data';$('satellites').textContent='Satellites: —';$('link').textContent='Network status unknown'}
function uptime(seconds){const d=Math.floor(seconds/86400),h=Math.floor(seconds%86400/3600),m=Math.floor(seconds%3600/60);return (d?d+' d ':'')+h+' h '+m+' min'}
async function refresh(){clearTimeout(timer);if(document.hidden){timer=setTimeout(refresh,5000);return}const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);try{const response=await fetch('/api/status',{cache:'no-store',signal:controller.signal});if(!response.ok)throw Error('HTTP '+response.status);const s=await response.json();if(!s.available||s.stale){unknown('ESP status data is not available yet or is stale.');return}lastSuccess=Date.now();$('badge').textContent=s.synchronized?'Synchronized':'Not synchronized';$('badge').className=s.synchronized?'good':'warn';$('message').textContent=s.synchronized?'':'No valid GNSS/PPS time reference yet. Do not use as a reliable time source.';$('utc').textContent=s.utc;$('stratum').textContent=s.stratum;$('reference').textContent=s.synchronized?'Reference: GPS':'Unsynchronized';$('gnss').textContent=s.gnss_valid?'Valid':'Waiting';$('satellites').textContent='Satellites used: '+(s.satellites===null?'—':s.satellites);$('pps').textContent=s.pps?'Active':'Missing';$('ip').textContent=s.ip||'Not assigned';$('link').textContent=s.link?'Ethernet connected · '+(s.dhcp?'DHCP':'Static IP'):'No Ethernet link';$('requests').textContent=new Intl.NumberFormat('en-GB').format(s.requests);$('uptime').textContent=uptime(s.uptime_s);$('serving').textContent=s.serving?'NTP service enabled':'NTP awaiting startup';$('updated').textContent='Retrieved at '+new Date().toLocaleTimeString('en-GB')+' · sample '+(s.age_ms/1000).toFixed(1)+' s old'}catch(error){unknown('ESP unreachable. Check the network connection.')}finally{clearTimeout(timeout);timer=setTimeout(refresh,5000)}}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh()});setInterval(()=>{if(lastSuccess&&Date.now()-lastSuccess>12000)unknown('No fresh status data received for more than 12 seconds.')},1000);refresh();
</script></body></html>)PAGE"
