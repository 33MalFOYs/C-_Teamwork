#pragma once

// Connector 8.1's mysql_util.h chooses MSVC-only i64/ui64 suffixes for all
// Windows compilers. Its #ifndef guards allow standard C++ integer suffixes
// here without changing the downloaded source or arithmetic semantics.
#if defined(__MINGW32__)
#define UL64(value) value##ULL
#define L64(value) value##LL
#endif
