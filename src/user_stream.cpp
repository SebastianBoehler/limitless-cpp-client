#include "limitless/user_stream.hpp"

namespace limitless
{
    UserStream::UserStream(Environment environment, HmacCredentials credentials)
        : socket_(std::move(environment))
    {
        socket_.set_credentials(std::move(credentials));
    }

    UserStream::~UserStream() = default;
    UserStream::UserStream(UserStream &&) noexcept = default;
    UserStream &UserStream::operator=(UserStream &&) noexcept = default;

    void UserStream::on_event(MarketsSocket::EventCallback callback)
    {
        socket_.on_event(std::move(callback));
    }

    void UserStream::on_connect(MarketsSocket::VoidCallback callback)
    {
        socket_.on_connect(std::move(callback));
    }

    void UserStream::on_disconnect(MarketsSocket::ErrorCallback callback)
    {
        socket_.on_disconnect(std::move(callback));
    }

    void UserStream::start()
    {
        socket_.start();
    }

    void UserStream::stop()
    {
        socket_.stop();
    }

    bool UserStream::subscribe_positions(const MarketPriceSubscription &subscription)
    {
        return socket_.subscribe_positions(subscription);
    }

    bool UserStream::subscribe_order_events()
    {
        return socket_.subscribe_order_events();
    }

    bool UserStream::subscribe_transactions()
    {
        return socket_.subscribe_transactions();
    }

    std::string UserStream::last_error() const
    {
        return socket_.last_error();
    }
}
