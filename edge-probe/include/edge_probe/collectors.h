#pragma once

#include "edge_probe/telemetry_sender.h"

#include <string>
#include <vector>

namespace edge_probe
{

struct CommandSpec
{
    std::string id;
    std::vector<std::string> commands;
};

std::vector<CommandSpec> default_command_plan();

std::vector<MetricSample> parse_system_identity(const std::string &uname_output,
                                                const std::string &os_release_output,
                                                const std::string &pid1_output,
                                                const std::string &systemctl_version_output);

std::vector<MetricSample> parse_ip_br_link(const std::string &output);
std::vector<MetricSample> parse_ip_br_addr(const std::string &output);
std::vector<MetricSample> parse_ip_route(const std::string &output);

std::vector<MetricSample> parse_iw_dev(const std::string &output);
std::vector<MetricSample> parse_iw_phy(const std::string &output);

std::vector<MetricSample> parse_service_list_units(const std::string &output);
std::vector<MetricSample> parse_systemd_status(const std::string &service_name,
                                               const std::string &output);
std::vector<MetricSample> parse_command_path(const std::string &command_name,
                                             const std::string &output);
std::vector<MetricSample> parse_dnsmasq_leases(const std::string &output);

std::vector<MetricSample> parse_mmcli_snapshot(const std::string &command_path_output,
                                               const std::string &list_output,
                                               const std::string &modem_output);
std::vector<MetricSample> parse_nmcli_device_status(const std::string &output);
std::vector<MetricSample> parse_device_node_listing(const std::string &ttyusb_output,
                                                    const std::string &cdc_wdm_output);

std::vector<MetricSample> parse_loadavg(const std::string &output);
std::vector<MetricSample> parse_meminfo(const std::string &output);
std::vector<MetricSample> parse_proc_net_dev(const std::string &output);
std::vector<MetricSample> parse_thermal_zone_temp(const std::string &output,
                                                  const std::string &zone_name);

std::vector<MetricSample> parse_iptables(const std::string &output);
std::vector<MetricSample> parse_nft_ruleset(const std::string &output);
std::vector<MetricSample> parse_ip_neigh(const std::string &output);

}  // namespace edge_probe
