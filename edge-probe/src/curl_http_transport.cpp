#include "edge_probe/curl_http_transport.h"

#include <curl/curl.h>

#include <mutex>

namespace edge_probe
{

namespace
{

std::once_flag g_curl_init_once;

void ensure_curl_global_init()
{
    std::call_once(g_curl_init_once, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::size_t write_callback(char *ptr, std::size_t size, std::size_t nmemb, void *userdata)
{
    if (userdata == nullptr)
    {
        return 0;
    }

    auto *output = static_cast<std::string *>(userdata);
    output->append(ptr, size * nmemb);
    return size * nmemb;
}

}  // namespace

HttpTransport::Result CurlHttpTransport::post_json_lines(const TelemetryConfig &config,
                                                         const std::string &body)
{
    ensure_curl_global_init();

    Result result;
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        result.error_message = "curl_easy_init failed";
        return result;
    }

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Expect:");

    curl_easy_setopt(curl, CURLOPT_URL, config.endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_USERNAME, config.username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, config.password.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config.connect_timeout_sec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config.request_timeout_sec);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, config.verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, config.verify_host ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.response_body);

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
    {
        result.error_message = curl_easy_strerror(code);
    }
    else
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.http_code);
        result.ok = result.http_code >= 200 && result.http_code < 300;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

}  // namespace edge_probe
