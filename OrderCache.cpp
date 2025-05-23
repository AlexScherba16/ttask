#include "OrderCache.h"
#include <sstream>

namespace helper = order_cache::helpers;

OrderCache::OrderCache()
{
    m_orders.reserve(ORDERS_MAP_CAPACITY);
    m_userOrders.reserve(USER_ORDER_IDS_MAP_CAPACITY);
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
    auto user{order.user()};

    auto [_, orderInserted] = m_orders.emplace(orderId, std::move(order));
    if (!orderInserted)
    {
        return;
    }

    {
        auto userOrdersIt{m_userOrders.find(user)};
        if (userOrdersIt == m_userOrders.end())
        {
            std::vector<OrderID> orderIds{orderId};
            orderIds.reserve(ORDER_IDS_VECTOR_CAPACITY);
            m_userOrders.emplace_hint(userOrdersIt, std::move(user), std::move(orderIds));
        }
        else
        {
            userOrdersIt->second.emplace_back(orderId);
        }
    }
}

void OrderCache::cancelOrder(const std::string& orderId)
{
    const auto orderIt{m_orders.find(orderId)};
    if (orderIt == m_orders.end())
    {
        return;
    }

    auto user{orderIt->second.user()};
    if (auto userOrdersIt{m_userOrders.find(user)}; userOrdersIt != m_userOrders.end())
    {
        auto& orderIds{userOrdersIt->second};
        auto idIt{std::find(orderIds.begin(), orderIds.end(), orderId)};
        if (idIt != orderIds.end())
        {
            std::swap(*idIt, orderIds.back());
            orderIds.pop_back();
        }

        if (orderIds.empty())
        {
            m_userOrders.erase(userOrdersIt);
        }
    }

    m_orders.erase(orderIt);
}

void OrderCache::cancelOrdersForUser(const std::string& user)
{
    if (auto userOrdersIt{m_userOrders.find(user)}; userOrdersIt != m_userOrders.end())
    {
        auto orderIds{userOrdersIt->second};
        for (const auto& orderId : orderIds)
        {
            cancelOrder(orderId);
        }
    }
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
