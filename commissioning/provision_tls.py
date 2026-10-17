"""Create a per-device local TLS identity. Private material is never published."""
import argparse
import ipaddress
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--ip', action='append', default=[], help='IP address to include in the certificate')
p.add_argument('--dns', action='append', default=[], help='DNS name to include (does not enable DNS/mDNS)')
a = p.parse_args()
folder = root / 'local-backups/tls'
cert, key = folder / 'server-cert.pem', folder / 'server-key.pem'
header = root / 'main/local_tls_identity.h'
if cert.exists() != key.exists():
    raise SystemExit('Incomplete TLS identity; restore the missing certificate/key from backup.')
if not cert.exists():
    dns = a.dns or ['ntp-server', 'ntp-server.local']
    if any(not re.fullmatch(r'[A-Za-z0-9](?:[A-Za-z0-9.-]{0,251}[A-Za-z0-9])?', n) for n in dns):
        raise SystemExit('Invalid DNS name')
    ips = [str(ipaddress.ip_address(ip)) for ip in a.ip]
    folder.mkdir(parents=True, exist_ok=True)
    folder.chmod(0o700)
    sans = ['DNS:' + n for n in dns] + ['IP:' + ip for ip in ips]
    config = ('[req]\nprompt=no\ndistinguished_name=dn\nx509_extensions=ext\n'
              '[dn]\nCN=' + dns[0] + '\n[ext]\nsubjectAltName=' + ','.join(sans) + '\n'
              'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n'
              'extendedKeyUsage=serverAuth\nsubjectKeyIdentifier=hash\n')
    with tempfile.TemporaryDirectory() as temp:
        tmp = Path(temp)
        (tmp / 'request.cnf').write_text(config)
        subprocess.run(['openssl', 'ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', str(tmp / 'key.pem')], check=True, capture_output=True)
        subprocess.run(['openssl', 'req', '-new', '-x509', '-sha256', '-days', '365', '-key', str(tmp / 'key.pem'), '-out', str(tmp / 'cert.pem'), '-config', str(tmp / 'request.cnf')], check=True, capture_output=True)
        key.write_bytes((tmp / 'key.pem').read_bytes())
        key.chmod(0o600)
        cert.write_bytes((tmp / 'cert.pem').read_bytes())
    print('Created local TLS certificate (valid for 365 days); keep the key private.')
# Repeated builds retain the identity, preserving browser trust across OTA updates.
content = ('#pragma once\n'
           'static const char local_tls_cert[] = R"CERT(' + cert.read_text() + ')CERT";\n'
           'static const char local_tls_key[] = R"KEY(' + key.read_text() + ')KEY";\n')
if not header.exists() or header.read_text() != content:
    header.write_text(content)
    header.chmod(0o600)
