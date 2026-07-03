// =============================================================================
//  PetPal - SaveManager.h
//  Serializes PersistentState to a versioned, CRC-checked binary blob and
//  reads it back. Handles the physical file (SD card on 3DS, working dir on a
//  PC test build), atomic writes via a temp file, and a one-slot backup so a
//  crash mid-write never destroys a good save. Also exports a human-portable
//  backup for the "backup save export" requirement.
//
//  File format is documented in docs/SAVE_FORMAT.md.
// =============================================================================
#pragma once

#include "save/SaveData.h"
#include "util/ByteBuffer.h"   // ByteWriter / ByteReader
#include <string>
#include <vector>

namespace petpal {

enum class SaveError {
    None = 0,
    FileNotFound,
    IoError,
    BadMagic,
    BadVersion,
    Corrupt,        // CRC mismatch or truncated
    Unsupported,    // newer save than this build understands
};

class SaveManager {
public:
    // `baseDir` should end with a path separator. On 3DS pass something under
    // "sdmc:/3ds/PetPal/"; on PC pass "./" for tests.
    explicit SaveManager(std::string baseDir);

    // --- Primary API --------------------------------------------------------
    bool      exists() const;
    SaveError load(PersistentState& out) const;   // tries main, then backup
    SaveError save(const PersistentState& state) const; // atomic + backup

    // --- Backup export / import (player-portable copies) --------------------
    // Writes a timestamped copy to baseDir + "backups/". Returns the path.
    SaveError exportBackup(const PersistentState& state, std::string& outPath) const;
    SaveError importBackup(const std::string& path, PersistentState& out) const;

    // --- Serialization (exposed for unit tests / NetPass debug) -------------
    static std::vector<uint8_t> serialize(const PersistentState& state);
    static SaveError            deserialize(const uint8_t* data, size_t len,
                                            PersistentState& out);

    const std::string& mainPath()   const { return mainPath_; }
    const std::string& backupPath() const { return backupPath_; }

private:
    // Body (everything after the file header) is serialized/parsed here.
    // `version` lets decode tolerate older saves (fields appended over time).
    static void encodeBody(ByteWriter& w, const PersistentState& s);
    static bool decodeBody(ByteReader& r, PersistentState& s, uint16_t version);

    static bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes);
    static bool readFile(const std::string& path, std::vector<uint8_t>& out);

    std::string baseDir_;
    std::string mainPath_;
    std::string backupPath_;
};

} // namespace petpal
