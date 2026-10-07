#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace limitless {

// Engine.IO v4 + Socket.IO packet helpers for the /markets namespace.
// Limitless is not a raw JSON websocket. Framing notes: docs/websocket.md.

enum class SocketPacketKind {
    Open,
    Ping,
    Pong,
    Close,
    Connect,
    ConnectAck,
    Event,
    ConnectError,
    Other,
};

struct SocketPacket {
    SocketPacketKind kind = SocketPacketKind::Other;
    std::string nsp;
    std::string event;
    nlohmann::json data;
    nlohmann::json payload;
    std::string raw;
};

std::string socketio_connect_packet(const std::string &nsp);
std::string socketio_event_packet(const std::string &nsp, const std::string &event,
                                  const nlohmann::json &data);
std::string engineio_pong_packet();
SocketPacket parse_socket_packet(std::string raw);

}  // namespace limitless
