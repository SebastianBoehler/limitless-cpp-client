#pragma once

#include "limitless/sdk_error.hpp"
#include "limitless/types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace limitless {

std::array<std::uint8_t, 32> keccak256(const std::uint8_t *data, std::size_t len);
std::array<std::uint8_t, 32> keccak256(std::string_view data);

std::string to_hex(const std::uint8_t *data, std::size_t len, bool prefix = true);
bool decode_hex(std::string_view hex, std::string &error, std::vector<std::uint8_t> &out);
std::string to_checksum_address(std::string_view address_or_20_bytes_hex);

class OrderSigner {
public:
    explicit OrderSigner(std::string private_key_hex);
    ~OrderSigner();

    OrderSigner(const OrderSigner &) = delete;
    OrderSigner &operator=(const OrderSigner &) = delete;
    OrderSigner(OrderSigner &&) noexcept;
    OrderSigner &operator=(OrderSigner &&) noexcept;

    const std::string &address() const { return address_; }

    Result<std::array<std::uint8_t, 32>> order_digest(const Eip712Domain &domain,
                                                      const UnsignedOrder &order) const;

    Result<SignedOrder> sign_order(const Eip712Domain &domain, UnsignedOrder order,
                                   std::string price = {}) const;

private:
    std::array<std::uint8_t, 32> private_key_{};
    std::string address_;
    bool ok_ = false;
    std::string init_error_;
};

std::string next_order_salt();

}  // namespace limitless
