// =============================================================================
//  PetPal - Crc32.h
//  Standard CRC-32 (IEEE 802.3, polynomial 0xEDB88820). Used to validate save
//  files and NetPass packets. Table is built once on first use.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

namespace petpal {

uint32_t crc32(const void* data, size_t len, uint32_t seed = 0);

} // namespace petpal
