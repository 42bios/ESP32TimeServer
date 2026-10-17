# Web management

Status: `https://<IP>/` - management: `/settings`.
The address follows DHCP or the saved static IPv4 configuration.

## Credentials

On the first build, a unique initial password is generated locally in
`local-backups/WEB-ACCESS.txt`; there is no shared default password. Open
`https://<IP>/settings`, sign in with that password, then use **Access** to set and
repeat your own password (12-128 characters) and save. You will need to sign in
again. The initial password change is recommended, not enforced by the interface.
This file and `main/local_admin_seed.h` are excluded from Git. The seed contains
only a salt and SHA-256 password hash. Password changes persist in NVS across
restarts and normal updates. Generating a new seed does not overwrite an existing
password on the device.

## Local HTTPS certificate

The first build generates a unique ECDSA P-256 key and a self-signed certificate
using OpenSSL. They are retained in `local-backups/tls/`; the private key and
`main/local_tls_identity.h` must stay private. No shared certificate/key is
included in the repository. Keep this identity for future OTA builds.

For an initial certificate containing a reserved IP, run this **before** the first
build (replace the example address with your device address):

```sh
python commissioning/provision_tls.py --ip 192.168.1.50
```

Optional `--dns` arguments specify names; defaults are `ntp-server` and
`ntp-server.local`. This does not enable a DNS or mDNS service. Repeated runs
preserve the existing identity and do not change its addresses.

Browsers initially warn because the local certificate is not publicly trusted.
Verify it against the locally generated public certificate before trusting it:

```sh
openssl x509 -in local-backups/tls/server-cert.pem -noout -fingerprint -sha256
```

The public `server-cert.pem` may be imported into a client trust store, or the
browser may offer a per-site exception. Never share `server-key.pem` or the
firmware image containing it. Certificates last 365 days; renew before expiry.
An address change can require a new certificate. To renew/change addresses,
back up the old identity, move its certificate and key out of `local-backups/tls/`,
then rerun provisioning with the intended addresses and rebuild/update. Clients
must verify and trust the new certificate again. Do not disable certificate
validation globally.

Commissioning scripts use HTTPS and explicitly trust this public certificate;
`--cacert` selects another certificate file. They do not disable TLS verification.

## Settings

- Device name: used as the DHCP hostname after restarting.
- DHCP or static IPv4 with gateway, subnet mask and optional DNS, after restarting.
- OLED brightness and dimming delay: applied immediately. A zero delay disables
  automatic dimming. Dimmed brightness must not exceed normal brightness.
  The display remains on with the complete status screen.
- Administrator password (12-128 characters), restart and firmware upload.

MQTT is not supported. When loading older NVS records, retired service fields are
cleared while network/display settings and administrator credentials are preserved.

## Network recovery

New network settings are saved first. After restarting, confirm them using
**Confirm network settings** within 120 seconds. Without confirmation, the device
restarts and restores the previous configuration. Another manual restart during
an unconfirmed trial also restores the previous settings.
After changing a static IP, reopen management at the new address.

## Firmware updates

Upload only `build/ESP32TimeServer.bin`, not a merged/factory image.
An unused OTA partition receives the image. The boot partition changes only after
image validation succeeds. Invalid images and other project names are rejected.
The bootloader supports rollback: a new application confirms itself after 30 seconds
if web management has started. Otherwise, the next restart restores the previous app.
This checks startup capability, not absolute timing accuracy.

During flash updates, NTP reports an invalid reference. Delays or outages can occur.
Do not disconnect power during an update.

## Access and storage

Web management uses TLS 1.2 on port 443. Port 80 only redirects GET requests to
HTTPS and has no login, settings or update handlers. Status remains readable
without signing in on the LAN;
management requires a random, 15-minute session token. Mutations also check the
request origin. Login attempts are rate limited. Password changes and signing out
invalidate the session. The browser does not persist tokens in cookies or local storage.

The administrator password is stored as a salted hash in NVS. Flash encryption and
Secure Boot are not enabled; physical access to flash is outside this protection.

## Implementation

HTTPS and OLED run at low priority on Core 0. HTTP status reads a cached snapshot and
does not query GNSS. NTP/PPS processing uses the original implementation. NVS writes
and OTA operations can still temporarily affect execution.

Development tools live in `commissioning/`. `test_admin_http.py <IP> --ota` actively
changes settings, restarts the device and optionally installs firmware. It requires
local credentials and restores the original display settings/password after a
successful run. Use it only in a maintenance window. Web management and OTA were
validated on the device on 17 October 2026. Network-trial rollback and long-term
operation still require validation.
