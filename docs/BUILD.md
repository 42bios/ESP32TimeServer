# Build and install

Requirements: **ESP-IDF v6.1**, Python, Git and OpenSSL. Managed dependencies are pinned in
`dependencies.lock`, including Arduino-ESP32 3.3.12. SparkFun GNSS and U8g2 are pinned
Git submodules.

```sh
git clone --recurse-submodules https://github.com/42bios/ESP32TimeServer.git
cd ESP32TimeServer
. /path/to/esp-idf/export.sh
./BUILD.sh
idf.py -p /dev/your-serial-port flash monitor
```

On the first build, `BUILD.sh` selects the P4 revision 3.1+ profile. Other revisions
have not been validated for this build. The initial installation must include the
bootloader, partition table and application to enable OTA rollback.

The build creates unique bootstrap credentials in **`local-backups/WEB-ACCESS.txt`**
and a local seed containing the password salt/hash. Both are excluded from Git.
There is no shared default password. After flashing, open `https://<IP>/settings`,
sign in with the generated password, then set your own under **Access** and save.
Use 12-128 characters. This change is recommended, but is not currently forced
on first login. The password persists across restarts and firmware updates; a new
seed does not reset an existing device password.

The first build also generates a local TLS identity in `local-backups/tls/`.
Keep this folder for later builds so the device certificate remains unchanged.
To include a reserved IP in the first certificate, run
`python commissioning/provision_tls.py --ip <device-IP>` before the first build.

Create separate credentials and TLS keys for each device. Devices flashed from
the same binary share these identities; firmware binaries are not published.

For certificate trust and administration, see [Web management](WEB-MANAGEMENT.md).

NTP authentication uses a separate locally generated identity in
`local-backups/ntp-auth/`. Retain it for subsequent builds; see
[NTP authentication](NTP-AUTHENTICATION.md) for client key files.
