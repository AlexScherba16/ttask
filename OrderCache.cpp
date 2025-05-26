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

    const auto index{idValue.value()};
    m_orderStorage.addOrder(std::move(order), index);
    {
        const auto& tmp{m_orderStorage.getOrder(index)};
        _addOrderId(m_userOrderIds, tmp.userSv(), index);
        _addOrderId(m_securityOrderIds, tmp.securityIdSv(), index);
    }
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
    const auto secIt{m_securityOrderIds.find(securityId)};
    if (secIt == m_securityOrderIds.end())
    {
        return 0;
    }

    struct CompanyVolume
    {
        std::string_view companyName{};
        uint64_t buy{0};
        uint64_t sell{0};

        bool operator<(std::string_view name) const
        {
            return companyName < name;
        }
    };

    const auto ids{secIt->second};
    int64_t totalBuy{0};
    int64_t totalSell{0};
    uint64_t maxVolume{0};

    std::vector<CompanyVolume> companyOrders;
    companyOrders.reserve(ids.size());

    for (const auto index : ids)
    {
        if (!m_orderStorage.hasOrder(index))
        {
            continue;
        }

        auto& order{m_orderStorage.getOrder(index)};
        CompanyVolume tmp{order.companySv()};
        {
            auto isBuy{order.sideSv() == BUY_SIDE};
            auto& total{isBuy ? totalBuy : totalSell};
            auto& companyVolume{isBuy ? tmp.buy : tmp.sell};
            total += order.qty();
            companyVolume += order.qty();
        }

        auto it{std::lower_bound(companyOrders.begin(), companyOrders.end(), order.companySv())}; //, cmp)};
        if (it != companyOrders.end() && it->companyName == order.companySv())
        {
            it->buy += tmp.buy;
            it->sell += tmp.sell;
            maxVolume = std::max(maxVolume, it->buy + it->sell);
        }
        else
        {
            maxVolume = std::max(maxVolume, tmp.buy + tmp.sell);
            companyOrders.insert(it, tmp);
        }
    }

    if (totalBuy == 0 || totalSell == 0)
    {
        return 0;
    }

    const auto Vmax{static_cast<int64_t>(maxVolume)};
    const auto exBuy{std::max(static_cast<int64_t>(0), Vmax - totalSell)};
    const auto exSell{std::max(static_cast<int64_t>(0), Vmax - totalBuy)};
    const auto matchBuy{std::max(static_cast<int64_t>(0), totalBuy - exBuy)};
    const auto matchSell{std::max(static_cast<int64_t>(0), totalSell - exSell)};
    return std::min(matchBuy, matchSell);
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
    _removeOrderId(m_userOrderIds, order.userSv(), index);
    _removeOrderId(m_securityOrderIds, order.securityIdSv(), index);
    m_orderStorage.cancelOrder(index);
}

void OrderCache::_addOrderId(OrderIdsMap& map, std::string_view key, uint64_t id)
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

void OrderCache::_removeOrderId(OrderIdsMap& map, std::string_view key, uint64_t id)
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
