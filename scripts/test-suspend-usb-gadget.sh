#!/bin/bash
set -euo pipefail

ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HOOK=$ROOT/archlinux/rootfs-overlay/usr/lib/r11t/suspend-usb-gadget
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT

setup() {
	rm -rf "$TEST_ROOT/state"
	mkdir -p "$TEST_ROOT/state/config/usb_gadget/r11t" \
		"$TEST_ROOT/state/class/a800000.usb" "$TEST_ROOT/state/run"
	printf '%s' a800000.usb > "$TEST_ROOT/state/config/usb_gadget/r11t/UDC"
}

run_hook() {
	R11T_CONFIGFS_ROOT="$TEST_ROOT/state/config" \
	R11T_UDC_CLASS="$TEST_ROOT/state/class" \
	R11T_RUN_DIR="$TEST_ROOT/state/run" "$HOOK" "$1"
}

[[ -x $HOOK ]]
sh -n "$HOOK"

setup
run_hook pre
[[ -z $(cat "$TEST_ROOT/state/config/usb_gadget/r11t/UDC") ]]
[[ $(cat "$TEST_ROOT/state/run/suspend-usb-gadget.udc") == a800000.usb ]]
run_hook post
[[ $(cat "$TEST_ROOT/state/config/usb_gadget/r11t/UDC") == a800000.usb ]]
[[ ! -e $TEST_ROOT/state/run/suspend-usb-gadget.udc ]]

setup
rm -rf "$TEST_ROOT/state/class/a800000.usb"
if run_hook pre; then
	echo 'pre succeeded without a UDC device' >&2
	exit 1
fi
[[ $(cat "$TEST_ROOT/state/config/usb_gadget/r11t/UDC") == a800000.usb ]]
[[ ! -e $TEST_ROOT/state/run/suspend-usb-gadget.udc ]]

setup
run_hook pre
rm -rf "$TEST_ROOT/state/class/a800000.usb"
if run_hook post; then
	echo 'post succeeded without a UDC device' >&2
	exit 1
fi
[[ ! -e $TEST_ROOT/state/run/suspend-usb-gadget.udc ]]

setup
run_hook pre
printf '%s' other-udc > "$TEST_ROOT/state/config/usb_gadget/r11t/UDC"
if run_hook post; then
	echo 'post overwrote a conflicting UDC binding' >&2
	exit 1
fi
[[ $(cat "$TEST_ROOT/state/config/usb_gadget/r11t/UDC") == other-udc ]]
[[ ! -e $TEST_ROOT/state/run/suspend-usb-gadget.udc ]]

echo 'suspend-usb-gadget tests: pass'
