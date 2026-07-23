// =============================================================================
//  PetPal - NetPassManager.cpp
//  Transport-agnostic logic: packet building, validation, and the poll loop.
//  Device specifics live in CecdTransport.cpp.
// =============================================================================
#include "netpass/NetPassManager.h"
#include "util/Crc32.h"
#include "util/Log.h"
#include "util/NameFilter.h"
#include <cstring>

namespace petpal {

// -----------------------------------------------------------------------------
//  Packet build / validate
// -----------------------------------------------------------------------------
PetPalPacket NetPassManager::buildPacket(const Pet& pet, ItemId gift, uint16_t giftQty) {
    PetPalPacket pkt{};
    pkt.magic     = kNetPassMagic;
    pkt.version   = kNetPassVersion;
    // Stamp our app version (minor<<8 | patch) into the otherwise-unused
    // reserved0 slot so teampetpal.com/api/pass can refuse out-of-date clients
    // (see functions/api/pass.js, cfg:min_app_version). It's covered by the CRC
    // below and ignored by receivers, so it stays compatible with older builds
    // that send 0 here.
    pkt.reserved0 = static_cast<uint16_t>(kAppVersion & 0xFFFF);

    pkt.petId = pet.id();
    std::memset(pkt.name, 0, sizeof(pkt.name));
    std::strncpy(pkt.name, pet.name(), kMaxPetNameLen);
    pkt.name[kMaxPetNameLen] = '\0';
    cleanName(pkt.name, sizeof(pkt.name)); // never broadcast a bad name

    pkt.species = static_cast<uint8_t>(pet.species());
    pkt.stage   = static_cast<uint8_t>(pet.stage());
    pkt.level   = pet.level();

    pkt.primaryColor   = static_cast<uint8_t>(pet.primaryColor());
    pkt.secondaryColor = static_cast<uint8_t>(pet.secondaryColor());
    pkt.accentColor    = static_cast<uint8_t>(pet.accentColor());
    pkt.style          = static_cast<uint8_t>(pet.equippedStyle());

    pkt.favoriteItem = static_cast<uint16_t>(pet.favoriteItem());
    pkt.reserved1    = 0;

    pkt.giftItem = static_cast<uint16_t>(gift);
    pkt.giftQty  = giftQty;

    // CRC over everything except the crc field itself (which is last).
    pkt.crc32 = 0;
    pkt.crc32 = crc32(&pkt, sizeof(PetPalPacket) - sizeof(uint32_t));
    return pkt;
}

bool NetPassManager::validate(const uint8_t* data, size_t len, PetPalPacket& out) {
    if (!data || len < sizeof(PetPalPacket)) return false;
    std::memcpy(&out, data, sizeof(PetPalPacket));

    if (out.magic != kNetPassMagic) return false;
    if (out.version != kNetPassVersion) return false; // future: migrate older
    if (out.species >= kSpeciesCount)  return false;
    if (out.stage   >= kEvolutionStageCount) return false;

    const uint32_t claimed = out.crc32;
    out.crc32 = 0;
    const uint32_t actual = crc32(&out, sizeof(PetPalPacket) - sizeof(uint32_t));
    out.crc32 = claimed;
    if (claimed != actual) { PP_WARN("packet crc mismatch"); return false; }

    // Defensive: ensure the name is null-terminated.
    out.name[kMaxPetNameLen] = '\0';
    return true;
}

// -----------------------------------------------------------------------------
//  Manager lifecycle
// -----------------------------------------------------------------------------
NetPassManager::NetPassManager(std::unique_ptr<INetPassTransport> transport)
    : transport_(std::move(transport)) {
    if (!transport_) transport_ = std::make_unique<LoopbackTransport>();
}

NetPassManager::~NetPassManager() { end(); }

bool NetPassManager::begin() {
    if (started_) return true;
    started_ = transport_->init();
    return started_;
}

void NetPassManager::end() {
    if (!started_) return;
    transport_->shutdown();
    started_ = false;
}

bool NetPassManager::available() const {
    return transport_ && transport_->isAvailable();
}

bool NetPassManager::publish(const Pet& pet, ItemId gift, uint16_t giftQty) {
    if (!started_) return false;
    const PetPalPacket pkt = buildPacket(pet, gift, giftQty);
    return transport_->setOutbox(reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

std::vector<ReceivedPet> NetPassManager::poll(uint64_t ownPetId) {
    std::vector<ReceivedPet> result;
    if (!started_) return result;

    std::vector<std::vector<uint8_t>> raw;
    const int n = transport_->drainInbox(raw);
    if (n <= 0) return result;

    const Timestamp now = nowSeconds();
    for (const auto& msg : raw) {
        PetPalPacket pkt;
        if (!validate(msg.data(), msg.size(), pkt)) continue;
        if (pkt.petId == ownPetId) continue; // ignore our own echo
        result.push_back(ReceivedPet{ pkt, now });
    }
    PP_LOG("netpass poll: %d raw, %d valid", n, (int)result.size());
    return result;
}

// -----------------------------------------------------------------------------
//  LoopbackTransport
// -----------------------------------------------------------------------------
bool LoopbackTransport::setOutbox(const uint8_t* data, size_t len) {
    outbox_.assign(data, data + len);
    return true;
}

int LoopbackTransport::drainInbox(std::vector<std::vector<uint8_t>>& out) {
    const int n = static_cast<int>(inbox_.size());
    for (auto& m : inbox_) out.push_back(std::move(m));
    inbox_.clear();
    return n;
}

void LoopbackTransport::injectInbox(const uint8_t* data, size_t len) {
    inbox_.emplace_back(data, data + len);
}

} // namespace petpal
