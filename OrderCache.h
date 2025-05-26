#pragma once

#include "Order.h"
#include "OrderIndexedStorage.h"

#include <cstdint>
#include <optional>
#include <unordered_map>


// Provide an implementation for the OrderCacheInterface interface class.
// Your implementation class should hold all relevant data structures you think
// are needed.
class OrderCacheInterface
{
public:
    virtual ~OrderCacheInterface() = default;

    // implement the 6 methods below, do not alter signatures

    // add order to the cache
    virtual void addOrder(Order order) = 0;

    // remove order with this unique order id from the cache
    virtual void cancelOrder(const std::string& orderId) = 0;

    // remove all orders in the cache for this user
    virtual void cancelOrdersForUser(const std::string& user) = 0;

    // remove all orders in the cache for this security with qty >= minQty
    virtual void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) = 0;

    // return the total qty that can match for the security id
    virtual unsigned int getMatchingSizeForSecurity(const std::string& securityId) = 0;

    // return all orders in cache in a vector
    virtual std::vector<Order> getAllOrders() const = 0;
};

class OrderCache : public OrderCacheInterface
{
public:
    OrderCache();
    OrderCache(const OrderCache&) = delete;
    OrderCache(OrderCache&&) = delete;
    OrderCache& operator=(const OrderCache&) = delete;
    OrderCache& operator=(OrderCache&&) = delete;
    ~OrderCache() override = default;

    void addOrder(Order order) override;

    void cancelOrder(const std::string& orderId) override;

    void cancelOrdersForUser(const std::string& user) override;

    void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) override;

    unsigned int getMatchingSizeForSecurity(const std::string& securityId) override;

    std::vector<Order> getAllOrders() const override;

private:
    static constexpr size_t ORDERS_STORAGE_CAPACITY{1'048'576};
    static constexpr size_t USER_ORDER_IDS_MAP_CAPACITY{2'048};
    static constexpr size_t SECURITY_ORDER_IDS_MAP_CAPACITY{2'048};
    static constexpr size_t ORDER_IDS_VECTOR_CAPACITY{1'128};


    using OrderIdIndex = uint64_t;
    using User = std::string_view;
    using SecurityID = std::string_view;
    using OrderIdsMap = std::unordered_map<std::string_view, std::vector<OrderIdIndex>>;

    order_cache::storage::OrderIndexedStorage m_orderStorage;
    OrderIdsMap m_userOrderIds;
    OrderIdsMap m_securityOrderIds;


    void _cancelOrderByIndex(uint64_t index);

    [[nodiscard]] static inline std::optional<uint64_t> _idToIndex(std::string_view id);

    static inline void _addOrderId(OrderIdsMap& map, std::string_view key, uint64_t id);
    static inline void _removeOrderId(OrderIdsMap& map, std::string_view key, uint64_t id);
};
