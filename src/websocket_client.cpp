#include "limitless/websocket_client.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <poll.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace limitless {
namespace {

struct WsConn {
    int fd = -1;
    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;
    std::string pending;
    ~WsConn() {
        if (ssl) SSL_free(ssl);
        if (ctx) SSL_CTX_free(ctx);
        if (fd >= 0) ::close(fd);
    }
};

SdkError ws_error(SdkErrorCode code, std::string message) {
    SdkError error;
    error.code = code;
    error.message = std::move(message);
    error.retryable = code == SdkErrorCode::Transport || code == SdkErrorCode::Unavailable;
    return error;
}

std::string route_or(const std::string &specific, const std::string &fallback) {
    return specific.empty() ? fallback : specific;
}

struct ParsedUrl {
    std::string host;
    std::string path;
    int port = 443;
};

bool parse_wss(const std::string &url, ParsedUrl &out) {
    const std::string prefix = "wss://";
    if (url.rfind(prefix, 0) != 0) return false;
    const auto slash = url.find('/', prefix.size());
    std::string hostport = slash == std::string::npos ? url.substr(prefix.size()) : url.substr(prefix.size(), slash - prefix.size());
    out.path = slash == std::string::npos ? "/" : url.substr(slash);
    const auto colon = hostport.rfind(':');
    if (colon != std::string::npos && hostport.find(':') == colon) {
        out.host = hostport.substr(0, colon);
        out.port = std::atoi(hostport.substr(colon + 1).c_str());
    } else {
        out.host = hostport;
        out.port = 443;
    }
    return !out.host.empty() && out.port > 0;
}

std::string base64_encode(const unsigned char *data, std::size_t len) {
    std::string out(((len + 2) / 3) * 4, '\0');
    const int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()), data, static_cast<int>(len));
    out.resize(static_cast<std::size_t>(n < 0 ? 0 : n));
    return out;
}

bool wait_fd(int fd, short events, std::chrono::steady_clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    if (left <= 0) return false;
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = events;
    return ::poll(&pfd, 1, static_cast<int>(left)) > 0;
}

int ssl_read_some(WsConn &conn, unsigned char *buf, int len, std::chrono::steady_clock::time_point deadline) {
    if (!conn.pending.empty()) {
        const int n = static_cast<int>(std::min(conn.pending.size(), static_cast<std::size_t>(len)));
        std::memcpy(buf, conn.pending.data(), static_cast<std::size_t>(n));
        conn.pending.erase(0, static_cast<std::size_t>(n));
        return n;
    }
    while (std::chrono::steady_clock::now() < deadline) {
        const int n = SSL_read(conn.ssl, buf, len);
        if (n > 0) return n;
        const int err = SSL_get_error(conn.ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            if (!wait_fd(conn.fd, POLLIN, deadline)) return 0;
            continue;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            if (!wait_fd(conn.fd, POLLOUT, deadline)) return 0;
            continue;
        }
        return -1;
    }
    return 0;
}

// 1 = ok, 0 = timeout, -1 = transport error.
int ssl_read_exact(WsConn &conn, unsigned char *buf, std::size_t len, std::chrono::steady_clock::time_point deadline) {
    std::size_t got = 0;
    while (got < len) {
        const int n = ssl_read_some(conn, buf + got, static_cast<int>(len - got), deadline);
        if (n == 0) return 0;
        if (n < 0) return -1;
        got += static_cast<std::size_t>(n);
    }
    return 1;
}

Result<std::string> read_fail(int code) {
    if (code == 0) return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "timeout"));
    return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "websocket frame truncated"));
}

bool ssl_write_all(WsConn &conn, const void *data, std::size_t len, std::chrono::steady_clock::time_point deadline) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    std::size_t sent = 0;
    while (sent < len) {
        const int n = SSL_write(conn.ssl, bytes + sent, static_cast<int>(len - sent));
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        const int err = SSL_get_error(conn.ssl, n);
        if (err == SSL_ERROR_WANT_READ) {
            if (!wait_fd(conn.fd, POLLIN, deadline)) return false;
            continue;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            if (!wait_fd(conn.fd, POLLOUT, deadline)) return false;
            continue;
        }
        return false;
    }
    return true;
}

bool send_ws_frame(WsConn &conn, unsigned char opcode, const std::string &payload,
                   std::chrono::steady_clock::time_point deadline) {
    std::vector<unsigned char> frame;
    frame.push_back(static_cast<unsigned char>(0x80 | opcode));
    if (payload.size() < 126) {
        frame.push_back(static_cast<unsigned char>(0x80 | payload.size()));
    } else if (payload.size() <= 65535) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((payload.size() >> 8) & 0xff));
        frame.push_back(static_cast<unsigned char>(payload.size() & 0xff));
    } else {
        frame.push_back(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<unsigned char>((static_cast<std::uint64_t>(payload.size()) >> shift) & 0xff));
        }
    }
    unsigned char mask[4];
    if (RAND_bytes(mask, 4) != 1) return false;
    frame.insert(frame.end(), mask, mask + 4);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]);
    }
    return ssl_write_all(conn, frame.data(), frame.size(), deadline);
}

Result<std::string> read_ws_text(WsConn &conn, std::chrono::steady_clock::time_point deadline) {
    std::string message;
    while (std::chrono::steady_clock::now() < deadline) {
        unsigned char hdr[2];
        if (const int code = ssl_read_exact(conn, hdr, 2, deadline); code != 1) return read_fail(code);
        const unsigned char opcode = hdr[0] & 0x0f;
        const bool fin = (hdr[0] & 0x80) != 0;
        std::uint64_t len = hdr[1] & 0x7f;
        if (len == 126) {
            unsigned char ext[2];
            if (const int code = ssl_read_exact(conn, ext, 2, deadline); code != 1) return read_fail(code);
            len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
        } else if (len == 127) {
            unsigned char ext[8];
            if (const int code = ssl_read_exact(conn, ext, 8, deadline); code != 1) return read_fail(code);
            len = 0;
            for (unsigned char byte : ext) len = (len << 8) | byte;
        }
        if (len > 8 * 1024 * 1024) {
            return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "websocket frame too large"));
        }
        std::string payload(static_cast<std::size_t>(len), '\0');
        if (len > 0) {
            if (const int code = ssl_read_exact(conn, reinterpret_cast<unsigned char *>(payload.data()), payload.size(),
                                               deadline);
                code != 1) {
                return read_fail(code);
            }
        }
        if (opcode == 0x8) {
            return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "websocket closed"));
        }
        if (opcode == 0x9) {
            if (!send_ws_frame(conn, 0xA, payload, deadline)) {
                return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "websocket pong failed"));
            }
            continue;
        }
        if (opcode == 0xA) continue;
        if (opcode != 0x1 && opcode != 0x0) continue;
        message += payload;
        if (fin) return Result<std::string>::success(std::move(message));
    }
    return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "timeout"));
}

}  // namespace

WebSocketClient::WebSocketClient(Environment env) : env_(std::move(env)) {}

WebSocketClient::~WebSocketClient() { close(); }

void WebSocketClient::configure(WebSocketOptions options) { options_ = std::move(options); }

void WebSocketClient::set_credentials(HmacCredentials credentials) {
    credentials_ = std::move(credentials);
    have_credentials_ = !credentials_.token_id.empty();
}

void WebSocketClient::on_event(EventHandler handler) { handler_ = std::move(handler); }

void WebSocketClient::subscribe_market_prices(std::vector<std::string> market_slugs,
                                              std::vector<std::string> market_addresses) {
    market_slugs_ = std::move(market_slugs);
    market_addresses_ = std::move(market_addresses);
    if (connected_) {
        nlohmann::json payload = nlohmann::json::object();
        if (!market_slugs_.empty()) payload["marketSlugs"] = market_slugs_;
        if (!market_addresses_.empty()) payload["marketAddresses"] = market_addresses_;
        (void)send_text(socketio_event_packet(env_.websocket_namespace, "subscribe_market_prices", payload));
    }
}

void WebSocketClient::subscribe_authenticated(const std::string &event, nlohmann::json payload) {
    auth_subs_.erase(std::remove_if(auth_subs_.begin(), auth_subs_.end(),
                                    [&](const auto &item) { return item.first == event; }),
                     auth_subs_.end());
    auth_subs_.emplace_back(event, std::move(payload));
    if (connected_) {
        (void)send_text(socketio_event_packet(env_.websocket_namespace, event, auth_subs_.back().second));
    }
}

void WebSocketClient::reset_easy() {
    delete static_cast<WsConn *>(easy_);
    easy_ = nullptr;
    connected_ = false;
}

void WebSocketClient::close() { reset_easy(); }

Result<bool> WebSocketClient::send_text(const std::string &payload) {
    auto *conn = static_cast<WsConn *>(easy_);
    if (!conn || !conn->ssl) {
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "websocket is not connected"));
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options_.connect_timeout_ms);
    if (!send_ws_frame(*conn, 0x1, payload, deadline)) {
        connected_ = false;
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "websocket send failed"));
    }
    return Result<bool>::success(true);
}

Result<std::string> WebSocketClient::recv_text(int timeout_ms) {
    auto *conn = static_cast<WsConn *>(easy_);
    if (!conn || !conn->ssl) {
        return Result<std::string>::failure(ws_error(SdkErrorCode::Transport, "websocket is not connected"));
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    return read_ws_text(*conn, deadline);
}

Result<bool> WebSocketClient::emit_subscriptions() {
    if (!market_slugs_.empty() || !market_addresses_.empty()) {
        nlohmann::json payload = nlohmann::json::object();
        if (!market_slugs_.empty()) payload["marketSlugs"] = market_slugs_;
        if (!market_addresses_.empty()) payload["marketAddresses"] = market_addresses_;
        auto sent = send_text(socketio_event_packet(env_.websocket_namespace, "subscribe_market_prices", payload));
        if (!sent) return sent;
    }
    for (const auto &sub : auth_subs_) {
        auto sent = send_text(socketio_event_packet(env_.websocket_namespace, sub.first, sub.second));
        if (!sent) return sent;
    }
    return Result<bool>::success(true);
}

Result<bool> WebSocketClient::open_socket() {
    reset_easy();
    const std::string proxy = route_or(options_.proxy_url, default_network_route().proxy_url);
    if (!proxy.empty()) {
        return Result<bool>::failure(ws_error(
            SdkErrorCode::Transport,
            "Socket.IO transport is a direct TLS websocket; HTTP proxy tunneling is not implemented"));
    }

    ParsedUrl url;
    if (!parse_wss(env_.websocket_connect_url(), url)) {
        return Result<bool>::failure(ws_error(SdkErrorCode::InvalidArgument, "websocket URL must be wss://"));
    }

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo *resolved = nullptr;
    const std::string port = std::to_string(url.port);
    if (getaddrinfo(url.host.c_str(), port.c_str(), &hints, &resolved) != 0) {
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "DNS lookup failed for " + url.host));
    }

    auto *conn = new WsConn();
    easy_ = conn;
    const std::string iface = route_or(options_.interface_name, default_network_route().interface_name);
    for (addrinfo *it = resolved; it; it = it->ai_next) {
        conn->fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (conn->fd < 0) continue;
        if (!iface.empty() && setsockopt(conn->fd, SOL_SOCKET, SO_BINDTODEVICE, iface.c_str(),
                                         static_cast<socklen_t>(iface.size())) != 0) {
            ::close(conn->fd);
            conn->fd = -1;
            freeaddrinfo(resolved);
            reset_easy();
            return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "failed to bind interface " + iface));
        }
        if (::connect(conn->fd, it->ai_addr, it->ai_addrlen) == 0) break;
        ::close(conn->fd);
        conn->fd = -1;
    }
    freeaddrinfo(resolved);
    if (conn->fd < 0) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "TCP connect failed"));
    }

    conn->ctx = SSL_CTX_new(TLS_client_method());
    if (!conn->ctx) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "SSL_CTX_new failed"));
    }
    SSL_CTX_set_default_verify_paths(conn->ctx);
    conn->ssl = SSL_new(conn->ctx);
    SSL_set_fd(conn->ssl, conn->fd);
    SSL_set_tlsext_host_name(conn->ssl, url.host.c_str());
    SSL_set1_host(conn->ssl, url.host.c_str());
    SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, nullptr);
    if (SSL_connect(conn->ssl) != 1) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "TLS handshake failed"));
    }

    unsigned char key_raw[16];
    if (RAND_bytes(key_raw, sizeof(key_raw)) != 1) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "RAND_bytes failed"));
    }
    const std::string ws_key = base64_encode(key_raw, sizeof(key_raw));
    std::ostringstream request;
    request << "GET " << url.path << " HTTP/1.1\r\n"
            << "Host: " << url.host << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << ws_key << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "User-Agent: " << options_.user_agent << "\r\n";
    if (have_credentials_) {
        try {
            const auto signed_headers = sign_websocket_handshake(credentials_, env_.socketio_path);
            for (const auto &header : signed_headers.headers) {
                request << header.first << ": " << header.second << "\r\n";
            }
        } catch (const std::exception &ex) {
            reset_easy();
            return Result<bool>::failure(ws_error(SdkErrorCode::Signing, ex.what()));
        }
    }
    request << "\r\n";
    const std::string handshake = request.str();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options_.connect_timeout_ms);
    if (!ssl_write_all(*conn, handshake.data(), handshake.size(), deadline)) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "websocket upgrade write failed"));
    }

    std::string response;
    while (response.find("\r\n\r\n") == std::string::npos) {
        unsigned char buf[1024];
        const int n = ssl_read_some(*conn, buf, sizeof(buf), deadline);
        if (n <= 0) {
            reset_easy();
            return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "websocket upgrade timed out"));
        }
        response.append(reinterpret_cast<char *>(buf), static_cast<std::size_t>(n));
        if (response.size() > 16384) {
            reset_easy();
            return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "websocket upgrade response too large"));
        }
    }
    const auto header_end = response.find("\r\n\r\n");
    if (response.find(" 101 ") == std::string::npos || header_end == std::string::npos) {
        reset_easy();
        const auto line_end = response.find("\r\n");
        return Result<bool>::failure(
            ws_error(SdkErrorCode::Transport, "websocket upgrade rejected: " + response.substr(0, line_end)));
    }
    conn->pending = response.substr(header_end + 4);

    bool opened = false;
    bool namespace_ready = false;
    const auto ns_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options_.connect_timeout_ms);
    while (std::chrono::steady_clock::now() < ns_deadline && !(opened && namespace_ready)) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(ns_deadline - std::chrono::steady_clock::now()).count();
        auto incoming = recv_text(static_cast<int>(std::max<std::int64_t>(left, 1)));
        if (!incoming) {
            if (incoming.error().message == "timeout") break;
            reset_easy();
            return Result<bool>::failure(incoming.error());
        }
        const SocketPacket packet = parse_socket_packet(incoming.value());
        if (packet.kind == SocketPacketKind::Ping) {
            auto pong = send_text(engineio_pong_packet());
            if (!pong) return pong;
            continue;
        }
        if (packet.kind == SocketPacketKind::Open) {
            opened = true;
            auto connect = send_text(socketio_connect_packet(env_.websocket_namespace));
            if (!connect) return connect;
        }
        if (packet.kind == SocketPacketKind::Connect) namespace_ready = true;
        if (packet.kind == SocketPacketKind::ConnectError) {
            reset_easy();
            return Result<bool>::failure(ws_error(SdkErrorCode::Auth, incoming.value()));
        }
    }
    if (!opened || !namespace_ready) {
        reset_easy();
        return Result<bool>::failure(ws_error(SdkErrorCode::Transport, "Socket.IO namespace handshake timed out"));
    }
    connected_ = true;
    return emit_subscriptions();
}

Result<bool> WebSocketClient::connect() { return open_socket(); }

Result<bool> WebSocketClient::reconnect() { return open_socket(); }

Result<int> WebSocketClient::poll(int timeout_ms) {
    if (!connected_) return Result<int>::failure(ws_error(SdkErrorCode::Transport, "websocket is not connected"));
    int events = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        auto incoming = recv_text(static_cast<int>(std::max<std::int64_t>(left, 1)));
        if (!incoming) {
            if (incoming.error().message == "timeout") break;
            return Result<int>::failure(incoming.error());
        }
        const SocketPacket packet = parse_socket_packet(incoming.value());
        if (packet.kind == SocketPacketKind::Ping) {
            auto pong = send_text(engineio_pong_packet());
            if (!pong) return Result<int>::failure(pong.error());
            continue;
        }
        if (packet.kind == SocketPacketKind::Event) {
            ++events;
            if (handler_) handler_(packet);
        }
    }
    return Result<int>::success(events);
}

}  // namespace limitless
