#include "expense_tracker/memory_expense_service.hpp"
#include "expense_tracker/validation.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Exception = std::invalid_argument, typename Action>
void rejects(Action action, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const Exception&) {
        // Expected rejection.
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: " << message << " (unexpected error: " << error.what() << ")\n";
    }
}

void checkSummarySums(const expense_tracker::WeeklyReport& report) {
    expense_tracker::Money day_sum = 0;
    expense_tracker::Money category_sum = 0;
    for (const auto& day : report.days) day_sum += day.total;
    for (const auto& category : report.categories) category_sum += category.second;
    check(day_sum == report.total, "daily totals sum to weekly total");
    check(category_sum == report.total, "category totals sum to weekly total");
}
}

int main() {
    using namespace expense_tracker;
    static_assert(std::is_base_of_v<ExpenseWorkspace, MemoryExpenseService>);
    static_assert(!std::is_copy_constructible_v<MemoryExpenseService>);

    MemoryExpenseService workspace;
    ExpenseWorkspace& api = workspace;
    check(api.matching(ExpenseFilter()).empty() && api.total(ExpenseFilter()) == 0,
          "new workspace is empty and has zero total");
    const auto empty = api.weekly("2026-12-30");
    check(empty.total == 0 && empty.count == 0 && empty.categories.empty(),
          "empty week has zero totals/count and no categories");
    const std::array<const char*, 7> expected_dates{
        "2026-12-28", "2026-12-29", "2026-12-30", "2026-12-31",
        "2027-01-01", "2027-01-02", "2027-01-03"
    };
    for (std::size_t day = 0; day < expected_dates.size(); ++day) {
        check(empty.days[day].date == expected_dates[day] && empty.days[day].total == 0,
              "empty week keeps all seven calendar days including new year");
    }
    checkSummarySums(empty);

    rejects([&] { api.add({0, "Food", "2026-12-28", "invalid"}); },
            "invalid expense is rejected by shared service");
    const int monday = api.add({1000, "Food", "2026-12-28", "Monday"});
    const int wednesday = api.add({2000, "Travel", "2026-12-30", "Wednesday"});
    const int sunday = api.add({500, " Food ", "2027-01-03", "Sunday"});
    const int next_monday = api.add({9900, "Food", "2027-01-04", "Next week"});
    check(monday == 1 && wednesday == 2 && sunday == 3 && next_monday == 4,
          "temporary IDs are sequential and failed insertion consumes no ID");
    check(api.searchExpense(sunday)->category() == "Food", "stored category is normalized");

    const auto week = api.weekly("2026-12-30");
    check(week.dates.start == "2026-12-28" && week.dates.end == "2027-01-03",
          "weekly report spans Monday through Sunday across year boundary");
    check(week.total == 3500 && week.count == 3, "weekly fixture totals 35.00 across three records");
    check(week.days[0].total == 1000 && week.days[2].total == 2000 &&
          week.days[6].total == 500 && week.days[1].total == 0 &&
          week.days[3].total == 0 && week.days[4].total == 0 && week.days[5].total == 0,
          "daily rows include zero-spending days and exclude next Monday");
    check(week.categories.at("Food") == 1500 && week.categories.at("Travel") == 2000,
          "category breakdown matches independent expected totals");
    check(validation::formatDailyAverage(week.total) == "5.00", "weekly average divides by seven");
    checkSummarySums(week);
    check(api.weekly("2026-12-28").total == 3500 && api.weekly("2027-01-03").total == 3500,
          "Monday and Sunday select the same complete week");
    check(api.weekly("2027-01-04").total == 9900, "following Monday starts a separate report");

    check(api.matching(ExpenseFilter::exactDate("2026-12-30")).size() == 1 &&
          api.total(ExpenseFilter::exactDate("2026-12-30")) == 2000,
          "exact date count and total use the same predicate");
    const ExpenseFilter inclusive(DateRange{"2026-12-28", "2027-01-03"}, std::nullopt);
    check(api.matching(inclusive).size() == 3 && api.total(inclusive) == 3500,
          "inclusive range includes both endpoints");
    const ExpenseFilter combined(DateRange{"2026-12-28", "2027-01-03"}, " Food ");
    check(api.matching(combined).size() == 2 && api.total(combined) == 1500,
          "category and date criteria combine with AND");
    const ExpenseFilter wrong_case(std::nullopt, "food");
    check(api.matching(wrong_case).empty() && api.total(wrong_case) == 0,
          "category comparison is exact and case-sensitive");
    check(api.searchExpense(std::string(" Food ")).size() == 3,
          "category search overload returns all normalized matches");
    check(api.searchExpense(monday)->amount() == 1000, "ID search overload finds one record");
    check(!api.searchExpense(999).has_value() && !api.update(999, {100, "Other", "2026-01-01", ""}) &&
          !api.erase(999), "nonexistent ID operations return explicit absence");
    rejects([&] { api.searchExpense(0); }, "nonpositive IDs are rejected by shared service");
    rejects([&] { api.searchExpense(std::string("   ")); }, "blank category search is rejected");

    ExpenseFilter active = combined;
    rejects([&] {
        active = ExpenseFilter(DateRange{"2027-01-03", "2026-12-28"}, std::nullopt);
    }, "reversed range is rejected");
    check(api.total(active) == 1500, "invalid replacement filter preserves previous active view");
    rejects([] { ExpenseFilter(DateRange{"", "2026-12-28"}, std::nullopt); },
            "missing custom endpoint is rejected");
    rejects([] { ExpenseFilter::exactDate("2026-02-29"); }, "invalid calendar date filter rejected");
    check(api.total(ExpenseFilter::thisWeek("2026-12-31")) == 3500,
          "this-week predicate uses supplied local calendar date");
    check(api.total(ExpenseFilter::thisMonth("2027-01-03")) == 10400,
          "this-month predicate includes the correct complete month");
    check(api.total(ExpenseFilter()) == 13400 && api.matching(ExpenseFilter()).size() == 4,
          "clear filter restores all workspace records");

    const auto december = api.monthly({2026, 12});
    const auto january = api.monthly({2027, 1});
    check(december.total == 3000 && december.count == 2 && december.categories.at("Travel") == 2000,
          "December report includes year and category totals");
    check(january.total == 10400 && january.count == 2 && api.monthly({2026, 1}).total == 0,
          "monthly reports never combine different years");
    rejects([&] { api.monthly({2026, 0}); }, "invalid report month rejected");
    rejects([&] { api.monthly({999, 1}); }, "unsupported report year rejected");

    const auto old_value = api.searchExpense(monday);
    rejects([&] { api.update(monday, {100, "Food", "2026-02-29", "bad edit"}); },
            "invalid shared update rejected");
    check(api.searchExpense(monday)->amount() == old_value->amount() &&
          api.weekly("2026-12-30").total == 3500, "invalid update changes neither record nor report");
    check(api.update(monday, {1200, "Travel", "2026-12-29", "edited"}), "valid update succeeds");
    const auto edited = api.weekly("2026-12-30");
    check(edited.total == 3700 && edited.days[0].total == 0 && edited.days[1].total == 1200 &&
          edited.categories.at("Travel") == 3200, "edit refreshes every affected report component");
    checkSummarySums(edited);
    check(api.erase(sunday) && api.weekly("2026-12-30").total == 3200,
          "deletion immediately changes computed totals");
    check(api.add({1, "Other", "2027-02-01", "after deletion"}) == 5,
          "temporary IDs are not reused after deletion");
    check(old_value->amount() == 1000 && old_value->date() == "2026-12-28",
          "search returns an independent value snapshot");

    MemoryExpenseService leap_workspace;
    leap_workspace.add({123, "Food", "2024-02-29", "leap"});
    leap_workspace.add({77, "Food", "2024-03-03", "Sunday"});
    leap_workspace.add({900, "Food", "2024-03-04", "excluded"});
    const auto leap_week = leap_workspace.weekly("2024-02-29");
    check(leap_week.dates.start == "2024-02-26" && leap_week.dates.end == "2024-03-03" &&
          leap_week.total == 200 && leap_week.count == 2 && leap_week.days[3].total == 123,
          "leap-day report spans February/March without splitting the week");
    check(leap_workspace.monthly({2024, 2}).total == 123,
          "leap February includes the 29th but excludes March");
    checkSummarySums(leap_week);
    check(api.searchExpense(1)->amount() == 1200 && leap_workspace.searchExpense(1)->amount() == 123,
          "separate temporary workspaces do not share records");
    rejects<std::out_of_range>([] { ExpenseFilter::thisWeek("1000-01-01"); },
                               "week below supported range rejected");
    rejects<std::out_of_range>([] { ExpenseFilter::thisWeek("9999-12-31"); },
                               "week above supported range rejected");

    ExpenseList protected_records;
    protected_records.insert(Expense(1, 1000, "Food", "2026-12-28", "protected fixture"));
    Session signed_out;
    ReportService protected_reports(signed_out, protected_records);
    rejects<std::logic_error>([&] { signed_out.requireUserId(); }, "signed-out session has no account ID");
    rejects<std::logic_error>([&] { protected_reports.weekly("2026-12-30"); },
                             "production report rejects unauthenticated reads even with records");
    rejects<std::logic_error>([&] { protected_reports.monthly({2026, 12}); },
                             "production monthly report keeps the authenticated gate");
    check(!signed_out.authenticated() && !signed_out.cacheReady(),
          "temporary workspace never authenticates the production Session");

    if (failures == 0) std::cout << "Phase 2 filter, report, and workspace checks passed.\n";
    return failures == 0 ? 0 : 1;
}
