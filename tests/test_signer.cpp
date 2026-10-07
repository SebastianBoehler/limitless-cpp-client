#include "check.hpp"
#include "limitless/environment.hpp"
#include "limitless/order_signer.hpp"

#include <stdexcept>

int main()
{
    const auto empty = limitless::keccak256("");
    CHECK(limitless::to_hex(empty.data(), empty.size()) ==
          "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470");
    const auto abc = limitless::keccak256("abc");
    CHECK(limitless::to_hex(abc.data(), abc.size()) ==
          "0x4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45");

    const auto one = limitless::uint256_from_decimal("256");
    CHECK(one[30] == 1);
    CHECK(one[31] == 0);
    CHECK(limitless::to_checksum_address("0x5aaeb6053f3e94c9b9a09f33669435e7ef1beaed") ==
          "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed");

    limitless::OrderSigner signer(
        "0x0000000000000000000000000000000000000000000000000000000000000001");
    CHECK(signer.address() == "0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf");
    CHECK(signer.chain_id() == limitless::kBaseChainId);

    limitless::OrderDraft draft;
    draft.salt = "1";
    draft.token_id = "12345";
    draft.maker_amount = "5000000";
    draft.taker_amount = "10000000";
    draft.fee_rate_bps = 200;
    draft.side = limitless::Side::Buy;
    const char *exchange = "0x05c748E2f4DcDe0ec9Fa8DDc40DE6b867f923fa5";
    const auto first = signer.sign_order(draft, exchange);
    const auto second = signer.sign_order(draft, exchange);
    CHECK(first.signature == second.signature);
    CHECK(first.signature.size() == 132);
    CHECK(first.order.maker == signer.address());
    CHECK(first.order.signer == signer.address());
    CHECK(first.order.taker == limitless::kZeroAddress);
    CHECK(first.order.expiration == "0");
    CHECK(first.order.nonce == "0");

    const auto other_exchange = "0xF1De958F8641448A5ba78c01f434085385Af096D";
    const auto other = signer.order_digest(first.order, other_exchange);
    CHECK(other != first.digest);

    bool rejected = false;
    try
    {
        draft.expiration = "1";
        signer.sign_order(draft, exchange);
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    CHECK(rejected);

    auto environment = limitless::Environment::base();
    environment.validate();
    environment.chain_id = 1;
    rejected = false;
    try
    {
        environment.validate();
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    CHECK(rejected);
    RETURN_TEST();
}
