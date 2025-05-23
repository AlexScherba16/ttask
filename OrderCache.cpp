#include "OrderCache.h"
#include <sstream>

namespace helper = order_cache::helpers;

OrderCache::OrderCache()
{
    m_orders.reserve(ORDERS_MAP_CAPACITY);
    m_userOrders.reserve(USER_ORDER_IDS_MAP_CAPACITY);
    m_securityOrders.reserve(SECURITY_ORDER_IDS_MAP_CAPACITY);
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
    auto securityId{order.securityId()};

    auto [_, orderInserted] = m_orders.emplace(orderId, std::move(order));
    if (!orderInserted)
    {
        return;
    }

    _addOrderId(m_userOrders, std::move(user), orderId);
    _addOrderId(m_securityOrders, std::move(securityId), orderId);
}

void OrderCache::cancelOrder(const std::string& orderId)
{
    const auto orderIt{m_orders.find(orderId)};
    if (orderIt == m_orders.end())
    {
        return;
    }

    const auto user{orderIt->second.user()};
    const auto securityId{orderIt->second.securityId()};

    _removeOrderId(m_userOrders, user, orderId);
    _removeOrderId(m_securityOrders, securityId, orderId);

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
    if (minQty == 0)
    {
        return;
    }

    if (auto securityOrdersIt{m_securityOrders.find(securityId)}; securityOrdersIt != m_securityOrders.end())
    {
        auto orderIds{securityOrdersIt->second};
        for (const auto& orderId : orderIds)
        {
            auto orderIt{m_orders.find(orderId)};
            if (orderIt != m_orders.end() && orderIt->second.qty() >= minQty)
            {
                cancelOrder(orderId);
            }
        }
    }
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
