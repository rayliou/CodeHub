#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace edge_probe
{

struct MetricSample
{
    std::string name;
    double value {0.0};
    std::map<std::string, std::string> labels;
    int64_t timestamp_ms {0};
};

struct TelemetryConfig
{
    std::string endpoint;
    std::string username;
    std::string password;

    std::size_t max_batch_samples {50};
    int flush_interval_ms {5000};

    int retry_initial_ms {5000};
    int retry_max_ms {60000};

    std::size_t max_pending_payload_bytes {256 * 1024};
    long connect_timeout_sec {10};
    long request_timeout_sec {20};

    bool verify_peer {true};
    bool verify_host {true};
};

class Clock
{
public:
    virtual ~Clock() = default;
    virtual int64_t monotonic_now_ms() const = 0;
    virtual int64_t unix_epoch_ms() const = 0;
};

class SystemClock final : public Clock
{
public:
    int64_t monotonic_now_ms() const override;
    int64_t unix_epoch_ms() const override;
};

class HttpTransport
{
public:
    struct Result
    {
        bool ok {false};
        long http_code {0};
        std::string response_body;
        std::string error_message;
    };

    virtual ~HttpTransport() = default;
    virtual Result post_json_lines(const TelemetryConfig &config,
                                   const std::string &body) = 0;
};

std::string json_escape(const std::string &value);

class TelemetryWriter
{
public:
    TelemetryWriter(TelemetryConfig config,
                    std::shared_ptr<HttpTransport> transport,
                    std::shared_ptr<Clock> clock = std::make_shared<SystemClock>());
    ~TelemetryWriter();

    bool submit(MetricSample sample);
    void tick();
    void force_flush();

    std::uint64_t sent_batches() const;
    std::uint64_t send_failures() const;
    std::uint64_t dropped_samples() const;
    std::uint64_t dropped_batches() const;

    std::size_t buffered_samples() const;
    bool has_pending_payload() const;
    int64_t next_retry_at_ms() const;
    int64_t last_success_unix_ms() const;

private:
    class BatchBuffer;

    void flush_batch();
    void retry_pending();
    void schedule_retry(const HttpTransport::Result &result);
    static std::string truncate(const std::string &value, std::size_t max_len);
    static void log(const std::string &level, const std::string &message);

    TelemetryConfig config_;
    std::shared_ptr<HttpTransport> transport_;
    std::shared_ptr<Clock> clock_;
    std::unique_ptr<BatchBuffer> batch_;

    std::optional<std::string> pending_payload_;
    std::size_t pending_payload_sample_count_ {0};

    int retry_count_ {0};
    int64_t next_retry_at_ms_ {0};
    int next_retry_delay_ms_ {0};

    std::uint64_t sent_batches_ {0};
    std::uint64_t send_failures_ {0};
    std::uint64_t dropped_samples_ {0};
    std::uint64_t dropped_batches_ {0};
    int64_t last_success_unix_ms_ {0};
};

}  // namespace edge_probe
