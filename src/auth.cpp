#include "limitless/auth.hpp"

#include <openssl/evp.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace limitless
{
    namespace
    {
        std::vector<unsigned char> base64_decode(std::string_view text)
        {
            std::string filtered;
            filtered.reserve(text.size());
            for (char character : text)
            {
                if (character != ' ' && character != '\n' && character != '\r' && character != '\t')
                {
                    filtered.push_back(character);
                }
            }
            if (filtered.empty() || filtered.size() % 4 != 0)
            {
                throw std::invalid_argument("HMAC secret is not valid base64");
            }
            std::vector<unsigned char> out((filtered.size() / 4) * 3);
            const int length = EVP_DecodeBlock(out.data(),
                                               reinterpret_cast<const unsigned char *>(filtered.data()),
                                               static_cast<int>(filtered.size()));
            if (length < 0)
            {
                throw std::invalid_argument("HMAC secret is not valid base64");
            }
            int padding = 0;
            if (filtered.back() == '=')
            {
                ++padding;
            }
            if (filtered.size() > 1 && filtered[filtered.size() - 2] == '=')
            {
                ++padding;
            }
            out.resize(static_cast<std::size_t>(length - padding));
            return out;
        }

        std::string base64_encode(const unsigned char *data, std::size_t size)
        {
            std::string out(4 * ((size + 2) / 3), '\0');
            const int length = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()), data, static_cast<int>(size));
            out.resize(static_cast<std::size_t>(length));
            return out;
        }

        std::string hmac_sha256_base64(const std::vector<unsigned char> &key, std::string_view message)
        {
            EVP_MAC *mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
            if (!mac)
            {
                throw std::runtime_error("OpenSSL HMAC is unavailable");
            }
            EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
            EVP_MAC_free(mac);
            if (!ctx)
            {
                throw std::runtime_error("OpenSSL HMAC context failed");
            }
            char digest_name[] = "SHA256";
            OSSL_PARAM params[] = {
                OSSL_PARAM_construct_utf8_string("digest", digest_name, 0),
                OSSL_PARAM_construct_end()};
            const bool ok = EVP_MAC_init(ctx, key.data(), key.size(), params) == 1 &&
                            EVP_MAC_update(ctx, reinterpret_cast<const unsigned char *>(message.data()), message.size()) == 1;
            unsigned char out[EVP_MAX_MD_SIZE];
            std::size_t out_len = 0;
            const bool final_ok = ok && EVP_MAC_final(ctx, out, &out_len, sizeof(out)) == 1;
            EVP_MAC_CTX_free(ctx);
            if (!final_ok)
            {
                throw std::runtime_error("HMAC-SHA256 failed");
            }
            return base64_encode(out, out_len);
        }

        std::string upper_method(std::string_view method)
        {
            std::string out(method);
            for (char &character : out)
            {
                if (character >= 'a' && character <= 'z')
                {
                    character = static_cast<char>(character - 'a' + 'A');
                }
            }
            return out;
        }
    }

    std::vector<std::pair<std::string, std::string>> HmacHeaders::fields() const
    {
        return {{"lmts-api-key", api_key},
                {"lmts-timestamp", timestamp},
                {"lmts-signature", signature}};
    }

    std::string hmac_canonical_message(std::string_view timestamp,
                                       std::string_view method,
                                       std::string_view path_and_query,
                                       std::string_view body)
    {
        std::string message;
        message.reserve(timestamp.size() + method.size() + path_and_query.size() + body.size() + 3);
        message.append(timestamp);
        message.push_back('\n');
        message.append(upper_method(method));
        message.push_back('\n');
        message.append(path_and_query);
        message.push_back('\n');
        message.append(body);
        return message;
    }

    HmacHeaders sign_request(const HmacCredentials &credentials,
                             std::string_view method,
                             std::string_view path_and_query,
                             std::string_view body,
                             std::string_view timestamp)
    {
        if (credentials.token_id.empty())
        {
            throw std::invalid_argument("lmts-api-key token id is empty");
        }
        if (path_and_query.empty() || path_and_query.front() != '/')
        {
            throw std::invalid_argument("HMAC path must start with /");
        }
        const auto key = base64_decode(credentials.secret_base64);
        const std::string message = hmac_canonical_message(timestamp, method, path_and_query, body);
        HmacHeaders headers;
        headers.api_key = credentials.token_id;
        headers.timestamp = std::string(timestamp);
        headers.signature = hmac_sha256_base64(key, message);
        return headers;
    }

    HmacHeaders sign_request_now(const HmacCredentials &credentials,
                                 std::string_view method,
                                 std::string_view path_and_query,
                                 std::string_view body)
    {
        return sign_request(credentials, method, path_and_query, body, iso8601_utc_millis_now());
    }

    HmacHeaders sign_websocket_handshake(const HmacCredentials &credentials, std::string_view timestamp)
    {
        return sign_request(credentials, "GET", endpoints::socket_io_handshake_path, "", timestamp);
    }

    HmacHeaders sign_websocket_handshake_now(const HmacCredentials &credentials)
    {
        return sign_websocket_handshake(credentials, iso8601_utc_millis_now());
    }

    std::string iso8601_utc_millis(std::int64_t unix_millis)
    {
        const std::time_t seconds = static_cast<std::time_t>(unix_millis / 1000);
        const int millis = static_cast<int>(unix_millis % 1000);
        std::tm utc{};
        gmtime_r(&seconds, &utc);
        std::ostringstream stream;
        stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
               << '.' << std::setw(3) << std::setfill('0') << millis << 'Z';
        return stream.str();
    }

    std::string iso8601_utc_millis_now()
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return iso8601_utc_millis(static_cast<std::int64_t>(millis));
    }
}
