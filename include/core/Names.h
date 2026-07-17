// =============================================================================
//  PetPal - Names.h
//  Human-readable display names and flavor text for the enum types in Types.h.
//  These are presentation-only; never persist a name string, persist the enum.
// =============================================================================
#pragma once

#include "core/Types.h"

namespace petpal {

const char* speciesName(Species s);
const char* speciesDescription(Species s);   // one-line "about" blurb
const char* speciesPersonality(Species s);   // playful personality flavor text

const char* colorName(PetColor c);
uint32_t    colorRGBA(PetColor c);           // 0xRRGGBBAA for rendering/tinting

const char* accessoryName(Accessory a);
const char* styleName(Style s);

const char* evolutionStageName(EvolutionStage e);
const char* friendshipLevelName(FriendshipLevel f);
const char* moodName(Mood m);
const char* locationName(Location l);
const char* adventureDurationName(AdventureDuration d);

const char* itemName(ItemId id);
const char* itemDescription(ItemId id);
ItemCategory itemCategory(ItemId id);

const char* achievementName(AchievementId id);
const char* achievementDescription(AchievementId id);

} // namespace petpal
