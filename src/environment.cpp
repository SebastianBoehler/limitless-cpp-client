#include "limitless/environment.hpp"

#include <stdexcept>

namespace limitless
{
    Environment Environment::base()
    {
        return Environment{};
    }

    void Environment::validate() const
    {
        if (chain_id != kBaseChainId)
        {
            throw std::invalid_argument("Limitless production chain id is 8453");
        }
        if (rest_url.empty() || websocket_url.empty() || websocket_namespace.empty())
        {
            throw std::invalid_argument("environment hosts must be set");
        }
        if (websocket_namespace.front() != '/')
        {
            throw std::invalid_argument("websocket namespace must start with /");
        }
    }
}
