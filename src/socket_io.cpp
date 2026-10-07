#include "limitless/socket_io.hpp"

namespace limitless {

std::string socketio_connect_packet(const std::string &nsp) {
    if (nsp.empty() || nsp == "/") return "40";
    return "40" + nsp + ",";
}

std::string socketio_event_packet(const std::string &nsp, const std::string &event,
                                  const nlohmann::json &data) {
    nlohmann::json payload = nlohmann::json::array({event, data});
    std::string packet = "42";
    if (!nsp.empty() && nsp != "/") packet += nsp + ",";
    packet += payload.dump();
    return packet;
}

std::string engineio_pong_packet() { return "3"; }

SocketPacket parse_socket_packet(std::string raw) {
    SocketPacket packet;
    packet.raw = raw;
    if (raw.empty()) return packet;
    if (raw[0] == '0') {
        packet.kind = SocketPacketKind::Open;
        if (raw.size() > 1) {
            try {
                packet.payload = nlohmann::json::parse(raw.substr(1));
            } catch (...) {
            }
        }
        return packet;
    }
    if (raw[0] == '1') {
        packet.kind = SocketPacketKind::Close;
        return packet;
    }
    if (raw[0] == '2') {
        packet.kind = SocketPacketKind::Ping;
        return packet;
    }
    if (raw[0] == '3') {
        packet.kind = SocketPacketKind::Pong;
        return packet;
    }
    if (raw[0] != '4' || raw.size() < 2) return packet;

    const char sio = raw[1];
    std::string rest = raw.substr(2);
    if (!rest.empty() && rest[0] == '/') {
        const auto comma = rest.find(',');
        if (comma == std::string::npos) {
            packet.nsp = rest;
            rest.clear();
        } else {
            packet.nsp = rest.substr(0, comma);
            rest = rest.substr(comma + 1);
        }
    }
    if (!rest.empty()) {
        try {
            packet.payload = nlohmann::json::parse(rest);
        } catch (...) {
        }
    }
    switch (sio) {
        case '0':
            packet.kind = SocketPacketKind::Connect;
            break;
        case '1':
            packet.kind = SocketPacketKind::ConnectAck;
            break;
        case '2':
            packet.kind = SocketPacketKind::Event;
            if (packet.payload.is_array() && !packet.payload.empty() && packet.payload[0].is_string()) {
                packet.event = packet.payload[0].get<std::string>();
                if (packet.payload.size() > 1) packet.data = packet.payload[1];
            }
            break;
        case '4':
            packet.kind = SocketPacketKind::ConnectError;
            break;
        default:
            packet.kind = SocketPacketKind::Other;
            break;
    }
    return packet;
}

}  // namespace limitless
