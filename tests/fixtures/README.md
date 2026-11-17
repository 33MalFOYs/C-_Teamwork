# Fictional fixture

`weekly_expenses.csv` is test input only. Do not load it automatically, insert it
into a user's database, or treat its IDs as persistent IDs. CSV import is a later
phase; this file does not represent an implemented import function.

For any selected date from **2026-12-28 through 2027-01-03**, the containing week
has these independent expected results:

| Value | Expected |
| --- | --- |
| Period | Monday 2026-12-28 through Sunday 2027-01-03, inclusive |
| Count | 3 |
| Total | USD 35.00 (3500 cents) |
| Daily totals, Monday through Sunday | 10.00, 0.00, 20.00, 0.00, 0.00, 0.00, 5.00 |
| Categories | Food 15.00; Transport 20.00 |
| Daily average | USD 5.00 |
| Excluded record | ID 4 on Monday 2027-01-04, USD 99.00 |

All-record total is USD 134.00. December 2026 total is USD 30.00; January 2027
total is USD 104.00. Exact date 2027-01-03 returns one record worth USD 5.00.
The selected week combined with category `Food` returns two records worth USD
15.00. Category `Other` combined with that week has no matches. The first and
last dates both remain included.

After a CSV round trip, the first description contains a comma and the second
contains literal double quotes around `demo`. A successful eventual import must
allocate new IDs and take ownership from the signed-in session, not this file.

Separate later tests must cover invalid/reversed dates, leap days, invalid money,
node ownership, database failures, and multiple accounts. This fixture is not
evidence those features have already been tested.
