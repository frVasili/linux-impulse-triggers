#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf 'Usage: sudo %s VENDOR_ID PRODUCT_ID\n' "$0" >&2
    printf 'Example: sudo %s 2dc8 2015\n' "$0" >&2
    exit 2
}

(( $# == 2 )) || usage
vendor="${1,,}"
product="${2,,}"

[[ "$vendor" =~ ^[0-9a-f]{4}$ ]] || usage
[[ "$product" =~ ^[0-9a-f]{4}$ ]] || usage

if (( EUID != 0 )); then
    printf 'Run this uninstaller with sudo.\n' >&2
    usage
fi

rule_target="/etc/udev/rules.d/70-xbox-gip-${vendor}-${product}.rules"
rm -f -- "$rule_target"
udevadm control --reload-rules
udevadm trigger --action=add --subsystem-match=usb \
    --attr-match="idVendor=${vendor}" --attr-match="idProduct=${product}"

printf 'Removed %s if it existed.\n' "$rule_target"
printf 'Unplug and reconnect that controller once.\n'
