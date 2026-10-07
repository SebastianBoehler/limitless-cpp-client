#pragma once

#include <optional>
#include <string>
#include <utility>

namespace limitless {

enum class SdkErrorCode {
    Transport,
    Api,
    Auth,
    RateLimit,
    Parse,
    Signing,
    InvalidArgument,
    Unavailable,
};

struct SdkError {
    SdkErrorCode code = SdkErrorCode::Api;
    int http_status = 0;
    std::string message;
    std::string endpoint;
    std::string response_excerpt;
    bool retryable = false;
};

const char *sdk_error_code_to_string(SdkErrorCode code);

template <typename T>
class Result {
public:
    static Result success(T value) {
        Result out;
        out.value_ = std::move(value);
        return out;
    }

    static Result failure(SdkError error) {
        Result out;
        out.error_ = std::move(error);
        return out;
    }

    explicit operator bool() const { return value_.has_value(); }
    bool ok() const { return value_.has_value(); }

    T &value() & { return *value_; }
    const T &value() const & { return *value_; }
    T &&value() && { return std::move(*value_); }

    const SdkError &error() const { return *error_; }

private:
    std::optional<T> value_;
    std::optional<SdkError> error_;
};

}  // namespace limitless
