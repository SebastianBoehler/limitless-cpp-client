#include "limitless/sdk_error.hpp"

namespace limitless {

const char *sdk_error_code_to_string(SdkErrorCode code) {
    switch (code) {
        case SdkErrorCode::Transport:
            return "transport";
        case SdkErrorCode::Api:
            return "api";
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
        case SdkErrorCode::Unavailable:
            return "unavailable";
    }
    return "api";
}

}  // namespace limitless
