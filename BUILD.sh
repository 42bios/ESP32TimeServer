#!/bin/sh
set -eu
cd "$(dirname "$0")"
if ! command -v idf.py >/dev/null 2>&1; then
    echo 'Load the ESP-IDF v6.1 environment (export.sh) before running ./BUILD.sh.' >&2
    exit 1
fi
python commissioning/provision_admin.py
python commissioning/provision_ntp_auth.py
python commissioning/provision_tls.py
if [ ! -f sdkconfig ]; then
    idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;config/esp32p4_rev_v3_1.defaults' set-target esp32p4
fi
idf.py build
