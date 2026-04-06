#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $0 <dts-without-extension|dts-file>" >&2
    exit 1
fi

src="$1"
case "$src" in
    *.dts) src="${src%.dts}" ;;
esac

if [ ! -f "${src}.dts" ]; then
    echo "error: ${src}.dts not found" >&2
    exit 1
fi

echo "Compiling ${src}.dts -> ${src}.dtbo"
dtc -@ -I dts -O dtb -o "${src}.dtbo" "${src}.dts"
echo "Done: ${src}.dtbo"
# sudo cp "${src}.dtbo" /boot/firmware/overlays/
