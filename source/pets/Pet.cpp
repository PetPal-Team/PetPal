// =============================================================================
//  PetPal - Pet.cpp
// =============================================================================
#include "pets/Pet.h"
#include "core/Names.h"   // itemCategory()
#include "util/NameFilter.h"
#include "util/Random.h"
#include <algorithm>
#include <cstring>

namespace petpal {

namespace {
// Clamp helper for the 0..kStatMax stats.
inline uint8_t clampStat(int v) {
    if (v < 0) return 0;
    if (v > kStatMax) return kStatMax;
    return static_cast<uint8_t>(v);
}
} // namespace

Pet::Pet()
    : id_(0),
      species_(Species::Fox),
      name_("Pet"),
      primary_(PetColor::Orange),
      secondary_(PetColor::White),
      accent_(PetColor::Brown),
      equippedAccessory_(Accessory::None),
      equippedStyle_(Style::Classic),
      accessoryUnlockMask_(1u << static_cast<int>(Accessory::None)),
      styleUnlockMask_(1u << static_cast<int>(Style::Classic)),
      level_(1),
      experience_(0),
      happiness_(80),
      energy_(80),
      friendship_(0),
      adventureRank_(0),
      streetpassEncounters_(0),
      uniquePetsMet_(0),
      adventuresCompleted_(0),
      stage_(EvolutionStage::Baby),
      favoriteItem_(ItemId::Cookie) {}

Pet Pet::createStarter(Species species, const char* name) {
    Pet p;
    // 64-bit id from two PRNG draws so two pets are extremely unlikely to clash.
    Random& r = Random::global();
    p.id_ = (static_cast<uint64_t>(r.next()) << 32) ^ r.next();
    p.species_ = species;
    p.setName(name);

    // Give each species a pleasant default palette.
    static const PetColor kDefaults[kSpeciesCount][3] = {
        /* Fox     */ { PetColor::Orange, PetColor::White,  PetColor::Brown  },
        /* Cat     */ { PetColor::Yellow, PetColor::White,  PetColor::Pink   },
        /* Bunny   */ { PetColor::White,  PetColor::Pink,   PetColor::Brown  },
        /* Dragon  */ { PetColor::Red,    PetColor::Yellow, PetColor::Orange },
        /* Slime   */ { PetColor::Green,  PetColor::White,  PetColor::Blue   },
        /* Robot   */ { PetColor::Blue,   PetColor::White,  PetColor::Red    },
        /* Axolotl */ { PetColor::Pink,   PetColor::White,  PetColor::Purple },
    };
    const int s = static_cast<int>(species);
    p.setColors(kDefaults[s][0], kDefaults[s][1], kDefaults[s][2]);
    return p;
}

void Pet::setName(const char* name) {
    if (!name) return;
    name_.assign(name);
    if (static_cast<int>(name_.size()) > kMaxPetNameLen)
        name_.resize(kMaxPetNameLen);
    if (isBadName(name_.c_str())) name_ = "Pal";  // no bad names, even our own
}

void Pet::setColors(PetColor primary, PetColor secondary, PetColor accent) {
    primary_   = primary;
    secondary_ = secondary;
    accent_    = accent;
}

// -----------------------------------------------------------------------------
//  Appearance unlocks
// -----------------------------------------------------------------------------
bool Pet::isAccessoryUnlocked(Accessory a) const {
    return (accessoryUnlockMask_ >> static_cast<int>(a)) & 1u;
}
bool Pet::isStyleUnlocked(Style s) const {
    return (styleUnlockMask_ >> static_cast<int>(s)) & 1u;
}
void Pet::unlockAccessory(Accessory a) {
    accessoryUnlockMask_ |= (1u << static_cast<int>(a));
}
void Pet::unlockStyle(Style s) {
    styleUnlockMask_ |= (1u << static_cast<int>(s));
}
bool Pet::equipAccessory(Accessory a) {
    if (!isAccessoryUnlocked(a)) return false;
    equippedAccessory_ = a;
    return true;
}
bool Pet::equipStyle(Style s) {
    if (!isStyleUnlocked(s)) return false;
    equippedStyle_ = s;
    return true;
}
int Pet::unlockedAccessoryCount() const {
    int n = 0;
    for (int i = 1; i < kAccessoryCount; ++i) // skip None
        if (isAccessoryUnlocked(static_cast<Accessory>(i))) ++n;
    return n;
}

// -----------------------------------------------------------------------------
//  Experience & levels
//  XP needed to clear a level grows gently so early play feels rewarding:
//      need(L) = kXpPerLevelBase * L  (e.g. 100, 200, 300, ...)
// -----------------------------------------------------------------------------
uint32_t Pet::xpForLevel(uint16_t level) {
    if (level >= kMaxLevel) return 0xFFFFFFFFu; // effectively unreachable
    return kXpPerLevelBase * static_cast<uint32_t>(level);
}

float Pet::levelProgress() const {
    const uint32_t need = xpForLevel(level_);
    if (need == 0) return 1.0f;
    float p = static_cast<float>(experience_) / static_cast<float>(need);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

ActionResult Pet::addExperience(uint32_t xp) {
    ActionResult res;
    experience_ += xp;
    while (level_ < kMaxLevel && experience_ >= xpForLevel(level_)) {
        experience_ -= xpForLevel(level_);
        ++level_;
        res.leveledUp = true;
        res.newLevel  = level_;
    }
    if (level_ >= kMaxLevel) experience_ = 0;
    res.canEvolveNow = canEvolve();
    return res;
}

// -----------------------------------------------------------------------------
//  Daily activities
// -----------------------------------------------------------------------------
ActionResult Pet::feed(ItemId food) {
    ActionResult res;
    int restore = 12;
    if (itemCategory(food) == ItemCategory::RareFood) restore = 30;
    if (food == favoriteItem_) restore += 8; // pets love their favorite
    energy_    = clampStat(energy_ + restore);
    happiness_ = clampStat(happiness_ + 4);
    res.statChanged = true;
    // A small XP nibble for caretaking.
    ActionResult xp = addExperience(5);
    res.leveledUp = xp.leveledUp; res.newLevel = xp.newLevel;
    res.canEvolveNow = canEvolve();
    return res;
}

ActionResult Pet::play() {
    ActionResult res;
    happiness_ = clampStat(happiness_ + 10);
    energy_    = clampStat(energy_ - 8);
    res.statChanged = true;
    ActionResult xp = addExperience(10);
    res.leveledUp = xp.leveledUp; res.newLevel = xp.newLevel;
    res.canEvolveNow = canEvolve();
    return res;
}

ActionResult Pet::petPet() {
    ActionResult res;
    happiness_ = clampStat(happiness_ + 6);
    res.statChanged = true;
    res.canEvolveNow = canEvolve();
    return res;
}

ActionResult Pet::talk() {
    ActionResult res;
    happiness_ = clampStat(happiness_ + 3);
    addFriendship(1);
    res.statChanged = true;
    res.canEvolveNow = canEvolve();
    return res;
}

void Pet::addFriendship(uint32_t amount) {
    friendship_ += amount;
}

// -----------------------------------------------------------------------------
//  Encounters & evolution
// -----------------------------------------------------------------------------
ActionResult Pet::recordEncounter(bool unique) {
    ++streetpassEncounters_;
    if (unique) ++uniquePetsMet_;
    addFriendship(2);
    ActionResult res = addExperience(unique ? 25 : 10);
    res.canEvolveNow = canEvolve();
    return res;
}

bool Pet::canEvolve() const {
    const int next = static_cast<int>(stage_) + 1;
    if (next >= kEvolutionStageCount) return false;
    const EvolutionRequirement& req = kEvolutionTable[next];
    return streetpassEncounters_ >= req.streetpassEncounters &&
           uniquePetsMet_        >= req.uniquePetsMet;
}

EvolutionStage Pet::evolve() {
    if (canEvolve())
        stage_ = static_cast<EvolutionStage>(static_cast<int>(stage_) + 1);
    return stage_;
}

void Pet::recomputeStage() {
    // Used after load to ensure stage reflects earned progress (in case the
    // evolution sequence was interrupted before being applied).
    for (int s = kEvolutionStageCount - 1; s >= 0; --s) {
        const EvolutionRequirement& req = kEvolutionTable[s];
        if (streetpassEncounters_ >= req.streetpassEncounters &&
            uniquePetsMet_ >= req.uniquePetsMet) {
            // Only auto-advance if the player has *passed through* lower stages.
            // We keep stage_ as the max of stored value and earned floor only
            // for Baby->Teen->Adult; Rare/Legendary remain player-confirmed.
            if (s <= static_cast<int>(EvolutionStage::Adult) &&
                static_cast<int>(stage_) < s) {
                stage_ = static_cast<EvolutionStage>(s);
            }
            break;
        }
    }
}

void Pet::setTransform(TransformForm f, uint32_t seconds) {
    if (f == TransformForm::None || seconds == 0) {
        transformForm_ = TransformForm::None;
        transformExpiry_ = 0;
        return;
    }
    transformForm_   = f;
    transformExpiry_ = nowSeconds() + seconds;
}

TransformForm Pet::currentForm() const {
    if (transformForm_ != TransformForm::None && nowSeconds() < transformExpiry_)
        return transformForm_;
    return TransformForm::None;
}

void Pet::applyDecay(uint32_t days) {
    if (days == 0) return;
    const int drop = std::min<int>(days * 5, kStatMax);
    energy_    = clampStat(energy_ - drop);
    happiness_ = clampStat(happiness_ - drop / 2);
}

} // namespace petpal
