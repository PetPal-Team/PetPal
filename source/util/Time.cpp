#include "util/Time.h"
#include <cstdio>

namespace petpal {

void formatDate(Timestamp t, char* out, int outSize) {
    if (!out || outSize <= 0) return;
    const std::time_t tt = static_cast<std::time_t>(t);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &tt);
#else
    // libctru / newlib provides localtime_r
    localtime_r(&tt, &tmv);
#endif
    std::snprintf(out, outSize, "%04d-%02d-%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

} // namespace petpal
