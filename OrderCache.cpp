#include "OrderCache.h"
#include "OrderValidator.h"

#include <sstream>
#include <charconv>
#include <algorithm>

using namespace order_cache::validator;

OrderCache::OrderCache() : m_orderStorage(ORDERS_STORAGE_CAPACITY)
{
    m_userOrderIds.reserve(USER_ORDER_IDS_MAP_CAPACITY);
    m_securityOrderIds.reserve(SECURITY_ORDER_IDS_MAP_CAPACITY);
    m_securitySnapshots.reserve(SECURITY_SNAPSHOTS_MAP_CAPACITY);
}

void OrderCache::addOrder(Order order)
{
    if (auto err{OrderValidator::validateOrder(order)})
    {
        std::stringstream message;
        message << "Invalid order : " << OrderValidator::errorToString(err.value());
        throw std::invalid_argument(message.str());
    }

    const auto idValue{_idToIndex(order.orderIdSv())};
    if (!idValue.has_value())
    {
        throw std::invalid_argument("Failed to parse order ID value due adding : " + order.orderId());
    }

    if (m_orderStorage.hasOrder(idValue.value()))
    {
        return;
    }

    const auto orderId{order.orderId()};
    const auto user{order.user()};
    const auto securityId{order.securityId()};
    const auto index{idValue.value()};

    _addOrderId(m_userOrderIds, user, index);
    _addOrderId(m_securityOrderIds, securityId, index);
    _updateSecuritySnapshots(order, SecuritySnapshotAction::ADD_ORDER);
    m_orderStorage.addOrder(std::move(order), index);
}

void OrderCache::cancelOrder(const std::string& orderId)
{
    const auto idValue{_idToIndex(orderId)};
    if (!idValue.has_value())
    {
        throw std::invalid_argument("Failed to parse order ID value due cancellation : " + orderId);
    }

    const auto index{idValue.value()};
    if (!m_orderStorage.hasOrder(index))
    {
        return;
    }
    _cancelOrderByIndex(index);
}

void OrderCache::cancelOrdersForUser(const std::string& user)
{
    if (auto userOrdersIt{m_userOrderIds.find(user)}; userOrdersIt != m_userOrderIds.end())
    {
        auto orderIds{userOrdersIt->second};
        for (const auto index : orderIds)
        {
            if (!m_orderStorage.hasOrder(index))
            {
                continue;
            }
            _cancelOrderByIndex(index);
        }
    }
}

void OrderCache::cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty)
{
    if (minQty == 0)
    {
        return;
    }

    auto securityOrdersIt{m_securityOrderIds.find(securityId)};
    if (securityOrdersIt == m_securityOrderIds.end())
    {
        return;
    }

    auto orderIds{securityOrdersIt->second};
    for (const auto index : orderIds)
    {
        if (!m_orderStorage.hasOrder(index) || m_orderStorage.getOrder(index).qty() < minQty)
        {
            continue;
        }
        _cancelOrderByIndex(index);
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

        // instead of iterating over unmatched buy/sell per company, we are proceeding to the maximum volume (V)
        // company-leader for current security
        const auto V{static_cast<int64_t>(snapshot.maxVolumes.empty() ? 0 : snapshot.maxVolumes.back())};
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
    return m_orderStorage.getAllOrders();
}

std::optional<uint64_t> OrderCache::_idToIndex(std::string_view id)
{
    constexpr auto prefixLen{ORDER_ID_PREFIX.size()};

    // ID less than expected prefix
    if (id.size() <= prefixLen)
    {
        return std::nullopt;
    }

    // expected ID prefix is right at the very beginning of the string
    if (id.compare(0, prefixLen, ORDER_ID_PREFIX) != 0)
    {
        return std::nullopt;
    }

    // retrieve digital part of ID
    std::string_view numPart = id.substr(prefixLen);
    uint64_t value = 0;

    auto [_, ec] = std::from_chars(numPart.data(),
                                   numPart.data() + numPart.size(),
                                   value);
    if (ec != std::errc{})
    {
        return std::nullopt;
    }
    return value;
}

void OrderCache::_cancelOrderByIndex(uint64_t index)
{
    const auto& order{m_orderStorage.getOrder(index)};
    const auto user{order.user()};
    const auto securityId{order.securityId()};

    _removeOrderId(m_userOrderIds, user, index);
    _removeOrderId(m_securityOrderIds, securityId, index);
    _updateSecuritySnapshots(order, SecuritySnapshotAction::REMOVE_ORDER);
    m_orderStorage.cancelOrder(index);
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
    const auto isBuy{order.sideSv() == BUY_SIDE};

    auto& total{isBuy ? snapshot.totalBuy : snapshot.totalSell};
    auto& volume{isBuy ? compVol.buy : compVol.sell};

    total += qty;
    volume += qty;

    const auto insertVolume{compVol.buy + compVol.sell};
    snapshot.maxVolumes.insert(std::lower_bound(snapshot.maxVolumes.begin(),
                                                snapshot.maxVolumes.end(), insertVolume),
                               insertVolume);
}

void OrderCache::_removeOrderFromSnapshot(const Order& order, SecuritySnapshot& snapshot)
{
    auto& compVol = snapshot.companyVolumes[order.company()];

    const auto qty{order.qty()};
    const auto isBuy{order.sideSv() == BUY_SIDE};
    const auto removeCompanyVolume{compVol.buy + compVol.sell};

    auto& total{isBuy ? snapshot.totalBuy : snapshot.totalSell};
    auto& volume{isBuy ? compVol.buy : compVol.sell};

    total -= qty;
    volume -= qty;

    auto removeIt{std::lower_bound(snapshot.maxVolumes.begin(), snapshot.maxVolumes.end(), removeCompanyVolume)};
    if (removeIt != snapshot.maxVolumes.end() && *removeIt == removeCompanyVolume)
    {
        snapshot.maxVolumes.erase(removeIt);
    }
}

void OrderCache::_addOrderId(std::unordered_map<std::string, std::vector<uint64_t>>& map, const std::string& key,
                             const uint64_t id)
{
    auto mapIt{map.find(key)};
    if (mapIt == map.end())
    {
        std::vector<uint64_t> orderIds{id};
        orderIds.reserve(ORDER_IDS_VECTOR_CAPACITY);
        map.emplace_hint(mapIt, key, std::move(orderIds));
    }
    else
    {
        mapIt->second.emplace_back(id);
    }
}

void OrderCache::_removeOrderId(std::unordered_map<std::string, std::vector<uint64_t>>& map, const std::string& key,
                                uint64_t id)
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
