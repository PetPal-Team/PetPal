// =============================================================================
//  PetPal - PetPalPacket.h
//  The fixed-size payload PetPal broadcasts and receives over StreetPass /
//  NetPass. This is a packed POD so it can be memcpy'd straight into the
//  CECD message box. NEVER change field order or sizes without bumping
//  kNetPassVersion and handling migration in NetPassManager.
//
//  Layout is little-endian (the 3DS is LE). Total size is kept small and
//  16-byte aligned to fit comfortably in a StreetPass message slot.
// =============================================================================
#pragma once

#include "core/Types.h"

namespace petpal {

#pragma pack(push, 1)
struct PetPalPacket {
    // --- Header -------------------------------------------------------------
    uint32_t magic;        // kNetPassMagic, identifies a PetPal payload
    uint16_t version;      // kNetPassVersion
    uint16_t reserved0;    // padding / future flags

    // --- Pet identity & appearance -----------------------------------------
    uint64_t petId;        // sender's unique pet id
    char     name[kMaxNameBuffer]; // null-terminated, may be shorter
    uint8_t  species;      // Species
    uint8_t  stage;        // EvolutionStage
    uint16_t level;        // 1..kMaxLevel

    uint8_t  primaryColor;   // PetColor
    uint8_t  secondaryColor; // PetColor
    uint8_t  accentColor;    // PetColor
    uint8_t  style;          // Style

    uint16_t favoriteItem;   // ItemId
    uint16_t reserved1;      // padding / future flags

    // --- Optional gift the visiting pet leaves behind -----------------------
    uint16_t giftItem;       // ItemId, or 0xFFFF for "no gift"
    uint16_t giftQty;

    // --- Integrity ----------------------------------------------------------
    uint32_t crc32;        // CRC32 of all preceding bytes (crc field = 0)
};
#pragma pack(pop)

constexpr uint16_t kNoGift = 0xFFFF;

// Compile-time sanity: keep the packet a predictable, modest size.
static_assert(sizeof(PetPalPacket) <= 64, "PetPalPacket grew too large for a StreetPass slot");

} // namespace petpal
