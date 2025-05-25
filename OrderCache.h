#pragma once

#include <charconv>
#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace order_cache
{
    static constexpr auto BUY_SIDE = "Buy";
    static constexpr auto SELL_SIDE = "Sell";
    static constexpr std::string_view ORDER_ID_PREFIX = "OrdId";

    namespace helpers
    {
        class OrderValidator;
    }
}

class Order
{
public:
    // do not alter signature of this constructor
    Order(
        const std::string& ordId,
        const std::string& secId,
        const std::string& side,
        const unsigned int qty,
        const std::string& user,
        const std::string& company)
        : m_orderId(ordId),
          m_securityId(secId),
          m_side(side),
          m_qty(qty),
          m_user(user),
          m_company(company)
    {
    }

    // do not alter these accessor methods
    std::string orderId() const { return m_orderId; }
    std::string securityId() const { return m_securityId; }
    std::string side() const { return m_side; }
    std::string user() const { return m_user; }
    std::string company() const { return m_company; }
    unsigned int qty() const { return m_qty; }

    // avoid redundant string copies
    [[nodiscard]] std::string_view orderIdSv() const { return m_orderId; }

private:
    // use the below to hold the order data
    // do not remove the these member variables
    std::string m_orderId; // unique order id
    std::string m_securityId; // security identifier
    std::string m_side; // side of the order, eg Buy or Sell
    unsigned int m_qty; // qty for this order
    std::string m_user; // user name who owns this order
    std::string m_company; // company for user

    friend order_cache::helpers::OrderValidator;
};

namespace order_cache::helpers
{
    class OrderValidator final
    {
    public:
        enum class OrderValidationError : uint32_t
        {
            EmptyOrderId = 0,
            InvalidOrderIdFormat,
            EmptySecurityId,
            EmptyUser,
            EmptyCompany,
            InvalidSide,
            ZeroQuantity,
        };

        [[nodiscard]] static std::optional<OrderValidationError> validateOrder(const Order& o) noexcept
        {
            if (o.m_orderId.empty()) { return OrderValidationError::EmptyOrderId; }
            if (auto err{_validateOrderIdFormat(o.orderIdSv())}; err != std::nullopt)
            {
                return err.value();
            }
            if (o.m_securityId.empty()) { return OrderValidationError::EmptySecurityId; }
            if (o.m_user.empty()) { return OrderValidationError::EmptyUser; }
            if (o.m_company.empty()) { return OrderValidationError::EmptyCompany; }
            if (o.m_side != BUY_SIDE && o.m_side != SELL_SIDE) { return OrderValidationError::InvalidSide; }
            if (o.m_qty == 0) { return OrderValidationError::ZeroQuantity; }

            return std::nullopt;
        }

        [[nodiscard]] static std::string orderErrorToString(const OrderValidationError err) noexcept
        {
            switch (err)
            {
            case OrderValidationError::EmptyOrderId: return "Empty order ID";
            case OrderValidationError::InvalidOrderIdFormat:
                {
                    std::stringstream message;
                    message << "Expected order ID format \"" << ORDER_ID_PREFIX << 123 << "\"";
                    return message.str();
                }
            case OrderValidationError::EmptySecurityId: return "Empty security ID";
            case OrderValidationError::EmptyUser: return "Empty user";
            case OrderValidationError::EmptyCompany: return "Empty company";
            case OrderValidationError::InvalidSide: return "Invalid side";
            case OrderValidationError::ZeroQuantity: return "Zero quantity";
            default: return "Unknown error";
            }
        }

    private:
        [[nodiscard]] static std::optional<OrderValidationError> _validateOrderIdFormat(std::string_view id)
        {
            {
                const auto pos{id.find(ORDER_ID_PREFIX)};
                if (pos != 0 || pos == std::string::npos)
                {
                    return OrderValidationError::InvalidOrderIdFormat;
                }
            }

            for (size_t i = ORDER_ID_PREFIX.size(); i < id.size(); ++i)
            {
                if (!std::isdigit(id[i]))
                {
                    return OrderValidationError::InvalidOrderIdFormat;
                }
            }

            return std::nullopt;
        }
    };


    class OrderLinearStorage final
    {
    public:
        OrderLinearStorage(const size_t minSize) { m_orders.resize(minSize); }
        OrderLinearStorage(OrderLinearStorage&&) = delete;
        OrderLinearStorage& operator=(OrderLinearStorage&&) = delete;
        OrderLinearStorage(const OrderLinearStorage&) = delete;
        OrderLinearStorage& operator=(const OrderLinearStorage&) = delete;

        enum class OrderStorageError : uint32_t
        {
            FailedToParseOrderIdValue = 0,
            OrderAlreadyStored,
            NoOrder,
        };

        void addOrder(Order&& order, const uint64_t index)
        {
            if (index > m_orders.size() - 1)
            {
                m_orders.resize(index + 1);
            }
            m_orders[index].order = std::move(order);
            m_indexes.emplace(index);
        };

        [[nodiscard]] bool hasOrder(const uint64_t index) const
        {
            return m_indexes.find(index) != m_indexes.end();
        }

        [[nodiscard]] const Order& getOrder(const uint64_t index) const
        {
            return m_orders[index].order;
        }

        void cancelOrder(const uint64_t index)
        {
            if (const auto indexIt{m_indexes.find(index)}; indexIt != m_indexes.end())
            {
                m_indexes.erase(indexIt);
            }
        }

        [[nodiscard]] std::vector<Order> getAllOrders() const
        {
            if (m_indexes.empty()) { return {}; }

            std::vector<Order> result;
            result.reserve(m_indexes.size());
            for (const auto& index : m_indexes)
            {
                result.push_back(m_orders[index].order);
            }
            return result;
        }

    private:
        struct StoredOrder
        {
            Order order{"", "", "", 0, "", ""};
        };

        std::vector<StoredOrder> m_orders;
        std::set<uint64_t> m_indexes;
    };
}


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
        std::multiset<uint64_t> maxVolumes;
        std::unordered_map<Company, CompanyOrderVolume> companyVolumes;

        SecuritySnapshot()
        {
            companyVolumes.reserve(COMPANY_ORDER_VOLUMES_MAP_CAPACITY);
        }
    };

    enum class SecuritySnapshotAction : uint32_t
    {
        ADD_ORDER = 0,
        REMOVE_ORDER,
    };


    order_cache::helpers::OrderLinearStorage m_storage;
    std::unordered_map<User, std::vector<OrderID>> m_userOrders;
    std::unordered_map<SecurityID, std::vector<OrderID>> m_securityOrders;
    std::unordered_map<SecurityID, SecuritySnapshot> m_securitySnapshots;


    [[nodiscard]] std::optional<uint64_t> _idToStorageIndex(std::string_view id);

    void _updateSecuritySnapshots(const Order& order, SecuritySnapshotAction action);

    static inline void _addOrderToSnapshot(const Order& order, SecuritySnapshot& snapshot);
    static inline void _removeOrderFromSnapshot(const Order& order, SecuritySnapshot& snapshot);

    static inline void _addOrderId(
        std::unordered_map<std::string, std::vector<OrderID>>& map,
        const std::string& key, const std::string& id);

    static inline void _removeOrderId(
        std::unordered_map<std::string, std::vector<OrderID>>& map,
        const std::string& key, const std::string& id);
};
