#include "limitless/environment.hpp"
#include "limitless/order_signer.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const char *private_key = std::getenv("PRIVATE_KEY");
    const char *exchange = std::getenv("VENUE_EXCHANGE");
    if (!private_key || !exchange) {
        std::cout << "Set PRIVATE_KEY and VENUE_EXCHANGE (venue.exchange from GET /markets/{slug}) "
                     "to sign a sample GTC order offline.\n";
        return 0;
    }

    limitless::OrderSigner signer(private_key);
    if (signer.address().empty()) {
        std::cerr << "invalid PRIVATE_KEY\n";
        return 1;
    }

    limitless::UnsignedOrder order;
    order.salt = "1234567890";
    order.maker = signer.address();
    order.signer = signer.address();
    order.token_id = "1";
    order.maker_amount = "5000000";
    order.taker_amount = "10000000";
    order.fee_rate_bps = "0";
    order.side = 0;
    order.signature_type = 0;

    const auto env = limitless::Environment::production();
    auto signed_order = signer.sign_order(env.order_domain(exchange), order, "0.5");
    if (!signed_order) {
        std::cerr << signed_order.error().message << "\n";
        return 1;
    }
    std::cout << signed_order.value().order.maker << "\n" << signed_order.value().signature << "\n";
    return 0;
}
