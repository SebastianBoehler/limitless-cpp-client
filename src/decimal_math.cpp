#include "limitless/decimal_math.hpp"

#include <cctype>
#include <vector>

namespace limitless {
namespace {

bool all_digits(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

std::string strip_zeros(std::string s) {
    std::size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return s.substr(i);
}

std::string mul_int(std::string_view a, std::string_view b) {
    if (a == "0" || b == "0" || a.empty() || b.empty()) return "0";
    std::vector<int> acc(a.size() + b.size(), 0);
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        const int da = a[static_cast<std::size_t>(i)] - '0';
        for (int j = static_cast<int>(b.size()) - 1; j >= 0; --j) {
            acc[static_cast<std::size_t>(i + j + 1)] += da * (b[static_cast<std::size_t>(j)] - '0');
        }
    }
    for (int k = static_cast<int>(acc.size()) - 1; k > 0; --k) {
        acc[static_cast<std::size_t>(k - 1)] += acc[static_cast<std::size_t>(k)] / 10;
        acc[static_cast<std::size_t>(k)] %= 10;
    }
    std::string out;
    std::size_t i = 0;
    while (i + 1 < acc.size() && acc[i] == 0) ++i;
    for (; i < acc.size(); ++i) out.push_back(static_cast<char>('0' + acc[i]));
    return out;
}

bool div_exact(std::string_view n, int divisor, std::string &quotient) {
    int rem = 0;
    std::string out;
    out.reserve(n.size());
    for (char c : n) {
        const int cur = rem * 10 + (c - '0');
        out.push_back(static_cast<char>('0' + cur / divisor));
        rem = cur % divisor;
    }
    if (rem != 0) return false;
    quotient = strip_zeros(out.empty() ? "0" : out);
    return true;
}

int cmp_int(std::string_view left, std::string_view right) {
    const std::string a = strip_zeros(std::string(left));
    const std::string b = strip_zeros(std::string(right));
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    if (a == b) return 0;
    return a < b ? -1 : 1;
}

std::string price_from_millis(std::string millis) {
    while (millis.size() < 3) millis.insert(millis.begin(), '0');
    std::string ip = millis.substr(0, millis.size() - 3);
    std::string fp = millis.substr(millis.size() - 3);
    if (ip.empty()) ip = "0";
    while (fp.size() > 1 && fp.back() == '0') fp.pop_back();
    return ip + "." + fp;
}

}  // namespace

bool scale_decimal(std::string_view human, int scale, std::string &out, std::string &error,
                   bool allow_zero) {
    if (scale < 0) {
        error = "invalid scale";
        return false;
    }
    std::string s(human);
    if (!s.empty() && s.front() == '+') s.erase(s.begin());
    if (s.empty() || s.find('-') != std::string::npos) {
        error = "amount must be a positive decimal";
        return false;
    }
    const auto dot = s.find('.');
    if (s.find('.') != dot) {
        error = "amount must be a positive decimal";
        return false;
    }
    std::string ip = dot == std::string::npos ? s : s.substr(0, dot);
    std::string fp = dot == std::string::npos ? std::string() : s.substr(dot + 1);
    if (ip.empty()) ip = "0";
    if (!all_digits(ip) || (!fp.empty() && !all_digits(fp))) {
        error = "amount must be a positive decimal";
        return false;
    }
    if (static_cast<int>(fp.size()) > scale) {
        error = "too many decimal places";
        return false;
    }
    fp.append(static_cast<std::size_t>(scale) - fp.size(), '0');
    out = strip_zeros(ip + fp);
    if (out == "0" && !allow_zero) {
        error = "amount must be positive";
        return false;
    }
    return true;
}

bool gtc_amounts(OrderSide side, std::string_view price, std::string_view size_shares,
                 ScaledAmounts &out, std::string &error) {
    std::string millis;
    std::string size_raw;
    if (!scale_decimal(price, 3, millis, error)) return false;
    if (!scale_decimal(size_shares, 6, size_raw, error)) return false;
    if (cmp_int(millis, "10") < 0 || cmp_int(millis, "990") > 0) {
        error = "price must be between 0.01 and 0.99 with at most 3 decimal places";
        return false;
    }
    std::string collateral;
    if (!div_exact(mul_int(millis, size_raw), 1000, collateral)) {
        error = "price * size is not an exact 6-decimal raw amount";
        return false;
    }
    if (side == OrderSide::Buy) {
        out.maker_amount = collateral;
        out.taker_amount = size_raw;
    } else {
        out.maker_amount = size_raw;
        out.taker_amount = collateral;
    }
    out.price = price_from_millis(millis);
    return true;
}

bool fok_amounts(OrderSide side, std::string_view human_amount, ScaledAmounts &out,
                 std::string &error) {
    std::string raw;
    if (!scale_decimal(human_amount, 6, raw, error)) return false;
    out.maker_amount = raw;
    out.taker_amount = "1";
    out.price.clear();
    (void)side;
    return true;
}

}  // namespace limitless
