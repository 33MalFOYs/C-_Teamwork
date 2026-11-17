#include "expense_tracker/expense_filter.hpp"
#include "expense_tracker/expense_list.hpp"
#include "expense_tracker/validation.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Test-only ordinary-new accounting and node-allocation fault injection. This
// observes ExpenseNode allocations made by our code. A shared standard library
// may allocate its strings internally, so this is not a whole-process leak scan.
namespace allocation_check {
std::ptrdiff_t live = 0;
bool fail_next = false;
void* allocate(std::size_t size) {
    if (fail_next) { fail_next = false; throw std::bad_alloc(); }
    auto* memory = std::malloc(size == 0 ? 1 : size);
    if (!memory) throw std::bad_alloc();
    ++live;
    return memory;
}
void release(void* memory) noexcept {
    if (memory) { --live; std::free(memory); }
}
}

void* operator new(std::size_t size) { return allocation_check::allocate(size); }
void* operator new[](std::size_t size) { return allocation_check::allocate(size); }
void operator delete(void* memory) noexcept { allocation_check::release(memory); }
void operator delete[](void* memory) noexcept { allocation_check::release(memory); }
void operator delete(void* memory, std::size_t) noexcept { allocation_check::release(memory); }
void operator delete[](void* memory, std::size_t) noexcept { allocation_check::release(memory); }

namespace {
int failures = 0;
int checks = 0;
void check(bool condition, const char* description) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Exception = std::exception, class Operation>
void throws(Operation operation, const char* description) {
    try { operation(); check(false, description); }
    catch (const Exception&) { check(true, description); }
    catch (...) { check(false, description); }
}

using namespace expense_tracker;
using namespace expense_tracker::validation;

Expense expense(int id, Money amount = 1000, const std::string& category = "Food") {
    return Expense{id, amount, category, "2026-12-28", "Fictional sample"};
}

void moneyChecks() {
    check(parseAmount("0.01") == 1, "smallest amount");
    check(parseAmount(" 12.5 \t") == 1250, "one decimal digit and ASCII surrounding whitespace");
    check(parseAmount("12") == 1200, "whole decimal amount");
    check(parseAmount("9999999999.99") == maximum_amount, "largest DECIMAL amount");
    check(parseAmount("00012.50") == 1250, "leading zeros do not change amount");
    check(parseAmount("0", true) == 0, "zero is supported for future budgets only");
    for (const std::string text : {"", " ", "0", "0.00", "-1", "+1", "1e2", "NaN", "1,000", "1.001", "1.", ".50", "1.2.3", "1 2", "\t-0.01"}) {
        throws([&] { (void)parseAmount(text); }, "invalid decimal input rejected");
    }
    throws<std::out_of_range>([] { (void)parseAmount("10000000000"); }, "overrange whole amount rejected");
    throws<std::out_of_range>([] { (void)parseAmount(std::string(100, '9')); }, "very large input rejected without overflow");
    check(formatMoney(0) == "0.00" && formatMoney(1) == "0.01" && formatMoney(12345) == "123.45", "decimal formatting");
    check(formatMoney(-123) == "-1.23", "negative remaining budget formatting");
    check(formatMoney(std::numeric_limits<Money>::min()) == "-92233720368547758.08", "format signed minimum safely");
    check(formatDailyAverage(3500) == "5.00", "weekly average exact");
    check(formatDailyAverage(3) == "0.00" && formatDailyAverage(4) == "0.01", "weekly average rounds only display");
    check(checkedAdd(300, 250) == 550 && checkedAdd(-300, 250) == -50, "checked sums");
    throws<std::overflow_error>([] { (void)checkedAdd(std::numeric_limits<Money>::max(), 1); }, "positive aggregate overflow rejected");
    throws<std::overflow_error>([] { (void)checkedAdd(std::numeric_limits<Money>::min(), -1); }, "negative aggregate overflow rejected");
    check(parsePositiveId(" 2147483647 ") == std::numeric_limits<int>::max(), "maximum SQL INT ID");
    for (const std::string text : {"", "0", "-1", "+1", "1.0", "word", "2147483648"}) {
        throws([&] { (void)parsePositiveId(text); }, "invalid ID rejected");
    }
}

void dateChecks() {
    for (const std::string text : {"1000-01-01", "9999-12-31", "2000-02-29", "2024-02-29", "2026-11-16"}) {
        validateDate(text);
        check(true, "valid Gregorian date");
    }
    for (const std::string text : {"", "2024-2-29", "2024-02-9", "2023-02-29", "1900-02-29", "2100-02-29", "2026-04-31", "2026-00-01", "2026-13-01", "2026-01-00", "0000-01-01", "0999-12-31", "10000-01-01", "202a-01-01", "2026/01/01", "2026-01-01 "}) {
        throws<std::invalid_argument>([&] { validateDate(text); }, "invalid calendar date rejected");
    }
    check(addDays("2024-02-28", 1) == "2024-02-29", "leap day addition");
    check(addDays("2024-02-29", 1) == "2024-03-01", "leap day successor");
    check(addDays("2027-01-01", -1) == "2026-12-31", "previous year addition");
    check(addDays("1000-01-01", 3287181) == "9999-12-31", "large whole-range date addition");
    check(addDays("9999-12-31", -3287181) == "1000-01-01", "large whole-range date subtraction");
    throws<std::out_of_range>([] { (void)addDays("1000-01-01", -1); }, "lower date boundary");
    throws<std::out_of_range>([] { (void)addDays("9999-12-31", 1); }, "upper date boundary");
    throws<std::out_of_range>([] { (void)addDays("2026-01-01", std::numeric_limits<int>::max()); }, "large day offset cannot overflow");
    const auto monday = weekContaining("2026-12-28");
    const auto sunday = weekContaining("2027-01-03");
    check(monday.start == "2026-12-28" && monday.end == "2027-01-03", "Monday starts cross-year week");
    check(sunday.start == monday.start && sunday.end == monday.end, "Sunday belongs to same week");
    const auto leapWeek = weekContaining("2024-02-29");
    check(leapWeek.start == "2024-02-26" && leapWeek.end == "2024-03-03", "leap week crosses month");
    const auto nextMonday = weekContaining("2027-01-04");
    check(nextMonday.start == "2027-01-04" && nextMonday.end == "2027-01-10", "next Monday belongs to next week");
    throws<std::out_of_range>([] { (void)weekContaining("1000-01-01"); }, "partial week beyond minimum year rejected");
    throws<std::out_of_range>([] { (void)weekContaining("9999-12-31"); }, "partial week beyond maximum year rejected");
    const auto february = monthContaining("2024-02-17");
    check(february.start == "2024-02-01" && february.end == "2024-02-29", "leap month range");
    validatePeriod(BudgetPeriod{1000, 1});
    validatePeriod(BudgetPeriod{9999, 12});
    for (const auto period : {BudgetPeriod{999, 1}, BudgetPeriod{10000, 1}, BudgetPeriod{2026, 0}, BudgetPeriod{2026, 13}}) {
        throws<std::invalid_argument>([&] { validatePeriod(period); }, "invalid budget period rejected");
    }
    validateDate(localToday());
    check(true, "local today is a supported calendar date");
}

void textAndExpenseChecks() {
    Expense unset;
    check(unset.id() == 0 && unset.amount() == 0 && unset.category().empty(), "default constructor produces unset value");
    Expense original{12, 1000, " Food ", "2024-02-29", "With comma, and \"quotes\""};
    check(original.id() == 12 && original.amount() == 1000 && original.category() == "Food", "parameter constructor validates and trims category");
    throws<std::invalid_argument>([] { (void)expense(0); }, "zero constructor ID rejected");
    throws<std::invalid_argument>([] { (void)expense(-1); }, "negative constructor ID rejected");
    for (const Money amount : {Money{0}, Money{-1}, maximum_amount + 1}) {
        throws<std::invalid_argument>([&] { (void)expense(1, amount); }, "invalid integer amount rejected");
    }
    check(normalizeCategory("\t  Travel \r\n") == "Travel", "surrounding ASCII whitespace normalized");
    check(normalizeCategory(std::string(50, 'a')).size() == 50, "category length upper bound");
    throws<std::invalid_argument>([] { (void)normalizeCategory(std::string(51, 'a')); }, "long category rejected");
    throws<std::invalid_argument>([] { (void)normalizeCategory(" \t\n"); }, "blank category rejected");
    std::string unicodeCategory;
    for (int index = 0; index < 50; ++index) unicodeCategory += "\xe1\x9e\x80"; // Khmer KA, one code point.
    check(normalizeCategory(unicodeCategory) == unicodeCategory, "UTF-8 length counts characters rather than bytes");
    throws<std::invalid_argument>([&] { (void)normalizeCategory(unicodeCategory + "x"); }, "Unicode category character limit");
    validateDescription("");
    validateDescription(std::string(255, 'x'));
    throws<std::invalid_argument>([] { validateDescription(std::string(256, 'x')); }, "description character limit");
    for (const std::string& text : std::vector<std::string>{std::string("a\0b", 3), "a\nb", "a\tb", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe1\x9e", "\x80", "\xe2\x80\xa8"}) {
        throws<std::invalid_argument>([&] { validateDescription(text); }, "invalid UTF-8/control text rejected");
    }
    throws<std::invalid_argument>([&] { original.update(ExpenseDraft{2000, "Changed", "2023-02-29", "new"}); }, "invalid replacement rejected");
    check(original.amount() == 1000 && original.category() == "Food" && original.date() == "2024-02-29" &&
          original.description() == "With comma, and \"quotes\"", "invalid update leaves every old field unchanged");
    original.update(ExpenseDraft{2050, " Travel ", "2026-11-16", ""});
    check(original.id() == 12 && original.amount() == 2050 && original.category() == "Travel" && original.description().empty(), "valid update replaces fields and preserves ID");
}

void listChecks() {
    ExpenseList list;
    check(list.size() == 0 && list.searchExpense(1) == nullptr, "empty list ID lookup");
    check(list.searchExpense("Food").empty() && list.filter(ExpenseFilter{}).empty(), "empty list category and filter lookup");
    check(list.total(ExpenseFilter{}) == 0, "empty list total");
    check(!list.erase(1) && !list.update(1, ExpenseDraft{100, "Food", "2026-01-01", ""}), "missing update and delete return false");
    list.clear();
    Expense unset;
    throws<std::invalid_argument>([&] { list.insert(unset); }, "cannot insert unset default expense");
    list.insert(expense(1));
    check(list.size() == 1 && list.searchExpense(1)->amount() == 1000, "one node insertion/search");
    throws<std::invalid_argument>([&] { list.insert(expense(1)); }, "duplicate ID rejected");
    check(list.size() == 1 && list.erase(1) && list.size() == 0, "only node deletion");
    list.insert(expense(1, 1000));
    list.insert(expense(2, 2000, "Travel"));
    list.insert(expense(3, 3000));
    list.insert(expense(4, 4000, "Other"));
    std::vector<int> visited;
    list.forEach([&](const Expense& value) { visited.push_back(value.id()); });
    check(visited == std::vector<int>({1, 2, 3, 4}), "traversal preserves insertion order");
    const auto food = list.searchExpense(" Food ");
    check(food.size() == 2 && food[0].id() == 1 && food[1].id() == 3, "overloaded category search returns all normalized matches");
    check(list.searchExpense("food").empty(), "category matching is case sensitive");
    check(list.total(ExpenseFilter{}) == 10000, "multi-node integer total");
    const auto candidate = expense(5);
    allocation_check::fail_next = true;
    throws<std::bad_alloc>([&] { list.insert(candidate); }, "node allocation failure reported");
    allocation_check::fail_next = false;
    check(list.size() == 4 && list.searchExpense(5) == nullptr, "failed insertion leaves list unchanged");
    throws<std::invalid_argument>([&] { list.update(2, ExpenseDraft{0, "Changed", "2026-01-01", ""}); }, "invalid list update rejected");
    check(list.searchExpense(2)->amount() == 2000 && list.searchExpense(2)->category() == "Travel", "failed list update preserves original");
    check(list.update(2, ExpenseDraft{2500, "Food", "2026-01-01", "Updated"}), "existing record update succeeds");
    check(food[0].amount() == 1000 && food.size() == 2, "category results are independent snapshots");
    check(list.erase(1) && list.searchExpense(1) == nullptr && list.size() == 3, "delete head");
    check(list.erase(3) && list.searchExpense(3) == nullptr && list.size() == 2, "delete middle");
    check(list.erase(4) && list.searchExpense(4) == nullptr && list.size() == 1, "delete tail");
    check(!list.erase(999) && list.searchExpense(999) == nullptr, "nonexistent ID feedback");
    list.clear();
    check(list.size() == 0 && list.searchExpense(2) == nullptr, "clear deletes remaining nodes");
    list.clear();
    list.insert(expense(9));
    check(list.size() == 1 && list.searchExpense(9), "list reusable after clear");
}

void cleanupChecks() {
    bool clean = true;
    for (int cycle = 0; cycle < 30; ++cycle) {
        const auto before = allocation_check::live;
        {
            ExpenseList list;
            for (int id = 1; id <= 100; ++id) {
                list.insert(Expense{id, 100, "Fictional", "2026-12-28", std::string(180, 'x')});
            }
            list.erase(1);
            list.erase(50);
            list.erase(100);
            if (cycle % 2 == 0) list.clear();
            // Other cycles exercise the destructor with 97 nodes still owned.
        }
        clean = clean && allocation_check::live == before;
    }
    check(clean, "allocation accounting: repeated clear/destruction balances ordinary allocations including nodes");
}
}

int main() {
    static_assert(!std::is_copy_constructible_v<expense_tracker::ExpenseList>);
    static_assert(!std::is_move_constructible_v<expense_tracker::ExpenseList>);
    static_assert(std::is_integral_v<expense_tracker::Money>);
    try {
        moneyChecks();
        dateChecks();
        textAndExpenseChecks();
        listChecks();
        cleanupChecks();
    } catch (const std::exception& error) {
        std::cerr << "Unexpected exception: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Domain checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
