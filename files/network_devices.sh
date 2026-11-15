#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later

. /lib/functions.sh
. /lib/functions/network.sh

find_qmodem_device() {
    local section="$1" owner address
    [ -z "$modem_device" ] || return 0
    config_get owner "$section" modem_config
    [ -n "$owner" ] || return 0
    network_get_ipaddr address "$section" &&
        network_get_device modem_device "$section"
}

network_get_device lan_device lan
if [ "$(uci -q get fm350.main.enabled)" = 1 ]; then
    fm350_iface="$(uci -q get fm350.main.iface)"
    if [ -n "$fm350_iface" ] && network_get_ipaddr address "$fm350_iface"; then
        network_get_device modem_device "$fm350_iface"
    fi
fi

if [ -z "$modem_device" ] && [ -f /etc/config/qmodem ]; then
    config_load network
    config_foreach find_qmodem_device interface
fi

printf '%s\n%s\n' "$lan_device" "$modem_device"
