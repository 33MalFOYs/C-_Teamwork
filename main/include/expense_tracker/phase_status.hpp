#pragma once

namespace expense_tracker {

inline constexpr int implementation_phase = 4;
const char* phase_summary() noexcept;
const char* authentication_status() noexcept;
bool authentication_available() noexcept;

} // namespace expense_tracker
