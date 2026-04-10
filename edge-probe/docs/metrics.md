# Metric Inventory

This document describes the metric families currently implemented from the command snapshot in [cmd.txt](../cmd.txt).

The mapping below reflects what the code parses today. It is not a generic Linux metric catalog. If the device commands change, the parsers and this document should be updated together.

## Command Plan

`default_command_plan()` currently expects these logical groups:

- `system_identity`
- `network_links`
- `wifi_identity`
- `hotspot_services`
- `dhcp_leases`
- `lte_modem`
- `system_health`
- `firewall`
- `neighbors`

## Group Mapping

### system_identity

Commands:

- `uname -a`
- `cat /etc/os-release`
- `ps -p 1 -o comm=`
- `systemctl --version`

Metric families:

- `edge_system_identity_info`
- `edge_systemd_version_info`
- `edge_systemd_major_version`

### network_links

Commands:

- `ip -br link`
- `ip -br addr`
- `ip route`

Metric families:

- `edge_network_link_info`
- `edge_network_link_up`
- `edge_network_lower_up`
- `edge_network_no_carrier`
- `edge_network_address_count`
- `edge_network_address_info`
- `edge_network_default_route_info`
- `edge_network_route_info`
- `edge_network_route_metric`

### wifi_identity

Commands:

- `iw dev`
- `iw phy`

Metric families:

- `edge_wifi_ap_info`
- `edge_wifi_interface_ifindex`
- `edge_wifi_phy_limit`
- `edge_wifi_supported_interface_mode_info`
- `edge_wifi_supported_command_info`
- `edge_wifi_wowlan_support_info`
- `edge_wifi_frame_type_support_info`
- `edge_wifi_extended_feature_info`
- `edge_wifi_band_frequency_info`
- `edge_wifi_band_frequencies_total`
- `edge_wifi_band_radar_frequencies_total`
- `edge_wifi_max_associated_stations`

### hotspot_services

Commands:

- `systemctl list-units --type=service | grep -E 'hostapd|dnsmasq|NetworkManager|connman'`
- `systemctl status hostapd --no-pager`
- `systemctl status dnsmasq --no-pager`
- `systemctl status NetworkManager --no-pager`
- `command -v hostapd_cli`
- `command -v nmcli`

Metric families:

- `edge_service_unit_state_info`
- `edge_service_loaded_info`
- `edge_service_active`
- `edge_service_active_state_info`
- `edge_service_main_pid`
- `edge_service_tasks`
- `edge_service_task_limit`
- `edge_service_memory_bytes`
- `edge_service_cpu_seconds`
- `edge_wifi_station_association_events_total`
- `edge_wifi_radius_accounting_events_total`
- `edge_command_available`
- `edge_command_path_info`

### dhcp_leases

Commands:

- `ls -l /var/lib/misc/`
- `cat /var/lib/misc/dnsmasq.leases 2>/dev/null`

Metric families:

- `edge_hotspot_dhcp_lease_info`
- `edge_hotspot_dhcp_lease_expiry_epoch_seconds`
- `edge_hotspot_dhcp_leases_total`

### lte_modem

Commands:

- `command -v mmcli`
- `mmcli -L 2>/dev/null`
- `mmcli -m 0 2>/dev/null`
- `nmcli device status 2>/dev/null`
- `ls /dev/ttyUSB* 2>/dev/null`
- `ls /dev/cdc-wdm* 2>/dev/null`

Metric families:

- `edge_command_available`
- `edge_command_path_info`
- `edge_lte_modemmanager_modems_total`
- `edge_lte_modem_state_info`
- `edge_lte_signal_quality_percent`
- `edge_lte_access_technology_info`
- `edge_nm_device_state_info`
- `edge_lte_device_node_info`
- `edge_lte_usb_tty_devices_total`
- `edge_lte_cdc_wdm_devices_total`

Note:

- The provided `cmd.txt` snapshot does not contain a successful `mmcli -m 0` modem dump.
- The parser already supports modem-state fields when that output becomes available.

### system_health

Commands:

- `cat /proc/loadavg`
- `cat /proc/meminfo | head -20`
- `cat /proc/net/dev`
- `cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null`

Metric families:

- `edge_system_load_average`
- `edge_system_processes_running`
- `edge_system_processes_total`
- `edge_system_last_pid`
- `edge_system_meminfo_bytes`
- `edge_network_receive_bytes`
- `edge_network_receive_packets`
- `edge_network_receive_errors`
- `edge_network_receive_drops`
- `edge_network_receive_fifo`
- `edge_network_receive_frame`
- `edge_network_receive_compressed`
- `edge_network_receive_multicast`
- `edge_network_transmit_bytes`
- `edge_network_transmit_packets`
- `edge_network_transmit_errors`
- `edge_network_transmit_drops`
- `edge_network_transmit_fifo`
- `edge_network_transmit_collisions`
- `edge_network_transmit_carrier`
- `edge_network_transmit_compressed`
- `edge_system_temperature_celsius`

### firewall

Commands:

- `command -v iptables`
- `iptables -L -v -n 2>/dev/null`
- `command -v nft`
- `nft list ruleset 2>/dev/null`
- `command -v conntrack`
- `conntrack -S 2>/dev/null`

Metric families:

- `edge_command_available`
- `edge_command_path_info`
- `edge_firewall_iptables_chain_policy_packets`
- `edge_firewall_iptables_chain_policy_bytes`
- `edge_firewall_iptables_rule_packets`
- `edge_firewall_iptables_rule_bytes`
- `edge_firewall_nft_rule_packets`
- `edge_firewall_nft_rule_bytes`

Note:

- The provided snapshot does not contain `conntrack -S` output.
- The current code records command availability for `conntrack`, but does not yet parse conntrack counters.

### neighbors

Commands:

- `ip neigh`

Metric families:

- `edge_neighbor_entry_info`
- `edge_neighbor_entries_total`

## Labeling Approach

The current implementation favors descriptive labels over aggressive normalization. Examples:

- Interface-oriented metrics use labels such as `interface`, `device`, `cidr`, or `mac`.
- Service metrics use labels such as `service`, `active_state`, or `sub_state`.
- Firewall metrics use labels such as `chain`, `table`, `target`, and `expression`.

This keeps the V1 collector easy to debug from raw device output.

## Important Limitations

- Some parsed values are raw counters, not derived rates.
- Some metrics are presence or info-style metrics with value `1`.
- The parser surface is only as complete as the command output currently available in `cmd.txt`.
- Future live-device snapshots will likely justify adding more LTE-specific parsing once real `mmcli -m` output is captured.
