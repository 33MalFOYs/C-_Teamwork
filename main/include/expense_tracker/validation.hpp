#pragma once

#include "expense_tracker/types.hpp"

namespace expense_tracker::validation {

std::string trimAscii(const std::string& text);
Money parseAmount(const std::string& text, bool allow_zero = false);
std::string formatMoney(Money amount);
std::string formatDailyAverage(Money weekly_total); // Divide by 7; round cents for display.
Money checkedAdd(Money left, Money right);
int parsePositiveId(const std::string& text);

void validateDate(const std::string& date);
void validatePeriod(BudgetPeriod period);
DateRange weekContaining(const std::string& date); // Monday through Sunday.
DateRange monthContaining(const std::string& date);
std::string addDays(const std::string& date, int days);
std::string localToday();

// Text is valid UTF-8, measured in Unicode code points. Embedded control
// characters are rejected; descriptions may be empty and are not trimmed.
std::string normalizeCategory(const std::string& category);
void validateDescription(const std::string& description);

} // namespace expense_tracker::validation
