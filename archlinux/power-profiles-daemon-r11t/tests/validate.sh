#!/usr/bin/env bash
set -euo pipefail

delivery_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
patch_file="$delivery_dir/power-profiles-daemon-r11t-osm.patch"
ppd_source_dir="${PPD_SOURCE_DIR:-}"

if [[ -z "$ppd_source_dir" ]]; then
  echo "usage: PPD_SOURCE_DIR=/path/to/power-profiles-daemon-0.30 $0" >&2
  exit 2
fi

ppd_source_dir="$(cd -- "$ppd_source_dir" && pwd)"

for path in \
  src/meson.build \
  src/power-profiles-daemon.c \
  src/ppd-driver-r11t-osm.c \
  src/ppd-driver-r11t-osm.h
do
  if ! grep -q "^diff --git a/$path b/$path\$" "$patch_file"; then
    echo "patch is missing $path" >&2
    exit 1
  fi
done

pkgbuild_file="$delivery_dir/PKGBUILD"
test -f "$pkgbuild_file"
bash -n "$pkgbuild_file"

expected_patch_sha="$(bash -c 'source "$1"; printf "%s" "${sha256sums[1]}"' _ "$pkgbuild_file")"
actual_patch_sha="$(sha256sum "$patch_file" | awk '{print $1}')"
if [[ "$expected_patch_sha" != "$actual_patch_sha" ]]; then
  echo "PKGBUILD patch checksum does not match $patch_file" >&2
  exit 1
fi
echo "PKGBUILD syntax and checksum: PASS"

git -C "$ppd_source_dir" apply --check --whitespace=error-all "$patch_file"
echo "git apply --check: PASS"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

extract_added_file() {
  local relative_path="$1"
  local output_path="$2"

  awk -v target="a/$relative_path" '
    /^diff --git / {
      inside = ($3 == target)
      body = 0
      next
    }
    inside && /^@@ / {
      body = 1
      next
    }
    inside && body && /^\+/ {
      sub(/^\+/, "")
      print
    }
  ' "$patch_file" > "$output_path"
}

extract_added_file src/ppd-driver-r11t-osm.c "$tmp_dir/ppd-driver-r11t-osm.c"
extract_added_file src/ppd-driver-r11t-osm.h "$tmp_dir/ppd-driver-r11t-osm.h"

cmp "$delivery_dir/src/ppd-driver-r11t-osm.c" "$tmp_dir/ppd-driver-r11t-osm.c"
cmp "$delivery_dir/src/ppd-driver-r11t-osm.h" "$tmp_dir/ppd-driver-r11t-osm.h"
echo "standalone sources match patch: PASS"

if command -v gcc >/dev/null && command -v pkg-config >/dev/null &&
   pkg-config --exists glib-2.0 gio-2.0 gudev-1.0; then
  # shellcheck disable=SC2046
  gcc -std=gnu11 -fsyntax-only \
    -Wall -Wextra -Werror -Wformat=2 -Wundef -Wstrict-prototypes \
    $(pkg-config --cflags glib-2.0 gio-2.0 gudev-1.0) \
    -I"$delivery_dir/src" \
    -I"$ppd_source_dir/src" \
    "$delivery_dir/src/ppd-driver-r11t-osm.c"
  echo "driver syntax check: PASS"
else
  echo "driver syntax check: SKIPPED (gcc/pkg-config/GLib development files unavailable)"
fi
