#include "limitless/hmac_auth.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <vector>

namespace limitless {
namespace {

std::string base64_decode(std::string_view in) {
    std::string filtered;
    filtered.reserve(in.size());
    for (char c : in) {
        if (c == '\n' || c == '\r' || c == ' ') continue;
        filtered.push_back(c);
    }
    std::vector<unsigned char> out(filtered.size());
    const int n = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char *>(filtered.data()),
                                  static_cast<int>(filtered.size()));
    if (n < 0) throw std::runtime_error("invalid base64 HMAC secret");
    int pad = 0;
    if (!filtered.empty() && filtered.back() == '=') ++pad;
    if (filtered.size() > 1 && filtered[filtered.size() - 2] == '=') ++pad;
    const int len = n - pad;
    return std::string(reinterpret_cast<char *>(out.data()), len < 0 ? 0 : len);
}

std::string base64_encode(const unsigned char *data, std::size_t len) {
    std::string out(((len + 2) / 3) * 4, '\0');
    const int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()), data,
                                  static_cast<int>(len));
    if (n < 0) throw std::runtime_error("base64 encode failed");
    out.resize(static_cast<std::size_t>(n));
    return out;
}

}  // namespace

std::string iso8601_millis_now() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto sec = clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    gmtime_r(&sec, &tm);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

HmacHeaders sign_request(const HmacCredentials &creds, std::string_view method,
                         std::string_view path_and_query, std::string_view body,
                         std::string_view timestamp_in) {
    const std::string timestamp = timestamp_in.empty() ? iso8601_millis_now() : std::string(timestamp_in);
    const std::string message = timestamp + "\n" + std::string(method) + "\n" +
                                std::string(path_and_query) + "\n" + std::string(body);
    const std::string key = base64_decode(creds.secret_base64);
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int mac_len = 0;
    if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char *>(message.data()), message.size(), mac,
             &mac_len) == nullptr) {
        throw std::runtime_error("HMAC-SHA256 failed");
    }
    HmacHeaders headers;
    headers.timestamp = timestamp;
    headers.signature = base64_encode(mac, mac_len);
    headers.headers = {{"lmts-api-key", creds.token_id},
                       {"lmts-timestamp", headers.timestamp},
                       {"lmts-signature", headers.signature}};
    return headers;
}

HmacHeaders sign_websocket_handshake(const HmacCredentials &creds, std::string_view socketio_path,
                                     std::string_view timestamp) {
    return sign_request(creds, "GET", socketio_path, "", timestamp);
}

}  // namespace limitless
