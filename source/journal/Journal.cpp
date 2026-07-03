#include "journal/Journal.h"
#include "core/Names.h"
#include "util/Random.h"
#include <cstdio>

namespace petpal {

void Journal::add(JournalCategory cat, Timestamp when, const char* text) {
    JournalEntry e;
    e.when = when;
    e.category = cat;
    e.text.assign(text ? text : "");
    entries_.insert(entries_.begin(), std::move(e)); // newest first
    if (static_cast<int>(entries_.size()) > kMaxEntries)
        entries_.pop_back();
}

void Journal::logMetFriend(const char* petName, Species friendSpecies, Timestamp when) {
    static const char* kTemplates[] = {
        "Today I met %s the %s. So exciting!",
        "I made a new friend: a %s named... well, %s!",
        "%s the %s waved hello. I waved back!",
    };
    const char* tmpl = kTemplates[Random::global().range(3)];
    char buf[96];
    std::snprintf(buf, sizeof(buf), tmpl, petName, speciesName(friendSpecies));
    add(JournalCategory::Friend, when, buf);
}

void Journal::logExplored(Location loc, Timestamp when) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "I explored the %s.", locationName(loc));
    add(JournalCategory::Adventure, when, buf);
}

void Journal::logFoundItem(ItemId item, Timestamp when) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "I found a %s!", itemName(item));
    add(JournalCategory::Item, when, buf);
}

void Journal::logEvolved(EvolutionStage newStage, Timestamp when) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "I evolved into a %s! I feel different...",
                  evolutionStageName(newStage));
    add(JournalCategory::Evolution, when, buf);
}

void Journal::logAchievement(AchievementId id, Timestamp when) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Achievement unlocked: %s!", achievementName(id));
    add(JournalCategory::Achievement, when, buf);
}

void Journal::logAdventure(const char* storyLine, Timestamp when) {
    add(JournalCategory::Adventure, when, storyLine);
}

} // namespace petpal
