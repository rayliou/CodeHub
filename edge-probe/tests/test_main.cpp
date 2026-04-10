#include "edge_probe/collectors.h"
#include "edge_probe/telemetry_sender.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using edge_probe::Clock;
using edge_probe::CommandSpec;
using edge_probe::HttpTransport;
using edge_probe::MetricSample;
using edge_probe::TelemetryConfig;
using edge_probe::TelemetryWriter;

class ManualClock final : public Clock
{
public:
    ManualClock(int64_t monotonic_ms, int64_t unix_ms)
        : monotonic_ms_(monotonic_ms), unix_ms_(unix_ms)
    {
    }

    int64_t monotonic_now_ms() const override
    {
        return monotonic_ms_;
    }

    int64_t unix_epoch_ms() const override
    {
        return unix_ms_;
    }

    void advance_ms(int64_t delta_ms)
    {
        monotonic_ms_ += delta_ms;
        unix_ms_ += delta_ms;
    }

private:
    int64_t monotonic_ms_;
    int64_t unix_ms_;
};

class FakeTransport final : public HttpTransport
{
public:
    explicit FakeTransport(std::vector<Result> scripted_results)
        : scripted_results_(std::move(scripted_results))
    {
    }

    Result post_json_lines(const TelemetryConfig &, const std::string &body) override
    {
        bodies.push_back(body);
        if (call_count_ < scripted_results_.size())
        {
            return scripted_results_[call_count_++];
        }

        ++call_count_;
        Result result;
        result.ok = true;
        result.http_code = 200;
        return result;
    }

    std::vector<std::string> bodies;

private:
    std::vector<Result> scripted_results_;
    std::size_t call_count_ {0};
};

struct TestContext
{
    int failures {0};

    void expect(bool condition, const std::string &message, int line)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL line " << line << ": " << message << '\n';
        }
    }

    void expect_near(double actual,
                     double expected,
                     double tolerance,
                     const std::string &message,
                     int line)
    {
        if (std::fabs(actual - expected) > tolerance)
        {
            ++failures;
            std::cerr << "FAIL line " << line << ": " << message << " expected "
                      << expected << " got " << actual << '\n';
        }
    }
};

#define EXPECT_TRUE(ctx, expr) (ctx).expect((expr), #expr, __LINE__)
#define EXPECT_EQ(ctx, actual, expected) \
    (ctx).expect(((actual) == (expected)), #actual " == " #expected, __LINE__)
#define EXPECT_NEAR(ctx, actual, expected, tolerance) \
    (ctx).expect_near((actual), (expected), (tolerance), #actual, __LINE__)

const std::vector<std::string> &fixture_lines()
{
    static const std::vector<std::string> lines = [] {
        std::ifstream input(std::string(EDGE_PROBE_SOURCE_DIR) + "/cmd.txt");
        if (!input)
        {
            throw std::runtime_error("failed to open cmd.txt");
        }

        std::vector<std::string> loaded;
        std::string line;
        while (std::getline(input, line))
        {
            loaded.push_back(line);
        }
        return loaded;
    }();
    return lines;
}

std::string slice_lines(int first_line, int last_line)
{
    const auto &lines = fixture_lines();
    if (first_line < 1 || last_line < first_line ||
        static_cast<std::size_t>(last_line) > lines.size())
    {
        throw std::out_of_range("slice_lines range is invalid");
    }

    std::ostringstream output;
    for (int i = first_line; i <= last_line; ++i)
    {
        output << lines[static_cast<std::size_t>(i - 1)];
        if (i != last_line)
        {
            output << '\n';
        }
    }
    return output.str();
}

bool labels_match_subset(const std::map<std::string, std::string> &labels,
                         const std::map<std::string, std::string> &subset)
{
    for (const auto &[key, expected] : subset)
    {
        const auto it = labels.find(key);
        if (it == labels.end() || it->second != expected)
        {
            return false;
        }
    }
    return true;
}

const MetricSample *find_metric(const std::vector<MetricSample> &metrics,
                                const std::string &name,
                                const std::map<std::string, std::string> &labels = {})
{
    for (const auto &metric : metrics)
    {
        if (metric.name == name && labels_match_subset(metric.labels, labels))
        {
            return &metric;
        }
    }
    return nullptr;
}

double require_metric_value(TestContext &ctx,
                            const std::vector<MetricSample> &metrics,
                            const std::string &name,
                            const std::map<std::string, std::string> &labels = {})
{
    const MetricSample *metric = find_metric(metrics, name, labels);
    std::ostringstream message;
    message << "missing metric " << name;
    if (!labels.empty())
    {
        message << " with labels ";
        bool first = true;
        for (const auto &[key, value] : labels)
        {
            if (!first)
            {
                message << ", ";
            }
            first = false;
            message << key << '=' << value;
        }
    }
    ctx.expect(metric != nullptr, message.str(), __LINE__);
    return metric == nullptr ? 0.0 : metric->value;
}

void require_metric_present(TestContext &ctx,
                            const std::vector<MetricSample> &metrics,
                            const std::string &name,
                            const std::map<std::string, std::string> &labels = {})
{
    EXPECT_TRUE(ctx, find_metric(metrics, name, labels) != nullptr);
}

void test_default_command_plan(TestContext &ctx)
{
    const auto plan = edge_probe::default_command_plan();
    EXPECT_EQ(ctx, plan.size(), std::size_t {9});
    EXPECT_EQ(ctx, plan.front().id, std::string("system_identity"));
    EXPECT_TRUE(ctx, plan[3].commands[0].find("hostapd") != std::string::npos);
    EXPECT_TRUE(ctx, plan[5].commands[0].find("mmcli") != std::string::npos);
}

void test_sender_retry_flow(TestContext &ctx)
{
    auto clock = std::make_shared<ManualClock>(1000, 1700000000000LL);
    HttpTransport::Result first_result;
    first_result.ok = false;
    first_result.http_code = 500;
    first_result.response_body = "server error";

    HttpTransport::Result second_result;
    second_result.ok = true;
    second_result.http_code = 200;

    auto transport = std::make_shared<FakeTransport>(
        std::vector<HttpTransport::Result> {first_result, second_result});

    TelemetryConfig config;
    config.max_batch_samples = 2;
    config.flush_interval_ms = 10000;
    config.retry_initial_ms = 1000;
    config.retry_max_ms = 4000;

    TelemetryWriter writer(config, transport, clock);
    EXPECT_TRUE(ctx, writer.submit({"edge_test_metric", 1.0, {{"device", "busstop-001"}}, 0}));
    EXPECT_TRUE(ctx, writer.submit({"edge_test_metric", 2.0, {{"device", "busstop-001"}}, 0}));

    writer.tick();

    EXPECT_EQ(ctx, writer.send_failures(), std::uint64_t {1});
    EXPECT_EQ(ctx, writer.sent_batches(), std::uint64_t {0});
    EXPECT_TRUE(ctx, writer.has_pending_payload());
    EXPECT_EQ(ctx, transport->bodies.size(), std::size_t {1});
    EXPECT_TRUE(ctx, transport->bodies.front().find("\"edge_test_metric\"") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, transport->bodies.front().find("1700000000000") !=
                         std::string::npos);
    EXPECT_TRUE(ctx,
                !writer.submit({"edge_new_metric", 3.0, {{"device", "busstop-001"}}, 0}));
    EXPECT_EQ(ctx, writer.dropped_samples(), std::uint64_t {1});

    clock->advance_ms(999);
    writer.tick();
    EXPECT_EQ(ctx, transport->bodies.size(), std::size_t {1});

    clock->advance_ms(1);
    writer.tick();
    EXPECT_EQ(ctx, transport->bodies.size(), std::size_t {2});
    EXPECT_EQ(ctx, writer.sent_batches(), std::uint64_t {1});
    EXPECT_TRUE(ctx, !writer.has_pending_payload());
    EXPECT_EQ(ctx, writer.last_success_unix_ms(), 1700000001000LL);
}

void test_sender_oversized_payload(TestContext &ctx)
{
    auto clock = std::make_shared<ManualClock>(1000, 1700000000000LL);
    auto transport =
        std::make_shared<FakeTransport>(std::vector<HttpTransport::Result> {});

    TelemetryConfig config;
    config.max_batch_samples = 5;
    config.max_pending_payload_bytes = 32;

    TelemetryWriter writer(config, transport, clock);
    EXPECT_TRUE(ctx, writer.submit({"edge_large_metric_name", 1.0, {{"device", "x"}}, 0}));
    writer.force_flush();

    EXPECT_EQ(ctx, writer.dropped_batches(), std::uint64_t {1});
    EXPECT_EQ(ctx, transport->bodies.size(), std::size_t {0});
    EXPECT_EQ(ctx, writer.sent_batches(), std::uint64_t {0});
}

void test_system_identity_parser(TestContext &ctx)
{
    const auto metrics = edge_probe::parse_system_identity(
        slice_lines(6, 6), slice_lines(7, 15), slice_lines(16, 16), slice_lines(17, 18));

    require_metric_present(ctx,
                           metrics,
                           "edge_system_identity_info",
                           {{"host", "hotspot"},
                            {"kernel", "6.1.31-sun50iw9"},
                            {"arch", "aarch64"},
                            {"os_id", "debian"},
                            {"pid1", "systemd"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx, metrics, "edge_systemd_major_version"),
                252.0,
                0.001);
    require_metric_present(ctx,
                           metrics,
                           "edge_systemd_version_info",
                           {{"version", "252.39-1~deb12u1"}});
}

void test_network_parsers(TestContext &ctx)
{
    auto link_metrics = edge_probe::parse_ip_br_link(slice_lines(27, 31));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     link_metrics,
                                     "edge_network_link_up",
                                     {{"interface", "wlan0"}}),
                1.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     link_metrics,
                                     "edge_network_no_carrier",
                                     {{"interface", "end0"}}),
                1.0,
                0.001);
    require_metric_present(ctx,
                           link_metrics,
                           "edge_network_link_info",
                           {{"interface", "wwx00217edb231f"},
                            {"mac", "00:21:7e:db:23:1f"}});

    auto addr_metrics = edge_probe::parse_ip_br_addr(slice_lines(32, 36));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     addr_metrics,
                                     "edge_network_address_count",
                                     {{"interface", "end0"}}),
                0.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     addr_metrics,
                                     "edge_network_address_count",
                                     {{"interface", "wlan0"}}),
                1.0,
                0.001);
    require_metric_present(ctx,
                           addr_metrics,
                           "edge_network_address_info",
                           {{"interface", "wwx00217edb231f"},
                            {"cidr", "192.168.225.46/24"}});

    auto route_metrics = edge_probe::parse_ip_route(slice_lines(37, 41));
    require_metric_present(ctx,
                           route_metrics,
                           "edge_network_default_route_info",
                           {{"device", "wwx00217edb231f"},
                            {"gateway", "192.168.225.1"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     route_metrics,
                                     "edge_network_route_metric",
                                     {{"prefix", "169.254.0.0/16"},
                                      {"device", "wlan0"}}),
                1000.0,
                0.001);
}

void test_wifi_parsers(TestContext &ctx)
{
    auto dev_metrics = edge_probe::parse_iw_dev(slice_lines(52, 58));
    require_metric_present(ctx,
                           dev_metrics,
                           "edge_wifi_ap_info",
                           {{"interface", "wlan0"},
                            {"ssid", "NJTransit - Free WiFi"},
                            {"type", "AP"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     dev_metrics,
                                     "edge_wifi_interface_ifindex",
                                     {{"interface", "wlan0"}}),
                3.0,
                0.001);

    auto phy_metrics = edge_probe::parse_iw_phy(slice_lines(59, 295));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     phy_metrics,
                                     "edge_wifi_phy_limit",
                                     {{"limit", "max_scan_ssids"}}),
                12.0,
                0.001);
    require_metric_present(ctx,
                           phy_metrics,
                           "edge_wifi_supported_interface_mode_info",
                           {{"mode", "AP"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     phy_metrics,
                                     "edge_wifi_band_frequencies_total",
                                     {{"band", "1"}, {"disabled", "false"}}),
                14.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     phy_metrics,
                                     "edge_wifi_band_frequencies_total",
                                     {{"band", "2"}, {"disabled", "true"}}),
                10.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     phy_metrics,
                                     "edge_wifi_band_radar_frequencies_total",
                                     {{"band", "2"}}),
                16.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     phy_metrics,
                                     "edge_wifi_max_associated_stations"),
                10.0,
                0.001);
    require_metric_present(ctx,
                           phy_metrics,
                           "edge_wifi_supported_command_info",
                           {{"command", "start_ap"}});
    require_metric_present(ctx,
                           phy_metrics,
                           "edge_wifi_extended_feature_info",
                           {{"feature", "SCHED_SCAN_RELATIVE_RSSI"}});
}

void test_service_and_tool_parsers(TestContext &ctx)
{
    auto list_metrics = edge_probe::parse_service_list_units(slice_lines(311, 313));
    require_metric_present(ctx,
                           list_metrics,
                           "edge_service_unit_state_info",
                           {{"service", "hostapd.service"},
                            {"active_state", "active"},
                            {"sub_state", "running"}});

    auto hostapd_metrics =
        edge_probe::parse_systemd_status("hostapd", slice_lines(314, 331));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_service_active",
                                     {{"service", "hostapd"}}),
                1.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_service_main_pid",
                                     {{"service", "hostapd"}}),
                99331.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_service_memory_bytes",
                                     {{"service", "hostapd"}}),
                888.0 * 1024.0,
                0.1);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_service_cpu_seconds",
                                     {{"service", "hostapd"}}),
                0.699,
                0.0001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_wifi_station_association_events_total",
                                     {{"service", "hostapd"}}),
                1.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_metrics,
                                     "edge_wifi_radius_accounting_events_total",
                                     {{"service", "hostapd"}}),
                1.0,
                0.001);

    auto dnsmasq_metrics =
        edge_probe::parse_systemd_status("dnsmasq", slice_lines(332, 346));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     dnsmasq_metrics,
                                     "edge_service_memory_bytes",
                                     {{"service", "dnsmasq"}}),
                748.0 * 1024.0,
                0.1);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     dnsmasq_metrics,
                                     "edge_service_cpu_seconds",
                                     {{"service", "dnsmasq"}}),
                0.362,
                0.0001);

    auto network_manager_metrics =
        edge_probe::parse_systemd_status("NetworkManager", slice_lines(347, 368));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     network_manager_metrics,
                                     "edge_service_main_pid",
                                     {{"service", "NetworkManager"}}),
                609.0,
                0.001);

    auto hostapd_cli = edge_probe::parse_command_path("hostapd_cli", slice_lines(369, 369));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     hostapd_cli,
                                     "edge_command_available",
                                     {{"command", "hostapd_cli"}}),
                1.0,
                0.001);

    auto nmcli = edge_probe::parse_command_path("nmcli", slice_lines(370, 370));
    require_metric_present(ctx,
                           nmcli,
                           "edge_command_path_info",
                           {{"command", "nmcli"}, {"path", "/usr/bin/nmcli"}});
}

void test_dnsmasq_lease_parser(TestContext &ctx)
{
    auto metrics = edge_probe::parse_dnsmasq_leases(slice_lines(377, 377));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx, metrics, "edge_hotspot_dhcp_leases_total"),
                1.0,
                0.001);
    require_metric_present(ctx,
                           metrics,
                           "edge_hotspot_dhcp_lease_info",
                           {{"mac", "72:7e:cf:ce:66:f3"},
                            {"ip", "10.0.0.16"},
                            {"hostname", "OnePlus-8"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     metrics,
                                     "edge_hotspot_dhcp_lease_expiry_epoch_seconds",
                                     {{"mac", "72:7e:cf:ce:66:f3"}}),
                1775766365.0,
                0.001);
}

void test_modem_and_nmcli_parsers(TestContext &ctx)
{
    auto mmcli_metrics = edge_probe::parse_mmcli_snapshot("", "", "");
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     mmcli_metrics,
                                     "edge_command_available",
                                     {{"command", "mmcli"}}),
                0.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     mmcli_metrics,
                                     "edge_lte_modemmanager_modems_total"),
                0.0,
                0.001);

    auto nm_metrics = edge_probe::parse_nmcli_device_status(slice_lines(386, 390));
    require_metric_present(ctx,
                           nm_metrics,
                           "edge_nm_device_state_info",
                           {{"device", "wlan0"},
                            {"type", "wifi"},
                            {"state", "unmanaged"}});

    auto node_metrics = edge_probe::parse_device_node_listing(slice_lines(398, 398), "");
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     node_metrics,
                                     "edge_lte_usb_tty_devices_total"),
                5.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     node_metrics,
                                     "edge_lte_cdc_wdm_devices_total"),
                0.0,
                0.001);
    require_metric_present(ctx,
                           node_metrics,
                           "edge_lte_device_node_info",
                           {{"kind", "ttyUSB"}, {"path", "/dev/ttyUSB4"}});
}

void test_system_health_parsers(TestContext &ctx)
{
    auto load_metrics = edge_probe::parse_loadavg(slice_lines(405, 405));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     load_metrics,
                                     "edge_system_load_average",
                                     {{"window", "1m"}}),
                1.32,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx, load_metrics, "edge_system_processes_total"),
                144.0,
                0.001);

    auto mem_metrics = edge_probe::parse_meminfo(slice_lines(406, 425));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     mem_metrics,
                                     "edge_system_meminfo_bytes",
                                     {{"field", "MemTotal"}}),
                1005356.0 * 1024.0,
                0.1);

    auto net_metrics = edge_probe::parse_proc_net_dev(slice_lines(426, 432));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     net_metrics,
                                     "edge_network_receive_bytes",
                                     {{"interface", "wlan0"}}),
                352916.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     net_metrics,
                                     "edge_network_transmit_drops",
                                     {{"interface", "wg0"}}),
                4.0,
                0.001);

    auto thermal_metrics =
        edge_probe::parse_thermal_zone_temp(slice_lines(433, 433), "thermal_zone0");
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     thermal_metrics,
                                     "edge_system_temperature_celsius",
                                     {{"zone", "thermal_zone0"}}),
                52.793,
                0.0001);
}

void test_firewall_parsers(TestContext &ctx)
{
    auto iptables_cmd = edge_probe::parse_command_path("iptables", slice_lines(484, 484));
    auto nft_cmd = edge_probe::parse_command_path("nft", slice_lines(554, 554));
    auto conntrack_cmd = edge_probe::parse_command_path("conntrack", "");

    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     iptables_cmd,
                                     "edge_command_available",
                                     {{"command", "iptables"}}),
                1.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     nft_cmd,
                                     "edge_command_available",
                                     {{"command", "nft"}}),
                1.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     conntrack_cmd,
                                     "edge_command_available",
                                     {{"command", "conntrack"}}),
                0.0,
                0.001);

    auto iptables_metrics = edge_probe::parse_iptables(slice_lines(485, 553));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     iptables_metrics,
                                     "edge_firewall_iptables_chain_policy_packets",
                                     {{"chain", "INPUT"}, {"policy", "DROP"}}),
                111.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     iptables_metrics,
                                     "edge_firewall_iptables_rule_packets",
                                     {{"chain", "FORWARD"},
                                      {"target", "NJTRANSIT-COOLDOWN"},
                                      {"input", "wlan0"}}),
                657.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     iptables_metrics,
                                     "edge_firewall_iptables_rule_bytes",
                                     {{"chain", "NJTRANSIT-COOLDOWN"},
                                      {"target", "DROP"},
                                      {"extra", "MAC 72:7e:cf:ce:66:f3"}}),
                180.0,
                0.001);

    auto nft_metrics = edge_probe::parse_nft_ruleset(slice_lines(555, 691));
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     nft_metrics,
                                     "edge_firewall_nft_rule_packets",
                                     {{"table", "filter"},
                                      {"chain", "FORWARD"},
                                      {"expression",
                                       "iifname \"wlan0\" oifname \"wwx00217edb231f\" accept"}}),
                2.0,
                0.001);
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     nft_metrics,
                                     "edge_firewall_nft_rule_packets",
                                     {{"table", "nat"},
                                      {"chain", "POSTROUTING"},
                                      {"expression",
                                       "oifname \"wwx00217edb231f\" masquerade"}}),
                1038.0,
                0.001);
}

void test_neighbor_parser(TestContext &ctx)
{
    auto metrics = edge_probe::parse_ip_neigh(slice_lines(698, 699));
    require_metric_present(ctx,
                           metrics,
                           "edge_neighbor_entry_info",
                           {{"device", "wlan0"},
                            {"ip", "10.0.0.16"},
                            {"mac", "72:7e:cf:ce:66:f3"},
                            {"state", "STALE"}});
    EXPECT_NEAR(ctx,
                require_metric_value(ctx,
                                     metrics,
                                     "edge_neighbor_entries_total",
                                     {{"device", "wwx00217edb231f"}}),
                1.0,
                0.001);
}

}  // namespace

int main()
{
    TestContext ctx;

    const std::vector<std::pair<std::string, std::function<void(TestContext &)>>> tests = {
        {"default_command_plan", test_default_command_plan},
        {"sender_retry_flow", test_sender_retry_flow},
        {"sender_oversized_payload", test_sender_oversized_payload},
        {"system_identity_parser", test_system_identity_parser},
        {"network_parsers", test_network_parsers},
        {"wifi_parsers", test_wifi_parsers},
        {"service_and_tool_parsers", test_service_and_tool_parsers},
        {"dnsmasq_lease_parser", test_dnsmasq_lease_parser},
        {"modem_and_nmcli_parsers", test_modem_and_nmcli_parsers},
        {"system_health_parsers", test_system_health_parsers},
        {"firewall_parsers", test_firewall_parsers},
        {"neighbor_parser", test_neighbor_parser},
    };

    for (const auto &[name, test] : tests)
    {
        try
        {
            test(ctx);
        }
        catch (const std::exception &ex)
        {
            ++ctx.failures;
            std::cerr << "FAIL test " << name << " threw exception: " << ex.what()
                      << '\n';
        }
    }

    if (ctx.failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << ctx.failures << " test assertions failed\n";
    return 1;
}
