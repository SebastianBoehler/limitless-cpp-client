#include "check.hpp"
#include "limitless/clob_client.hpp"

#include <stdexcept>

int main()
{
    limitless::OrderSigner signer("0x0000000000000000000000000000000000000000000000000000000000000001");
    const std::string address = signer.address();
    limitless::ClobClient client;
    client.set_signer(std::move(signer));

    limitless::LimitOrderRequest request;
    request.market_slug = "btc-100k";
    request.token_id = "12345";
    request.verifying_contract = std::string(limitless::contracts::ctf_exchange_v3);
    request.side = limitless::Side::Buy;
    request.type = limitless::OrderType::Gtc;
    request.price = "0.50";
    request.shares = "10";
    request.owner_id = 42;
    request.fee_rate_bps = 200;
    request.salt = "1";
    request.post_only = true;
    const auto prepared = client.prepare_limit_order(request);
    CHECK(prepared.body["ownerId"] == 42);
    CHECK(prepared.body["orderType"] == "GTC");
    CHECK(prepared.body["marketSlug"] == "btc-100k");
    CHECK(prepared.body["postOnly"] == true);
    CHECK(prepared.body["order"]["maker"] == address);
    CHECK(prepared.body["order"]["signer"] == address);
    CHECK(prepared.body["order"]["makerAmount"] == 5000000);
    CHECK(prepared.body["order"]["takerAmount"] == 10000000);
    CHECK(prepared.body["order"]["nonce"] == 0);
    CHECK(prepared.body["order"]["expiration"] == "0");
    CHECK(prepared.body["order"]["feeRateBps"] == 200);
    CHECK(prepared.body["order"]["side"] == 0);
    CHECK(prepared.body["order"]["signatureType"] == 0);
    CHECK(prepared.body["order"]["price"] == 0.5);
    CHECK(prepared.body.dump().find("\"price\":0.5") != std::string::npos);
    CHECK(!prepared.body["order"].contains("timestamp"));

    limitless::MarketOrderRequest market;
    market.market_slug = "btc-100k";
    market.token_id = "12345";
    market.verifying_contract = request.verifying_contract;
    market.side = limitless::Side::Sell;
    market.amount = "4";
    market.owner_id = 42;
    market.salt = "2";
    const auto fok = client.prepare_market_order(market);
    CHECK(fok.body["orderType"] == "FOK");
    CHECK(fok.body["order"]["makerAmount"] == 4000000);
    CHECK(fok.body["order"]["takerAmount"] == 1);
    CHECK(!fok.body["order"].contains("price"));

    request.type = limitless::OrderType::Fak;
    request.post_only = true;
    bool rejected = false;
    try
    {
        client.prepare_limit_order(request);
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    CHECK(rejected);
    RETURN_TEST();
}
