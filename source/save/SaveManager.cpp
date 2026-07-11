// =============================================================================
//  PetPal - SaveManager.cpp
//  See docs/SAVE_FORMAT.md for the byte layout.
// =============================================================================
#include "save/SaveManager.h"
#include "util/ByteBuffer.h"
#include "util/Crc32.h"
#include "util/Log.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#if defined(_WIN32)
  #include <direct.h>
  #define PP_MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define PP_MKDIR(p) ::mkdir(p, 0777)
#endif

namespace petpal {

// File header that precedes the serialized body.
#pragma pack(push, 1)
struct SaveHeader {
    uint32_t magic;     // kSaveMagic
    uint16_t version;   // kSaveVersion
    uint16_t reserved;  // 0
    uint32_t bodyLen;   // bytes of body following the header
    uint32_t bodyCrc;   // crc32 of the body
};
#pragma pack(pop)

namespace {
// Create every directory along `path` (which ends in a separator). Existing
// directories are fine; failures are ignored (the later fopen will surface any
// real problem). Works for both "sdmc:/3ds/PetPal/" and "./".
void ensureDir(const std::string& path) {
    std::string acc;
    for (size_t i = 0; i < path.size(); ++i) {
        const char ch = path[i];
        acc.push_back(ch);
        if (ch == '/' || ch == '\\') {
            // Skip the drive/scheme root (e.g. "sdmc:/" or "C:/").
            if (acc == "." || acc == "./" || acc.back() == ':' ||
                (acc.size() >= 2 && acc[acc.size() - 2] == ':'))
                continue;
            PP_MKDIR(acc.c_str());
        }
    }
}
} // namespace

SaveManager::SaveManager(std::string baseDir) : baseDir_(std::move(baseDir)) {
    ensureDir(baseDir_);
    mainPath_   = baseDir_ + "save.bin";
    backupPath_ = baseDir_ + "save.bak";
}

// -----------------------------------------------------------------------------
//  Body encode/decode
// -----------------------------------------------------------------------------
void SaveManager::encodeBody(ByteWriter& w, const PersistentState& s) {
    // --- Pet ---
    const Pet& p = s.pet;
    w.putU64(p.id_);
    w.putU8(static_cast<uint8_t>(p.species_));
    w.putString(p.name_);
    w.putU8(static_cast<uint8_t>(p.primary_));
    w.putU8(static_cast<uint8_t>(p.secondary_));
    w.putU8(static_cast<uint8_t>(p.accent_));
    w.putU8(static_cast<uint8_t>(p.equippedAccessory_));
    w.putU8(static_cast<uint8_t>(p.equippedStyle_));
    w.putU16(p.accessoryUnlockMask_);
    w.putU16(p.styleUnlockMask_);
    w.putU16(p.level_);
    w.putU32(p.experience_);
    w.putU8(p.happiness_);
    w.putU8(p.energy_);
    w.putU32(p.friendship_);
    w.putU16(p.adventureRank_);
    w.putU32(p.streetpassEncounters_);
    w.putU32(p.uniquePetsMet_);
    w.putU32(p.adventuresCompleted_);
    w.putU8(static_cast<uint8_t>(p.stage_));
    w.putU16(static_cast<uint16_t>(p.favoriteItem_));

    // --- Inventory ---
    const Inventory& inv = s.inventory;
    w.putU16(static_cast<uint16_t>(kItemCount));
    for (int i = 0; i < kItemCount; ++i) w.putU16(inv.counts_[i]);
    w.putU32(inv.coins_);

    // --- Friends ---
    const auto& friends = s.friends.friends_;
    w.putU32(static_cast<uint32_t>(friends.size()));
    for (const Friend& f : friends) {
        w.putU64(f.id_);
        w.putString(f.name_);
        w.putU8(static_cast<uint8_t>(f.species_));
        w.putU16(f.level_);
        w.putU8(static_cast<uint8_t>(f.stage_));
        w.putU8(static_cast<uint8_t>(f.primary_));
        w.putU8(static_cast<uint8_t>(f.style_));
        w.putU16(static_cast<uint16_t>(f.favoriteItem_));
        w.putU64(f.dateMet_);
        w.putU32(f.meetings_);
        w.putU32(f.friendshipPoints_);
    }

    // --- Journal ---
    const auto& entries = s.journal.entries_;
    w.putU32(static_cast<uint32_t>(entries.size()));
    for (const JournalEntry& e : entries) {
        w.putU64(e.when);
        w.putU8(static_cast<uint8_t>(e.category));
        w.putString(e.text);
    }

    // --- Adventure ---
    const Adventure& a = s.adventure;
    w.putU8(a.active_ ? 1 : 0);
    w.putU8(static_cast<uint8_t>(a.location_));
    w.putU8(static_cast<uint8_t>(a.duration_));
    w.putU64(a.startTime_);

    // --- Achievements ---
    w.putU32(s.achievements.mask_);

    // --- World ---
    w.putU16(s.world.locationUnlockMask);

    // --- Settings ---
    w.putU8(s.settings.musicVolume);
    w.putU8(s.settings.sfxVolume);
    w.putU8(s.settings.autoSave ? 1 : 0);
    w.putU8(s.settings.rumble ? 1 : 0);

    // --- Meta ---
    w.putU64(s.meta.createdAt);
    w.putU64(s.meta.lastSavedAt);
    w.putU64(s.meta.playSeconds);
    w.putU8(s.meta.onboarded ? 1 : 0);

    // --- v2: pet transform (redeem codes) ---
    w.putU8(static_cast<uint8_t>(s.pet.transformForm_));
    w.putU64(s.pet.transformExpiry_);

    // --- v3: server account (id/token/linked) ---
    w.putString(s.account.id);
    w.putString(s.account.token);
    w.putU8(s.account.linked ? 1 : 0);
}

bool SaveManager::decodeBody(ByteReader& r, PersistentState& s, uint16_t version) {
    // --- Pet ---
    Pet& p = s.pet;
    p.id_      = r.getU64();
    p.species_ = static_cast<Species>(r.getU8());
    p.name_    = r.getString();
    p.primary_ = static_cast<PetColor>(r.getU8());
    p.secondary_ = static_cast<PetColor>(r.getU8());
    p.accent_  = static_cast<PetColor>(r.getU8());
    p.equippedAccessory_ = static_cast<Accessory>(r.getU8());
    p.equippedStyle_     = static_cast<Style>(r.getU8());
    p.accessoryUnlockMask_ = r.getU16();
    p.styleUnlockMask_     = r.getU16();
    p.level_      = r.getU16();
    p.experience_ = r.getU32();
    p.happiness_  = r.getU8();
    p.energy_     = r.getU8();
    p.friendship_ = r.getU32();
    p.adventureRank_ = r.getU16();
    p.streetpassEncounters_ = r.getU32();
    p.uniquePetsMet_        = r.getU32();
    p.adventuresCompleted_  = r.getU32();
    p.stage_        = static_cast<EvolutionStage>(r.getU8());
    p.favoriteItem_ = static_cast<ItemId>(r.getU16());

    // --- Inventory ---
    Inventory& inv = s.inventory;
    const uint16_t itemCount = r.getU16();
    for (int i = 0; i < itemCount; ++i) {
        const uint16_t v = r.getU16();
        if (i < kItemCount) inv.counts_[i] = v; // tolerate item-table growth
    }
    inv.coins_ = r.getU32();

    // --- Friends ---
    const uint32_t friendCount = r.getU32();
    s.friends.friends_.clear();
    for (uint32_t i = 0; i < friendCount && r.ok(); ++i) {
        Friend f;
        f.id_   = r.getU64();
        f.name_ = r.getString();
        f.species_ = static_cast<Species>(r.getU8());
        f.level_   = r.getU16();
        f.stage_   = static_cast<EvolutionStage>(r.getU8());
        f.primary_ = static_cast<PetColor>(r.getU8());
        f.style_   = static_cast<Style>(r.getU8());
        f.favoriteItem_ = static_cast<ItemId>(r.getU16());
        f.dateMet_  = r.getU64();
        f.meetings_ = r.getU32();
        f.friendshipPoints_ = r.getU32();
        s.friends.friends_.push_back(std::move(f));
    }

    // --- Journal ---
    const uint32_t journalCount = r.getU32();
    s.journal.entries_.clear();
    for (uint32_t i = 0; i < journalCount && r.ok(); ++i) {
        JournalEntry e;
        e.when     = r.getU64();
        e.category = static_cast<JournalCategory>(r.getU8());
        e.text     = r.getString();
        s.journal.entries_.push_back(std::move(e));
    }

    // --- Adventure ---
    Adventure& a = s.adventure;
    a.active_    = r.getU8() != 0;
    a.location_  = static_cast<Location>(r.getU8());
    a.duration_  = static_cast<AdventureDuration>(r.getU8());
    a.startTime_ = r.getU64();

    // --- Achievements ---
    s.achievements.mask_ = r.getU32();

    // --- World ---
    s.world.locationUnlockMask = r.getU16();

    // --- Settings ---
    s.settings.musicVolume = r.getU8();
    s.settings.sfxVolume   = r.getU8();
    s.settings.autoSave    = r.getU8() != 0;
    s.settings.rumble      = r.getU8() != 0;

    // --- Meta ---
    s.meta.createdAt   = r.getU64();
    s.meta.lastSavedAt = r.getU64();
    s.meta.playSeconds = r.getU64();
    s.meta.onboarded   = r.getU8() != 0;

    // --- v2: pet transform (absent in v1 saves; default to None) ---
    if (version >= 2) {
        s.pet.transformForm_   = static_cast<TransformForm>(r.getU8());
        s.pet.transformExpiry_ = r.getU64();
    } else {
        s.pet.transformForm_   = TransformForm::None;
        s.pet.transformExpiry_ = 0;
    }

    // --- v3: server account (absent in v1/v2 saves; stays empty) ---
    if (version >= 3) {
        s.account.id     = r.getString();
        s.account.token  = r.getString();
        s.account.linked = r.getU8() != 0;
    } else {
        s.account = Account{};
    }

    return r.ok();
}

// -----------------------------------------------------------------------------
//  Serialize / deserialize (header + body + crc)
// -----------------------------------------------------------------------------
std::vector<uint8_t> SaveManager::serialize(const PersistentState& state) {
    ByteWriter body;
    encodeBody(body, state);

    SaveHeader h{};
    h.magic    = kSaveMagic;
    h.version  = kSaveVersion;
    h.reserved = 0;
    h.bodyLen  = static_cast<uint32_t>(body.size());
    h.bodyCrc  = crc32(body.bytes().data(), body.size());

    std::vector<uint8_t> out;
    out.reserve(sizeof(SaveHeader) + body.size());
    const uint8_t* hp = reinterpret_cast<const uint8_t*>(&h);
    out.insert(out.end(), hp, hp + sizeof(SaveHeader));
    out.insert(out.end(), body.bytes().begin(), body.bytes().end());
    return out;
}

SaveError SaveManager::deserialize(const uint8_t* data, size_t len, PersistentState& out) {
    if (len < sizeof(SaveHeader)) return SaveError::Corrupt;
    SaveHeader h{};
    std::memcpy(&h, data, sizeof(SaveHeader));

    if (h.magic != kSaveMagic)       return SaveError::BadMagic;
    if (h.version == 0)              return SaveError::BadVersion;
    if (h.version > kSaveVersion)    return SaveError::Unsupported; // newer than us
    if (sizeof(SaveHeader) + h.bodyLen > len) return SaveError::Corrupt;

    const uint8_t* body = data + sizeof(SaveHeader);
    if (crc32(body, h.bodyLen) != h.bodyCrc) return SaveError::Corrupt;

    ByteReader r(body, h.bodyLen);
    if (!decodeBody(r, out, h.version)) return SaveError::Corrupt;
    return SaveError::None;
}

// -----------------------------------------------------------------------------
//  File I/O
// -----------------------------------------------------------------------------
bool SaveManager::writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
    const bool ok = (n == bytes.size());
    std::fflush(f);
    std::fclose(f);
    return ok;
}

bool SaveManager::readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(sz));
    const size_t n = (sz > 0) ? std::fread(out.data(), 1, out.size(), f) : 0;
    std::fclose(f);
    return n == out.size();
}

bool SaveManager::exists() const {
    std::FILE* f = std::fopen(mainPath_.c_str(), "rb");
    if (f) { std::fclose(f); return true; }
    f = std::fopen(backupPath_.c_str(), "rb");
    if (f) { std::fclose(f); return true; }
    return false;
}

// -----------------------------------------------------------------------------
//  Load: try main, fall back to backup on corruption.
// -----------------------------------------------------------------------------
SaveError SaveManager::load(PersistentState& out) const {
    std::vector<uint8_t> bytes;
    if (readFile(mainPath_, bytes)) {
        SaveError e = deserialize(bytes.data(), bytes.size(), out);
        if (e == SaveError::None) return e;
        PP_WARN("main save unreadable (%d), trying backup", (int)e);
    }
    if (readFile(backupPath_, bytes)) {
        SaveError e = deserialize(bytes.data(), bytes.size(), out);
        if (e == SaveError::None) { PP_LOG("restored from backup"); return e; }
        return e;
    }
    return SaveError::FileNotFound;
}

// -----------------------------------------------------------------------------
//  Save: write to temp, rotate current main -> backup, swap temp -> main.
// -----------------------------------------------------------------------------
SaveError SaveManager::save(const PersistentState& state) const {
    const std::vector<uint8_t> bytes = serialize(state);
    const std::string tmpPath = mainPath_ + ".tmp";

    if (!writeFile(tmpPath, bytes)) return SaveError::IoError;

    // Rotate the existing good save into the backup slot (best-effort).
    std::FILE* probe = std::fopen(mainPath_.c_str(), "rb");
    if (probe) {
        std::fclose(probe);
        std::remove(backupPath_.c_str());           // rename fails if dest exists
        std::rename(mainPath_.c_str(), backupPath_.c_str());
    }

    std::remove(mainPath_.c_str());
    if (std::rename(tmpPath.c_str(), mainPath_.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return SaveError::IoError;
    }
    return SaveError::None;
}

// -----------------------------------------------------------------------------
//  Portable backup export / import
// -----------------------------------------------------------------------------
SaveError SaveManager::exportBackup(const PersistentState& state, std::string& outPath) const {
    const std::string dir = baseDir_ + "backups/";
    PP_MKDIR(dir.c_str()); // ignore "already exists"

    char stamp[32];
    const std::time_t t = static_cast<std::time_t>(state.meta.lastSavedAt
                              ? state.meta.lastSavedAt : nowSeconds());
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d_%02d%02d%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    outPath = dir + "petpal_" + stamp + ".ppsav";
    const std::vector<uint8_t> bytes = serialize(state);
    return writeFile(outPath, bytes) ? SaveError::None : SaveError::IoError;
}

SaveError SaveManager::importBackup(const std::string& path, PersistentState& out) const {
    std::vector<uint8_t> bytes;
    if (!readFile(path, bytes)) return SaveError::FileNotFound;
    return deserialize(bytes.data(), bytes.size(), out);
}

} // namespace petpal
