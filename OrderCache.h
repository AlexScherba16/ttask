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
    static constexpr size_t SECURITY_SNAPSHOTS_MAP_CAPACITY{2'048};
    static constexpr size_t COMPANY_ORDER_VOLUMES_MAP_CAPACITY{128};
    static constexpr size_t ORDER_IDS_VECTOR_CAPACITY{1'024};

    using OrderID = std::string;
    using User = std::string;
    using SecurityID = std::string;
    using Company = std::string;

    struct CompanyOrderVolume
    {
        uint64_t buy{0};
        uint64_t sell{0};
    };

    struct SecuritySnapshot
    {
        uint64_t totalBuy{0};
        uint64_t totalSell{0};
        std::vector<uint64_t> maxVolumes;
        std::unordered_map<Company, CompanyOrderVolume> companyVolumes;

        SecuritySnapshot()
        {
            maxVolumes.reserve(COMPANY_ORDER_VOLUMES_MAP_CAPACITY);
            companyVolumes.reserve(COMPANY_ORDER_VOLUMES_MAP_CAPACITY);
        }
    };

    enum class SecuritySnapshotAction : uint32_t
    {
        ADD_ORDER = 0,
        REMOVE_ORDER,
    };

    order_cache::storage::OrderIndexedStorage m_orderStorage;
    std::unordered_map<User, std::vector<OrderID>> m_userOrderIds;
    std::unordered_map<SecurityID, std::vector<OrderID>> m_securityOrderIds;
    std::unordered_map<SecurityID, SecuritySnapshot> m_securitySnapshots;


    void _updateSecuritySnapshots(const Order& order, SecuritySnapshotAction action);

    [[nodiscard]] static inline std::optional<uint64_t> _idToIndex(std::string_view id);
    static inline void _addOrderToSnapshot(const Order& order, SecuritySnapshot& snapshot);
    static inline void _removeOrderFromSnapshot(const Order& order, SecuritySnapshot& snapshot);

    static inline void _addOrderId(
        std::unordered_map<std::string, std::vector<OrderID>>& map,
        const std::string& key, const std::string& id);

    static inline void _removeOrderId(
        std::unordered_map<std::string, std::vector<OrderID>>& map,
        const std::string& key, const std::string& id);
};
