#pragma once

#include <string>
#include <string_view>

static constexpr std::string_view BUY_SIDE = "Buy";
static constexpr std::string_view SELL_SIDE = "Sell";
static constexpr std::string_view ORDER_ID_PREFIX = "OrdId";

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

    [[nodiscard]] std::string_view orderIdSv() const { return m_orderId; }
    [[nodiscard]] std::string_view securityIdSv() const { return m_securityId; }
    [[nodiscard]] std::string_view sideSv() const { return m_side; }
    [[nodiscard]] std::string_view userSv() const { return m_user; }
    [[nodiscard]] std::string_view companySv() const { return m_company; }

private:
    // use the below to hold the order data
    // do not remove the these member variables
    std::string m_orderId; // unique order id
    std::string m_securityId; // security identifier
    std::string m_side; // side of the order, eg Buy or Sell
    unsigned int m_qty; // qty for this order
    std::string m_user; // user name who owns this order
    std::string m_company; // company for user
};
