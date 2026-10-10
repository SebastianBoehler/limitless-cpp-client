#include "check.hpp"
#include "limitless/websocket_client.hpp"
#include <chrono>
#include <condition_variable>
#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>

int main()
{
    std::mutex mutex;
    std::condition_variable changed;
    int updates = 0;
    const int port = ix::getFreePort();
    ix::WebSocketServer server(port, "127.0.0.1");
    server.setOnClientMessageCallback(
        [&](std::shared_ptr<ix::ConnectionState>, ix::WebSocket &socket, const ix::WebSocketMessagePtr &message)
        {
            if (message->type == ix::WebSocketMessageType::Open)
                socket.send("0{}");
            if (message->type == ix::WebSocketMessageType::Message && message->str == "40/markets,")
            {
                socket.send("40/markets,");
                socket.send("42/markets,[\"orderbookUpdate\",{\"version\":3}]");
            }
        });
    const auto listening = server.listen();
    CHECK(listening.first);
    if (!listening.first)
        RETURN_TEST();
    server.start();
    auto environment = limitless::Environment::base();
    environment.websocket_url = "ws://127.0.0.1:" + std::to_string(port);
    {
        limitless::MarketsSocket active(environment);
        active.on_event(
            [&](const limitless::SocketIoEvent &event)
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (event.name == "orderbookUpdate" && event.args[0]["version"] == 3)
                    ++updates;
                changed.notify_all();
            });
        active.start();
        {
            std::unique_lock<std::mutex> lock(mutex);
            CHECK(changed.wait_for(lock, std::chrono::seconds(5), [&] { return updates == 1; }));
        }
        limitless::MarketsSocket moved(std::move(active));
        limitless::MarketsSocket replacement(environment);
        // Replacing a running implementation must join before destroying callback state.
        moved = std::move(replacement);
        CHECK(!moved.connected());
    }
    server.stop();
    RETURN_TEST();
}
