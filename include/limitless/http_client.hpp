#pragma once

#include "limitless/network.hpp"

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace limitless {

struct HttpClientOptions {
    int timeout_ms = 15000;
    int connect_timeout_ms = 5000;
    int dns_cache_timeout_seconds = 60;
    std::string user_agent = "limitless-cpp-client/0.1.0";
    std::string proxy_url;
    std::string interface_name;
    bool tcp_nodelay = true;
};

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct HttpResponse {
    long status = 0;
    std::string body;
    std::string transport_error;
    std::map<std::string, std::string> headers;

    bool transport_ok() const { return transport_error.empty(); }
    bool ok() const { return transport_ok() && status >= 200 && status < 300; }
};

class HttpClient {
public:
    explicit HttpClient(HttpClientOptions options = {});
    ~HttpClient();

    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    HttpResponse request(const HttpRequest &request);
    const HttpClientOptions &options() const { return options_; }

private:
    HttpClientOptions options_;
    void *easy_ = nullptr;
    std::mutex mutex_;
    bool ensure_easy();
};

}  // namespace limitless
