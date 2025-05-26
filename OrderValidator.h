#pragma once

#include "Order.h"

#include <sstream>

namespace order_cache::validator
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
            if (o.orderIdSv().empty()) { return OrderValidationError::EmptyOrderId; }
            if (auto err{_validateOrderIdFormat(o.orderIdSv())}; err != std::nullopt)
            {
                return err.value();
            }
            if (o.securityIdSv().empty()) { return OrderValidationError::EmptySecurityId; }
            if (o.userSv().empty()) { return OrderValidationError::EmptyUser; }
            if (o.companySv().empty()) { return OrderValidationError::EmptyCompany; }
            if (o.sideSv() != BUY_SIDE && o.sideSv() != SELL_SIDE) { return OrderValidationError::InvalidSide; }
            if (o.qty() == 0) { return OrderValidationError::ZeroQuantity; }

            return std::nullopt;
        }

        [[nodiscard]] static std::string errorToString(const OrderValidationError err) noexcept
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
}
