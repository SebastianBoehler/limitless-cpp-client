#pragma once

#include "limitless/network.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace limitless
{
    struct HttpResponse
    {
        long status{0};
        std::string body;
        std::string error;
        std::string retry_after;
    };

    // One keep-alive libcurl handle. Not safe for concurrent requests on the same instance.
    class HttpClient
    {
    public:
        explicit HttpClient(NetworkOptions options = {});
        ~HttpClient();

        HttpClient(HttpClient &&) noexcept;
        HttpClient &operator=(HttpClient &&) noexcept;
        HttpClient(const HttpClient &) = delete;
        HttpClient &operator=(const HttpClient &) = delete;

        HttpResponse request(const std::string &method,
                             const std::string &url,
                             const std::string &body,
                             const std::vector<std::pair<std::string, std::string>> &headers) const;

        const NetworkOptions &options() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
