#!/bin/sh
# Run on Linux with an ASan/UBSan-capable C compiler; no NFC hardware required.
set -eu
cd "$(dirname "$0")/.."
nfc_test_dir=$(mktemp -d)
trap 'rm -rf "$nfc_test_dir"' EXIT HUP INT TERM
nfc_sanitize='-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer'
${CC:-cc} $nfc_sanitize -ffunction-sections -fdata-sections -Dmain=nfcDemoMain -Isrc/include -Idemoapp -c demoapp/main.c -o "$nfc_test_dir/demo.o"
${CC:-cc} $nfc_sanitize -Wl,--gc-sections tests/hce-apdu.c "$nfc_test_dir/demo.o" -o "$nfc_test_dir/apdu"
"$nfc_test_dir/apdu"
