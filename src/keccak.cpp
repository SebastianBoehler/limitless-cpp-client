#include "limitless/order_signer.hpp"

extern "C" {
#include "sha3.h"
}

namespace limitless {

std::array<std::uint8_t, 32> keccak256(const std::uint8_t *data, std::size_t len) {
    sha3_ctx_t ctx;
    sha3_init(&ctx, 32);
    sha3_update(&ctx, data, len);
    ctx.st.b[ctx.pt] ^= 0x01;
    ctx.st.b[ctx.rsiz - 1] ^= 0x80;
    sha3_keccakf(ctx.st.q);
    std::array<std::uint8_t, 32> out{};
    for (int i = 0; i < 32; ++i) out[static_cast<std::size_t>(i)] = ctx.st.b[i];
    return out;
}

std::array<std::uint8_t, 32> keccak256(std::string_view data) {
    return keccak256(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
}

}  // namespace limitless
