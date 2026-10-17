# NTP key authentication

NTPv4 shared-key authentication is enabled. Ordinary NTPv3/v4 clients can still
request time without a key; this is not an access-control requirement. A client
configured to require authentication must know the matching key.

The build generates a unique 32-character secret with key ID **1**, retained in
`local-backups/ntp-auth/`. Nothing in that folder or `main/local_ntp_auth.h` belongs
in Git, logs or shared firmware downloads. Keep the identity for future builds.
The NTP secret is separate from the web password and HTTPS key.

## Client setup

Private client key files are generated locally:

- `local-backups/ntp-auth/chrony.keys` for chrony.
- `local-backups/ntp-auth/meinberg.keys` for Meinberg NTP.

Copy the appropriate key file privately to the client and configure its key file
path and server association. For chrony, the association is:

```text
server <ESP-IP> iburst version 4 key 1
```

The key must also be present in chrony's configured `keyfile`. Check the client's
authentication state after restarting it. Built-in time clients on some devices
do not support this mechanism and can continue using ordinary NTP.

Authentication checks packet integrity and knowledge of the shared secret. It
does not encrypt time traffic, establish absolute time accuracy, or protect keys
from extraction from firmware/client files. It is separate from NTS.

## Implementation and checks

The inherited SHA-256 construction is retained: the first 20 bytes of
SHA-256(secret || 48-byte NTP packet), with a four-byte key ID. The PSA hash result
handling was corrected so successful hash operations are recognized. Key contents
are no longer printed to the startup log.

`commissioning/check_ntp_auth.py <ESP-IP>` checks authenticated replies and an
interleaved exchange, rejects wrong keys and malformed authenticated requests,
and confirms ordinary NTPv3/v4 still works. It reads the local identity without
printing it. Actual client setup must be verified separately on each client.
