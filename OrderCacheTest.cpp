#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include "OrderCache.h"
#include "gtest/gtest.h"

using namespace std::chrono_literals;

// Global flag to indicate test failure
std::atomic<bool> test_failed{false};

// Custom GTest event listener to set the flag on failure
class FailureListener : public ::testing::EmptyTestEventListener {
    void OnTestPartResult(const ::testing::TestPartResult& result) override {
        if (result.failed()) {
            test_failed = true;
        }
    }
};

// Macro to check the global failure flag
#define CHECK_GLOBAL_FAILURE_FLAG() \
    if (test_failed) { \
        GTEST_SKIP() << "\033[38;5;214mTest skipped due to a previous failure.\033[0m"; \
        return; \
    }

class OrderCacheTest : public ::testing::Test {
protected:
    OrderCache cache;
    std::vector<std::string> users;
    std::vector<std::string> secIds;
    std::vector<std::string> companies;
    std::vector<std::string> sides{"Buy", "Sell"};
    std::mt19937 gen;
    static double benchmark_time;  // Static variable to store the benchmark time

    // Constants for the test
    static constexpr unsigned int NUM_USERS = 1000;
    static constexpr unsigned int NUM_COMPANIES = 100;
    static constexpr unsigned int NUM_SECURITIES = 1000;
    static constexpr unsigned int ORDER_MIN_QTY = 100;
    static constexpr unsigned int ORDER_MAX_QTY = 5000;
    static constexpr unsigned int ORDER_QTY_MULTIPLIER = 100;

    // ANSI escape code for blue text
    const char* BLUE_COLOR = "\033[34m";

    // ANSI escape code to reset color
    const char* RESET_COLOR = "\033[0m";

    // Set up the test environment
    void SetUp() override {
        // Populating users, companies, and securities
        for (int i = 0; i < NUM_USERS; i++) {
            users.push_back("User" + std::to_string(i));
        }
        for (int i = 0; i < NUM_COMPANIES; i++) {
            companies.push_back("Comp" + std::to_string(i));
        }
        for (int i = 0; i < NUM_SECURITIES; i++) {
            secIds.push_back("SecId" + std::to_string(i));
        }
        // Using a fixed seed for reproducibility
        std::seed_seq seed{1, 2, 3, 4, 5};
        gen = std::mt19937(seed);
    }

    std::vector<Order> generateOrders(unsigned int numOrders) {
        std::vector<Order> orders;
        std::uniform_int_distribution<int> usersDist(0, users.size() - 1);
        std::uniform_int_distribution<int> companiesDist(0, companies.size() - 1);
        std::uniform_int_distribution<int> secIdsDist(0, secIds.size() - 1);
        std::uniform_int_distribution<int> sidesDist(0, sides.size() - 1);
        std::uniform_int_distribution<int> qtyDist(1, 50);

        for (int i = 0; i < numOrders; i++) {
            const auto& user = users[usersDist(gen)];
            const auto& company = companies[companiesDist(gen)];
            const auto& secId = secIds[secIdsDist(gen)];
            const auto& side = sides[sidesDist(gen)];
            unsigned int qty = qtyDist(gen) * ORDER_QTY_MULTIPLIER;

            orders.push_back(Order{"OrdId" + std::to_string(i), secId, side, qty, user, company});
        }

        return orders;
    }

    static void SetUpTestCase() {
        const char* BLUE_COLOR = "\033[34m";
        const char* RESET_COLOR = "\033[0m";
        const char* TEST_VERSION = "1.4";

        // Test version
        std::cout << BLUE_COLOR << "[     INFO ] Test version: " << TEST_VERSION << RESET_COLOR << std::endl;

        // Calculate the Fibonacci number as the benchmark
        auto start = std::chrono::high_resolution_clock::now();
        volatile int fib = fibRecursive(30); // Calculation (Fibonacci of 30)
        auto end = std::chrono::high_resolution_clock::now();
        benchmark_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << BLUE_COLOR << "[     INFO ] 1 NCU = " << benchmark_time << "ms" << RESET_COLOR << std::endl;
    }

    static int fibRecursive(int n) {
        if (n <= 1) return n;
        return fibRecursive(n - 1) + fibRecursive(n - 2);
    }
};

double OrderCacheTest::benchmark_time = 0; // Initialize static benchmark_time

// Do not use Boost C++ Library
TEST_F(OrderCacheTest, ThirdParty_Dependencies_BoostNotUsed) {
    #ifdef BOOST_VERSION
        FAIL() << "Boost is not allowed to be used.";
    #else
        SUCCEED();
    #endif
}

// BasicOperations: Add order
TEST_F(OrderCacheTest, BasicOperations_AddOrder_AddOrderWithoutException) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add buy order
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});

    // Assert that order has been added
    ASSERT_EQ(cache.getAllOrders().size(), 1);

    // Add sell order
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});

    // Assert that order has been added
    ASSERT_EQ(cache.getAllOrders().size(), 2);
}

// BasicOperations: Get all orders
TEST_F(OrderCacheTest, BasicOperations_GetAllOrders_ReturnsCorrectNumberOfOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 500, "User3", "CompanyA"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 600, "User4", "CompanyC"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy", 100, "User5", "CompanyB"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 1000, "User6", "CompanyD"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Buy", 2000, "User7", "CompanyE"});
    cache.addOrder(Order{"OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE"});

    ASSERT_EQ(cache.getAllOrders().size(), 8);
}

// BasicOperations: Cancel a specific order
TEST_F(OrderCacheTest, BasicOperations_CancelOrder_RemovesSpecificOrderById) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 100, "User1", "Company1"});
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);

      // Cancel specific order
    cache.cancelOrder("OrdId1");

    // Assuming getAllOrders() returns all current orders, the size should be 0 for an empty cache.
    allOrders = cache.getAllOrders();
    ASSERT_TRUE(allOrders.empty());
}

// BasicOperations: Cancel all orders for a specific user
TEST_F(OrderCacheTest, BasicOperations_CancelOrdersForUser_RemovesAllUserOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 300, "User1", "Company1"});
    cache.addOrder(Order{"OrdId3", "SecId3", "Buy", 400, "User2", "Company2"});

    // Cancel all orders for User1
    cache.cancelOrdersForUser("User1");

    std::vector<Order> allOrders = cache.getAllOrders();
    // Only the order for User2 should remain
    ASSERT_EQ(allOrders.size(), 1);
    ASSERT_EQ(allOrders[0].orderId(), "OrdId3");
}

// BasicOperations: Cancel all orders for a security with minimum quantity
TEST_F(OrderCacheTest, BasicOperations_CancelOrdersWithMinimumQty_RemovesQualifyingOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 200, "User2", "Company1"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 100, "User1", "Company1"});
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 300);
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 3);

    // Cancel all orders with security ID 1 and minimum quantity 200
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 200);
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);

    // Cancel all orders with security ID 1 and minimum quantity 100
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 100);
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 0);
}

// MatchingSize: First example from README.txt
TEST_F(OrderCacheTest, MatchingSize_ReadmeExample1_MatchesCorrectly) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add orders exactly as in README example 1
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy",  1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell",  500, "User3", "CompanyA"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy",   600, "User4", "CompanyC"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy",   100, "User5", "CompanyB"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy",  1000, "User6", "CompanyD"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Buy",  2000, "User7", "CompanyE"});
    cache.addOrder(Order{"OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE"});

    // Test for SecId1: should have no matches because both orders are from CompanyA
    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 0);

    // Test for SecId2: should match with total quantity of 2700
    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 2700);

    // Test for SecId3: should have no matches because there's only one Buy order
    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 0);
}

// MatchingSize: Second example from README.txt
TEST_F(OrderCacheTest, MatchingSize_ReadmeExample2_MatchesCorrectly) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Sell", 100, "User10", "Company2"});
    cache.addOrder(Order{"OrdId2", "SecId3", "Sell", 200, "User8", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 300, "User13", "Company2"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Sell", 400, "User12", "Company2"});
    cache.addOrder(Order{"OrdId5", "SecId3", "Sell", 500, "User7", "Company2"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 600, "User3", "Company1"});
    cache.addOrder(Order{"OrdId7", "SecId1", "Sell", 700, "User10", "Company2"});
    cache.addOrder(Order{"OrdId8", "SecId1", "Sell", 800, "User2", "Company1"});
    cache.addOrder(Order{"OrdId9", "SecId2", "Buy", 900, "User6", "Company2"});
    cache.addOrder(Order{"OrdId10", "SecId2", "Sell", 1000, "User5", "Company1"});
    cache.addOrder(Order{"OrdId11", "SecId1", "Sell", 1100, "User13", "Company2"});
    cache.addOrder(Order{"OrdId12", "SecId2", "Buy", 1200, "User9", "Company2"});
    cache.addOrder(Order{"OrdId13", "SecId1", "Sell", 1300, "User1", "Company1"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 300);

    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 1000);

    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 600);
}

// MatchingSize: Third example from README.txt
TEST_F(OrderCacheTest, MatchingSize_ReadmeExample3_MatchesCorrectly) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId3", "Sell", 100, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId3", "Sell", 200, "User3", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 300, "User2", "Company1"});
    cache.addOrder(Order{"OrdId4", "SecId3", "Sell", 400, "User5", "Company2"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Sell", 500, "User2", "Company1"});
    cache.addOrder(Order{"OrdId6", "SecId2", "Buy", 600, "User3", "Company2"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Sell", 700, "User1", "Company1"});
    cache.addOrder(Order{"OrdId8", "SecId1", "Sell", 800, "User2", "Company1"});
    cache.addOrder(Order{"OrdId9", "SecId1", "Buy", 900, "User5", "Company2"});
    cache.addOrder(Order{"OrdId10", "SecId1", "Sell", 1000, "User1", "Company1"});
    cache.addOrder(Order{"OrdId11", "SecId2", "Sell", 1100, "User6", "Company2"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 900);

    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 600);

    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 0);
}

// MatchingSize: Matching orders with different quantities
TEST_F(OrderCacheTest, MatchingSize_OneToMany_MatchesMultipleSellers) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 5000, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 2000, "User2", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 1000, "User3", "Company3"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 3000); // 2000 from Order 2 and 1000 from Order 3 should match with Order 1
}

// MatchingSize: Matching complex order combinations
TEST_F(OrderCacheTest, MatchingSize_ComplexCombinations_MatchesCorrectly) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId2", "Buy", 7000, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId2", "Sell", 4000, "User3", "Company3"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 500, "User4", "Company4"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Sell", 500, "User5", "Company5"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    // 7000 from Order 1 and 500 from Order 4 should fully match with Orders 2, 3, and 5
    ASSERT_EQ(matchingSize, 7500);
}

// MatchingSize: Orders from the same company should not match
TEST_F(OrderCacheTest, MatchingSize_SameCompany_DoesNotMatch) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId3", "Buy", 2000, "User1", "Company1"});
     // Adding an order with the same company
    cache.addOrder(Order{"OrdId2", "SecId3", "Sell", 2000, "User2", "Company1"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    // No match should occur since both orders are from the same company
    ASSERT_EQ(matchingSize, 0);
}

// MatchingSize: Multiple small orders matching a larger order
TEST_F(OrderCacheTest, MatchingSize_LargeBuyer_MatchesWithMultipleSmallSellers) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 10000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 2000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 1500, "User3", "CompanyC"});
    cache.addOrder(Order{"OrdId4", "SecId1", "Sell", 2500, "User4", "CompanyD"});
    cache.addOrder(Order{"OrdId5", "SecId1", "Sell", 4000, "User5", "CompanyE"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    // Total of 10000 from orders 2, 3, 4, and 5 should match with order 1
    ASSERT_EQ(matchingSize, 10000);
}

// MatchingSize: Multiple matching combinations
TEST_F(OrderCacheTest, MatchingSize_ManyToMany_MatchesBidirectionally) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId2", "Buy", 6000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 2000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId2", "Sell", 3000, "User3", "CompanyC"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 1000, "User4", "CompanyD"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Sell", 1500, "User5", "CompanyE"});

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    // Total of 6500 (2000 from Order 2, 3000 from Order 3, 1500 from Order 5) should match with Orders 1 and 4
    ASSERT_EQ(matchingSize, 6500);
}

// MatchingSize: Matching size when only buy orders are present should be zero.
TEST_F(OrderCacheTest, MatchingSize_OnlyBuyOrders_ReturnsZero) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 100, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Buy", 200, "User2", "Company2"});
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId1"), 0);
}

// MatchingSize: Matching size when only sell orders are present should be zero.
TEST_F(OrderCacheTest, MatchingSize_OnlySellOrders_ReturnsZero) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Sell", 150, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 250, "User2", "Company2"});
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId1"), 0);
}

// EdgeCases: Test that adding an order with an empty order ID
TEST_F(OrderCacheTest, EdgeCases_AddOrder_EmptyOrderId) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an empty order ID
    Order orderWithEmptyId{"", "SecId1", "Buy", 500, "User1", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithEmptyId);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that adding an order with an empty security ID
TEST_F(OrderCacheTest, EdgeCases_AddOrder_EmptySecurityId) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an empty security ID
    Order orderWithEmptySecId{"OrdId1", "", "Buy", 500, "User1", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithEmptySecId);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that adding an order with an empty user ID
TEST_F(OrderCacheTest, EdgeCases_AddOrder_EmptyUserId) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an empty user ID
    Order orderWithEmptyUser{"OrdId1", "SecId1", "Buy", 500, "", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithEmptyUser);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that adding an order with an empty company name
TEST_F(OrderCacheTest, EdgeCases_AddOrder_EmptyCompany) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an empty company name
    Order orderWithEmptyCompany{"OrdId1", "SecId1", "Buy", 500, "User1", ""};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithEmptyCompany);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 0);
}

// EdgeCases: Test that adding an order with an empty side
TEST_F(OrderCacheTest, EdgeCases_AddOrder_EmptySide) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an empty side
    Order orderWithEmptySide{"OrdId1", "SecId1", "", 500, "User1", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithEmptySide);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that adding an order with an invalid side
TEST_F(OrderCacheTest, EdgeCases_AddOrder_InvalidSide) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Create an order with an invalid side
    Order orderWithInvalidSide{"OrdId1", "SecId1", "Hold", 500, "User1", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(orderWithInvalidSide);
    } catch (...) {
      // Do nothing
    }

    // Verify that no order was added to the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that adding order with zero quantity
TEST_F(OrderCacheTest, EdgeCases_AddOrder_ZeroQuantity) {
    CHECK_GLOBAL_FAILURE_FLAG();

    Order zeroQuantityOrder{"OrdId1", "SecId1", "Buy", 0, "User1", "Company1"};

    // Attempt to add the order
    try {
      cache.addOrder(zeroQuantityOrder);
    } catch (...) {
      // Do nothing
    }

    // Nothing should change in the cache
    ASSERT_EQ(cache.getAllOrders().size(), 0);
}

// EdgeCases: Test that attempting to replace an existing order
TEST_F(OrderCacheTest, EdgeCases_AddOrder_ReplaceExistingOrder) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add the original order
    Order originalOrder{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"};
    cache.addOrder(originalOrder);

    // Verify the original order was added
    std::vector<Order> ordersAfterAdd = cache.getAllOrders();
    ASSERT_EQ(ordersAfterAdd.size(), 1);

    // Create a replacement order with the same order ID but different attributes
    Order replacementOrder{"OrdId1", "SecId2", "Sell", 1000, "User2", "Company2"};

    // Attempt to replace the order
    try {
      cache.addOrder(replacementOrder);
    } catch (...) {
      // Do nothing
    }

    // Verify the original order remains unchanged
    std::vector<Order> ordersAfterReplace = cache.getAllOrders();
    ASSERT_EQ(ordersAfterReplace.size(), 1);
    ASSERT_EQ(ordersAfterReplace[0].orderId(), "OrdId1");
    ASSERT_EQ(ordersAfterReplace[0].securityId(), "SecId1");
    ASSERT_EQ(ordersAfterReplace[0].side(), "Buy");
    ASSERT_EQ(ordersAfterReplace[0].qty(), 500);
    ASSERT_EQ(ordersAfterReplace[0].user(), "User1");
    ASSERT_EQ(ordersAfterReplace[0].company(), "Company1");
}

// EdgeCases: Test that canceling an order with an empty order ID
TEST_F(OrderCacheTest, EdgeCases_CancelOrder_EmptyOrderId) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel an order with an empty order ID
    try {
      cache.cancelOrder("");
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    ASSERT_EQ(cache.getAllOrders().size(), 1);
}

// EdgeCases: Test that canceling a non-existent order
TEST_F(OrderCacheTest, EdgeCases_CancelOrder_NonexistentOrder) {
    CHECK_GLOBAL_FAILURE_FLAG();

    try {
      cache.cancelOrder("OrdId1");
    } catch (...) {
      // Do nothing
    }

    // Assuming getAllOrders() returns all current orders, the size should be 0 for an empty cache.
    ASSERT_TRUE(cache.getAllOrders().empty());
}

// EdgeCases: Canceling all orders for a user and then adding new orders for that user
TEST_F(OrderCacheTest, EdgeCases_CancelOrder_CancelThenAddNewOrdersForSameUserShouldSucceed) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add multiple orders for User1
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 700, "User1", "Company1"});
    cache.addOrder(Order{"OrdId3", "SecId3", "Buy", 300, "User1", "Company1"});

    // Add an order for a different user
    cache.addOrder(Order{"OrdId4", "SecId1", "Sell", 400, "User2", "Company2"});

    // Cancel all orders for User1
    cache.cancelOrdersForUser("User1");

    // Verify only User1's orders were canceled
    std::vector<Order> orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 1);
    ASSERT_EQ(orders[0].orderId(), "OrdId4");

    // Add new orders for User1
    cache.addOrder(Order{"OrdId5", "SecId4", "Buy", 800, "User1", "Company1"});
    cache.addOrder(Order{"OrdId6", "SecId5", "Sell", 200, "User1", "Company1"});

    // Verify the new orders were added correctly
    orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 3);

    // Count orders for User1
    int user1OrderCount = 0;
    for (const auto& order : orders) {
        if (order.user() == "User1") {
            user1OrderCount++;
        }
    }
    ASSERT_EQ(user1OrderCount, 2);
}

// EdgeCases: Test that canceling orders for a user with an empty user ID
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForUser_EmptyUser) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel orders for an empty user ID
    try {
      cache.cancelOrdersForUser("");
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);
}

// EdgeCases: Test that canceling orders for a user without orders
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForUser_NoOrdersFound) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel orders for a user without orders
    try {
      cache.cancelOrdersForUser("User2");
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);
}

// EdgeCases: Canceling orders for a user who has orders across multiple securities
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForUser_AcrossMultipleSecuritiesShouldRemoveAll) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add multiple orders for User1 across different securities
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 700, "User1", "Company1"});
    cache.addOrder(Order{"OrdId3", "SecId3", "Buy", 300, "User1", "Company1"});

    // Add orders for other users
    cache.addOrder(Order{"OrdId4", "SecId1", "Sell", 400, "User2", "Company2"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy", 600, "User3", "Company3"});

    // Cancel all orders for User1
    cache.cancelOrdersForUser("User1");

    // Verify only User1's orders were canceled across all securities
    std::vector<Order> orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 2);

    for (const auto& order : orders) {
        ASSERT_NE(order.user(), "User1");
    }

    // Verify matching sizes for each security (no User1 orders should remain)
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId1"), 0);
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId2"), 0);
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId3"), 0);
}

// EdgeCases: Test that canceling orders with an empty security ID
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForSecIdWithMinimumQty_EmptySecurityId) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel orders with an empty security ID
    try {
      cache.cancelOrdersForSecIdWithMinimumQty("", 100);
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    ASSERT_EQ(cache.getAllOrders().size(), 1);
}

// EdgeCases: Test that canceling orders with a zero quantity
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForSecIdWithMinimumQty_ZeroQuantity) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel orders with an zero quantity
    try {
      cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 0);
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    ASSERT_EQ(cache.getAllOrders().size(), 1);
}

// EdgeCases: Test that canceling orders for a security with no orders
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForSecIdWithMinimumQty_NonOrdersFound) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to cancel orders with an security that has no orders
    try {
      cache.cancelOrdersForSecIdWithMinimumQty("SecId2", 100);
    } catch (...) {
      // Do nothing
    }

    // Verify that the original order is still in the cache
    ASSERT_EQ(cache.getAllOrders().size(), 1);
}

// EdgeCases: Canceling high-quantity orders for a security and verifying that low-quantity orders remain
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForSecIdWithMinimumQty_LowQuantityOrdersShouldRemain) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add orders with various quantities for SecId1
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 500, "User2", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 800, "User3", "Company3"});
    cache.addOrder(Order{"OrdId4", "SecId1", "Sell", 300, "User4", "Company4"});
    cache.addOrder(Order{"OrdId5", "SecId1", "Buy", 1000, "User5", "Company5"});

    // Add orders for other securities
    cache.addOrder(Order{"OrdId6", "SecId2", "Buy", 600, "User6", "Company6"});

    // Cancel all orders for SecId1 with qty >= 500
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 500);

    // Verify only high-qty orders for SecId1 were canceled
    std::vector<Order> orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 3);

    // Check that low-qty orders for SecId1 remain
    int secId1LowQtyCount = 0;
    for (const auto& order : orders) {
        if (order.securityId() == "SecId1") {
            ASSERT_LT(order.qty(), 500);
            secId1LowQtyCount++;
        }
    }
    ASSERT_EQ(secId1LowQtyCount, 2);

    // Verify orders with higher quantities were removed
    bool highQtyOrdersExist = false;
    for (const auto& order : orders) {
        if (order.securityId() == "SecId1" && order.qty() >= 500) {
            highQtyOrdersExist = true;
            break;
        }
    }
    ASSERT_FALSE(highQtyOrdersExist);
}

// EdgeCases: Canceling orders by security ID and then adding new orders for that security
TEST_F(OrderCacheTest, EdgeCases_CancelOrdersForSecIdWithMinimumQty_CancelThenAddNewOrdersForSameSecurityShouldSucceed) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add multiple orders for SecId1
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 700, "User2", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 300, "User3", "Company3"});

    // Add an order for a different security
    cache.addOrder(Order{"OrdId4", "SecId2", "Sell", 400, "User4", "Company4"});

    // Cancel all orders for SecId1 with qty >= 500
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 500);

    // Verify only high-qty orders for SecId1 were canceled
    std::vector<Order> orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 2);

    // Add new orders for SecId1
    try {
      cache.addOrder(Order{"OrdId5", "SecId1", "Buy", 800, "User5", "Company5"});
      cache.addOrder(Order{"OrdId6", "SecId1", "Sell", 600, "User6", "Company6"});
    } catch (...) {
      // Do nothing
    }

    // Verify the new orders were added correctly
    orders = cache.getAllOrders();
    ASSERT_EQ(orders.size(), 4);

    // Count orders for SecId1
    int secId1OrderCount = 0;
    for (const auto& order : orders) {
        if (order.securityId() == "SecId1") {
            secId1OrderCount++;
        }
    }
    ASSERT_EQ(secId1OrderCount, 3);
}

// EdgeCases: Test that getting matching size for an empty security ID
TEST_F(OrderCacheTest, EdgeCases_GetMatchingSizeForSecurity_EmptySecurity) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Attempt to get matching size for an empty security ID
    try {
      cache.getMatchingSizeForSecurity("");
    } catch (...) {
      // Do nothing
    }
}

// EdgeCases: Test that getting matching size for security that does not exist returns zero
TEST_F(OrderCacheTest, EdgeCases_GetMatchingSizeForSecurity_NonexistentSecurityReturnsZero) {
    CHECK_GLOBAL_FAILURE_FLAG();

    // Add a valid order to the cache
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 500, "User1", "Company1"});

    // Attempt to get matching size for a security ID that does not exist
    ASSERT_EQ(cache.getMatchingSizeForSecurity("SecId2"), 0);
}

// EdgeCases: Modifying the vector returned by getAllOrders() should not affect the cache.
TEST_F(OrderCacheTest, EdgeCases_GetAllOrders_ResultNotModifiableExternally) {
    CHECK_GLOBAL_FAILURE_FLAG();

    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 100, "User1", "Company1"});
    std::vector<Order> ordersCopy = cache.getAllOrders();
    ordersCopy.clear();
    std::vector<Order> ordersAfter = cache.getAllOrders();
    ASSERT_EQ(ordersAfter.size(), 1);
    ASSERT_EQ(ordersAfter[0].orderId(), "OrdId1");
}

// Performance: Add and match 1,000 orders
TEST_F(OrderCacheTest, Performance_SmallDataset_1KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 1000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 5,000 orders
TEST_F(OrderCacheTest, Performance_SmallDataset_5KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 5000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 10,000 orders
TEST_F(OrderCacheTest, Performance_MediumDataset_10KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 10000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 50,000 orders
TEST_F(OrderCacheTest, Performance_MediumDataset_50KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 50000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 100,000 orders
TEST_F(OrderCacheTest, Performance_LargeDataset_100KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 100000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 500,000 orders
TEST_F(OrderCacheTest, Performance_LargeDataset_500KOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 500000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

// Performance: Add and match 1,000,000 orders
TEST_F(OrderCacheTest, Performance_VeryLargeDataset_1MOrders) {
    CHECK_GLOBAL_FAILURE_FLAG();

    unsigned int NUM_ORDERS = 1000000;
    std::vector<Order> orders = generateOrders(NUM_ORDERS);
    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the cache
    for (int i = 0; i < orders.size(); i++) {
        cache.addOrder(orders[i]);
    }

    // Get matching size for every security and measure the time it takes
    for (const auto& secId : secIds) {
        cache.getMatchingSizeForSecurity(secId);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double ncu = duration / benchmark_time;  // Calculate the NCU based on benchmark

    // Display the result
    std::cout << BLUE_COLOR << "[     INFO ] Matched " << NUM_ORDERS << " orders in " << ncu << " NCUs (" << duration << "ms)" << RESET_COLOR << std::endl;
    ASSERT_LE(ncu, 1500);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Custom failure listener
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new FailureListener);

    return RUN_ALL_TESTS();
}
