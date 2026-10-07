#include "limitless/order_signer.hpp"

#define OPENSSL_SUPPRESS_DEPRECATED 1

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/hmac.h>
#include <openssl/obj_mac.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace limitless {
namespace {

SdkError sign_error(std::string message) {
    SdkError error;
    error.code = SdkErrorCode::Signing;
    error.message = std::move(message);
    return error;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string strip_int_zeros(std::string s) {
    std::size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return s.substr(i);
}

bool decimal_to_be32(std::string_view dec, std::array<std::uint8_t, 32> &out, std::string &error) {
    out.fill(0);
    if (dec.empty()) {
        error = "missing integer";
        return false;
    }
    std::vector<std::uint8_t> le{0};
    for (char c : dec) {
        if (c < '0' || c > '9') {
            error = "integer field is not decimal";
            return false;
        }
        int carry = c - '0';
        for (auto &byte : le) {
            const int value = byte * 10 + carry;
            byte = static_cast<std::uint8_t>(value & 0xff);
            carry = value >> 8;
        }
        while (carry > 0) {
            le.push_back(static_cast<std::uint8_t>(carry & 0xff));
            carry >>= 8;
        }
    }
    if (le.size() > 32) {
        error = "integer does not fit in uint256";
        return false;
    }
    for (std::size_t i = 0; i < le.size(); ++i) out[31 - i] = le[i];
    return true;
}

void append_word(std::vector<std::uint8_t> &buf, const std::array<std::uint8_t, 32> &word) {
    buf.insert(buf.end(), word.begin(), word.end());
}

void append_hash(std::vector<std::uint8_t> &buf, std::string_view text) {
    const auto hash = keccak256(text);
    buf.insert(buf.end(), hash.begin(), hash.end());
}

bool address_word(std::string_view address, std::array<std::uint8_t, 32> &word, std::string &error) {
    std::vector<std::uint8_t> raw;
    if (!decode_hex(address, error, raw) || raw.size() != 20) {
        error = "address must be 20 bytes";
        return false;
    }
    word.fill(0);
    std::memcpy(word.data() + 12, raw.data(), 20);
    return true;
}

struct BnFree {
    void operator()(BIGNUM *p) const { BN_free(p); }
};
struct EcGroupFree {
    void operator()(EC_GROUP *p) const { EC_GROUP_free(p); }
};
struct EcPointFree {
    void operator()(EC_POINT *p) const { EC_POINT_free(p); }
};
struct BnCtxFree {
    void operator()(BN_CTX *p) const { BN_CTX_free(p); }
};

using Bn = std::unique_ptr<BIGNUM, BnFree>;
using Group = std::unique_ptr<EC_GROUP, EcGroupFree>;
using Point = std::unique_ptr<EC_POINT, EcPointFree>;

bool bn_to_32(const BIGNUM *n, std::uint8_t out[32]) {
    std::memset(out, 0, 32);
    const int len = BN_num_bytes(n);
    if (len > 32) return false;
    BN_bn2bin(n, out + (32 - len));
    return true;
}

std::array<std::uint8_t, 32> hmac_sha256(const std::uint8_t *key, std::size_t key_len, const std::uint8_t *data,
                                        std::size_t data_len) {
    std::array<std::uint8_t, 32> out{};
    unsigned int out_len = 32;
    HMAC(EVP_sha256(), key, static_cast<int>(key_len), data, data_len, out.data(), &out_len);
    return out;
}

// RFC 6979 deterministic nonce for a 256-bit curve order and a 32-byte digest.
Bn rfc6979_k(const BIGNUM *priv, const BIGNUM *order, const std::uint8_t hash[32], BN_CTX *ctx,
             int attempt) {
    std::uint8_t bx[32];
    std::uint8_t bh[32];
    bn_to_32(priv, bx);
    Bn hint(BN_bin2bn(hash, 32, nullptr));
    if (BN_cmp(hint.get(), order) >= 0) BN_sub(hint.get(), hint.get(), order);
    bn_to_32(hint.get(), bh);

    std::uint8_t V[32];
    std::uint8_t K[32];
    std::memset(V, 0x01, sizeof(V));
    std::memset(K, 0x00, sizeof(K));

    std::uint8_t step[32 + 1 + 32 + 32];
    std::memcpy(step, V, 32);
    step[32] = 0x00;
    std::memcpy(step + 33, bx, 32);
    std::memcpy(step + 65, bh, 32);
    auto key = hmac_sha256(K, 32, step, sizeof(step));
    std::memcpy(K, key.data(), 32);
    key = hmac_sha256(K, 32, V, 32);
    std::memcpy(V, key.data(), 32);
    std::memcpy(step, V, 32);
    step[32] = 0x01;
    key = hmac_sha256(K, 32, step, sizeof(step));
    std::memcpy(K, key.data(), 32);
    key = hmac_sha256(K, 32, V, 32);
    std::memcpy(V, key.data(), 32);

    for (int i = 0; i <= attempt; ++i) {
        key = hmac_sha256(K, 32, V, 32);
        std::memcpy(V, key.data(), 32);
        Bn candidate(BN_bin2bn(V, 32, nullptr));
        if (i == attempt && !BN_is_zero(candidate.get()) && BN_cmp(candidate.get(), order) < 0) {
            return candidate;
        }
        std::uint8_t retry[33];
        std::memcpy(retry, V, 32);
        retry[32] = 0x00;
        key = hmac_sha256(K, 32, retry, sizeof(retry));
        std::memcpy(K, key.data(), 32);
        key = hmac_sha256(K, 32, V, 32);
        std::memcpy(V, key.data(), 32);
        (void)ctx;
    }
    return Bn(BN_new());
}

std::array<std::uint8_t, 20> address_from_private(const EC_GROUP *group, const BIGNUM *priv) {
    std::unique_ptr<BN_CTX, BnCtxFree> ctx(BN_CTX_new());
    Point point(EC_POINT_new(group));
    if (!ctx || !point || !EC_POINT_mul(group, point.get(), priv, nullptr, nullptr, ctx.get())) {
        throw std::runtime_error("failed to derive public key");
    }
    std::uint8_t uncompressed[65];
    if (EC_POINT_point2oct(group, point.get(), POINT_CONVERSION_UNCOMPRESSED, uncompressed, 65,
                           ctx.get()) != 65) {
        throw std::runtime_error("failed to encode public key");
    }
    const auto digest = keccak256(uncompressed + 1, 64);
    std::array<std::uint8_t, 20> address{};
    std::memcpy(address.data(), digest.data() + 12, 20);
    return address;
}

}  // namespace

std::string to_hex(const std::uint8_t *data, std::size_t len, bool prefix) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2 + (prefix ? 2 : 0));
    std::size_t o = 0;
    if (prefix) {
        out[o++] = '0';
        out[o++] = 'x';
    }
    for (std::size_t i = 0; i < len; ++i) {
        out[o++] = kHex[data[i] >> 4];
        out[o++] = kHex[data[i] & 0x0f];
    }
    return out;
}

bool decode_hex(std::string_view hex, std::string &error, std::vector<std::uint8_t> &out) {
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex.remove_prefix(2);
    if (hex.size() % 2 != 0) {
        error = "odd-length hex";
        return false;
    }
    out.resize(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = hex_value(hex[i * 2]);
        const int lo = hex_value(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            error = "invalid hex";
            return false;
        }
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::string to_checksum_address(std::string_view address) {
    std::string error;
    std::vector<std::uint8_t> raw;
    if (!decode_hex(address, error, raw) || raw.size() != 20) return {};
    std::string lower = to_hex(raw.data(), raw.size(), false);
    const auto hash = keccak256(lower);
    for (std::size_t i = 0; i < lower.size(); ++i) {
        const int nibble = (i % 2 == 0) ? (hash[i / 2] >> 4) : (hash[i / 2] & 0x0f);
        if (nibble >= 8 && lower[i] >= 'a' && lower[i] <= 'f') {
            lower[i] = static_cast<char>(lower[i] - 'a' + 'A');
        }
    }
    return "0x" + lower;
}

OrderSigner::OrderSigner(std::string private_key_hex) {
    std::string error;
    std::vector<std::uint8_t> raw;
    if (!decode_hex(private_key_hex, error, raw) || raw.size() != 32) {
        init_error_ = "private key must be 32-byte hex";
        return;
    }
    std::memcpy(private_key_.data(), raw.data(), 32);
    Group group(EC_GROUP_new_by_curve_name(NID_secp256k1));
    Bn priv(BN_bin2bn(private_key_.data(), 32, nullptr));
    if (!group || !priv) {
        init_error_ = "failed to load secp256k1 key";
        return;
    }
    try {
        const auto address = address_from_private(group.get(), priv.get());
        address_ = to_checksum_address(to_hex(address.data(), address.size()));
    } catch (const std::exception &ex) {
        init_error_ = ex.what();
        return;
    }
    ok_ = !address_.empty();
    if (!ok_) init_error_ = "failed to checksum signer address";
}

OrderSigner::~OrderSigner() = default;
OrderSigner::OrderSigner(OrderSigner &&) noexcept = default;
OrderSigner &OrderSigner::operator=(OrderSigner &&) noexcept = default;

Result<std::array<std::uint8_t, 32>> OrderSigner::order_digest(const Eip712Domain &domain,
                                                               const UnsignedOrder &order) const {
    if (!ok_) return Result<std::array<std::uint8_t, 32>>::failure(sign_error(init_error_));
    std::string error;
    auto word_of = [&](std::string_view dec, std::array<std::uint8_t, 32> &word) {
        return decimal_to_be32(strip_int_zeros(std::string(dec)), word, error);
    };

    std::array<std::uint8_t, 32> salt{}, maker{}, signer{}, taker{}, token{}, maker_amount{},
        taker_amount{}, expiration{}, nonce{}, fee{}, side{}, sig_type{}, chain{};
    if (!word_of(order.salt, salt) || !address_word(order.maker, maker, error) ||
        !address_word(order.signer, signer, error) || !address_word(order.taker, taker, error) ||
        !word_of(order.token_id, token) || !word_of(order.maker_amount, maker_amount) ||
        !word_of(order.taker_amount, taker_amount) || !word_of(order.expiration, expiration) ||
        !word_of(order.nonce, nonce) || !word_of(order.fee_rate_bps, fee)) {
        return Result<std::array<std::uint8_t, 32>>::failure(sign_error(error));
    }
    side[31] = order.side;
    sig_type[31] = order.signature_type;
    if (!decimal_to_be32(std::to_string(domain.chain_id), chain, error)) {
        return Result<std::array<std::uint8_t, 32>>::failure(sign_error(error));
    }
    std::array<std::uint8_t, 32> verifying{};
    if (!address_word(domain.verifying_contract, verifying, error)) {
        return Result<std::array<std::uint8_t, 32>>::failure(sign_error(error));
    }

    static constexpr std::string_view kDomainType =
        "EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)";
    static constexpr std::string_view kOrderType =
        "Order(uint256 salt,address maker,address signer,address taker,uint256 tokenId,"
        "uint256 makerAmount,uint256 takerAmount,uint256 expiration,uint256 nonce,"
        "uint256 feeRateBps,uint8 side,uint8 signatureType)";

    std::vector<std::uint8_t> domain_body;
    append_hash(domain_body, kDomainType);
    append_hash(domain_body, domain.name);
    append_hash(domain_body, domain.version);
    append_word(domain_body, chain);
    append_word(domain_body, verifying);
    const auto domain_separator = keccak256(domain_body.data(), domain_body.size());

    std::vector<std::uint8_t> order_body;
    append_hash(order_body, kOrderType);
    append_word(order_body, salt);
    append_word(order_body, maker);
    append_word(order_body, signer);
    append_word(order_body, taker);
    append_word(order_body, token);
    append_word(order_body, maker_amount);
    append_word(order_body, taker_amount);
    append_word(order_body, expiration);
    append_word(order_body, nonce);
    append_word(order_body, fee);
    append_word(order_body, side);
    append_word(order_body, sig_type);
    const auto struct_hash = keccak256(order_body.data(), order_body.size());

    std::vector<std::uint8_t> packed;
    packed.push_back(0x19);
    packed.push_back(0x01);
    packed.insert(packed.end(), domain_separator.begin(), domain_separator.end());
    packed.insert(packed.end(), struct_hash.begin(), struct_hash.end());
    return Result<std::array<std::uint8_t, 32>>::success(keccak256(packed.data(), packed.size()));
}

Result<SignedOrder> OrderSigner::sign_order(const Eip712Domain &domain, UnsignedOrder order,
                                            std::string price) const {
    if (!ok_) return Result<SignedOrder>::failure(sign_error(init_error_));
    order.maker = to_checksum_address(order.maker);
    order.signer = to_checksum_address(order.signer);
    order.taker = to_checksum_address(order.taker);
    if (order.maker.empty() || order.signer.empty() || order.taker.empty()) {
        return Result<SignedOrder>::failure(sign_error("maker, signer, and taker must be addresses"));
    }
    auto digest = order_digest(domain, order);
    if (!digest) return Result<SignedOrder>::failure(digest.error());

    Group group(EC_GROUP_new_by_curve_name(NID_secp256k1));
    Bn priv(BN_bin2bn(private_key_.data(), 32, nullptr));
    std::unique_ptr<BN_CTX, BnCtxFree> ctx(BN_CTX_new());
    if (!group || !priv || !ctx) {
        return Result<SignedOrder>::failure(sign_error("openssl init failed"));
    }
    Bn order_n(BN_new());
    if (!EC_GROUP_get_order(group.get(), order_n.get(), ctx.get())) {
        return Result<SignedOrder>::failure(sign_error("curve order unavailable"));
    }
    Bn half(BN_dup(order_n.get()));
    BN_rshift1(half.get(), half.get());

    Bn r(BN_new());
    Bn s(BN_new());
    int recid = -1;
    for (int attempt = 0; attempt < 8 && recid < 0; ++attempt) {
        Bn k = rfc6979_k(priv.get(), order_n.get(), digest.value().data(), ctx.get(), attempt);
        if (!k || BN_is_zero(k.get())) continue;
        Point point(EC_POINT_new(group.get()));
        if (!point || !EC_POINT_mul(group.get(), point.get(), k.get(), nullptr, nullptr, ctx.get())) continue;
        BIGNUM *x = BN_new();
        BIGNUM *y = BN_new();
        if (!EC_POINT_get_affine_coordinates(group.get(), point.get(), x, y, ctx.get())) {
            BN_free(x);
            BN_free(y);
            continue;
        }
        int y_odd = BN_is_odd(y);
        int overflow = 0;
        if (BN_cmp(x, order_n.get()) >= 0) {
            BN_mod(x, x, order_n.get(), ctx.get());
            overflow = 2;
        }
        BN_free(y);
        if (BN_is_zero(x)) {
            BN_free(x);
            continue;
        }
        Bn kinv(BN_mod_inverse(nullptr, k.get(), order_n.get(), ctx.get()));
        Bn e(BN_bin2bn(digest.value().data(), 32, nullptr));
        if (BN_cmp(e.get(), order_n.get()) >= 0) BN_sub(e.get(), e.get(), order_n.get());
        Bn tmp(BN_new());
        BN_copy(r.get(), x);
        BN_free(x);
        BN_mod_mul(tmp.get(), r.get(), priv.get(), order_n.get(), ctx.get());
        BN_mod_add(tmp.get(), tmp.get(), e.get(), order_n.get(), ctx.get());
        BN_mod_mul(s.get(), tmp.get(), kinv.get(), order_n.get(), ctx.get());
        if (BN_is_zero(s.get())) continue;
        if (BN_cmp(s.get(), half.get()) > 0) {
            BN_sub(s.get(), order_n.get(), s.get());
            y_odd ^= 1;
        }
        recid = y_odd | overflow;
    }
    if (recid < 0) return Result<SignedOrder>::failure(sign_error("ECDSA sign failed"));

    std::uint8_t sig65[65];
    if (!bn_to_32(r.get(), sig65) || !bn_to_32(s.get(), sig65 + 32)) {
        return Result<SignedOrder>::failure(sign_error("signature encoding failed"));
    }
    sig65[64] = static_cast<std::uint8_t>(recid + 27);

    SignedOrder signed_order;
    signed_order.order = std::move(order);
    signed_order.signature = to_hex(sig65, 65);
    signed_order.price = std::move(price);
    return Result<SignedOrder>::success(std::move(signed_order));
}

std::string next_order_salt() {
    static std::atomic<std::uint64_t> seq{0};
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    const auto value = static_cast<std::uint64_t>(ms) * 1000ull + (seq.fetch_add(1) % 1000ull);
    return std::to_string(value);
}

}  // namespace limitless
