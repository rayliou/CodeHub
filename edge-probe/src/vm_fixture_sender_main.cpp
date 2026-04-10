#include "edge_probe/collectors.h"
#include "edge_probe/curl_http_transport.h"
#include "edge_probe/telemetry_sender.h"

#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using edge_probe::MetricSample;

std::vector<std::string> load_lines(const std::string &path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open fixture file: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

std::string slice_lines(const std::vector<std::string> &lines, int first_line, int last_line)
{
    if (first_line < 1 || last_line < first_line ||
        static_cast<std::size_t>(last_line) > lines.size())
    {
        throw std::out_of_range("invalid fixture slice");
    }

    std::string output;
    for (int line = first_line; line <= last_line; ++line)
    {
        output += lines[static_cast<std::size_t>(line - 1)];
        if (line != last_line)
        {
            output.push_back('\n');
        }
    }
    return output;
}

void append_metrics(std::vector<MetricSample> &target,
                    std::vector<MetricSample> source,
                    const std::map<std::string, std::string> &extra_labels)
{
    for (auto &sample : source)
    {
        for (const auto &[key, value] : extra_labels)
        {
            sample.labels.emplace(key, value);
        }
        target.push_back(std::move(sample));
    }
}

std::vector<MetricSample> collect_cmd_fixture_metrics(const std::string &path,
                                                      const std::string &device_label)
{
    const auto lines = load_lines(path);
    std::vector<MetricSample> metrics;

    const std::map<std::string, std::string> extra_labels {
        {"fixture", "cmd_txt"},
        {"device", device_label},
    };

    append_metrics(metrics,
                   edge_probe::parse_system_identity(slice_lines(lines, 6, 6),
                                                     slice_lines(lines, 7, 15),
                                                     slice_lines(lines, 16, 16),
                                                     slice_lines(lines, 17, 18)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_ip_br_link(slice_lines(lines, 27, 31)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_ip_br_addr(slice_lines(lines, 32, 36)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_ip_route(slice_lines(lines, 37, 41)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_iw_dev(slice_lines(lines, 52, 58)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_iw_phy(slice_lines(lines, 59, 295)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_service_list_units(slice_lines(lines, 311, 313)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_systemd_status("hostapd", slice_lines(lines, 314, 331)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_systemd_status("dnsmasq", slice_lines(lines, 332, 346)),
                   extra_labels);
    append_metrics(
        metrics,
        edge_probe::parse_systemd_status("NetworkManager", slice_lines(lines, 347, 368)),
        extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_command_path("hostapd_cli", slice_lines(lines, 369, 369)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_command_path("nmcli", slice_lines(lines, 370, 370)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_dnsmasq_leases(slice_lines(lines, 377, 377)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_mmcli_snapshot("", "", ""), extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_nmcli_device_status(slice_lines(lines, 386, 390)),
                   extra_labels);
    append_metrics(
        metrics, edge_probe::parse_device_node_listing(slice_lines(lines, 398, 398), ""),
        extra_labels);
    append_metrics(metrics, edge_probe::parse_loadavg(slice_lines(lines, 405, 405)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_meminfo(slice_lines(lines, 406, 425)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_proc_net_dev(slice_lines(lines, 426, 432)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_thermal_zone_temp(slice_lines(lines, 433, 433),
                                                       "thermal_zone0"),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_command_path("iptables", slice_lines(lines, 484, 484)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_iptables(slice_lines(lines, 485, 553)), extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_command_path("nft", slice_lines(lines, 554, 554)),
                   extra_labels);
    append_metrics(metrics,
                   edge_probe::parse_nft_ruleset(slice_lines(lines, 555, 691)),
                   extra_labels);
    append_metrics(metrics, edge_probe::parse_command_path("conntrack", ""), extra_labels);
    append_metrics(metrics, edge_probe::parse_ip_neigh(slice_lines(lines, 698, 699)),
                   extra_labels);

    return metrics;
}

std::string read_option(const std::vector<std::string> &args,
                        const std::string &name,
                        const std::string &default_value = "")
{
    for (std::size_t i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] == name)
        {
            return args[i + 1];
        }
    }
    return default_value;
}

bool has_flag(const std::vector<std::string> &args, const std::string &name)
{
    for (const auto &arg : args)
    {
        if (arg == name)
        {
            return true;
        }
    }
    return false;
}

void print_usage()
{
    std::cerr
        << "Usage: edge_probe_vm_fixture_sender [options]\n"
        << "  --fixture PATH\n"
        << "  --endpoint URL\n"
        << "  --username USER\n"
        << "  --password PASS\n"
        << "  --device-label DEVICE\n"
        << "  --insecure\n";
}

}  // namespace

int main(int argc, char **argv)
{
    try
    {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i)
        {
            args.emplace_back(argv[i]);
        }

        if (has_flag(args, "--help"))
        {
            print_usage();
            return 0;
        }

        const std::string fixture_path =
            read_option(args, "--fixture", "/opt/edge-probe/cmd.txt");
        const std::string device_label =
            read_option(args, "--device-label", "busstop-001");

        const auto metrics = collect_cmd_fixture_metrics(fixture_path, device_label);
        if (metrics.empty())
        {
            std::cerr << "no metrics were generated from fixture\n";
            return 2;
        }

        edge_probe::TelemetryConfig config;
        config.endpoint =
            read_option(args, "--endpoint", "http://127.0.0.1:8428/api/v1/import");
        config.username = read_option(args, "--username");
        config.password = read_option(args, "--password");
        config.max_batch_samples = metrics.size() + 1;
        config.flush_interval_ms = 1000;
        config.retry_initial_ms = 1000;
        config.retry_max_ms = 5000;
        config.max_pending_payload_bytes = 1024 * 1024;
        config.connect_timeout_sec = 5;
        config.request_timeout_sec = 10;
        config.verify_peer = !has_flag(args, "--insecure");
        config.verify_host = !has_flag(args, "--insecure");

        auto transport = std::make_shared<edge_probe::CurlHttpTransport>();
        edge_probe::TelemetryWriter writer(config, transport);

        std::size_t accepted = 0;
        for (const auto &metric : metrics)
        {
            if (writer.submit(metric))
            {
                ++accepted;
            }
        }

        writer.force_flush();

        std::cout << "fixture=" << fixture_path << '\n'
                  << "accepted_samples=" << accepted << '\n'
                  << "total_samples=" << metrics.size() << '\n'
                  << "sent_batches=" << writer.sent_batches() << '\n'
                  << "send_failures=" << writer.send_failures() << '\n'
                  << "dropped_samples=" << writer.dropped_samples() << '\n'
                  << "dropped_batches=" << writer.dropped_batches() << '\n';

        if (writer.sent_batches() == 0 || writer.send_failures() != 0 ||
            writer.has_pending_payload() || writer.dropped_samples() != 0 ||
            writer.dropped_batches() != 0)
        {
            std::cerr << "sender did not complete successfully\n";
            return 3;
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "fatal: " << ex.what() << '\n';
        return 1;
    }
}
