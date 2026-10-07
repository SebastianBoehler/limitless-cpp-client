#include "limitless/order_signer.hpp"

#include <ethash/keccak.hpp>
#include <openssl/crypto.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

namespace limitless
{
    namespace
    {
        int hex_nibble(char character)
        {
            if (character >= '0' && character <= '9')
            {
                return character - '0';
            }
            if (character >= 'a' && character <= 'f')
            {
                return character - 'a' + 10;
            }
            if (character >= 'A' && character <= 'F')
            {
                return character - 'A' + 10;
            }
            throw std::invalid_argument("invalid hex");
        }

        std::vector<std::uint8_t> decode_hex(std::string_view hex)
        {
            if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
            {
                hex.remove_prefix(2);
            }
            if (hex.size() % 2 != 0)
            {
                throw std::invalid_argument("hex length must be even");
            }
            std::vector<std::uint8_t> out(hex.size() / 2);
            for (std::size_t index = 0; index < out.size(); ++index)
            {
                out[index] = static_cast<std::uint8_t>((hex_nibble(hex[index * 2]) << 4) |
                                                       hex_nibble(hex[index * 2 + 1]));
            }
            return out;
        }

        std::string hex_lower(const std::uint8_t *data, std::size_t size)
        {
            static constexpr char kDigits[] = "0123456789abcdef";
            std::string out(size * 2, '0');
            for (std::size_t index = 0; index < size; ++index)
            {
                out[index * 2] = kDigits[data[index] >> 4];
                out[index * 2 + 1] = kDigits[data[index] & 0x0f];
            }
            return out;
        }

        void append_word(std::vector<std::uint8_t> &out, const std::array<std::uint8_t, 32> &word)
        {
            out.insert(out.end(), word.begin(), word.end());
        }

        std::array<std::uint8_t, 32> word_uint256(std::uint64_t value)
        {
            std::array<std::uint8_t, 32> word{};
            for (int index = 0; index < 8; ++index)
            {
                word[31 - index] = static_cast<std::uint8_t>(value & 0xffu);
                value >>= 8;
            }
            return word;
        }

        std::array<std::uint8_t, 32> address_word(std::string_view address)
        {
            const auto bytes = decode_hex(address);
            if (bytes.size() != 20)
            {
                throw std::invalid_argument("address must be 20 bytes");
            }
            std::array<std::uint8_t, 32> word{};
            std::memcpy(word.data() + 12, bytes.data(), 20);
            return word;
        }

        const std::array<std::uint8_t, 32> &domain_typehash()
        {
            static const auto hash = keccak256(
                "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)");
            return hash;
        }

        const std::array<std::uint8_t, 32> &order_typehash()
        {
            static const auto hash = keccak256(
                "Order(uint256 salt,address maker,address signer,address taker,uint256 tokenId,"
                "uint256 makerAmount,uint256 takerAmount,uint256 expiration,uint256 nonce,"
                "uint256 feeRateBps,uint8 side,uint8 signatureType)");
            return hash;
        }

        struct ContextDeleter
        {
            void operator()(secp256k1_context *context) const noexcept
            {
                if (context)
                {
                    secp256k1_context_destroy(context);
                }
            }
        };
    }

    std::string to_hex(const std::uint8_t *data, std::size_t size)
    {
        return "0x" + hex_lower(data, size);
    }

    std::array<std::uint8_t, 32> keccak256(const std::uint8_t *data, std::size_t size)
    {
        const auto hash = ethash::keccak256(data, size);
        std::array<std::uint8_t, 32> out{};
        std::memcpy(out.data(), hash.bytes, 32);
        return out;
    }

    std::array<std::uint8_t, 32> keccak256(std::string_view data)
    {
        return keccak256(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
    }

    std::array<std::uint8_t, 32> uint256_from_decimal(std::string_view decimal)
    {
        if (decimal.empty())
        {
            throw std::invalid_argument("uint256 decimal is empty");
        }
        std::array<std::uint8_t, 32> out{};
        for (char character : decimal)
        {
            if (character < '0' || character > '9')
            {
                throw std::invalid_argument("uint256 decimal must contain digits");
            }
            int carry = character - '0';
            for (int index = 31; index >= 0; --index)
            {
                const int value = static_cast<int>(out[static_cast<std::size_t>(index)]) * 10 + carry;
                out[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(value & 0xff);
                carry = value >> 8;
            }
            if (carry != 0)
            {
                throw std::invalid_argument("uint256 decimal overflows");
            }
        }
        return out;
    }

    std::string to_checksum_address(std::string_view address)
    {
        auto bytes = decode_hex(address);
        if (bytes.size() != 20)
        {
            throw std::invalid_argument("address must be 20 bytes");
        }
        const std::string lower = hex_lower(bytes.data(), bytes.size());
        const auto hash = keccak256(lower);
        const std::string hash_hex = hex_lower(hash.data(), hash.size());
        std::string out = "0x";
        out.reserve(42);
        for (std::size_t index = 0; index < lower.size(); ++index)
        {
            char character = lower[index];
            const int nibble = hex_nibble(hash_hex[index]);
            if (character >= 'a' && character <= 'f' && nibble >= 8)
            {
                character = static_cast<char>(character - 'a' + 'A');
            }
            out.push_back(character);
        }
        return out;
    }

    bool is_checksum_address(std::string_view address)
    {
        try
        {
            return to_checksum_address(address) == address;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    struct OrderSigner::Impl
    {
        std::array<std::uint8_t, 32> private_key{};
        std::string address;
        int chain_id{kBaseChainId};
        std::unique_ptr<secp256k1_context, ContextDeleter> context;

        ~Impl()
        {
            OPENSSL_cleanse(private_key.data(), private_key.size());
        }
    };

    OrderSigner::OrderSigner(std::string private_key_hex, int chain_id)
        : impl_(std::make_unique<Impl>())
    {
        auto key = decode_hex(private_key_hex);
        if (key.size() != 32)
        {
            OPENSSL_cleanse(key.data(), key.size());
            throw std::invalid_argument("private key must be 32 bytes");
        }
        std::memcpy(impl_->private_key.data(), key.data(), 32);
        OPENSSL_cleanse(key.data(), key.size());
        impl_->chain_id = chain_id;
        impl_->context.reset(secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY));
        if (!impl_->context)
        {
            throw std::runtime_error("secp256k1 context failed");
        }
        if (secp256k1_ec_seckey_verify(impl_->context.get(), impl_->private_key.data()) != 1)
        {
            throw std::invalid_argument("private key is out of range");
        }
        secp256k1_pubkey pubkey;
        if (secp256k1_ec_pubkey_create(impl_->context.get(), &pubkey, impl_->private_key.data()) != 1)
        {
            throw std::runtime_error("public key derivation failed");
        }
        std::uint8_t uncompressed[65];
        std::size_t length = sizeof(uncompressed);
        secp256k1_ec_pubkey_serialize(impl_->context.get(),
                                      uncompressed,
                                      &length,
                                      &pubkey,
                                      SECP256K1_EC_UNCOMPRESSED);
        const auto hash = keccak256(uncompressed + 1, 64);
        impl_->address = to_checksum_address(to_hex(hash.data() + 12, 20));
    }

    OrderSigner::~OrderSigner() = default;
    OrderSigner::OrderSigner(OrderSigner &&) noexcept = default;
    OrderSigner &OrderSigner::operator=(OrderSigner &&) noexcept = default;

    const std::string &OrderSigner::address() const
    {
        return impl_->address;
    }

    int OrderSigner::chain_id() const
    {
        return impl_->chain_id;
    }

    std::string OrderSigner::generate_salt()
    {
        thread_local std::mt19937_64 generator{std::random_device{}()};
        std::uniform_int_distribution<std::uint64_t> distribution;
        return std::to_string(distribution(generator));
    }

    std::array<std::uint8_t, 32> OrderSigner::domain_separator(std::string_view verifying_contract) const
    {
        std::vector<std::uint8_t> encoded;
        encoded.reserve(5 * 32);
        append_word(encoded, domain_typehash());
        append_word(encoded, keccak256(kExchangeDomainName));
        append_word(encoded, keccak256(kExchangeDomainVersion));
        append_word(encoded, word_uint256(static_cast<std::uint64_t>(impl_->chain_id)));
        append_word(encoded, address_word(verifying_contract));
        return keccak256(encoded.data(), encoded.size());
    }

    std::array<std::uint8_t, 32> OrderSigner::order_digest(const OrderDraft &order,
                                                          std::string_view verifying_contract) const
    {
        if (order.expiration != "0" || order.nonce != "0")
        {
            throw std::invalid_argument("expiration and nonce must be 0");
        }
        if (order.signature_type > 3)
        {
            throw std::invalid_argument("signatureType must be 0 through 3");
        }
        const int side = static_cast<int>(order.side);
        std::vector<std::uint8_t> encoded;
        encoded.reserve(13 * 32);
        append_word(encoded, order_typehash());
        append_word(encoded, uint256_from_decimal(order.salt));
        append_word(encoded, address_word(order.maker));
        append_word(encoded, address_word(order.signer));
        append_word(encoded, address_word(order.taker.empty() ? kZeroAddress : order.taker));
        append_word(encoded, uint256_from_decimal(order.token_id));
        append_word(encoded, uint256_from_decimal(order.maker_amount));
        append_word(encoded, uint256_from_decimal(order.taker_amount));
        append_word(encoded, uint256_from_decimal(order.expiration));
        append_word(encoded, uint256_from_decimal(order.nonce));
        append_word(encoded, word_uint256(order.fee_rate_bps));
        append_word(encoded, word_uint256(static_cast<std::uint64_t>(side)));
        append_word(encoded, word_uint256(order.signature_type));
        const auto structure = keccak256(encoded.data(), encoded.size());
        const auto domain = domain_separator(verifying_contract);
        std::uint8_t preimage[66];
        preimage[0] = 0x19;
        preimage[1] = 0x01;
        std::memcpy(preimage + 2, domain.data(), 32);
        std::memcpy(preimage + 34, structure.data(), 32);
        return keccak256(preimage, sizeof(preimage));
    }

    SignedOrder OrderSigner::sign_order(const OrderDraft &draft, std::string_view verifying_contract) const
    {
        OrderDraft order = draft;
        order.maker = to_checksum_address(order.maker.empty() ? impl_->address : order.maker);
        order.signer = to_checksum_address(order.signer.empty() ? impl_->address : order.signer);
        order.taker = to_checksum_address(order.taker.empty() ? kZeroAddress : order.taker);
        if (order.salt.empty())
        {
            order.salt = generate_salt();
        }
        SignedOrder signed_order;
        signed_order.order = std::move(order);
        signed_order.digest = order_digest(signed_order.order, verifying_contract);
        secp256k1_ecdsa_recoverable_signature signature;
        if (secp256k1_ecdsa_sign_recoverable(impl_->context.get(),
                                             &signature,
                                             signed_order.digest.data(),
                                             impl_->private_key.data(),
                                             nullptr,
                                             nullptr) != 1)
        {
            throw std::runtime_error("ECDSA sign failed");
        }
        std::uint8_t compact[64];
        int recovery = 0;
        secp256k1_ecdsa_recoverable_signature_serialize_compact(impl_->context.get(),
                                                                compact,
                                                                &recovery,
                                                                &signature);
        std::uint8_t packed[65];
        std::memcpy(packed, compact, 64);
        packed[64] = static_cast<std::uint8_t>(recovery + 27);
        signed_order.signature = to_hex(packed, sizeof(packed));
        return signed_order;
    }
}
