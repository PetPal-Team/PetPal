// =============================================================================
//  PetPal - Journal.h
//  The pet's diary. Entries are generated automatically by gameplay systems and
//  are written in a charming first-person voice ("Today I met Mochi..."). The
//  journal is a capped, newest-first log so the save stays bounded.
// =============================================================================
#pragma once

#include "core/Types.h"
#include "util/Time.h"
#include <string>
#include <vector>

namespace petpal {

// Drives the little icon shown beside each entry.
enum class JournalCategory : uint8_t {
    General = 0,
    Friend,
    Adventure,
    Item,
    Evolution,
    Achievement,
    Count
};

struct JournalEntry {
    Timestamp       when = 0;
    JournalCategory category = JournalCategory::General;
    std::string     text;
};

class Journal {
public:
    static constexpr int kMaxEntries = 200; // oldest entries roll off

    // Raw add. Most callers should use the typed helpers below.
    void add(JournalCategory cat, Timestamp when, const char* text);

    // --- Typed helpers: build personality-driven, pet-named sentences --------
    void logMetFriend(const char* petName, Species friendName, Timestamp when);
    void logExplored(Location loc, Timestamp when);
    void logFoundItem(ItemId item, Timestamp when);
    void logEvolved(EvolutionStage newStage, Timestamp when);
    void logAchievement(AchievementId id, Timestamp when);
    void logAdventure(const char* storyLine, Timestamp when);

    int  size() const { return static_cast<int>(entries_.size()); }
    // Index 0 is the newest entry.
    const JournalEntry& at(int i) const { return entries_[i]; }

    const std::vector<JournalEntry>& all() const { return entries_; }

private:
    std::vector<JournalEntry> entries_; // front() == newest
    friend class SaveManager;
};

} // namespace petpal
