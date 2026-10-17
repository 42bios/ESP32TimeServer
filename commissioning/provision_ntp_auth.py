"""Provision a unique NTP shared secret and client key files, without logging keys."""
from pathlib import Path
import json
import secrets
import string

root = Path(__file__).resolve().parents[1]
folder = root / 'local-backups/ntp-auth'
folder.mkdir(parents=True, exist_ok=True)
folder.chmod(0o700)
source = folder / 'identity.json'
if not source.exists():
    identity = {'key_id': 1, 'secret': ''.join(secrets.choice(string.ascii_letters + string.digits) for _ in range(32))}
    source.write_text(json.dumps(identity) + '\n')
    source.chmod(0o600)
identity = json.loads(source.read_text())
key_id, secret = identity['key_id'], identity['secret']
if not isinstance(key_id, int) or not 1 <= key_id <= 65535 or not isinstance(secret, str) or len(secret) != 32 or any(c not in string.ascii_letters + string.digits for c in secret):
    raise SystemExit('Invalid local NTP identity; restore its backup.')
hexkey = secret.encode('ascii').hex().upper()
files = {
    folder / 'chrony.keys': f'{key_id} SHA256 HEX:{hexkey}\n',
    folder / 'meinberg.keys': f'{key_id} SHA256 {hexkey}\n',
    root / 'main/local_ntp_auth.h': '#pragma once\nstatic constexpr symmetric_key_t symmetric_keys[] = {{' + str(key_id) + ', "' + secret + '"}};\n',
}
for path, text in files.items():
    if not path.exists() or path.read_text() != text:
        path.write_text(text)
        path.chmod(0o600)
print('Local NTP authentication key and client files ready; secret not printed.')
