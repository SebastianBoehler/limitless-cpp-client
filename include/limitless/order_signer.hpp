#pragma once

#include "limitless/types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace limitless
{
    std::string to_hex(const std::uint8_t *data, std::size_t size);
    std::string to_checksum_address(std::string_view address);
    bool is_checksum_address(std::string_view address);
    std::array<std::uint8_t, 32> keccak256(const std::uint8_t *data, std::size_t size);
    std::array<std::uint8_t, 32> keccak256(std::string_view data);
    std::array<std::uint8_t, 32> uint256_from_decimal(std::string_view decimal);

    struct OrderDraft
    {
        std::string salt;
        std::string maker;
        std::string signer;
        std::string taker{kZeroAddress};
        std::string token_id;
        std::string maker_amount;
        std::string taker_amount;
        std::string expiration{"0"};
        std::string nonce{"0"};
        std::uint64_t fee_rate_bps{0};
        Side side{Side::Buy};
        // 0 is the EOA type named in the signing guide. The API schema also accepts 1, 2, and 3.
        std::uint8_t signature_type{0};
    };

    struct SignedOrder
    {
        OrderDraft order;
        std::string signature;
        std::array<std::uint8_t, 32> digest{};
    };

    class OrderSigner
    {
    public:
        explicit OrderSigner(std::string private_key_hex, int chain_id = kBaseChainId);
        ~OrderSigner();

        OrderSigner(OrderSigner &&) noexcept;
        OrderSigner &operator=(OrderSigner &&) noexcept;
        OrderSigner(const OrderSigner &) = delete;
        OrderSigner &operator=(const OrderSigner &) = delete;

        const std::string &address() const;
        int chain_id() const;

        std::array<std::uint8_t, 32> domain_separator(std::string_view verifying_contract) const;
        std::array<std::uint8_t, 32> order_digest(const OrderDraft &order,
                                                  std::string_view verifying_contract) const;
        SignedOrder sign_order(const OrderDraft &order, std::string_view verifying_contract) const;

        static std::string generate_salt();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
