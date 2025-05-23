#include "OrderCache.h"
#include <sstream>

namespace helper = order_cache::helpers;

OrderCache::OrderCache()
{
    m_orders.reserve(ORDERS_MAP_CAPACITY);
}


void OrderCache::addOrder(Order order)
{
    if (auto err{helper::OrderValidator::validateOrder(order)})
    {
        std::stringstream message;
        message << "Invalid order : " << helper::OrderValidator::orderErrorToString(err.value());
        throw std::invalid_argument(message.str());
    }

    const auto orderId{order.orderId()};

    auto [orderIt, orderInserted] = m_orders.emplace(orderId, std::move(order));
    if (!orderInserted)
    {
        return;
    }

    orderInserted = true;
}

void OrderCache::cancelOrder(const std::string& orderId)
{
    // Todo...
}

void OrderCache::cancelOrdersForUser(const std::string& user)
{
    // Todo...
}

void OrderCache::cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty)
{
    // Todo...
}

unsigned int OrderCache::getMatchingSizeForSecurity(const std::string& securityId)
{
    // Todo...
    return 0;
}

std::vector<Order> OrderCache::getAllOrders() const
{
    if (m_orders.empty())
    {
        return {};
    }

    std::vector<Order> orders;
    orders.reserve(m_orders.size());

    if (m_orders.size() < ORDERS_MAP_THRESHOLD)
    {
        for (const auto& idOrderPair : m_orders)
        {
            orders.emplace_back(idOrderPair.second);
        }
    }
    else
    {
        for (auto copyPair : m_orders)
        {
            orders.emplace_back(std::move(copyPair.second));
        }
    }

    return orders;
}
