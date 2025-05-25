#pragma once

#include "OrderCache.h"

#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>


namespace order_cache
{
    template <size_t MIN_SIZE>
    class OrderLinearStorage final
    {
    public:
        OrderLinearStorage()
        {
            m_orders.resize(MIN_SIZE);
        }

    private:
        struct StoredOrder
        {
            bool isValid{false};
            Order order;
        };

        std::vector<StoredOrder> m_orders;
    };
}
