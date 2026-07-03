// =============================================================================
//  PetPal - host test harness
//  Compiles ONLY the portable model layer (no libctru / citro2d) and exercises
//  the core systems off-device. This is the proof that gameplay logic, save
//  serialization, and the NetPass meeting pipeline are testable without a 3DS.
//
//  Build (any host g++/clang++, C++17):
//      see tests/Makefile  ->  `make -C tests`  (or the g++ line in tests/README)
// =============================================================================
#include "core/Types.h"
#include "core/Names.h"
#include "pets/Pet.h"
#include "items/Inventory.h"
#include "friends/FriendList.h"
#include "journal/Journal.h"
#include "adventure/Adventure.h"
#include "achievements/Achievements.h"
#include "netpass/NetPassManager.h"
#include "save/SaveManager.h"
#include "util/Random.h"

#include <cstdio>
#include <cstring>

using namespace petpal;

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond) do { \
    ++g_checks; \
    if (!(cond)) { ++g_failures; std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

// -----------------------------------------------------------------------------
static void testPetProgression() {
    std::printf("[pet progression]\n");
    Pet p = Pet::createStarter(Species::Fox, "Ziggy");
    CHECK(std::strcmp(p.name(), "Ziggy") == 0);
    CHECK(p.level() == 1);
    CHECK(p.stage() == EvolutionStage::Baby);

    // Name truncation.
    p.setName("AbsurdlyLongPetName");
    CHECK(std::strlen(p.name()) <= (size_t)kMaxPetNameLen);

    // XP -> level up.
    ActionResult r = p.addExperience(Pet::xpForLevel(1));
    CHECK(r.leveledUp);
    CHECK(p.level() == 2);

    // Encounters drive evolution eligibility.
    CHECK(!p.canEvolve());
    for (int i = 0; i < 50; ++i) p.recordEncounter(true);
    CHECK(p.canEvolve());
    CHECK(p.evolve() == EvolutionStage::Teen);
}

static void testInventory() {
    std::printf("[inventory]\n");
    Inventory inv;
    inv.add(ItemId::Apple, 3);
    CHECK(inv.count(ItemId::Apple) == 3);
    CHECK(inv.remove(ItemId::Apple, 5) == 3);
    CHECK(inv.count(ItemId::Apple) == 0);

    inv.addCoins(100);
    CHECK(inv.spendCoins(40));
    CHECK(inv.coins() == 60);
    CHECK(!inv.spendCoins(1000));
    CHECK(inv.coins() == 60);
}

static void testNetPassMeeting() {
    std::printf("[netpass meeting]\n");
    Pet me   = Pet::createStarter(Species::Cat, "Mochi");
    Pet other= Pet::createStarter(Species::Dragon, "Luna");

    // Build the visitor's packet and validate the round-trip.
    PetPalPacket pkt = NetPassManager::buildPacket(other);
    PetPalPacket parsed;
    CHECK(NetPassManager::validate(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt), parsed));
    CHECK(parsed.petId == other.id());

    // Corrupting a byte must fail validation (CRC).
    pkt.level ^= 0xFF;
    CHECK(!NetPassManager::validate(reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt), parsed));

    // Full pipeline through the loopback transport.
    auto transport = std::make_unique<LoopbackTransport>();
    LoopbackTransport* loop = transport.get();
    NetPassManager net(std::move(transport));
    net.begin();
    net.publish(me);

    PetPalPacket good = NetPassManager::buildPacket(other);
    loop->injectInbox(reinterpret_cast<uint8_t*>(&good), sizeof(good));

    std::vector<ReceivedPet> met = net.poll(me.id());
    CHECK(met.size() == 1);

    FriendList friends;
    MeetingOutcome out = friends.ingest(met[0].packet, met[0].receivedAt);
    CHECK(out.isNewFriend);
    CHECK(friends.size() == 1);
    CHECK(std::strcmp(friends.at(0).name(), "Luna") == 0);

    // Meeting the same pet again updates, not duplicates.
    friends.ingest(met[0].packet, met[0].receivedAt + 100);
    CHECK(friends.size() == 1);
    CHECK(friends.at(0).meetings() == 2);
}

static void testAchievements() {
    std::printf("[achievements]\n");
    Achievements ach;
    AchievementContext ctx;
    ctx.totalFriends = 1;
    auto earned = ach.evaluate(ctx);
    CHECK(ach.isUnlocked(AchievementId::FirstFriend));
    CHECK(earned.size() == 1);
    // Re-evaluating doesn't re-award.
    earned = ach.evaluate(ctx);
    CHECK(earned.empty());
}

static void testAdventure() {
    std::printf("[adventure]\n");
    Random rng(12345);
    Adventure adv;
    const Timestamp t0 = 1000;
    CHECK(adv.begin(Location::Beach, AdventureDuration::Short, t0));
    CHECK(adv.active());
    CHECK(!adv.isComplete(t0));                       // not yet
    const Timestamp done = t0 + kAdventureSeconds[(int)AdventureDuration::Short];
    CHECK(adv.isComplete(done));
    AdventureResult r = adv.collect("Ziggy", done, rng);
    CHECK(r.valid);
    CHECK(r.coins > 0);
    CHECK(!r.items.empty());
    CHECK(!adv.active());
}

static void testSaveRoundTrip() {
    std::printf("[save round-trip]\n");
    PersistentState a;
    a.pet = Pet::createStarter(Species::Robot, "Bolt");
    a.pet.addExperience(250);
    a.inventory.add(ItemId::Gem, 7);
    a.inventory.addCoins(123);
    a.world.unlock(Location::Forest);
    a.world.unlock(Location::Beach);
    a.journal.add(JournalCategory::General, 42, "Hello world.");
    a.meta.onboarded = true;
    a.meta.createdAt = 1700000000;

    std::vector<uint8_t> blob = SaveManager::serialize(a);
    CHECK(!blob.empty());

    PersistentState b;
    SaveError e = SaveManager::deserialize(blob.data(), blob.size(), b);
    CHECK(e == SaveError::None);
    CHECK(b.pet.id() == a.pet.id());
    CHECK(std::strcmp(b.pet.name(), "Bolt") == 0);
    CHECK(b.inventory.count(ItemId::Gem) == 7);
    CHECK(b.inventory.coins() == 123);
    CHECK(b.world.isUnlocked(Location::Forest));
    CHECK(b.world.isUnlocked(Location::Beach));
    CHECK(b.journal.size() == 1);
    CHECK(b.meta.onboarded);

    // Corruption is detected.
    blob[blob.size() / 2] ^= 0xFF;
    PersistentState c;
    CHECK(SaveManager::deserialize(blob.data(), blob.size(), c) == SaveError::Corrupt);
}

int main() {
    std::printf("PetPal model tests\n==================\n");
    testPetProgression();
    testInventory();
    testNetPassMeeting();
    testAchievements();
    testAdventure();
    testSaveRoundTrip();

    std::printf("==================\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
