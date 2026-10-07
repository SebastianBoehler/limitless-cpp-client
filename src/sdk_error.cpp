#include "limitless/sdk_error.hpp"

#include <nlohmann/json.hpp>

namespace limitless
{
    std::string sdk_error_code_to_string(SdkErrorCode code)
    {
        switch (code)
        {
        case SdkErrorCode::HttpTransport:
            return "http_transport";
        case SdkErrorCode::ApiResponse:
            return "api_response";
        case SdkErrorCode::Auth:
            return "auth";
        case SdkErrorCode::RateLimit:
            return "rate_limit";
        case SdkErrorCode::Parse:
            return "parse";
        case SdkErrorCode::Signing:
            return "signing";
        case SdkErrorCode::InvalidArgument:
            return "invalid_argument";
        }
        return "api_response";
    }

    SdkError make_transport_error(const std::string &message, const std::string &endpoint)
    {
        SdkError error;
        error.code = SdkErrorCode::HttpTransport;
        error.message = message.empty() ? "HTTP request failed" : message;
        error.endpoint = endpoint;
        error.retryable = true;
        return error;
    }

    namespace
    {
        std::string excerpt(const std::string &body)
        {
            constexpr std::size_t kMax = 512;
            if (body.size() <= kMax)
            {
                return body;
            }
            return body.substr(0, kMax);
        }

        std::string message_from_body(const std::string &body, long status)
        {
            if (body.empty())
            {
                return "HTTP " + std::to_string(status);
            }
            const auto parsed = nlohmann::json::parse(body, nullptr, false);
            if (parsed.is_discarded() || !parsed.is_object() || !parsed.contains("message"))
            {
                return excerpt(body);
            }
            const auto &message = parsed["message"];
            if (message.is_string())
            {
                return message.get<std::string>();
            }
            return message.dump();
        }
    }

    SdkError make_http_error(long status,
                             const std::string &body,
                             const std::string &endpoint,
                             const std::string &retry_after)
    {
        SdkError error;
        error.http_status = status;
        error.endpoint = endpoint;
        error.response_body_excerpt = excerpt(body);
        error.message = message_from_body(body, status);
        if (status == 401 || status == 403)
        {
            error.code = SdkErrorCode::Auth;
        }
        else if (status == 429)
        {
            error.code = SdkErrorCode::RateLimit;
            error.retryable = true;
            if (!retry_after.empty())
            {
                error.message += " (Retry-After: " + retry_after + ")";
            }
        }
        else
        {
            error.code = SdkErrorCode::ApiResponse;
            error.retryable = status >= 500;
        }
        return error;
    }

    SdkError make_parse_error(const std::string &message,
                              const std::string &endpoint,
                              const std::string &body)
    {
        SdkError error;
        error.code = SdkErrorCode::Parse;
        error.message = message;
        error.endpoint = endpoint;
        error.response_body_excerpt = excerpt(body);
        return error;
    }

    SdkError make_invalid_argument(const std::string &message)
    {
        SdkError error;
        error.code = SdkErrorCode::InvalidArgument;
        error.message = message;
        return error;
    }

    SdkError make_signing_error(const std::string &message)
    {
        SdkError error;
        error.code = SdkErrorCode::Signing;
        error.message = message;
        return error;
    }
}
