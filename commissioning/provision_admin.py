"""Create a unique bootstrap hash and local credential file; never print secrets."""
from pathlib import Path
import hashlib
import secrets
root = Path(__file__).resolve().parents[1]
seed = root / 'main/local_admin_seed.h'
if not seed.exists():
    password = secrets.token_urlsafe(18)
    salt = secrets.token_bytes(16)
    digest = hashlib.sha256(salt + password.encode()).digest()
    seed.write_text('#pragma once\nstatic const unsigned char initial_salt[16] = {' + ','.join(map(str, salt)) + '};\nstatic const unsigned char initial_password_hash[32] = {' + ','.join(map(str, digest)) + '};\n')
    folder = root / 'local-backups'
    folder.mkdir(exist_ok=True)
    credentials = folder / 'WEB-ACCESS.txt'
    credentials.write_text('ESP32 NTP web management\nURL: https://<IP>/settings\nPassword: ' + password + '\n\nChange after signing in. Verify and trust the local device certificate before signing in.\n')
    credentials.chmod(0o600)
    print('Unique bootstrap credentials generated locally; existing NVS passwords remain unchanged.')
