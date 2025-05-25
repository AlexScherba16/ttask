#include "OrderCache.h"

#include <algorithm>

namespace Helper = order_cache::helpers;
using Storage = order_cache::helpers::OrderLinearStorage;

OrderCache::OrderCache() : m_storage(ORDERS_STORAGE_CAPACITY)
{
    m_userOrders.reserve(USER_ORDER_IDS_MAP_CAPACITY);
    m_securityOrders.reserve(SECURITY_ORDER_IDS_MAP_CAPACITY);
    m_securitySnapshots.reserve(SECURITY_SNAPSHOTS_MAP_CAPACITY);
}

void OrderCache::addOrder(Order order)
{
    if (auto err{Helper::OrderValidator::validateOrder(order)})
    {
        std::stringstream message;
        message << "Invalid order : " << Helper::OrderValidator::orderErrorToString(err.value());
        throw std::invalid_argument(message.str());
    }

    const auto idValue{_idToStorageIndex(order.orderIdSv())};
    if (!idValue.has_value())
    {
        throw std::invalid_argument("Failed to parse order ID value : " + order.orderId());
    }

    if (m_storage.hasOrder(idValue.value()))
    {
        return;
    }

    const auto orderId{order.orderId()};
    const auto user{order.user()};
    const auto securityId{order.securityId()};
    const auto index{idValue.value()};

    _addOrderId(m_userOrders, user, orderId);
    _addOrderId(m_securityOrders, securityId, orderId);
    _updateSecuritySnapshots(order, SecuritySnapshotAction::ADD_ORDER);
    m_storage.addOrder(std::move(order), index);
}

void OrderCache::cancelOrder(const std::string& orderId)
{
    const auto idValue{_idToStorageIndex(orderId)};
    if (!idValue.has_value())
    {
        throw std::invalid_argument("Failed to parse order ID value due cancellation : " + orderId);
    }
    const auto index{idValue.value()};

    if (!m_storage.hasOrder(index))
    {
        return;
    }

    const auto& order{m_storage.getOrder(index)};
    const auto user{order.user()};
    const auto securityId{order.securityId()};

    _removeOrderId(m_userOrders, user, orderId);
    _removeOrderId(m_securityOrders, securityId, orderId);
    _updateSecuritySnapshots(order, SecuritySnapshotAction::REMOVE_ORDER);

    m_storage.cancelOrder(index);
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

    auto securityOrdersIt{m_securityOrders.find(securityId)};
    if (securityOrdersIt == m_securityOrders.end())
    {
        return;
    }

    auto orderIds{securityOrdersIt->second};
    for (const auto& orderId : orderIds)
    {
        const auto idValue{_idToStorageIndex(orderId)};
        if (!idValue.has_value())
        {
            throw std::invalid_argument("Failed to parse order ID value due cancellation for security : " + orderId);
        }

        const auto index{idValue.value()};
        if (!m_storage.hasOrder(index))
        {
            continue;
        }

        const auto& order{m_storage.getOrder(index)};
        if (order.qty() >= minQty)
        {
            cancelOrder(orderId);
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
    return m_storage.getAllOrders();
}

std::optional<uint64_t> OrderCache::_idToStorageIndex(std::string_view id)
{
    // if (id.size() <= order_cache::ORDER_ID_PREFIX.size())
    //     return std::nullopt;
    //
    // const auto from{id.data() + order_cache::ORDER_ID_PREFIX.size()};
    // const auto to{id.data() + id.size()};
    // uint64_t result{0};
    // auto [_, ec] = std::from_chars(from, to, result);
    // if (ec != std::errc{})
    // {
    //     return std::nullopt;
    // }
    // return result;

    constexpr auto prefix = order_cache::ORDER_ID_PREFIX;
    constexpr std::size_t prefixLen = prefix.size();

    // 1. длина
    if (id.size() <= prefixLen)
        return std::nullopt;

    // 2. префикс
    if (id.compare(0, prefixLen, prefix) != 0)
        return std::nullopt;

    // 3. числовая часть
    std::string_view numPart = id.substr(prefixLen);
    uint64_t value = 0;

    auto [ptr, ec] = std::from_chars(numPart.data(),
                                     numPart.data() + numPart.size(),
                                     value);
    // полностью ли разобрали и нет ли ошибки
    if (ec != std::errc{} || ptr != numPart.data() + numPart.size())
        return std::nullopt;

    return value;
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
