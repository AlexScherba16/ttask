#include "OrderCache.h"

#include <sstream>
#include <algorithm>

namespace helper = order_cache::helpers;

OrderCache::OrderCache()
{
    m_orders.reserve(ORDERS_MAP_CAPACITY);
    m_userOrders.reserve(USER_ORDER_IDS_MAP_CAPACITY);
    m_securityOrders.reserve(SECURITY_ORDER_IDS_MAP_CAPACITY);
    m_securitySnapshots.reserve(SECURITY_SNAPSHOTS_MAP_CAPACITY);
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
    const auto user{order.user()};
    const auto securityId{order.securityId()};

    const auto [orderIt, orderInserted] = m_orders.emplace(orderId, std::move(order));
    if (!orderInserted)
    {
        return;
    }

    _addOrderId(m_userOrders, user, orderId);
    _addOrderId(m_securityOrders, securityId, orderId);
    _updateSecuritySnapshots(orderIt->second, SecuritySnapshotAction::ADD_ORDER);
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
    _updateSecuritySnapshots(orderIt->second, SecuritySnapshotAction::REMOVE_ORDER);

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
    if (auto secSnapshotIt{m_securitySnapshots.find(securityId)}; secSnapshotIt != m_securitySnapshots.end())
    {
        const auto& snapshot{secSnapshotIt->second};
        const auto B{static_cast<int64_t>(snapshot.totalBuy)};
        const auto S{static_cast<int64_t>(snapshot.totalSell)};
        if (B == 0 || S == 0)
        {
            return 0;
        }

        const auto V{static_cast<int64_t>(snapshot.maxVolumes.empty() ? 0 : *snapshot.maxVolumes.rbegin())};
        const auto exBuy{std::max(static_cast<int64_t>(0), V - S)};
        const auto exSell{std::max(static_cast<int64_t>(0), V - B)};

        const auto matchBuy{std::max(static_cast<int64_t>(0), B - exBuy)};
        const auto matchSell{std::max(static_cast<int64_t>(0), S - exSell)};
        return std::min(matchBuy, matchSell);
    }
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
    for (const auto& idOrderPair : m_orders)
    {
        orders.emplace_back(idOrderPair.second);
    }
    return orders;
}

void OrderCache::_updateSecuritySnapshots(const Order& order, SecuritySnapshotAction action)
{
    auto& snapshot{m_securitySnapshots[order.securityId()]};
    if (action == SecuritySnapshotAction::ADD_ORDER)
    {
        _addOrderToSnapshot(order, snapshot);
        return;
    }
    _removeOrderFromSnapshot(order, snapshot);
}

void OrderCache::_addOrderToSnapshot(const Order& order, SecuritySnapshot& snapshot)
{
    auto& compVol = snapshot.companyVolumes[order.company()];

    const auto qty{order.qty()};
    const auto isBuy{order.side() == order_cache::BUY_SIDE};

    auto& total{isBuy ? snapshot.totalBuy : snapshot.totalSell};
    auto& volume{isBuy ? compVol.buy : compVol.sell};

    total += qty;
    volume += qty;

    snapshot.maxVolumes.emplace(compVol.buy + compVol.sell);
}

void OrderCache::_removeOrderFromSnapshot(const Order& order, SecuritySnapshot& snapshot)
{
    auto& compVol = snapshot.companyVolumes[order.company()];

    const auto qty{order.qty()};
    const auto isBuy{order.side() == order_cache::BUY_SIDE};
    const auto removeCompanyVolume{compVol.buy + compVol.sell};

    auto& total{isBuy ? snapshot.totalBuy : snapshot.totalSell};
    auto& volume{isBuy ? compVol.buy : compVol.sell};

    total -= qty;
    volume -= qty;

    snapshot.maxVolumes.erase(removeCompanyVolume);
}

void OrderCache::_addOrderId(std::unordered_map<std::string, std::vector<OrderID>>& map, const std::string& key,
                             const std::string& id)
{
    auto it{map.find(key)};
    if (it == map.end())
    {
        std::vector<OrderID> orderIds{id};
        orderIds.reserve(ORDER_IDS_VECTOR_CAPACITY);
        map.emplace_hint(it, key, std::move(orderIds));
    }
    else
    {
        it->second.emplace_back(id);
    }
}

void OrderCache::_removeOrderId(std::unordered_map<std::string, std::vector<OrderID>>& map, const std::string& key,
                                const std::string& id)
{
    if (auto mapIt{map.find(key)}; mapIt != map.end())
    {
        auto& orderIds{mapIt->second};
        auto idIt{std::find(orderIds.begin(), orderIds.end(), id)};
        if (idIt != orderIds.end())
        {
            std::swap(*idIt, orderIds.back());
            orderIds.pop_back();
        }

        if (orderIds.empty())
        {
            map.erase(mapIt);
        }
    }
}
