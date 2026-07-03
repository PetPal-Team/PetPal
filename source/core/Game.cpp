// =============================================================================
//  PetPal - Game.cpp
// =============================================================================
#include "core/Game.h"
#include "core/Names.h"
#include "audio/Audio.h"
#include "util/Log.h"

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#include <curl/curl.h>
#endif

#include <cstdio>
#include <cstring>

namespace petpal {

namespace {
constexpr float kAutosaveInterval = 30.0f; // seconds between autosaves
constexpr float kNetPassInterval  = 10.0f; // seconds between inbox polls
} // namespace

#ifdef __3DS__
namespace {
// 3ds-curl needs BSD sockets (soc) initialized once for the whole app, plus a
// single curl_global_init. We do it here (not per request) so the RedeemTask
// worker can just call curl_easy_* directly. soc:U service access is granted in
// petpal.rsf.
constexpr size_t kSocBufferSize = 0x100000; // 1 MiB, must be 0x1000-aligned
u32* g_socBuffer = nullptr;
bool g_netReady  = false;

bool netInit() {
    if (g_netReady) return true;
    g_socBuffer = static_cast<u32*>(memalign(0x1000, kSocBufferSize));
    if (!g_socBuffer) { PP_WARN("soc buffer alloc failed"); return false; }
    if (R_FAILED(socInit(g_socBuffer, kSocBufferSize))) {
        PP_WARN("socInit failed");
        free(g_socBuffer);
        g_socBuffer = nullptr;
        return false;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_netReady = true;
    return true;
}

void netExit() {
    if (!g_netReady) return;
    curl_global_cleanup();
    socExit();
    if (g_socBuffer) { free(g_socBuffer); g_socBuffer = nullptr; }
    g_netReady = false;
}
} // namespace
#endif

Game::Game(std::string baseDir)
    : baseDir_(std::move(baseDir)),
      save_(baseDir_),
#ifdef __3DS__
      // Internet relay (our own StreetPass over teampetpal.com). Homebrew can't
      // create a real CEC box, so CecdTransport is a dead end; HttpPassTransport
      // delivers the same "meet other pets" pipeline over the net instead.
      netpass_(std::make_unique<HttpPassTransport>()),
#else
      netpass_(nullptr),                            // LoopbackTransport in tests
#endif
      ui_(this),
      rng_(static_cast<uint64_t>(nowSeconds()) * 2654435761ULL + 0x1234) {}

Game::~Game() { shutdown(); }

bool Game::init() {
    // 1) Load existing save, or stand up a fresh state for onboarding.
    if (save_.exists()) {
        SaveError e = save_.load(state_);
        if (e != SaveError::None) {
            PP_WARN("save load failed (%d); starting fresh", (int)e);
            state_ = PersistentState{};
        } else {
            // Catch the pet up on time passed while the app was closed.
            const uint32_t days = daysBetween(state_.meta.lastSavedAt, nowSeconds());
            state_.pet.applyDecay(days);
        }
    }
    if (state_.meta.createdAt == 0) state_.meta.createdAt = nowSeconds();

    // 2) Graphics + screens.
    if (!ui_.init()) {
        PP_ERR("UI init failed");
        return false;
    }

#ifdef __3DS__
    // 2b) Networking for online code redemption (sockets + curl). Non-fatal:
    // the app still runs offline; only the redeem feature needs this.
    if (!netInit()) PP_WARN("network init failed; online codes unavailable");
#endif

    // 2c) Audio (streamed BGM + SFX). Non-fatal: silent if the DSP is
    // unavailable. Apply the player's saved volumes.
    if (audio::init()) {
        audio::setMusicVolume(state_.settings.musicVolume / 100.0f);
        audio::setSfxVolume(state_.settings.sfxVolume / 100.0f);
    }

    // 3) NetPass / StreetPass exchange.
    if (netpass_.begin() && state_.meta.onboarded)
        publishSelf();

    lastTick_ = nowSeconds();
    return true;
}

void Game::run() {
#ifdef __3DS__
    u64 prevTick = svcGetSystemTick();
    float netAcc = 0.0f;

    while (aptMainLoop() && !quit_) {
        const u64 now = svcGetSystemTick();
        float dt = static_cast<float>(now - prevTick) / static_cast<float>(SYSCLOCK_ARM11);
        prevTick = now;
        if (dt > 0.1f) dt = 0.1f; // clamp after suspend

        // Periodic StreetPass/NetPass inbox poll.
        netAcc += dt;
        if (netAcc >= kNetPassInterval) { netAcc = 0.0f; if (state_.meta.onboarded) processNetPass(); }

        // Track playtime.
        state_.meta.playSeconds += static_cast<uint64_t>(dt);

        if (!ui_.tick(dt)) quit_ = true;
        autosaveTick(dt);
    }
    requestSave(); // final save on exit
#else
    // PC/test build: no windowing loop. Tests drive systems directly.
    PP_LOG("Game::run() is a no-op off device");
#endif
}

void Game::shutdown() {
    audio::exit();
    netpass_.end();
    ui_.shutdown();
#ifdef __3DS__
    netExit();
#endif
}

// -----------------------------------------------------------------------------
//  Onboarding
// -----------------------------------------------------------------------------
void Game::createPet(Species species, const char* name) {
    state_.pet = Pet::createStarter(species, name);
    state_.meta.onboarded = true;
    state_.meta.createdAt = nowSeconds();

    // Starter kit so a new player can immediately feed, shop, and explore
    // instead of staring at an empty inventory.
    inventory().addCoins(50);
    inventory().add(ItemId::Apple, 3);
    inventory().add(ItemId::Cookie, 2);

    journal().add(JournalCategory::General, nowSeconds(),
                  "My adventure begins today! I can't wait to make friends.");

    publishSelf();
    requestSave();
}

// -----------------------------------------------------------------------------
//  Daily activities
// -----------------------------------------------------------------------------
static ActionFeedback toFeedback(const ActionResult& r, std::string msg) {
    ActionFeedback fb;
    fb.leveledUp = r.leveledUp;
    fb.newLevel  = r.newLevel;
    fb.canEvolve = r.canEvolveNow;
    fb.message   = std::move(msg);
    return fb;
}

ActionFeedback Game::feedPet(ItemId food) {
    if (!inventory().has(food)) {
        ActionFeedback fb; fb.ok = false; fb.message = "You don't have that food.";
        return fb;
    }
    inventory().remove(food, 1);
    audio::playSfx(audio::Sfx::Eat);
    ActionResult r = pet().feed(food);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Yum! %s loves the %s.", pet().name(), itemName(food));
    ActionFeedback fb = toFeedback(r, buf);
    if (fb.leveledUp) ui_.celebrate(UIManager::Celebration::LevelUp);
    return fb;
}

ActionFeedback Game::playWithPet() {
    ActionResult r = pet().play();
    ActionFeedback fb = toFeedback(r, "What fun!");
    if (fb.leveledUp) ui_.celebrate(UIManager::Celebration::LevelUp);
    return fb;
}

ActionFeedback Game::petThePet() {
    ActionResult r = pet().petPet();
    return toFeedback(r, "So cozy!");
}

ActionFeedback Game::talkToPet() {
    ActionResult r = pet().talk();
    return toFeedback(r, "You had a nice chat.");
}

// -----------------------------------------------------------------------------
//  Adventures
// -----------------------------------------------------------------------------
bool Game::startAdventure(Location loc, AdventureDuration dur) {
    if (!world().isUnlocked(loc)) return false;
    if (pet().energy() < 10) return false; // too tired
    return adventure().begin(loc, dur, nowSeconds());
}

AdventureResult Game::collectAdventure() {
    AdventureResult res = adventure().collect(pet().name(), nowSeconds(), rng_);
    if (!res.valid) return res;

    inventory().addCoins(res.coins);
    for (const RewardItem& it : res.items)
        inventory().add(it.id, it.qty);

    pet().onAdventureCompleted();
    journal().logAdventure(res.story.c_str(), nowSeconds());

    ui_.celebrate(UIManager::Celebration::Reward);
    runAchievementChecks();
    requestSave();
    return res;
}

// -----------------------------------------------------------------------------
//  Evolution
// -----------------------------------------------------------------------------
void Game::confirmEvolution() {
    if (!pet().canEvolve()) return;
    const EvolutionStage stage = pet().evolve();

    journal().logEvolved(stage, nowSeconds());
    ui_.celebrate(UIManager::Celebration::Evolution);

    // Evolution grants a cosmetic flourish: unlock a style per milestone.
    switch (stage) {
        case EvolutionStage::Teen:          pet().unlockStyle(Style::Spring);   break;
        case EvolutionStage::Adult:         pet().unlockStyle(Style::Royal);    break;
        case EvolutionStage::RareForm:      pet().unlockStyle(Style::Space);    break;
        case EvolutionStage::LegendaryForm: pet().unlockStyle(Style::Cyber);    break;
        default: break;
    }

    publishSelf();
    runAchievementChecks();
    requestSave();
}

// -----------------------------------------------------------------------------
//  StreetPass / NetPass
// -----------------------------------------------------------------------------
void Game::processOneEncounter(const PetPalPacket& pkt, Timestamp when) {
    MeetingOutcome out = friends().ingest(pkt, when);

    // Pet gains XP/friendship from the encounter.
    ActionResult r = pet().recordEncounter(out.isNewFriend);

    // Journal the meeting in the pet's voice.
    if (out.isNewFriend)
        journal().logMetFriend(pkt.name, static_cast<Species>(pkt.species), when);

    // Apply any gift the visitor left behind.
    if (pkt.giftItem != kNoGift && pkt.giftItem < kItemCount) {
        const ItemId gift = static_cast<ItemId>(pkt.giftItem);
        inventory().add(gift, pkt.giftQty ? pkt.giftQty : 1);
        journal().logFoundItem(gift, when);
    }

    // Encounters occasionally open up a new place to explore.
    maybeUnlockLocation(); // journal handled inside

    if (out.isNewFriend) ui_.celebrate(UIManager::Celebration::NewFriend);
    if (r.leveledUp)     ui_.celebrate(UIManager::Celebration::LevelUp);
}

int Game::processNetPass() {
    std::vector<ReceivedPet> met = netpass_.poll(pet().id());
    if (met.empty()) return 0;

    for (const ReceivedPet& rp : met)
        processOneEncounter(rp.packet, rp.receivedAt);

    runAchievementChecks();
    publishSelf(); // our stats changed; refresh the outbox
    requestSave();
    return static_cast<int>(met.size());
}

#if PETPAL_DEBUG
void Game::injectTestPass() {
    static uint64_t seq = 1;
    PetPalPacket pkt{};
    pkt.magic   = kNetPassMagic;
    pkt.version = kNetPassVersion;
    pkt.petId   = 0x7E570000ULL + seq;          // "TEST" ids, always non-self
    std::snprintf(pkt.name, sizeof(pkt.name), "TestPal%u", (unsigned)seq);
    ++seq;
    pkt.species        = static_cast<uint8_t>(nowSeconds() % kSpeciesCount);
    pkt.stage          = 0;
    pkt.level          = 5;
    pkt.primaryColor   = 0;
    pkt.secondaryColor = 1;
    pkt.accentColor    = 2;
    pkt.style          = 0;
    pkt.favoriteItem   = static_cast<uint16_t>(ItemId::Apple);
    pkt.giftItem       = static_cast<uint16_t>(ItemId::Cookie);
    pkt.giftQty        = 1;

    // Straight into the real pipeline (bypasses CECD + CRC validate on purpose).
    processOneEncounter(pkt, nowSeconds());
    runAchievementChecks();
    publishSelf();
    requestSave();
}
#endif

void Game::applyRedeem(const RedeemResult& res) {
    if (!res.ok) return;

    switch (res.reward) {
        case RedeemResult::Reward::Items:
            inventory().add(res.item, res.qty);
            break;
        case RedeemResult::Reward::Accessory:
            pet().unlockAccessory(res.accessory);
            pet().equipAccessory(res.accessory);
            break;
        case RedeemResult::Reward::Transform:
            pet().setTransform(res.form, res.seconds);
            if (res.form == TransformForm::Bonzi) audio::playSfx(audio::Sfx::Bonzi);
            break;
        default: break;
    }

    journal().add(JournalCategory::General, nowSeconds(),
                  res.message.empty() ? "I redeemed a code!" : res.message.c_str());
    ui_.celebrate(UIManager::Celebration::Reward);
    publishSelf();
    requestSave();
}

void Game::publishSelf() {
    // Attach the pet's favorite item as a small gift, if we can spare one.
    ItemId gift = static_cast<ItemId>(kNoGift);
    uint16_t qty = 0;
    if (inventory().count(pet().favoriteItem()) > 1) {
        gift = pet().favoriteItem();
        qty  = 1;
    }
    netpass_.publish(pet(), gift, qty);
}

bool Game::maybeUnlockLocation() {
    // 20% chance to unlock the next locked location.
    if (!rng_.chance(20)) return false;
    for (int i = 0; i < kLocationCount; ++i) {
        const Location loc = static_cast<Location>(i);
        if (!world().isUnlocked(loc)) {
            world().unlock(loc);
            journal().logExplored(loc, nowSeconds());
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Currency
// -----------------------------------------------------------------------------
bool Game::buyItem(ItemId id, uint32_t price, uint16_t qty) {
    if (!inventory().spendCoins(price)) return false;
    inventory().add(id, qty);
    requestSave();
    return true;
}

// -----------------------------------------------------------------------------
//  Achievements
// -----------------------------------------------------------------------------
AchievementContext Game::buildAchievementContext() const {
    AchievementContext ctx;
    ctx.totalFriends        = state_.friends.size();
    ctx.petStage            = state_.pet.stage();
    ctx.locationsUnlocked   = state_.world.unlockedCount();
    ctx.accessoriesUnlocked = state_.pet.unlockedAccessoryCount();
    ctx.adventuresCompleted = state_.pet.adventuresCompleted();
    return ctx;
}

void Game::runAchievementChecks() {
    std::vector<AchievementId> earned = achievements().evaluate(buildAchievementContext());
    for (AchievementId id : earned) {
        AchievementReward reward = Achievements::rewardFor(id);
        if (reward.kind == RewardKind::Accessory)
            pet().unlockAccessory(static_cast<Accessory>(reward.value));
        else
            pet().unlockStyle(static_cast<Style>(reward.value));

        journal().logAchievement(id, nowSeconds());
        ui_.celebrate(UIManager::Celebration::Achievement);
    }
}

// -----------------------------------------------------------------------------
//  Persistence
// -----------------------------------------------------------------------------
void Game::requestSave() {
    state_.meta.lastSavedAt = nowSeconds();
    SaveError e = save_.save(state_);
    if (e != SaveError::None) PP_WARN("save failed (%d)", (int)e);
}

void Game::exportBackup(std::string& outPath) {
    save_.exportBackup(state_, outPath);
}

void Game::autosaveTick(float dt) {
    if (!settings().autoSave) return;
    autosaveAcc_ += dt;
    if (autosaveAcc_ >= kAutosaveInterval) {
        autosaveAcc_ = 0.0f;
        requestSave();
    }
}

} // namespace petpal
