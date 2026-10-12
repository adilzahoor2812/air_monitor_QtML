#!/usr/bin/env bash
# Configure Jetson Ethernet for a direct cable to STM32F756ZG (static IP).
set -euo pipefail

IFACE="${IFACE:-enP8p1s0}"
JETSON_IP="${JETSON_IP:-192.168.10.10/24}"
STM32_IP="${STM32_IP:-192.168.10.2}"
CONN="${CONN:-Wired connection 1}"

echo "Interface: $IFACE"
echo "Jetson:    $JETSON_IP"
echo "STM32:     $STM32_IP"
echo
echo "You will be asked for your password (pkexec/sudo)."
echo

run() {
  if command -v pkexec >/dev/null 2>&1; then
    pkexec "$@"
  else
    sudo "$@"
  fi
}

run nmcli connection modify "$CONN" \
  connection.interface-name "$IFACE" \
  ipv4.method manual \
  ipv4.addresses "$JETSON_IP" \
  ipv4.gateway "" \
  ipv4.dns "" \
  ipv4.never-default yes \
  ipv6.method link-local

run nmcli connection up "$CONN" ifname "$IFACE"
sleep 1
ip -br addr show "$IFACE"
echo
echo "Ping STM32..."
ping -c 3 -W 1 "$STM32_IP" || true
echo
echo "Probe TCP 5000 / 502..."
nc -vz -w 2 "$STM32_IP" 5000 || true
nc -vz -w 2 "$STM32_IP" 502 || true
