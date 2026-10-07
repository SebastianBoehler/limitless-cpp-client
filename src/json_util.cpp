#include "limitless/json_util.hpp"

namespace limitless {

std::string quote_big_integers(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 16);
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = 0; i < in.size();) {
        const char c = in[i];
        if (in_string) {
            out.push_back(c);
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            ++i;
            continue;
        }
        if (c == '"') {
            in_string = true;
            out.push_back(c);
            ++i;
            continue;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            std::size_t j = i;
            if (in[j] == '-') ++j;
            const std::size_t digits_at = j;
            while (j < in.size() && in[j] >= '0' && in[j] <= '9') ++j;
            const bool fractional = j < in.size() && (in[j] == '.' || in[j] == 'e' || in[j] == 'E');
            const std::size_t digits = j - digits_at;
            if (!fractional && digits > 15) {
                out.push_back('"');
                out.append(in.substr(i, j - i));
                out.push_back('"');
            } else {
                out.append(in.substr(i, j - i));
            }
            i = j;
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

nlohmann::json parse_json(std::string_view json_text) {
    return nlohmann::json::parse(quote_big_integers(json_text));
}

}  // namespace limitless
