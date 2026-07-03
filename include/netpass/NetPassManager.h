// =============================================================================
//  PetPal - NetPassManager.h
//  Owns the StreetPass / NetPass exchange. PetPal talks to the system's CECD
//  (StreetPass) service to publish an outbox packet describing our pet and to
//  read inbox packets left by other players. NetPass is a community relay that
//  forwards those same CECD messages over the internet, so the on-the-wire
//  format is identical - we only swap the transport.
//
//  The transport is abstracted behind INetPassTransport so the gameplay code is
//  testable off-device (LoopbackTransport) and the device specifics
//  (cecdu_*, NetPass relay) stay isolated in CecdTransport.
//
//  See docs/NETPASS_INTEGRATION.md for the full data-flow.
// =============================================================================
#pragma once

#include "netpass/PetPalPacket.h"
#include "pets/Pet.h"
#include "util/Time.h"
#include <memory>
#include <string>
#include <vector>

#if defined(__3DS__)
#include <3ds.h>
#include <atomic>
#endif

namespace petpal {

// A validated packet received from another player, with the time we got it.
struct ReceivedPet {
    PetPalPacket packet;
    Timestamp    receivedAt;
};

// Snapshot of the StreetPass subsystem for the UI / diagnostics. All fields are
// best-effort and cached (reading them never issues IPC), so a screen can poll
// this every frame safely.
struct StreetPassStatus {
    bool     serviceUp    = false; // transport/service is up (cecd:u opened)
    bool     boxReady     = false; // our CEC message box exists / provisioned
    bool     scanning     = false; // StreetPass exchange is enabled
    int      inboxWaiting = 0;     // messages waiting in the inbox (last poll)
    uint8_t  cecState     = 0;     // raw CecStateAbbreviated (device only)
    uint32_t lastError    = 0;     // last cecd Result code (0 == ok)
};

// -----------------------------------------------------------------------------
//  Transport abstraction
// -----------------------------------------------------------------------------
class INetPassTransport {
public:
    virtual ~INetPassTransport() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Publish the bytes other players will receive when they pass us.
    virtual bool setOutbox(const uint8_t* data, size_t len) = 0;

    // Drain the inbox: append each raw message's bytes to `out`. Returns the
    // number of messages drained, or -1 on error.
    virtual int drainInbox(std::vector<std::vector<uint8_t>>& out) = 0;

    virtual bool isAvailable() const = 0;

    // Diagnostics snapshot for the UI. Default is a minimal "up if available".
    virtual StreetPassStatus status() const {
        StreetPassStatus s;
        s.serviceUp = s.boxReady = s.scanning = isAvailable();
        return s;
    }

    // Run an on-device round-trip diagnostic (device transports only) and return
    // a short human-readable result for the UI. Default: nothing to test.
    virtual std::string selfTest() { return "self-test: n/a"; }
};

// -----------------------------------------------------------------------------
//  Manager
// -----------------------------------------------------------------------------
class NetPassManager {
public:
    // Takes ownership of a transport. Pass CecdTransport on device, or
    // LoopbackTransport in tests. If null, a LoopbackTransport is created.
    explicit NetPassManager(std::unique_ptr<INetPassTransport> transport = nullptr);
    ~NetPassManager();

    bool begin();   // init transport + publish current outbox
    void end();

    bool available() const;

    // Build (and CRC-stamp) the packet that represents `pet`. `gift` of kNoGift
    // means no gift is attached.
    static PetPalPacket buildPacket(const Pet& pet,
                                    ItemId gift = static_cast<ItemId>(kNoGift),
                                    uint16_t giftQty = 0);

    // Re-publish our outbox (call after the pet changes appearance/level).
    bool publish(const Pet& pet, ItemId gift = static_cast<ItemId>(kNoGift),
                 uint16_t giftQty = 0);

    // Validate a raw inbox message into a packet. Checks magic, version, size,
    // and CRC. Returns false for anything malformed or not a PetPal payload.
    static bool validate(const uint8_t* data, size_t len, PetPalPacket& out);

    // Poll the inbox and return every valid PetPal packet received since the
    // last poll. Self-packets (same petId as ours) are filtered out.
    std::vector<ReceivedPet> poll(uint64_t ownPetId);

    // StreetPass subsystem snapshot for the UI (works the same whether or not
    // NetPass is relaying - NetPass is a separate background app that moves the
    // very same CEC box).
    StreetPassStatus streetpassStatus() const {
        StreetPassStatus s;
        if (transport_) s = transport_->status();
        s.serviceUp = s.serviceUp && started_;
        return s;
    }

    // On-device CECD round-trip diagnostic (writes+reads our own outbox). Returns
    // a short result string for the UI. Safe to call anytime.
    std::string selfTest() { return transport_ ? transport_->selfTest() : "no transport"; }

private:
    std::unique_ptr<INetPassTransport> transport_;
    bool started_ = false;
};

// -----------------------------------------------------------------------------
//  Built-in transports
// -----------------------------------------------------------------------------

// In-memory transport for tests / PC builds. Whatever is "sent" can be looped
// back via injectInbox() so the full meeting pipeline can be exercised offline.
class LoopbackTransport : public INetPassTransport {
public:
    bool init() override { return true; }
    void shutdown() override {}
    bool setOutbox(const uint8_t* data, size_t len) override;
    int  drainInbox(std::vector<std::vector<uint8_t>>& out) override;
    bool isAvailable() const override { return true; }
    StreetPassStatus status() const override {
        StreetPassStatus s; s.serviceUp = s.boxReady = s.scanning = true;
        s.inboxWaiting = static_cast<int>(inbox_.size());
        return s;
    }

    // Test helper: queue a message as if a stranger passed us.
    void injectInbox(const uint8_t* data, size_t len);
    const std::vector<uint8_t>& lastOutbox() const { return outbox_; }

private:
    std::vector<uint8_t>              outbox_;
    std::vector<std::vector<uint8_t>> inbox_;
};

#if defined(__3DS__)
// Real device transport backed by cecdu (StreetPass) and the NetPass relay.
// Implementation lives in CecdTransport.cpp and is only compiled for the 3DS.
class CecdTransport : public INetPassTransport {
public:
    explicit CecdTransport(uint32_t cecTitleId);
    ~CecdTransport() override;

    bool init() override;
    void shutdown() override;
    bool setOutbox(const uint8_t* data, size_t len) override;
    int  drainInbox(std::vector<std::vector<uint8_t>>& out) override;
    bool isAvailable() const override { return available_; }
    StreetPassStatus status() const override;
    std::string selfTest() override;

private:
    uint32_t             titleId_;
    bool                 available_    = false;
    bool                 boxReady_     = false; // MBoxInfo exists/provisioned
    bool                 scanning_     = false; // CEC Start(START) succeeded
    uint8_t              cecState_     = 0;      // last CecStateAbbreviated
    int                  inboxWaiting_ = 0;      // messages seen at last poll
    uint32_t             lastError_    = 0;      // last cecd Result (0 == ok)
    uint32_t             titleRc_      = 0;      // last box-title write Result
    uint32_t             iconRc_       = 0;      // last box-icon write Result
    std::vector<uint8_t> outbox_;                // last published payload (cache)
};

// Internet "StreetPass": exchanges pet packets with other players through
// teampetpal.com (our own relay), sidestepping CECD entirely (homebrew cannot
// create a real CEC box). A background worker thread uploads our packet and
// downloads others, so the UI never blocks on the network. Same wire packet, so
// the meeting pipeline (friends/journal) is unchanged.
class HttpPassTransport : public INetPassTransport {
public:
    HttpPassTransport();
    ~HttpPassTransport() override;

    bool init() override;                 // start the worker thread
    void shutdown() override;             // stop + join it
    bool setOutbox(const uint8_t* data, size_t len) override; // cache our packet
    int  drainInbox(std::vector<std::vector<uint8_t>>& out) override; // drain queue
    bool isAvailable() const override { return true; }
    StreetPassStatus status() const override;
    std::string selfTest() override;      // one synchronous exchange + report

private:
    static void threadEntry(void* arg);
    void run();                           // worker loop
    bool exchangeOnce(bool upload);       // one upload/download round-trip

    Thread                     thread_ = nullptr;
    LightLock                  lock_;               // guards outbox_ / inbox_
    std::atomic<bool>          running_{false};
    std::atomic<bool>          dirty_{false};       // outbox changed -> re-upload
    std::atomic<bool>          poke_{false};        // wake the worker now
    std::atomic<int>           lastBatch_{0};       // packets received last time
    std::atomic<int>           recvTotal_{0};
    std::atomic<unsigned long> lastError_{0};       // 0 == last exchange ok
    std::atomic<bool>          lastOk_{false};
    std::vector<uint8_t>              outbox_;       // our packet (guarded)
    std::vector<std::vector<uint8_t>> inbox_;        // received packets (guarded)
};
#endif // __3DS__

} // namespace petpal
