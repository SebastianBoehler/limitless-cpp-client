#pragma once

#include "limitless/types.hpp"

#include <cstdint>
#include <string>

namespace limitless {

// Addresses published at https://docs.limitless.exchange/user-guide/smart-contracts
// Signing does not pick one of these by itself. POST /orders uses venue.exchange
// from GET /markets/{slug} as the EIP-712 verifyingContract.
inline constexpr const char *kUsdcAddress = "0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913";
inline constexpr const char *kConditionalTokensAddress = "0xC9c98965297Bc527861c898329Ee280632B76e18";

inline constexpr const char *kSimpleExchangeV1 = "0xa4409D988CA2218d956BeEFD3874100F444f0DC3";
inline constexpr const char *kSimpleExchangeV2 = "0xF1De958F8641448A5ba78c01f434085385Af096D";
inline constexpr const char *kSimpleExchangeV3 = "0x05c748E2f4DcDe0ec9Fa8DDc40DE6b867f923fa5";
inline constexpr const char *kNegRiskExchangeV1 = "0x5a38afc17F7E97ad8d6C547ddb837E40B4aEDfC6";
inline constexpr const char *kNegRiskExchangeV2 = "0x46e607D3f4a8494B0aB9b304d1463e2F4848891d";
inline constexpr const char *kNegRiskExchangeV3 = "0xe3E00BA3a9888d1DE4834269f62ac008b4BB5C47";
inline constexpr const char *kNegRiskAdapterV1 = "0xb8DAA4C8C9f690396f671BB601727A4c3741340C";
inline constexpr const char *kNegRiskAdapterV2 = "0x7afeB946986211950d17f24176039F12c2aB2436";
inline constexpr const char *kNegRiskAdapterV3 = "0x6151EF8368b6316c1aa3C68453EF083ad31E712D";

inline constexpr std::uint64_t kBaseChainId = 8453;
inline constexpr const char *kEip712DomainName = "Limitless CTF Exchange";
inline constexpr const char *kEip712DomainVersion = "1";

// Engine.IO handshake path signed for authenticated Socket.IO connections.
// See https://docs.limitless.exchange/developers/websocket/overview
inline constexpr const char *kSocketIoHandshakePath = "/socket.io/?EIO=4&transport=websocket";

struct Environment {
    std::string rest_url = "https://api.limitless.exchange";
    std::string websocket_url = "wss://ws.limitless.exchange";
    std::string websocket_namespace = "/markets";
    std::string socketio_path = kSocketIoHandshakePath;
    std::uint64_t chain_id = kBaseChainId;
    std::string eip712_name = kEip712DomainName;
    std::string eip712_version = kEip712DomainVersion;
    std::string usdc = kUsdcAddress;
    std::string conditional_tokens = kConditionalTokensAddress;

    static Environment production() { return Environment{}; }

    Eip712Domain order_domain(const std::string &verifying_contract) const {
        Eip712Domain domain;
        domain.name = eip712_name;
        domain.version = eip712_version;
        domain.chain_id = chain_id;
        domain.verifying_contract = verifying_contract;
        return domain;
    }

    std::string websocket_connect_url() const {
        std::string base = websocket_url;
        while (!base.empty() && base.back() == '/') base.pop_back();
        std::string path = socketio_path;
        if (path.empty() || path.front() != '/') path.insert(path.begin(), '/');
        return base + path;
    }
};

}  // namespace limitless
