#include "expense_tracker/validation.hpp"

#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace expense_tracker::validation {
namespace {

bool digit(char value) { return value >= '0' && value <= '9'; }
bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

int daysInMonth(int year, int month) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}

struct Date { int year; int month; int day; };

Date parseDate(const std::string& text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        throw std::invalid_argument("Date must use YYYY-MM-DD.");
    }
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (index != 4 && index != 7 && !digit(text[index])) {
            throw std::invalid_argument("Date must use YYYY-MM-DD.");
        }
    }
    const Date date{(text[0] - '0') * 1000 + (text[1] - '0') * 100 +
                        (text[2] - '0') * 10 + (text[3] - '0'),
                    (text[5] - '0') * 10 + (text[6] - '0'),
                    (text[8] - '0') * 10 + (text[9] - '0')};
    if (date.year < minimum_year || date.year > maximum_year || date.month < 1 ||
        date.month > 12 || date.day < 1 || date.day > daysInMonth(date.year, date.month)) {
        throw std::invalid_argument("Date must be a real calendar date in years 1000 through 9999.");
    }
    return date;
}

std::string dateText(Date date) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << date.year << '-'
           << std::setw(2) << date.month << '-' << std::setw(2) << date.day;
    return output.str();
}

int daysBeforeYear(int year) {
    const int previous = year - 1;
    return previous * 365 + previous / 4 - previous / 100 + previous / 400;
}

// Zero is 0001-01-01, a Monday in the proleptic Gregorian calendar.
int ordinal(Date date) {
    int result = daysBeforeYear(date.year) + date.day - 1;
    for (int month = 1; month < date.month; ++month) result += daysInMonth(date.year, month);
    return result;
}

Date dateFromOrdinal(int value) {
    int low = minimum_year;
    int high = maximum_year;
    while (low < high) {
        const int middle = low + (high - low + 1) / 2;
        if (daysBeforeYear(middle) <= value) low = middle;
        else high = middle - 1;
    }
    int dayOfYear = value - daysBeforeYear(low);
    int month = 1;
    while (dayOfYear >= daysInMonth(low, month)) {
        dayOfYear -= daysInMonth(low, month);
        ++month;
    }
    return Date{low, month, dayOfYear + 1};
}

void validateUtf8(const std::string& text, std::size_t maximum, const char* field) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::uint32_t codePoint = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if (first <= 0x7f) { codePoint = first; length = 1; }
        else if (first >= 0xc2 && first <= 0xdf) { codePoint = first & 0x1f; length = 2; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { codePoint = first & 0x0f; length = 3; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { codePoint = first & 0x07; length = 4; minimum = 0x10000; }
        else throw std::invalid_argument(std::string(field) + " must contain valid UTF-8.");
        if (length > text.size() - index) {
            throw std::invalid_argument(std::string(field) + " must contain valid UTF-8.");
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto next = static_cast<unsigned char>(text[index + offset]);
            if ((next & 0xc0) != 0x80) {
                throw std::invalid_argument(std::string(field) + " must contain valid UTF-8.");
            }
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        if (codePoint < minimum || codePoint > 0x10ffff ||
            (codePoint >= 0xd800 && codePoint <= 0xdfff)) {
            throw std::invalid_argument(std::string(field) + " must contain valid UTF-8.");
        }
        if (codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f) ||
            codePoint == 0x2028 || codePoint == 0x2029) {
            throw std::invalid_argument(std::string(field) + " cannot contain control characters or line breaks.");
        }
        if (++count > maximum) {
            throw std::invalid_argument(std::string(field) + " exceeds " + std::to_string(maximum) + " characters.");
        }
        index += length;
    }
}

} // namespace

std::string trimAscii(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n\f\v") - first + 1);
}

Money parseAmount(const std::string& raw, bool allow_zero) {
    const std::string text = trimAscii(raw);
    const auto point = text.find('.');
    const auto wholeLength = point == std::string::npos ? text.size() : point;
    if (wholeLength == 0) throw std::invalid_argument("Enter a decimal amount such as 12.50.");
    Money whole = 0;
    for (std::size_t index = 0; index < wholeLength; ++index) {
        if (!digit(text[index])) throw std::invalid_argument("Amount must contain decimal digits only.");
        const int next = text[index] - '0';
        if (whole > (maximum_amount / 100 - next) / 10) {
            throw std::out_of_range("Amount exceeds 9999999999.99 USD.");
        }
        whole = whole * 10 + next;
    }
    Money fraction = 0;
    if (point != std::string::npos) {
        const auto length = text.size() - point - 1;
        if (length < 1 || length > 2) throw std::invalid_argument("Use one or two digits after the decimal point.");
        for (std::size_t index = point + 1; index < text.size(); ++index) {
            if (!digit(text[index])) throw std::invalid_argument("Amount must contain decimal digits only.");
            fraction = fraction * 10 + (text[index] - '0');
        }
        if (length == 1) fraction *= 10;
    }
    const Money amount = whole * 100 + fraction;
    if (amount > maximum_amount) throw std::out_of_range("Amount exceeds 9999999999.99 USD.");
    if (amount == 0 && !allow_zero) throw std::invalid_argument("Expense amount must be greater than zero.");
    return amount;
}

std::string formatMoney(Money amount) {
    const bool negative = amount < 0;
    const auto absolute = negative ? static_cast<std::uint64_t>(-(amount + 1)) + 1
                                   : static_cast<std::uint64_t>(amount);
    const auto cents = absolute % 100;
    return (negative ? "-" : "") + std::to_string(absolute / 100) + "." +
           (cents < 10 ? "0" : "") + std::to_string(cents);
}

std::string formatDailyAverage(Money weekly_total) {
    if (weekly_total < 0) throw std::invalid_argument("Weekly total cannot be negative.");
    return formatMoney(weekly_total / 7 + (weekly_total % 7 >= 4 ? 1 : 0));
}

Money checkedAdd(Money left, Money right) {
    if ((right > 0 && left > std::numeric_limits<Money>::max() - right) ||
        (right < 0 && left < std::numeric_limits<Money>::min() - right)) {
        throw std::overflow_error("The total exceeds the supported integer range.");
    }
    return left + right;
}

int parsePositiveId(const std::string& raw) {
    const std::string text = trimAscii(raw);
    if (text.empty()) throw std::invalid_argument("Enter a positive whole-number ID.");
    int id = 0;
    for (char value : text) {
        if (!digit(value)) throw std::invalid_argument("Enter a positive whole-number ID.");
        const int next = value - '0';
        if (id > (std::numeric_limits<int>::max() - next) / 10) {
            throw std::out_of_range("ID exceeds the supported range.");
        }
        id = id * 10 + next;
    }
    if (id == 0) throw std::invalid_argument("ID must be greater than zero.");
    return id;
}

void validateDate(const std::string& date) { (void)parseDate(date); }

void validatePeriod(BudgetPeriod period) {
    if (period.year < minimum_year || period.year > maximum_year || period.month < 1 || period.month > 12) {
        throw std::invalid_argument("Month must be 1 through 12 and year 1000 through 9999.");
    }
}

std::string addDays(const std::string& text, int days) {
    const auto result = static_cast<std::int64_t>(ordinal(parseDate(text))) + days;
    if (result < daysBeforeYear(minimum_year) || result >= daysBeforeYear(maximum_year + 1)) {
        throw std::out_of_range("The requested date extends beyond years 1000 through 9999.");
    }
    return dateText(dateFromOrdinal(static_cast<int>(result)));
}

DateRange weekContaining(const std::string& text) {
    const int weekday = ordinal(parseDate(text)) % 7;
    return DateRange{addDays(text, -weekday), addDays(text, 6 - weekday)};
}

DateRange monthContaining(const std::string& text) {
    const Date date = parseDate(text);
    return DateRange{dateText(Date{date.year, date.month, 1}),
                     dateText(Date{date.year, date.month, daysInMonth(date.year, date.month)})};
}

std::string localToday() {
    const auto now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) throw std::runtime_error("Could not read the local date.");
    const std::tm* local = std::localtime(&now);
    if (!local) throw std::runtime_error("Could not read the local date.");
    const auto result = dateText(Date{local->tm_year + 1900, local->tm_mon + 1, local->tm_mday});
    validateDate(result);
    return result;
}

std::string normalizeCategory(const std::string& raw) {
    auto category = trimAscii(raw);
    if (category.empty()) throw std::invalid_argument("Category is required.");
    validateUtf8(category, 50, "Category");
    return category;
}

void validateDescription(const std::string& description) {
    validateUtf8(description, 255, "Description");
}

} // namespace expense_tracker::validation
