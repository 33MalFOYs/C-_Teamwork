# Reference-inspired GUI redesign

The native Qt interface now follows the supplied light finance-dashboard
reference: white rounded cards on a pale canvas, cyan charts, blue actions,
restrained teal/violet/amber accents, and consistent outline icons.

## Pages

- **Overview:** real weekly/monthly expense totals, monthly entry count, a blue
  monthly-plan card, budget progress, daily spending chart, category mix, and
  recent records.
- **Expenses:** a transaction ledger and receipt-style editor, large amount input,
  category presets, compact filters, and icon actions. The editor stacks below
  the ledger at narrow widths; New expense scrolls it into view.
- **Reports:** weekly trend and category charts with exact detail tables, plus an
  independent monthly category view.
- **Files:** separate cards for importing expenses and exporting all expenses,
  a weekly summary, or a filtered selection, with a collapsible format guide.
- **Sign in / registration:** a cyan welcome panel and a compact account form.

Essential amounts, dates, field labels, and confirmation text remain visible.
Icon actions have accessible names and tooltips. Keyboard navigation and
authentication gating remain in place. Primary-button and secondary-text
contrast were strengthened after the first screenshot review.

## Implementation

The shared styling and native vector-drawn icons live in main/gui/theme.cpp.
Shared charts live in main/gui/chart_widgets.cpp, with no new dependency.
The overview, navigation, and files workspace live in main/gui.cpp; remaining
pages live under main/gui.

Charts receive integer cents from the existing expense reports. Only plot
coordinates use floating-point conversion; exact amounts remain in accessible
descriptions and hover tooltips. Extra categories are grouped without dropping
their total, and empty periods display an explicit empty state. Chart data and
tooltips are cleared when the account is discarded. No sample transactions are
added to the user's account.

## Verification

A fresh build of the changed sources in build/app passed all **13 CTest
suites**, with zero failures or skips, in **12.80 seconds**. Database checks used
the isolated test schema and existing failure fixtures.

Regression checks cover:

- Existing authentication, expenses, filters, budgets, reports, and CSV actions.
- Icon-only actions having icons, accessible names, and tooltips.
- Account pages at 1200x860, 1060x820, and 850x720; authentication at 480x600.
- Input/button geometry and controls remaining reachable through scrolling.
- Chart values updating after editing/deleting expenses and changing periods.
- Empty, zero, single-point, multi-category, and large exact-money chart data.
- Category aggregation, overflow rejection, and clearing stale tooltips.
- Charts being hidden on logout and destroyed before a different account opens.

Screenshots in build/app/gui-redesign-*.png and build/app/gui-chart-*.png
were reviewed for layout. They contain fictional test data only. The normal
desktop shortcut continues to launch build/app/bin/expense_tracker_gui.exe.
