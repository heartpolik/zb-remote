#!/bin/sh
# Deploy the Home Assistant side of zb-remote over SSH (HAOS "Advanced SSH & Web Terminal").
#   scripts/deploy-ha.sh package     -> ha/zb_remote.yaml  => /config/packages/zb_remote.yaml, config check
#   scripts/deploy-ha.sh converter   -> z2m/zb-remote-14.mjs => /config/zigbee2mqtt/external_converters/
#   scripts/deploy-ha.sh all
# Env: HA_SSH (ssh host alias, default "homeassistant").
# After "package": reload Input select + Automations (Developer tools → YAML) or restart HA.
# After "converter": restart Zigbee2MQTT (it does not hot-reload external converters).
# Deploy the converter together with the matching firmware: writing an attribute the firmware
# does not know fails the whole display write.
set -e
cd "$(dirname "$0")/.."
HA_SSH=${HA_SSH:-homeassistant}
what=${1:-package}

deploy_package() {
	ssh "$HA_SSH" 'cat > /config/packages/zb_remote.yaml' < ha/zb_remote.yaml
	echo "package -> /config/packages/zb_remote.yaml"
	ssh "$HA_SSH" 'ha core check'
}
deploy_converter() {
	node --check z2m/zb-remote-14.mjs
	ssh "$HA_SSH" 'cat > /config/zigbee2mqtt/external_converters/zb-remote-14.mjs' < z2m/zb-remote-14.mjs
	echo "converter -> /config/zigbee2mqtt/external_converters/zb-remote-14.mjs (restart Zigbee2MQTT)"
}

case "$what" in
	package) deploy_package ;;
	converter) deploy_converter ;;
	all) deploy_converter; deploy_package ;;
	*) echo "usage: $0 [package|converter|all]" >&2; exit 2 ;;
esac
