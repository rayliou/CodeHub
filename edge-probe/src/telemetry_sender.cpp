#include "edge_probe/telemetry_sender.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace edge_probe
{

namespace
{

class TelemetryWriterBatchBuffer
{
public:
    explicit TelemetryWriterBatchBuffer(const TelemetryConfig &config) : config_(config)
    {
    }

    bool add(const MetricSample &sample, int64_t now_monotonic_ms)
    {
        if (samples_.empty())
        {
            first_sample_monotonic_ms_ = now_monotonic_ms;
        }

        if (samples_.size() >= config_.max_batch_samples)
        {
            return false;
        }

        samples_.push_back(sample);
        return true;
    }

    bool should_flush(int64_t now_monotonic_ms) const
    {
        if (samples_.empty())
        {
            return false;
        }

        if (samples_.size() >= config_.max_batch_samples)
        {
            return true;
        }

        return (now_monotonic_ms - first_sample_monotonic_ms_) >=
               config_.flush_interval_ms;
    }

    bool empty() const
    {
        return samples_.empty();
    }

    std::size_t size() const
    {
        return samples_.size();
    }

    std::string build_json_lines() const
    {
        std::ostringstream output;
        output << std::setprecision(15);

        for (const auto &sample : samples_)
        {
            output << "{\"metric\":{\"__name__\":\"" << json_escape(sample.name) << "\"";
            for (const auto &[key, value] : sample.labels)
            {
                output << ",\"" << json_escape(key) << "\":\"" << json_escape(value)
                       << "\"";
            }

            output << "},\"values\":[" << sample.value << "],\"timestamps\":["
                   << sample.timestamp_ms << "]}\n";
        }

        return output.str();
    }

    void clear()
    {
        samples_.clear();
        first_sample_monotonic_ms_ = 0;
    }

private:
    const TelemetryConfig &config_;
    std::vector<MetricSample> samples_;
    int64_t first_sample_monotonic_ms_ {0};
};

}  // namespace

class TelemetryWriter::BatchBuffer : public TelemetryWriterBatchBuffer
{
public:
    using TelemetryWriterBatchBuffer::TelemetryWriterBatchBuffer;
};

int64_t SystemClock::monotonic_now_ms() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t SystemClock::unix_epoch_ms() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string json_escape(const std::string &value)
{
    std::ostringstream output;
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (ch < 0x20)
                {
                    output << "\\u00" << std::hex << std::nouppercase
                           << static_cast<int>((ch >> 4) & 0x0F)
                           << static_cast<int>(ch & 0x0F) << std::dec;
                }
                else
                {
                    output << static_cast<char>(ch);
                }
                break;
        }
    }
    return output.str();
}

TelemetryWriter::TelemetryWriter(TelemetryConfig config,
                                 std::shared_ptr<HttpTransport> transport,
                                 std::shared_ptr<Clock> clock)
    : config_(std::move(config)),
      transport_(std::move(transport)),
      clock_(std::move(clock)),
      batch_(std::make_unique<BatchBuffer>(config_))
{
    if (transport_ == nullptr)
    {
        throw std::invalid_argument("transport must not be null");
    }

    if (clock_ == nullptr)
    {
        throw std::invalid_argument("clock must not be null");
    }
}

TelemetryWriter::~TelemetryWriter() = default;

bool TelemetryWriter::submit(MetricSample sample)
{
    if (sample.timestamp_ms == 0)
    {
        sample.timestamp_ms = clock_->unix_epoch_ms();
    }

    if (pending_payload_.has_value())
    {
        ++dropped_samples_;
        log("WARN",
            "pending payload exists, dropping sample; dropped_samples=" +
                std::to_string(dropped_samples_));
        return false;
    }

    if (!batch_->add(sample, clock_->monotonic_now_ms()))
    {
        ++dropped_samples_;
        log("WARN",
            "batch full, dropping sample; dropped_samples=" +
                std::to_string(dropped_samples_));
        return false;
    }

    return true;
}

void TelemetryWriter::tick()
{
    const int64_t now_monotonic_ms = clock_->monotonic_now_ms();

    if (pending_payload_.has_value())
    {
        if (now_monotonic_ms >= next_retry_at_ms_)
        {
            retry_pending();
        }
        return;
    }

    if (batch_->should_flush(now_monotonic_ms))
    {
        flush_batch();
    }
}

void TelemetryWriter::force_flush()
{
    if (pending_payload_.has_value())
    {
        retry_pending();
        return;
    }

    if (!batch_->empty())
    {
        flush_batch();
    }
}

std::uint64_t TelemetryWriter::sent_batches() const
{
    return sent_batches_;
}

std::uint64_t TelemetryWriter::send_failures() const
{
    return send_failures_;
}

std::uint64_t TelemetryWriter::dropped_samples() const
{
    return dropped_samples_;
}

std::uint64_t TelemetryWriter::dropped_batches() const
{
    return dropped_batches_;
}

std::size_t TelemetryWriter::buffered_samples() const
{
    return batch_->size();
}

bool TelemetryWriter::has_pending_payload() const
{
    return pending_payload_.has_value();
}

int64_t TelemetryWriter::next_retry_at_ms() const
{
    return next_retry_at_ms_;
}

int64_t TelemetryWriter::last_success_unix_ms() const
{
    return last_success_unix_ms_;
}

void TelemetryWriter::flush_batch()
{
    if (batch_->empty())
    {
        return;
    }

    const std::string payload = batch_->build_json_lines();
    if (payload.size() > config_.max_pending_payload_bytes)
    {
        ++dropped_batches_;
        log("ERROR",
            "payload too large, dropping batch; bytes=" +
                std::to_string(payload.size()) +
                " dropped_batches=" + std::to_string(dropped_batches_));
        batch_->clear();
        return;
    }

    const auto result = transport_->post_json_lines(config_, payload);
    if (result.ok)
    {
        ++sent_batches_;
        last_success_unix_ms_ = clock_->unix_epoch_ms();
        log("INFO",
            "send ok; http_code=" + std::to_string(result.http_code) +
                " samples=" + std::to_string(batch_->size()) +
                " bytes=" + std::to_string(payload.size()));
        batch_->clear();
        retry_count_ = 0;
        next_retry_delay_ms_ = 0;
        next_retry_at_ms_ = 0;
        return;
    }

    ++send_failures_;
    pending_payload_ = payload;
    pending_payload_sample_count_ = batch_->size();
    batch_->clear();
    schedule_retry(result);

    log("ERROR",
        "send failed, entering retry; http_code=" + std::to_string(result.http_code) +
            " err=" + result.error_message +
            " resp=" + truncate(result.response_body, 256));
}

void TelemetryWriter::retry_pending()
{
    if (!pending_payload_.has_value())
    {
        return;
    }

    const auto result = transport_->post_json_lines(config_, *pending_payload_);
    if (result.ok)
    {
        ++sent_batches_;
        last_success_unix_ms_ = clock_->unix_epoch_ms();
        log("INFO",
            "retry ok; http_code=" + std::to_string(result.http_code) +
                " samples=" + std::to_string(pending_payload_sample_count_) +
                " bytes=" + std::to_string(pending_payload_->size()));

        pending_payload_.reset();
        pending_payload_sample_count_ = 0;
        retry_count_ = 0;
        next_retry_delay_ms_ = 0;
        next_retry_at_ms_ = 0;
        return;
    }

    ++send_failures_;
    schedule_retry(result);
    log("ERROR",
        "retry failed; http_code=" + std::to_string(result.http_code) +
            " retry_count=" + std::to_string(retry_count_) +
            " next_retry_in_ms=" + std::to_string(next_retry_delay_ms_) +
            " err=" + result.error_message +
            " resp=" + truncate(result.response_body, 256));
}

void TelemetryWriter::schedule_retry(const HttpTransport::Result &result)
{
    ++retry_count_;

    if (retry_count_ == 1)
    {
        next_retry_delay_ms_ = config_.retry_initial_ms;
    }
    else
    {
        next_retry_delay_ms_ =
            std::min(next_retry_delay_ms_ * 2, config_.retry_max_ms);
    }

    if (result.http_code == 401 || result.http_code == 403)
    {
        next_retry_delay_ms_ = std::max(next_retry_delay_ms_, 60000);
    }

    next_retry_at_ms_ = clock_->monotonic_now_ms() + next_retry_delay_ms_;
}

std::string TelemetryWriter::truncate(const std::string &value, std::size_t max_len)
{
    if (value.size() <= max_len)
    {
        return value;
    }
    return value.substr(0, max_len) + "...";
}

void TelemetryWriter::log(const std::string &level, const std::string &message)
{
    std::cerr << "[" << level << "] " << message << '\n';
}

}  // namespace edge_probe
