#pragma once

#include "edge_probe/telemetry_sender.h"

namespace edge_probe
{

class CurlHttpTransport final : public HttpTransport
{
public:
    Result post_json_lines(const TelemetryConfig &config,
                           const std::string &body) override;
};

}  // namespace edge_probe
