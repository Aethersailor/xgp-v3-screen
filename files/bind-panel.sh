#!/bin/sh
set -eu

spi=/sys/bus/spi/devices/spi3.0
[ "$(cat /tmp/sysinfo/board_name)" = nlnet,xiguapi-v3 ] || {
    echo 'GC9307 SPI display requires NLnet XiGuaPi V3' >&2
    exit 1
}
[ "$(tr -d '\000' < "$spi/of_node/compatible")" = galaxycore,gc9307 ] || {
    echo 'spi3.0 is not a GC9307 panel' >&2
    exit 1
}

if [ -L "$spi/driver" ]; then
    [ "$(basename "$(readlink "$spi/driver")")" = spidev ] || {
        echo 'spi3.0 is already bound to another driver' >&2
        exit 1
    }
else
    printf '%s\n' spidev > "$spi/driver_override"
    if ! printf '%s\n' spi3.0 > /sys/bus/spi/drivers/spidev/bind; then
        printf '\n' > "$spi/driver_override"
        exit 1
    fi
fi

# OpenWrt hotplug can create the character device after the bind returns.
for attempt in 1 2 3; do
    [ ! -c /dev/spidev3.0 ] || exit 0
    sleep 1
done
[ ! -c /dev/spidev3.0 ] || exit 0
echo 'spidev3.0 character device did not appear' >&2
exit 1
