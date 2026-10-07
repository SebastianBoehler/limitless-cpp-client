#include "limitless/http_client.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>

namespace limitless {
namespace {

struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

void ensure_curl_global() { static CurlGlobal global; }

std::size_t write_body(char *ptr, std::size_t size, std::size_t nmemb, void *userdata) {
    auto *body = static_cast<std::string *>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

std::size_t write_header(char *ptr, std::size_t size, std::size_t nmemb, void *userdata) {
    auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);
    const std::string line(ptr, size * nmemb);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        auto trim = [](std::string &s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        };
        trim(key);
        trim(value);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        (*headers)[key] = value;
    }
    return size * nmemb;
}

std::string route_proxy(const HttpClientOptions &options) {
    if (!options.proxy_url.empty()) return options.proxy_url;
    return default_network_route().proxy_url;
}

std::string route_interface(const HttpClientOptions &options) {
    if (!options.interface_name.empty()) return options.interface_name;
    return default_network_route().interface_name;
}

}  // namespace

HttpClient::HttpClient(HttpClientOptions options) : options_(std::move(options)) {
    ensure_curl_global();
}

HttpClient::~HttpClient() {
    std::lock_guard lock(mutex_);
    if (easy_) curl_easy_cleanup(static_cast<CURL *>(easy_));
}

bool HttpClient::ensure_easy() {
    if (easy_) return true;
    easy_ = curl_easy_init();
    return easy_ != nullptr;
}

HttpResponse HttpClient::request(const HttpRequest &request) {
    std::lock_guard lock(mutex_);
    HttpResponse response;
    if (!ensure_easy()) {
        response.transport_error = "curl_easy_init failed";
        return response;
    }
    CURL *easy = static_cast<CURL *>(easy_);
    curl_easy_reset(easy);

    curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, static_cast<long>(options_.timeout_ms));
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options_.connect_timeout_ms));
    curl_easy_setopt(easy, CURLOPT_DNS_CACHE_TIMEOUT, static_cast<long>(options_.dns_cache_timeout_seconds));
    curl_easy_setopt(easy, CURLOPT_USERAGENT, options_.user_agent.c_str());
    curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(easy, CURLOPT_TCP_KEEPALIVE, 1L);
    if (options_.tcp_nodelay) curl_easy_setopt(easy, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &response.headers);

    const std::string proxy = route_proxy(options_);
    const std::string iface = route_interface(options_);
    if (!proxy.empty()) curl_easy_setopt(easy, CURLOPT_PROXY, proxy.c_str());
    if (!iface.empty()) curl_easy_setopt(easy, CURLOPT_INTERFACE, iface.c_str());

    curl_slist *headers = nullptr;
    bool has_content_type = false;
    for (const auto &header : request.headers) {
        const std::string line = header.first + ": " + header.second;
        headers = curl_slist_append(headers, line.c_str());
        if (header.first == "Content-Type" || header.first == "content-type") has_content_type = true;
    }
    if (!request.body.empty() && !has_content_type) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);

    if (request.method == "POST") {
        curl_easy_setopt(easy, CURLOPT_POST, 1L);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    } else if (request.method == "GET") {
        curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, request.method.c_str());
        if (!request.body.empty()) {
            curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        }
    }

    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errbuf);
    const CURLcode rc = curl_easy_perform(easy);
    curl_slist_free_all(headers);
    if (rc != CURLE_OK) {
        response.transport_error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
        return response;
    }
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &response.status);
    return response;
}

}  // namespace limitless
