#include "limitless/http_client.hpp"

#include <curl/curl.h>

#include <mutex>
#include <stdexcept>

namespace limitless
{
    namespace
    {
        void ensure_curl()
        {
            static std::once_flag once;
            std::call_once(once, [] {
                if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
                {
                    throw std::runtime_error("curl_global_init failed");
                }
            });
        }

        std::size_t write_body(char *ptr, std::size_t size, std::size_t nmemb, void *userdata)
        {
            const std::size_t bytes = size * nmemb;
            auto *body = static_cast<std::string *>(userdata);
            body->append(ptr, bytes);
            return bytes;
        }

        std::size_t write_header(char *ptr, std::size_t size, std::size_t nmemb, void *userdata)
        {
            const std::size_t bytes = size * nmemb;
            auto *retry_after = static_cast<std::string *>(userdata);
            std::string line(ptr, bytes);
            const std::string prefix = "retry-after:";
            if (line.size() >= prefix.size())
            {
                std::string lower = line;
                for (char &character : lower)
                {
                    if (character >= 'A' && character <= 'Z')
                    {
                        character = static_cast<char>(character - 'A' + 'a');
                    }
                }
                if (lower.compare(0, prefix.size(), prefix) == 0)
                {
                    std::string value = line.substr(prefix.size());
                    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
                    {
                        value.pop_back();
                    }
                    std::size_t start = 0;
                    while (start < value.size() && value[start] == ' ')
                    {
                        ++start;
                    }
                    *retry_after = value.substr(start);
                }
            }
            return bytes;
        }
    }

    struct HttpClient::Impl
    {
        NetworkOptions options;
        CURL *curl{nullptr};

        explicit Impl(NetworkOptions network)
            : options(std::move(network))
        {
            ensure_curl();
            curl = curl_easy_init();
            if (!curl)
            {
                throw std::runtime_error("curl_easy_init failed");
            }
        }

        ~Impl()
        {
            if (curl)
            {
                curl_easy_cleanup(curl);
            }
        }
    };

    HttpClient::HttpClient(NetworkOptions options)
        : impl_(std::make_unique<Impl>(std::move(options)))
    {
    }

    HttpClient::~HttpClient() = default;
    HttpClient::HttpClient(HttpClient &&) noexcept = default;
    HttpClient &HttpClient::operator=(HttpClient &&) noexcept = default;

    const NetworkOptions &HttpClient::options() const
    {
        return impl_->options;
    }

    HttpResponse HttpClient::request(const std::string &method,
                                     const std::string &url,
                                     const std::string &body,
                                     const std::vector<std::pair<std::string, std::string>> &headers) const
    {
        HttpResponse response;
        CURL *curl = impl_->curl;
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, impl_->options.timeout_ms);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, impl_->options.connect_timeout_ms);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.retry_after);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "limitless-cpp-client/0.1.0");

        if (!impl_->options.proxy_url.empty())
        {
            curl_easy_setopt(curl, CURLOPT_PROXY, impl_->options.proxy_url.c_str());
        }
        if (!impl_->options.interface_name.empty())
        {
            curl_easy_setopt(curl, CURLOPT_INTERFACE, impl_->options.interface_name.c_str());
        }

        if (method == "POST")
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
        else if (method != "GET")
        {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
            if (!body.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
            }
        }

        curl_slist *header_list = nullptr;
        for (const auto &header : headers)
        {
            const std::string line = header.first + ": " + header.second;
            header_list = curl_slist_append(header_list, line.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

        const CURLcode code = curl_easy_perform(curl);
        curl_slist_free_all(header_list);
        if (code != CURLE_OK)
        {
            response.error = curl_easy_strerror(code);
            return response;
        }
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
        return response;
    }
}
