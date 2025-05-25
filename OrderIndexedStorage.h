#pragma once

#include "Order.h"

#include <vector>
#include <cstdint>
#include <limits>

namespace order_cache::storage
{
    class OrderIndexedStorage final
    {
    public:
        explicit OrderIndexedStorage(std::size_t minSize = 0)
        {
            m_orders.resize(minSize, Order{"", "", "", 0, "", ""});
            m_orderPositions.resize(minSize, INVALID_ORDER_POSITION);
            m_aliveOrderIndexes.reserve(minSize);
        }

        OrderIndexedStorage(OrderIndexedStorage&&) = delete;
        OrderIndexedStorage& operator=(OrderIndexedStorage&&) = delete;
        OrderIndexedStorage(const OrderIndexedStorage&) = delete;
        OrderIndexedStorage& operator=(const OrderIndexedStorage&) = delete;

        void addOrder(Order&& order, uint64_t index)
        {
            if (index >= m_orders.size())
            {
                m_orders.resize(index + 1, Order{"", "", "", 0, "", ""});
                m_orderPositions.resize(index + 1, INVALID_ORDER_POSITION);
            }

            m_orderPositions[index] = static_cast<uint64_t>(m_aliveOrderIndexes.size());
            m_aliveOrderIndexes.emplace_back(index);
            m_orders[index] = std::move(order);
        }

        [[nodiscard]] bool hasOrder(uint64_t index) const noexcept
        {
            return index < m_orderPositions.size() && m_orderPositions[index] != INVALID_ORDER_POSITION;
        }

        [[nodiscard]] const Order& getOrder(uint64_t index) const noexcept
        {
            return m_orders[index];
        }

        void cancelOrder(uint64_t index) noexcept
        {
            const auto removePosition{m_orderPositions[index]};
            const auto lastAliveIdx{m_aliveOrderIndexes.back()};

            m_aliveOrderIndexes[removePosition] = lastAliveIdx;
            m_orderPositions[lastAliveIdx] = removePosition;
            m_aliveOrderIndexes.pop_back();

            m_orderPositions[index] = INVALID_ORDER_POSITION;
        }

        [[nodiscard]] std::vector<Order> getAllOrders() const
        {
            std::vector<Order> result;
            result.reserve(m_aliveOrderIndexes.size());
            for (uint64_t idx : m_aliveOrderIndexes)
                result.emplace_back(m_orders[idx]);
            return result;
        }

    private:
        std::vector<Order> m_orders;
        std::vector<uint64_t> m_orderPositions;
        std::vector<uint64_t> m_aliveOrderIndexes;

        static constexpr uint64_t INVALID_ORDER_POSITION{std::numeric_limits<uint32_t>::max()};
    };
}
