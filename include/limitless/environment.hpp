#pragma once

#include "limitless/types.hpp"

#include <string>
#include <string_view>

namespace limitless
{
    // Published Base mainnet addresses from the Limitless smart-contract guide.
    // CLOB orders do not use a single global exchange: sign against market.venue.exchange.
    namespace contracts
    {
        inline constexpr std::string_view usdc = "0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913";
        inline constexpr std::string_view conditional_tokens = "0xC9c98965297Bc527861c898329Ee280632B76e18";

        inline constexpr std::string_view ctf_exchange_v1 = "0xa4409D988CA2218d956BeEFD3874100F444f0DC3";
        inline constexpr std::string_view ctf_exchange_v2 = "0xF1De958F8641448A5ba78c01f434085385Af096D";
        inline constexpr std::string_view ctf_exchange_v3 = "0x05c748E2f4DcDe0ec9Fa8DDc40DE6b867f923fa5";

        inline constexpr std::string_view neg_risk_adapter_v1 = "0xb8DAA4C8C9f690396f671BB601727A4c3741340C";
        inline constexpr std::string_view neg_risk_exchange_v1 = "0x5a38afc17F7E97ad8d6C547ddb837E40B4aEDfC6";
        inline constexpr std::string_view neg_risk_adapter_v2 = "0x7afeB946986211950d17f24176039F12c2aB2436";
        inline constexpr std::string_view neg_risk_exchange_v2 = "0x46e607D3f4a8494B0aB9b304d1463e2F4848891d";
        inline constexpr std::string_view neg_risk_adapter_v3 = "0x6151EF8368b6316c1aa3C68453EF083ad31E712D";
        inline constexpr std::string_view neg_risk_exchange_v3 = "0xe3E00BA3a9888d1DE4834269f62ac008b4BB5C47";
    }

    // Production preset. Limitless publishes no sandbox, testnet, or regional host.
    struct Environment
    {
        std::string name{"base"};
        int chain_id{kBaseChainId};
        std::string rest_url{"https://api.limitless.exchange"};
        std::string websocket_url{"wss://ws.limitless.exchange"};
        std::string websocket_namespace{"/markets"};
        // Docs do not publish an RPC URL. Supply one for your own on-chain calls.
        std::string rpc_url;

        static Environment base();
        void validate() const;
    };
}
