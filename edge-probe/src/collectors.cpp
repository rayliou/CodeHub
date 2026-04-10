#include "edge_probe/collectors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace edge_probe
{

namespace
{

std::string trim(const std::string &value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return "";
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::vector<std::string> split_lines(const std::string &value)
{
    std::vector<std::string> lines;
    std::istringstream input(value);
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> split_whitespace(const std::string &value)
{
    std::vector<std::string> tokens;
    std::istringstream input(value);
    std::string token;
    while (input >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> split_columns(const std::string &value)
{
    std::vector<std::string> columns;
    static const std::regex separator("\\s{2,}");

    const std::string cleaned = trim(value);
    if (cleaned.empty())
    {
        return columns;
    }

    std::sregex_token_iterator it(cleaned.begin(), cleaned.end(), separator, -1);
    std::sregex_token_iterator end;
    for (; it != end; ++it)
    {
        const std::string token = trim(*it);
        if (!token.empty())
        {
            columns.push_back(token);
        }
    }
    return columns;
}

std::string strip_quotes(const std::string &value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

MetricSample make_metric(std::string name,
                         double value,
                         std::map<std::string, std::string> labels = {})
{
    MetricSample sample;
    sample.name = std::move(name);
    sample.value = value;
    sample.labels = std::move(labels);
    return sample;
}

MetricSample make_info(std::string name, std::map<std::string, std::string> labels = {})
{
    return make_metric(std::move(name), 1.0, std::move(labels));
}

std::set<std::string> parse_angle_flags(const std::string &value)
{
    std::set<std::string> flags;
    const auto left = value.find('<');
    const auto right = value.find('>');
    if (left == std::string::npos || right == std::string::npos || right <= left + 1)
    {
        return flags;
    }

    std::stringstream stream(value.substr(left + 1, right - left - 1));
    std::string flag;
    while (std::getline(stream, flag, ','))
    {
        flag = trim(flag);
        if (!flag.empty())
        {
            flags.insert(flag);
        }
    }
    return flags;
}

std::map<std::string, std::string> parse_key_value_lines(const std::string &value)
{
    std::map<std::string, std::string> parsed;
    for (const auto &line : split_lines(value))
    {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(line.substr(0, pos));
        const std::string val = strip_quotes(trim(line.substr(pos + 1)));
        if (!key.empty())
        {
            parsed[key] = val;
        }
    }
    return parsed;
}

double parse_human_quantity(const std::string &value)
{
    static const std::regex pattern("^([0-9]+(?:\\.[0-9]+)?)([KMGTP]?)(?:i?B|B)?$",
                                    std::regex::icase);
    std::smatch match;
    const std::string cleaned = trim(value);
    if (!std::regex_match(cleaned, match, pattern))
    {
        return std::stod(cleaned);
    }

    double numeric = std::stod(match[1].str());
    const std::string unit = match[2].str();
    if (unit.empty())
    {
        return numeric;
    }

    const char upper = static_cast<char>(std::toupper(unit.front()));
    switch (upper)
    {
        case 'K':
            numeric *= 1024.0;
            break;
        case 'M':
            numeric *= 1024.0 * 1024.0;
            break;
        case 'G':
            numeric *= 1024.0 * 1024.0 * 1024.0;
            break;
        case 'T':
            numeric *= 1024.0 * 1024.0 * 1024.0 * 1024.0;
            break;
        case 'P':
            numeric *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;
            break;
        default:
            break;
    }
    return numeric;
}

double parse_duration_seconds(const std::string &value)
{
    static const std::regex token_pattern("([0-9]+(?:\\.[0-9]+)?)(ms|s|min|h)");
    double seconds = 0.0;
    auto begin = std::sregex_iterator(value.begin(), value.end(), token_pattern);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it)
    {
        const double number = std::stod((*it)[1].str());
        const std::string unit = (*it)[2].str();
        if (unit == "ms")
        {
            seconds += number / 1000.0;
        }
        else if (unit == "s")
        {
            seconds += number;
        }
        else if (unit == "min")
        {
            seconds += number * 60.0;
        }
        else if (unit == "h")
        {
            seconds += number * 3600.0;
        }
    }
    return seconds;
}

std::string bool_label(bool value)
{
    return value ? "true" : "false";
}

std::string join_tail(const std::vector<std::string> &tokens, std::size_t index)
{
    std::ostringstream output;
    for (std::size_t i = index; i < tokens.size(); ++i)
    {
        if (i > index)
        {
            output << ' ';
        }
        output << tokens[i];
    }
    return output.str();
}

std::string normalize_spaces(const std::string &value)
{
    const auto tokens = split_whitespace(value);
    return join_tail(tokens, 0);
}

}  // namespace

std::vector<CommandSpec> default_command_plan()
{
    return {
        {"system_identity",
         {"uname -a",
          "cat /etc/os-release",
          "ps -p 1 -o comm=",
          "systemctl --version"}},
        {"network_links", {"ip -br link", "ip -br addr", "ip route"}},
        {"wifi_identity", {"iw dev", "iw phy"}},
        {"hotspot_services",
         {"systemctl list-units --type=service | grep -E 'hostapd|dnsmasq|NetworkManager|connman'",
          "systemctl status hostapd --no-pager",
          "systemctl status dnsmasq --no-pager",
          "systemctl status NetworkManager --no-pager",
          "command -v hostapd_cli",
          "command -v nmcli"}},
        {"dhcp_leases",
         {"ls -l /var/lib/misc/", "cat /var/lib/misc/dnsmasq.leases 2>/dev/null"}},
        {"lte_modem",
         {"command -v mmcli",
          "mmcli -L 2>/dev/null",
          "mmcli -m 0 2>/dev/null",
          "nmcli device status 2>/dev/null",
          "ls /dev/ttyUSB* 2>/dev/null",
          "ls /dev/cdc-wdm* 2>/dev/null"}},
        {"system_health",
         {"cat /proc/loadavg",
          "cat /proc/meminfo | head -20",
          "cat /proc/net/dev",
          "cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null"}},
        {"firewall",
         {"command -v iptables",
          "iptables -L -v -n 2>/dev/null",
          "command -v nft",
          "nft list ruleset 2>/dev/null",
          "command -v conntrack",
          "conntrack -S 2>/dev/null"}},
        {"neighbors", {"ip neigh"}}};
}

std::vector<MetricSample> parse_system_identity(const std::string &uname_output,
                                                const std::string &os_release_output,
                                                const std::string &pid1_output,
                                                const std::string &systemctl_version_output)
{
    std::vector<MetricSample> metrics;

    std::vector<std::string> uname_tokens = split_whitespace(trim(uname_output));
    std::map<std::string, std::string> labels;
    if (uname_tokens.size() >= 3)
    {
        labels["host"] = uname_tokens[1];
        labels["kernel"] = uname_tokens[2];
    }
    if (uname_tokens.size() >= 2)
    {
        labels["kernel_name"] = uname_tokens[0];
    }
    if (uname_tokens.size() >= 2)
    {
        labels["arch"] = uname_tokens[uname_tokens.size() - 2];
    }

    const auto os_release = parse_key_value_lines(os_release_output);
    const auto os_id = os_release.find("ID");
    if (os_id != os_release.end())
    {
        labels["os_id"] = os_id->second;
    }
    const auto os_pretty = os_release.find("PRETTY_NAME");
    if (os_pretty != os_release.end())
    {
        labels["os_pretty_name"] = os_pretty->second;
    }
    const auto os_version = os_release.find("VERSION_ID");
    if (os_version != os_release.end())
    {
        labels["os_version_id"] = os_version->second;
    }

    labels["pid1"] = trim(pid1_output);
    metrics.push_back(make_info("edge_system_identity_info", labels));

    const auto systemctl_lines = split_lines(systemctl_version_output);
    if (!systemctl_lines.empty())
    {
        std::smatch match;
        static const std::regex version_pattern("^systemd\\s+([0-9]+)\\s+\\(([^)]+)\\)$");
        const std::string version_line = trim(systemctl_lines.front());
        if (std::regex_match(version_line, match, version_pattern))
        {
            metrics.push_back(make_info("edge_systemd_version_info",
                                        {{"major", match[1].str()},
                                         {"version", match[2].str()}}));
            metrics.push_back(
                make_metric("edge_systemd_major_version", std::stod(match[1].str())));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_ip_br_link(const std::string &output)
{
    std::vector<MetricSample> metrics;

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::istringstream input(cleaned);
        std::string interface_name;
        std::string oper_state;
        input >> interface_name >> oper_state;
        std::string remainder;
        std::getline(input, remainder);
        remainder = trim(remainder);

        std::string mac;
        std::string flags_text;
        if (!remainder.empty())
        {
            const auto remainder_tokens = split_whitespace(remainder);
            if (!remainder_tokens.empty() &&
                remainder_tokens.front().find(':') != std::string::npos)
            {
                mac = remainder_tokens.front();
                flags_text = join_tail(remainder_tokens, 1);
            }
            else
            {
                flags_text = remainder;
            }
        }

        const auto flags = parse_angle_flags(flags_text);
        metrics.push_back(
            make_info("edge_network_link_info",
                      {{"interface", interface_name}, {"state", oper_state}, {"mac", mac}}));
        metrics.push_back(make_metric(
            "edge_network_link_up",
            flags.count("UP") > 0 ? 1.0 : 0.0,
            {{"interface", interface_name}}));
        metrics.push_back(make_metric(
            "edge_network_lower_up",
            flags.count("LOWER_UP") > 0 ? 1.0 : 0.0,
            {{"interface", interface_name}}));
        metrics.push_back(make_metric(
            "edge_network_no_carrier",
            flags.count("NO-CARRIER") > 0 ? 1.0 : 0.0,
            {{"interface", interface_name}}));
    }

    return metrics;
}

std::vector<MetricSample> parse_ip_br_addr(const std::string &output)
{
    std::vector<MetricSample> metrics;

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::istringstream input(cleaned);
        std::string interface_name;
        std::string oper_state;
        input >> interface_name >> oper_state;
        std::string remainder;
        std::getline(input, remainder);
        remainder = trim(remainder);

        const auto addresses = split_whitespace(remainder);
        metrics.push_back(make_metric("edge_network_address_count",
                                      static_cast<double>(addresses.size()),
                                      {{"interface", interface_name}}));
        for (const auto &address : addresses)
        {
            metrics.push_back(make_info("edge_network_address_info",
                                        {{"interface", interface_name},
                                         {"state", oper_state},
                                         {"cidr", address}}));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_ip_route(const std::string &output)
{
    std::vector<MetricSample> metrics;

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        auto tokens = split_whitespace(cleaned);
        if (tokens.empty())
        {
            continue;
        }

        std::map<std::string, std::string> labels;
        labels["prefix"] = tokens.front();
        for (std::size_t i = 1; i + 1 < tokens.size(); ++i)
        {
            const std::string &key = tokens[i];
            const std::string &value = tokens[i + 1];
            if (key == "dev")
            {
                labels["device"] = value;
            }
            else if (key == "via")
            {
                labels["gateway"] = value;
            }
            else if (key == "src")
            {
                labels["src"] = value;
            }
            else if (key == "scope")
            {
                labels["scope"] = value;
            }
            else if (key == "proto")
            {
                labels["proto"] = value;
            }
            else if (key == "metric")
            {
                labels["metric"] = value;
                metrics.push_back(make_metric("edge_network_route_metric",
                                              std::stod(value),
                                              {{"prefix", labels["prefix"]},
                                               {"device", labels["device"]}}));
            }
        }

        if (tokens.front() == "default")
        {
            labels.erase("prefix");
            metrics.push_back(make_info("edge_network_default_route_info", labels));
        }
        else
        {
            metrics.push_back(make_info("edge_network_route_info", labels));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_iw_dev(const std::string &output)
{
    std::vector<MetricSample> metrics;

    std::string interface_name;
    std::string mac;
    std::string ssid;
    std::string type;

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::smatch match;
        static const std::regex interface_pattern("^Interface\\s+(\\S+)$");
        static const std::regex ifindex_pattern("^ifindex\\s+([0-9]+)$");
        static const std::regex addr_pattern("^addr\\s+([0-9a-f:]+)$",
                                             std::regex::icase);
        static const std::regex ssid_pattern("^ssid\\s+(.+)$");
        static const std::regex type_pattern("^type\\s+(.+)$");

        if (std::regex_match(cleaned, match, interface_pattern))
        {
            interface_name = match[1].str();
        }
        else if (std::regex_match(cleaned, match, ifindex_pattern) &&
                 !interface_name.empty())
        {
            metrics.push_back(make_metric("edge_wifi_interface_ifindex",
                                          std::stod(match[1].str()),
                                          {{"interface", interface_name}}));
        }
        else if (std::regex_match(cleaned, match, addr_pattern))
        {
            mac = match[1].str();
        }
        else if (std::regex_match(cleaned, match, ssid_pattern))
        {
            ssid = match[1].str();
        }
        else if (std::regex_match(cleaned, match, type_pattern))
        {
            type = match[1].str();
            if (!interface_name.empty())
            {
                metrics.push_back(make_info("edge_wifi_ap_info",
                                            {{"interface", interface_name},
                                             {"ssid", ssid},
                                             {"type", type},
                                             {"mac", mac}}));
            }
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_iw_phy(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::string current_band;

    enum class Block
    {
        none,
        interface_modes,
        commands,
        wowlan,
        tx_frame_types,
        rx_frame_types,
        extended_features,
    };

    Block block = Block::none;
    std::unordered_map<std::string, int> band_enabled_count;
    std::unordered_map<std::string, int> band_disabled_count;
    std::unordered_map<std::string, int> band_radar_count;

    const auto emit_limit = [&metrics](const std::string &limit, double value) {
        metrics.push_back(make_metric("edge_wifi_phy_limit", value, {{"limit", limit}}));
    };

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::smatch match;
        static const std::regex number_line("^([^:]+):\\s+(-?[0-9]+)(?:\\s+.*)?$");
        static const std::regex band_pattern("^Band\\s+([0-9]+):$");
        static const std::regex freq_pattern(
            "^\\*\\s+([0-9]+)\\s+MHz\\s+\\[([0-9]+)\\]\\s+\\(([^)]+)\\)(?:\\s+\\(([^)]+)\\))?$");
        static const std::regex mode_pattern("^\\*\\s+(.+)$");
        static const std::regex frame_pattern("^\\*\\s+([^:]+):\\s+(.+)$");
        static const std::regex feature_pattern("^\\*\\s+(?:\\[\\s*)?([^\\]]+?)(?:\\s*\\])?(?::\\s+.+)?$");

        if (std::regex_match(cleaned, match, band_pattern))
        {
            current_band = match[1].str();
            block = Block::none;
            continue;
        }

        if (cleaned == "Supported interface modes:")
        {
            block = Block::interface_modes;
            continue;
        }
        if (cleaned == "Supported commands:")
        {
            block = Block::commands;
            continue;
        }
        if (cleaned == "WoWLAN support:")
        {
            block = Block::wowlan;
            continue;
        }
        if (cleaned == "Supported TX frame types:")
        {
            block = Block::tx_frame_types;
            continue;
        }
        if (cleaned == "Supported RX frame types:")
        {
            block = Block::rx_frame_types;
            continue;
        }
        if (cleaned == "Supported extended features:")
        {
            block = Block::extended_features;
            continue;
        }

        if (std::regex_match(cleaned, match, freq_pattern) && !current_band.empty())
        {
            const std::string mhz = match[1].str();
            const std::string channel = match[2].str();
            const std::string primary = match[3].str();
            const std::string extra = match[4].matched ? match[4].str() : "";
            const bool disabled = primary == "disabled";
            const bool radar = primary == "radar detection" || extra == "radar detection";

            if (disabled)
            {
                ++band_disabled_count[current_band];
            }
            else
            {
                ++band_enabled_count[current_band];
            }
            if (radar)
            {
                ++band_radar_count[current_band];
            }

            std::map<std::string, std::string> labels {
                {"band", current_band},
                {"mhz", mhz},
                {"channel", channel},
                {"disabled", bool_label(disabled)},
                {"radar_detection", bool_label(radar)},
            };
            if (!disabled)
            {
                labels["max_tx_power_dbm"] =
                    primary == "disabled" ? "" : primary.substr(0, primary.find(' '));
            }
            metrics.push_back(make_info("edge_wifi_band_frequency_info", labels));
            continue;
        }

        if (block == Block::interface_modes && std::regex_match(cleaned, match, mode_pattern))
        {
            metrics.push_back(make_info("edge_wifi_supported_interface_mode_info",
                                        {{"mode", match[1].str()}}));
            continue;
        }
        if (block == Block::commands && std::regex_match(cleaned, match, mode_pattern))
        {
            metrics.push_back(make_info("edge_wifi_supported_command_info",
                                        {{"command", match[1].str()}}));
            continue;
        }
        if (block == Block::wowlan && std::regex_match(cleaned, match, mode_pattern))
        {
            metrics.push_back(make_info("edge_wifi_wowlan_support_info",
                                        {{"feature", match[1].str()}}));
            continue;
        }
        if ((block == Block::tx_frame_types || block == Block::rx_frame_types) &&
            std::regex_match(cleaned, match, frame_pattern))
        {
            metrics.push_back(make_info(
                "edge_wifi_frame_type_support_info",
                {{"direction", block == Block::tx_frame_types ? "tx" : "rx"},
                 {"mode", trim(match[1].str())},
                 {"values", trim(match[2].str())}}));
            continue;
        }
        if (block == Block::extended_features &&
            std::regex_match(cleaned, match, feature_pattern))
        {
            metrics.push_back(make_info("edge_wifi_extended_feature_info",
                                        {{"feature", trim(match[1].str())}}));
            continue;
        }

        if (cleaned == "software interface modes (can always be added):" ||
            cleaned == "valid interface combinations:")
        {
            block = Block::none;
        }

        if (std::regex_match(cleaned, match, number_line))
        {
            const std::string key = trim(match[1].str());
            const double value = std::stod(match[2].str());
            if (key == "max # scan SSIDs")
            {
                emit_limit("max_scan_ssids", value);
            }
            else if (key == "max scan IEs length")
            {
                emit_limit("max_scan_ie_length_bytes", value);
            }
            else if (key == "max # sched scan SSIDs")
            {
                emit_limit("max_sched_scan_ssids", value);
            }
            else if (key == "max # match sets")
            {
                emit_limit("max_match_sets", value);
            }
            else if (key == "RTS threshold")
            {
                emit_limit("rts_threshold", value);
            }
            else if (key == "Retry short limit")
            {
                emit_limit("retry_short_limit", value);
            }
            else if (key == "Retry long limit")
            {
                emit_limit("retry_long_limit", value);
            }
            else if (key == "Coverage class")
            {
                emit_limit("coverage_class", value);
            }
            else if (key == "max # scan plans")
            {
                emit_limit("max_scan_plans", value);
            }
            else if (key == "max scan plan interval")
            {
                emit_limit("max_scan_plan_interval", value);
            }
            else if (key == "max scan plan iterations")
            {
                emit_limit("max_scan_plan_iterations", value);
            }
            else if (key == "Maximum associated stations in AP mode")
            {
                metrics.push_back(make_metric("edge_wifi_max_associated_stations", value));
            }
        }
    }

    std::set<std::string> bands;
    for (const auto &[band, _] : band_enabled_count)
    {
        bands.insert(band);
    }
    for (const auto &[band, _] : band_disabled_count)
    {
        bands.insert(band);
    }

    for (const auto &band : bands)
    {
        metrics.push_back(make_metric("edge_wifi_band_frequencies_total",
                                      static_cast<double>(band_enabled_count[band]),
                                      {{"band", band}, {"disabled", "false"}}));
        metrics.push_back(make_metric("edge_wifi_band_frequencies_total",
                                      static_cast<double>(band_disabled_count[band]),
                                      {{"band", band}, {"disabled", "true"}}));
        metrics.push_back(make_metric("edge_wifi_band_radar_frequencies_total",
                                      static_cast<double>(band_radar_count[band]),
                                      {{"band", band}}));
    }

    return metrics;
}

std::vector<MetricSample> parse_service_list_units(const std::string &output)
{
    std::vector<MetricSample> metrics;
    for (const auto &line : split_lines(output))
    {
        const auto tokens = split_whitespace(trim(line));
        if (tokens.size() < 5)
        {
            continue;
        }

        metrics.push_back(make_info("edge_service_unit_state_info",
                                    {{"service", tokens[0]},
                                     {"load_state", tokens[1]},
                                     {"active_state", tokens[2]},
                                     {"sub_state", tokens[3]},
                                     {"description", join_tail(tokens, 4)}}));
    }
    return metrics;
}

std::vector<MetricSample> parse_systemd_status(const std::string &service_name,
                                               const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::size_t associated_events = 0;
    std::size_t radius_events = 0;

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::smatch match;
        static const std::regex loaded_pattern(
            "^Loaded:\\s+([^(]+)\\s+\\(([^;]+);\\s*([^;]+);\\s*preset:\\s*([^)]+)\\)$");
        static const std::regex active_pattern("^Active:\\s+(\\w+)\\s+\\(([^)]+)\\).*$");
        static const std::regex main_pid_pattern("^Main PID:\\s+([0-9]+).*$");
        static const std::regex tasks_pattern("^Tasks:\\s+([0-9]+)\\s+\\(limit:\\s*([0-9]+)\\)$");
        static const std::regex memory_pattern("^Memory:\\s+([0-9.]+[KMGTP]?)$");
        static const std::regex cpu_pattern("^CPU:\\s+(.+)$");

        if (std::regex_match(cleaned, match, loaded_pattern))
        {
            metrics.push_back(make_info("edge_service_loaded_info",
                                        {{"service", service_name},
                                         {"load_state", trim(match[1].str())},
                                         {"unit_path", trim(match[2].str())},
                                         {"enabled_state", trim(match[3].str())},
                                         {"preset", trim(match[4].str())}}));
        }
        else if (std::regex_match(cleaned, match, active_pattern))
        {
            metrics.push_back(make_metric("edge_service_active",
                                          match[1].str() == "active" ? 1.0 : 0.0,
                                          {{"service", service_name}}));
            metrics.push_back(make_info("edge_service_active_state_info",
                                        {{"service", service_name},
                                         {"active_state", match[1].str()},
                                         {"sub_state", match[2].str()}}));
        }
        else if (std::regex_match(cleaned, match, main_pid_pattern))
        {
            metrics.push_back(make_metric("edge_service_main_pid",
                                          std::stod(match[1].str()),
                                          {{"service", service_name}}));
        }
        else if (std::regex_match(cleaned, match, tasks_pattern))
        {
            metrics.push_back(make_metric("edge_service_tasks",
                                          std::stod(match[1].str()),
                                          {{"service", service_name}}));
            metrics.push_back(make_metric("edge_service_task_limit",
                                          std::stod(match[2].str()),
                                          {{"service", service_name}}));
        }
        else if (std::regex_match(cleaned, match, memory_pattern))
        {
            metrics.push_back(make_metric("edge_service_memory_bytes",
                                          parse_human_quantity(match[1].str()),
                                          {{"service", service_name}}));
        }
        else if (std::regex_match(cleaned, match, cpu_pattern))
        {
            metrics.push_back(make_metric("edge_service_cpu_seconds",
                                          parse_duration_seconds(match[1].str()),
                                          {{"service", service_name}}));
        }

        if (cleaned.find("IEEE 802.11: associated") != std::string::npos)
        {
            ++associated_events;
        }
        if (cleaned.find("RADIUS: starting accounting session") != std::string::npos)
        {
            ++radius_events;
        }
    }

    metrics.push_back(make_metric("edge_wifi_station_association_events_total",
                                  static_cast<double>(associated_events),
                                  {{"service", service_name}}));
    metrics.push_back(make_metric("edge_wifi_radius_accounting_events_total",
                                  static_cast<double>(radius_events),
                                  {{"service", service_name}}));

    return metrics;
}

std::vector<MetricSample> parse_command_path(const std::string &command_name,
                                             const std::string &output)
{
    const std::string cleaned = trim(output);
    if (cleaned.empty())
    {
        return {make_metric("edge_command_available", 0.0, {{"command", command_name}})};
    }

    return {make_metric("edge_command_available", 1.0, {{"command", command_name}}),
            make_info("edge_command_path_info",
                      {{"command", command_name}, {"path", cleaned}})};
}

std::vector<MetricSample> parse_dnsmasq_leases(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::size_t count = 0;

    for (const auto &line : split_lines(output))
    {
        const auto tokens = split_whitespace(trim(line));
        if (tokens.size() < 5)
        {
            continue;
        }

        ++count;
        metrics.push_back(make_info("edge_hotspot_dhcp_lease_info",
                                    {{"mac", tokens[1]},
                                     {"ip", tokens[2]},
                                     {"hostname", tokens[3]},
                                     {"client_id", tokens[4]}}));
        metrics.push_back(make_metric("edge_hotspot_dhcp_lease_expiry_epoch_seconds",
                                      std::stod(tokens[0]),
                                      {{"mac", tokens[1]}, {"ip", tokens[2]}}));
    }

    metrics.push_back(make_metric("edge_hotspot_dhcp_leases_total",
                                  static_cast<double>(count)));
    return metrics;
}

std::vector<MetricSample> parse_mmcli_snapshot(const std::string &command_path_output,
                                               const std::string &list_output,
                                               const std::string &modem_output)
{
    std::vector<MetricSample> metrics = parse_command_path("mmcli", command_path_output);

    std::size_t modem_count = 0;
    for (const auto &line : split_lines(list_output))
    {
        if (line.find("/org/freedesktop/ModemManager1/Modem/") != std::string::npos)
        {
            ++modem_count;
        }
    }
    metrics.push_back(
        make_metric("edge_lte_modemmanager_modems_total", static_cast<double>(modem_count)));

    for (const auto &line : split_lines(modem_output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        const auto pos = cleaned.find(':');
        if (pos == std::string::npos)
        {
            continue;
        }

        const std::string key = trim(cleaned.substr(0, pos));
        const std::string value = trim(cleaned.substr(pos + 1));
        if (key == "state")
        {
            metrics.push_back(
                make_info("edge_lte_modem_state_info", {{"state", strip_quotes(value)}}));
        }
        else if (key == "signal quality")
        {
            std::smatch match;
            static const std::regex signal_pattern("^([0-9]+)%.*$");
            if (std::regex_match(value, match, signal_pattern))
            {
                metrics.push_back(make_metric("edge_lte_signal_quality_percent",
                                              std::stod(match[1].str())));
            }
        }
        else if (key == "access tech")
        {
            metrics.push_back(
                make_info("edge_lte_access_technology_info", {{"technology", value}}));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_nmcli_device_status(const std::string &output)
{
    std::vector<MetricSample> metrics;
    for (const auto &line : split_lines(output))
    {
        const auto columns = split_columns(line);
        if (columns.size() != 4 || columns.front() == "DEVICE")
        {
            continue;
        }

        metrics.push_back(make_info("edge_nm_device_state_info",
                                    {{"device", columns[0]},
                                     {"type", columns[1]},
                                     {"state", columns[2]},
                                     {"connection", columns[3]}}));
    }
    return metrics;
}

std::vector<MetricSample> parse_device_node_listing(const std::string &ttyusb_output,
                                                    const std::string &cdc_wdm_output)
{
    std::vector<MetricSample> metrics;
    static const std::regex tty_pattern("(/dev/ttyUSB[0-9]+)");
    static const std::regex cdc_pattern("(/dev/cdc-wdm[0-9]+)");

    std::size_t tty_count = 0;
    for (auto it = std::sregex_iterator(ttyusb_output.begin(), ttyusb_output.end(), tty_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        ++tty_count;
        metrics.push_back(
            make_info("edge_lte_device_node_info", {{"kind", "ttyUSB"}, {"path", (*it)[1]}}));
    }

    std::size_t cdc_count = 0;
    for (auto it = std::sregex_iterator(cdc_wdm_output.begin(), cdc_wdm_output.end(), cdc_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        ++cdc_count;
        metrics.push_back(
            make_info("edge_lte_device_node_info", {{"kind", "cdc-wdm"}, {"path", (*it)[1]}}));
    }

    metrics.push_back(
        make_metric("edge_lte_usb_tty_devices_total", static_cast<double>(tty_count)));
    metrics.push_back(
        make_metric("edge_lte_cdc_wdm_devices_total", static_cast<double>(cdc_count)));
    return metrics;
}

std::vector<MetricSample> parse_loadavg(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::smatch match;
    static const std::regex pattern(
        "^\\s*([0-9]+(?:\\.[0-9]+)?)\\s+([0-9]+(?:\\.[0-9]+)?)\\s+([0-9]+(?:\\.[0-9]+)?)\\s+([0-9]+)/([0-9]+)\\s+([0-9]+)\\s*$");
    const std::string cleaned = trim(output);
    if (!std::regex_match(cleaned, match, pattern))
    {
        return metrics;
    }

    metrics.push_back(
        make_metric("edge_system_load_average", std::stod(match[1].str()), {{"window", "1m"}}));
    metrics.push_back(
        make_metric("edge_system_load_average", std::stod(match[2].str()), {{"window", "5m"}}));
    metrics.push_back(
        make_metric("edge_system_load_average", std::stod(match[3].str()), {{"window", "15m"}}));
    metrics.push_back(
        make_metric("edge_system_processes_running", std::stod(match[4].str())));
    metrics.push_back(make_metric("edge_system_processes_total", std::stod(match[5].str())));
    metrics.push_back(make_metric("edge_system_last_pid", std::stod(match[6].str())));
    return metrics;
}

std::vector<MetricSample> parse_meminfo(const std::string &output)
{
    std::vector<MetricSample> metrics;
    static const std::regex pattern("^([^:]+):\\s+([0-9]+)\\s*(\\w+)?$");

    for (const auto &line : split_lines(output))
    {
        std::smatch match;
        const std::string cleaned = trim(line);
        if (!std::regex_match(cleaned, match, pattern))
        {
            continue;
        }

        double value = std::stod(match[2].str());
        const std::string unit = match[3].matched ? match[3].str() : "";
        if (unit == "kB")
        {
            value *= 1024.0;
        }

        metrics.push_back(make_metric("edge_system_meminfo_bytes",
                                      value,
                                      {{"field", trim(match[1].str())}}));
    }

    return metrics;
}

std::vector<MetricSample> parse_proc_net_dev(const std::string &output)
{
    std::vector<MetricSample> metrics;
    static const std::vector<std::string> metric_names = {
        "edge_network_receive_bytes",
        "edge_network_receive_packets",
        "edge_network_receive_errors",
        "edge_network_receive_drops",
        "edge_network_receive_fifo",
        "edge_network_receive_frame",
        "edge_network_receive_compressed",
        "edge_network_receive_multicast",
        "edge_network_transmit_bytes",
        "edge_network_transmit_packets",
        "edge_network_transmit_errors",
        "edge_network_transmit_drops",
        "edge_network_transmit_fifo",
        "edge_network_transmit_collisions",
        "edge_network_transmit_carrier",
        "edge_network_transmit_compressed",
    };

    for (const auto &line : split_lines(output))
    {
        const auto pos = line.find(':');
        if (pos == std::string::npos)
        {
            continue;
        }

        const std::string interface_name = trim(line.substr(0, pos));
        const auto tokens = split_whitespace(line.substr(pos + 1));
        if (tokens.size() != metric_names.size())
        {
            continue;
        }

        for (std::size_t i = 0; i < metric_names.size(); ++i)
        {
            metrics.push_back(make_metric(metric_names[i],
                                          std::stod(tokens[i]),
                                          {{"interface", interface_name}}));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_thermal_zone_temp(const std::string &output,
                                                  const std::string &zone_name)
{
    const std::string cleaned = trim(output);
    if (cleaned.empty())
    {
        return {};
    }

    return {make_metric("edge_system_temperature_celsius",
                        std::stod(cleaned) / 1000.0,
                        {{"zone", zone_name}})};
}

std::vector<MetricSample> parse_iptables(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::string current_chain;

    static const std::regex chain_pattern(
        "^Chain\\s+(\\S+)\\s+\\(policy\\s+(\\S+)\\s+([0-9]+)\\s+packets,\\s+([^\\)]+)\\s+bytes\\)$");
    static const std::regex reference_chain_pattern(
        "^Chain\\s+(\\S+)\\s+\\(([0-9]+)\\s+references\\)$");

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::smatch match;
        if (std::regex_match(cleaned, match, chain_pattern))
        {
            current_chain = match[1].str();
            metrics.push_back(make_metric("edge_firewall_iptables_chain_policy_packets",
                                          std::stod(match[3].str()),
                                          {{"chain", current_chain},
                                           {"policy", match[2].str()}}));
            metrics.push_back(make_metric("edge_firewall_iptables_chain_policy_bytes",
                                          parse_human_quantity(match[4].str()),
                                          {{"chain", current_chain},
                                           {"policy", match[2].str()}}));
            continue;
        }
        if (std::regex_match(cleaned, match, reference_chain_pattern))
        {
            current_chain = match[1].str();
            continue;
        }

        const auto tokens = split_whitespace(cleaned);
        if (tokens.size() < 9 || !std::isdigit(tokens[0].front()))
        {
            continue;
        }

        const std::string extra = tokens.size() > 9 ? join_tail(tokens, 9) : "";
        const std::map<std::string, std::string> labels {
            {"chain", current_chain},
            {"target", tokens[2]},
            {"input", tokens[5]},
            {"output", tokens[6]},
            {"source", tokens[7]},
            {"destination", tokens[8]},
            {"extra", extra},
        };

        metrics.push_back(make_metric("edge_firewall_iptables_rule_packets",
                                      std::stod(tokens[0]),
                                      labels));
        metrics.push_back(make_metric("edge_firewall_iptables_rule_bytes",
                                      parse_human_quantity(tokens[1]),
                                      labels));
    }

    return metrics;
}

std::vector<MetricSample> parse_nft_ruleset(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::string current_table;
    std::string current_chain;

    static const std::regex table_pattern("^table\\s+\\S+\\s+(\\S+)\\s*\\{$");
    static const std::regex chain_pattern("^chain\\s+(\\S+)\\s*\\{$");
    static const std::regex rule_pattern("^(.*)counter packets\\s+([0-9]+)\\s+bytes\\s+([0-9]+)(.*)$");

    for (const auto &line : split_lines(output))
    {
        const std::string cleaned = trim(line);
        if (cleaned.empty())
        {
            continue;
        }

        std::smatch match;
        if (std::regex_match(cleaned, match, table_pattern))
        {
            current_table = match[1].str();
            current_chain.clear();
            continue;
        }
        if (std::regex_match(cleaned, match, chain_pattern))
        {
            current_chain = match[1].str();
            continue;
        }
        if (cleaned == "}")
        {
            if (!current_chain.empty())
            {
                current_chain.clear();
            }
            else
            {
                current_table.clear();
            }
            continue;
        }

        if (std::regex_match(cleaned, match, rule_pattern) &&
            !current_table.empty() && !current_chain.empty())
        {
            const std::string expression =
                normalize_spaces(match[1].str() + " " + match[4].str());
            const std::map<std::string, std::string> labels {
                {"table", current_table},
                {"chain", current_chain},
                {"expression", expression},
            };
            metrics.push_back(make_metric("edge_firewall_nft_rule_packets",
                                          std::stod(match[2].str()),
                                          labels));
            metrics.push_back(make_metric("edge_firewall_nft_rule_bytes",
                                          std::stod(match[3].str()),
                                          labels));
        }
    }

    return metrics;
}

std::vector<MetricSample> parse_ip_neigh(const std::string &output)
{
    std::vector<MetricSample> metrics;
    std::unordered_map<std::string, std::size_t> count_by_device;

    for (const auto &line : split_lines(output))
    {
        const auto tokens = split_whitespace(trim(line));
        if (tokens.size() < 5)
        {
            continue;
        }

        std::string ip = tokens[0];
        std::string device;
        std::string mac;
        std::string state = tokens.back();
        for (std::size_t i = 1; i + 1 < tokens.size(); ++i)
        {
            if (tokens[i] == "dev")
            {
                device = tokens[i + 1];
            }
            else if (tokens[i] == "lladdr")
            {
                mac = tokens[i + 1];
            }
        }

        if (device.empty())
        {
            continue;
        }

        ++count_by_device[device];
        metrics.push_back(make_info("edge_neighbor_entry_info",
                                    {{"device", device},
                                     {"ip", ip},
                                     {"mac", mac},
                                     {"state", state}}));
    }

    for (const auto &[device, count] : count_by_device)
    {
        metrics.push_back(make_metric("edge_neighbor_entries_total",
                                      static_cast<double>(count),
                                      {{"device", device}}));
    }

    return metrics;
}

}  // namespace edge_probe
