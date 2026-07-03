#include "util/Crc32.h"

namespace petpal {

namespace {
struct Crc32Table {
    uint32_t t[256];
    Crc32Table() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88820u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
    }
};
const Crc32Table kTable;
} // namespace

uint32_t crc32(const void* data, size_t len, uint32_t seed) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t c = seed ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        c = kTable.t[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

} // namespace petpal
