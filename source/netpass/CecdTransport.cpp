// =============================================================================
//  PetPal - CecdTransport.cpp
//  REAL StreetPass transport over the 3DS CECD service (cecd:u). libctru ships
//  no CECD bindings, so this implements the raw IPC client directly. NetPass
//  relays these same CEC message boxes over the internet, so wiring CECD gives
//  both local StreetPass and NetPass for free.
//
//  Compiled ONLY for the 3DS (__3DS__). The PC/test build uses LoopbackTransport.
//
//  IPC command IDs / parameter layouts / structures below are taken from 3dbrew
//  and the Citra/Azahar HLE implementation. The message read/write path is
//  ABI-correct. CEC message-box *creation/exchange* for a homebrew title is the
//  part that can only be validated on real hardware (with another console or
//  NetPass) - it is written best-effort and degrades safely on any failure
//  (every call is bounded and checked; nothing here can hang).
// =============================================================================
#if defined(__3DS__)

#include "netpass/NetPassManager.h"
#include "util/Log.h"

#include <3ds.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace petpal {

namespace {

// -----------------------------------------------------------------------------
//  cecd:u raw IPC client
// -----------------------------------------------------------------------------
Handle g_cecd = 0;

// CEC "program id" identifying PetPal's message box. Must be identical across
// all PetPal installs so boxes match, and stable across versions. Derived from
// our title's unique id (0xf00d5). Change alongside the RSF UniqueId.
constexpr u32 kCecProgramId = 0x000F00D5;

enum class Path : u32 {
    MboxList   = 1,
    MboxInfo   = 2,
    InboxInfo  = 3,
    OutboxInfo = 4,
    OutboxIndex= 5,
    InboxMsg   = 6,
    OutboxMsg  = 7,
    RootDir    = 10,
    MboxDir    = 11,
    InboxDir   = 12,
    OutboxDir  = 13,
    BoxIcon    = 101,
    BoxTitle   = 110,
};

// CecOpenMode bits (read=1, write=2, create=3, check=4 -> bit positions).
enum OpenFlag : u32 {
    FlagRead   = 1u << 1,
    FlagWrite  = 1u << 2,
    FlagCreate = 1u << 3,
    FlagCheck  = 1u << 4,
};

// CecCommand values for Start/Stop (3dbrew "CECD Services"). START kicks the CEC
// daemon into its normal scan/exchange cycle; the system then swaps our box with
// passers-by (locally) or via NetPass (over the internet) - both use this state.
enum CecCommand : u32 {
    CecCmdNone       = 0x00,
    CecCmdStart      = 0x01,
    CecCmdResetStart = 0x02,
    CecCmdReadyScan  = 0x03,
    CecCmdStartScan  = 0x05,
    CecCmdStopWait   = 0x0A,
    CecCmdStop       = 0x0B,
    CecCmdStopForce  = 0x0C,
    CecCmdOverBoss   = 0x12,
};

Result cecdOpen() {
    if (g_cecd) return 0;
    return srvGetServiceHandle(&g_cecd, "cecd:u");
}
void cecdClose() {
    if (g_cecd) { svcCloseHandle(g_cecd); g_cecd = 0; }
}

// Open(0x000100C2): program id, path, open flags, [PID]. -> file/entry size.
Result cmdOpen(u32 programId, Path path, u32 flags, u32* outSize) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x1, 3, 2);
    cmd[1] = programId;
    cmd[2] = static_cast<u32>(path);
    cmd[3] = flags;
    cmd[4] = IPC_Desc_CurProcessId();
    cmd[5] = 0;
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    if (outSize) *outSize = cmd[2];
    return static_cast<Result>(cmd[1]);
}

// ReadMessage(0x00030104): program id, isOutbox, msgIdSize, bufSize,
//   <- msgId (read), -> message body (write). Returns bytes read.
Result cmdReadMessage(u32 programId, bool outbox, const u8 msgId[8],
                      void* buf, u32 bufSize, u32* outRead) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x3, 4, 4);
    cmd[1] = programId;
    cmd[2] = outbox ? 1 : 0;
    cmd[3] = 8;
    cmd[4] = bufSize;
    cmd[5] = IPC_Desc_Buffer(8, IPC_BUFFER_R);
    cmd[6] = reinterpret_cast<u32>(msgId);
    cmd[7] = IPC_Desc_Buffer(bufSize, IPC_BUFFER_W);
    cmd[8] = reinterpret_cast<u32>(buf);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    if (outRead) *outRead = cmd[2];
    return static_cast<Result>(cmd[1]);
}

// WriteMessageWithHMAC(0x000701C6): program id, isOutbox, msgIdSize, size,
//   <- message [header+body] (read), <- hmac key 0x20 (read), <-> msgId (r/w).
// Exact ABI from the NetPass source (proven on hardware). The system computes the
// HMAC from the provided key; the message buffer must be a full CecMessageHeader
// followed by the body.
Result cmdWriteMessageWithHMAC(u32 programId, bool outbox, u32 size,
                               const void* buf, const u8 msgId[8],
                               const u8 hmac[32]) {
    u8 idbuf[8];   std::memcpy(idbuf, msgId, sizeof(idbuf));
    u8 keybuf[32]; std::memcpy(keybuf, hmac, sizeof(keybuf));

    u32* cmd = getThreadCommandBuffer();
    cmd[0]  = IPC_MakeHeader(0x7, 4, 6);
    cmd[1]  = programId;
    cmd[2]  = outbox ? 1 : 0;
    cmd[3]  = 8;
    cmd[4]  = size;
    cmd[5]  = IPC_Desc_Buffer(size, IPC_BUFFER_R);
    cmd[6]  = reinterpret_cast<u32>(const_cast<void*>(buf));
    cmd[7]  = IPC_Desc_Buffer(32, IPC_BUFFER_R);
    cmd[8]  = reinterpret_cast<u32>(keybuf);
    cmd[9]  = IPC_Desc_Buffer(8, IPC_BUFFER_RW);
    cmd[10] = reinterpret_cast<u32>(idbuf);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    return static_cast<Result>(cmd[1]);
}

// Delete(0x00080102): program id, path, isOutbox, msgIdSize, <- msgId (read).
Result cmdDelete(u32 programId, Path path, bool outbox, const u8 msgId[8]) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x8, 4, 2);
    cmd[1] = programId;
    cmd[2] = static_cast<u32>(path);
    cmd[3] = outbox ? 1 : 0;
    cmd[4] = 8;
    cmd[5] = IPC_Desc_Buffer(8, IPC_BUFFER_R);
    cmd[6] = reinterpret_cast<u32>(msgId);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    return static_cast<Result>(cmd[1]);
}

// OpenAndRead(0x00120104): bufSize, program id, path, flags, [PID],
//   -> data (write). Returns bytes read.
Result cmdOpenAndRead(u32 programId, Path path, u32 flags,
                      void* buf, u32 bufSize, u32* outRead) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x12, 4, 4);
    cmd[1] = bufSize;
    cmd[2] = programId;
    cmd[3] = static_cast<u32>(path);
    cmd[4] = 0;            // NetPass passes 0 here; a nonzero value is rejected
    (void)flags;
    cmd[5] = IPC_Desc_CurProcessId();
    cmd[6] = 0;
    cmd[7] = IPC_Desc_Buffer(bufSize, IPC_BUFFER_W);
    cmd[8] = reinterpret_cast<u32>(buf);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    if (outRead) *outRead = cmd[2];
    return static_cast<Result>(cmd[1]);
}

// GetCecdState(0x000E0000): -> abbreviated state byte.
Result cmdGetState(u8* outState) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0xE, 0, 0);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    if (outState) *outState = static_cast<u8>(cmd[2]);
    return static_cast<Result>(cmd[1]);
}

// Start(0x000B0040): CecCommand. Kicks the CEC daemon into the requested state.
Result cmdStart(CecCommand command) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0xB, 1, 0);
    cmd[1] = static_cast<u32>(command);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    return static_cast<Result>(cmd[1]);
}

// Stop(0x000C0040): CecCommand. Kept for completeness; PetPal leaves CEC running
// on exit so the system keeps exchanging in sleep mode.
[[maybe_unused]] Result cmdStop(CecCommand command) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0xC, 1, 0);
    cmd[1] = static_cast<u32>(command);
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    return static_cast<Result>(cmd[1]);
}

// OpenAndWrite(0x00110104): bufSize, program id, path, flags, [PID],
//   <- data (read). Mirror of OpenAndRead but the buffer is an input.
Result cmdOpenAndWrite(u32 programId, Path path, u32 flags,
                       const void* buf, u32 bufSize) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x11, 4, 4);
    cmd[1] = bufSize;
    cmd[2] = programId;
    cmd[3] = static_cast<u32>(path);
    cmd[4] = 0;            // NetPass passes 0 here; a nonzero value is rejected
    (void)flags;
    cmd[5] = IPC_Desc_CurProcessId();
    cmd[6] = 0;
    cmd[7] = IPC_Desc_Buffer(bufSize, IPC_BUFFER_R);
    cmd[8] = reinterpret_cast<u32>(const_cast<void*>(buf));
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
    return static_cast<Result>(cmd[1]);
}

// -----------------------------------------------------------------------------
//  CEC on-disk structures (subset we need). See 3dbrew "CEC_Messages".
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
// Header of an Inbox/OutboxInfo blob, followed by message_num CecMessageHeaders.
struct CecBoxInfoHeader {
    u16 magic;            // 0x6262
    u16 padding0;
    u32 box_info_size;
    u32 max_box_size;
    u32 box_size;
    u32 max_message_num;
    u32 message_num;      // <- count of CecMessageHeader entries that follow
    u32 max_batch_size;
    u32 max_message_size;
};
constexpr int kMessageHeaderSize = 0x70; // CecMessageHeader
constexpr int kMessageIdOffset   = 0x20; // u8[8] message_id within the header

// MBoxInfo____ file (3dbrew "StreetPass"). We only touch magic + enabled; the
// rest (private_id, hmac_key) is provisioned by cecd on box creation and must be
// preserved when we rewrite the file, so read-modify-write the whole 0x60 blob.
struct CecMBoxInfo {
    u16 magic;            // 0x6363
    u16 padding0;
    u32 title_id;
    u32 private_id;
    u8  mbox_type_flags;  // 0x0C
    u8  enabled;          // 0x0D  <- StreetPass enable flag for this box
    u16 padding1;
    u8  hmac_key[0x20];   // 0x10
    u8  rest[0x30];       // timestamps + flags, out to 0x60
};
constexpr u16 kMBoxInfoMagic = 0x6363;
static_assert(sizeof(CecMBoxInfo) == 0x60, "CecMBoxInfo must be 0x60 bytes");

// CecMessageHeader (0x70). A written CEC message is this header IMMEDIATELY
// followed by the body; ReadMessage returns the same. See NarcolepticK CECDocs.
struct CecMessageHeader {
    u16 magic;             // 0x00  0x6060
    u16 padding0;          // 0x02
    u32 message_size;      // 0x04  total = header + body
    u32 total_header_size; // 0x08  0x70
    u32 body_size;         // 0x0C
    u32 title_id;          // 0x10
    u32 title_id2;         // 0x14
    u32 batch_id;          // 0x18
    u32 unknown1;          // 0x1C
    u8  message_id[8];     // 0x20
    u32 message_version;   // 0x28
    u8  message_id2[8];    // 0x2C
    u8  flags;             // 0x34
    u8  send_method;       // 0x35
    u8  unopened;          // 0x36
    u8  new_flag;          // 0x37
    u64 sender_id;         // 0x38
    u64 sender_id2;        // 0x40
    u8  sent[12];          // 0x48
    u8  received[12];      // 0x54
    u8  created[12];       // 0x60
    u8  send_count;        // 0x6C
    u8  forward_count;     // 0x6D
    u16 user_data;         // 0x6E
};
constexpr u16 kMessageMagic = 0x6060;
static_assert(sizeof(CecMessageHeader) == 0x70, "CecMessageHeader must be 0x70");
#pragma pack(pop)

// Fixed message id for our single outbox broadcast (overwritten each publish).
const u8 kOutboxMsgId[8] = { 'P', 'E', 'T', 'P', 'A', 'L', 0x00, 0x01 };

constexpr int kMaxInboxScan = 32;       // safety clamp on message enumeration

// Ensure our box is flagged StreetPass-enabled. cecd fills in private_id/hmac
// when it creates the box; we only flip the `enabled` byte, preserving the rest
// via read-modify-write of the whole MBoxInfo blob. Best-effort and bounded.
void ensureBoxEnabled(u32 titleId) {
    CecMBoxInfo info;
    u32 got = 0;
    if (R_FAILED(cmdOpenAndRead(titleId, Path::MboxInfo, FlagRead,
                                &info, sizeof(info), &got)) ||
        got < sizeof(info) || info.magic != kMBoxInfoMagic) {
        return; // can't read a valid MBoxInfo; leave whatever cecd created
    }
    if (info.enabled) return; // already enabled
    info.enabled = 1;
    cmdOpenAndWrite(titleId, Path::MboxInfo, FlagWrite, &info, sizeof(info));
}

// Write one message to a box the way the system expects (mirrors NetPass):
//  1) read MBoxInfo to get the box hmac key,
//  2) build [CecMessageHeader | body] and WriteMessageWithHMAC it,
//  3) refresh the box's BoxInfo so the message is listed for exchange.
// Returns the write Result (0 == ok). This replaces the old raw WriteMessage,
// which the system rejected with 0xC8A10BF0 (a raw body is not a valid message).
Result writeBoxMessage(u32 titleId, bool outbox, const u8 msgId[8],
                       const void* body, u32 bodyLen) {
    if (bodyLen > 256) bodyLen = 256;

    CecMBoxInfo mbox;
    u32 got = 0;
    Result rc = cmdOpenAndRead(titleId, Path::MboxInfo, 0, &mbox, sizeof(mbox), &got);
    if (R_FAILED(rc)) return rc;
    if (got < sizeof(mbox) || mbox.magic != kMBoxInfoMagic)
        return static_cast<Result>(0xC8A1DEAD); // our sentinel: no valid MBoxInfo

    // Build the full message: header immediately followed by the body.
    u8 msgbuf[sizeof(CecMessageHeader) + 256];
    std::memset(msgbuf, 0, sizeof(CecMessageHeader));
    CecMessageHeader* h = reinterpret_cast<CecMessageHeader*>(msgbuf);
    h->magic             = kMessageMagic;
    h->message_size      = sizeof(CecMessageHeader) + bodyLen;
    h->total_header_size = sizeof(CecMessageHeader);
    h->body_size         = bodyLen;
    h->title_id          = titleId;
    h->title_id2         = titleId;
    h->batch_id          = 1;
    h->message_version   = 1;
    std::memcpy(h->message_id,  msgId, 8);
    std::memcpy(h->message_id2, msgId, 8);
    h->unopened          = 1;
    h->new_flag          = 1;
    std::memcpy(msgbuf + sizeof(CecMessageHeader), body, bodyLen);

    rc = cmdWriteMessageWithHMAC(titleId, outbox, h->message_size,
                                 msgbuf, msgId, mbox.hmac_key);
    if (R_FAILED(rc)) return rc;

    // Best-effort: refresh BoxInfo to list our single message (needed so the
    // system/NetPass will actually pick it up). A failure here does not undo the
    // message write itself.
    const Path infoPath = outbox ? Path::OutboxInfo : Path::InboxInfo;
    static u8 boxbuf[sizeof(CecBoxInfoHeader) + kMessageHeaderSize];
    u32 bgot = 0;
    if (R_SUCCEEDED(cmdOpenAndRead(titleId, infoPath, 0, boxbuf, sizeof(boxbuf), &bgot)) &&
        bgot >= sizeof(CecBoxInfoHeader)) {
        CecBoxInfoHeader* bh = reinterpret_cast<CecBoxInfoHeader*>(boxbuf);
        if (bh->magic == 0x6262) {
            std::memcpy(boxbuf + sizeof(CecBoxInfoHeader), h, sizeof(CecMessageHeader));
            bh->message_num   = 1;
            bh->box_info_size = sizeof(CecBoxInfoHeader) + sizeof(CecMessageHeader);
            bh->box_size      = h->message_size;
            cmdOpenAndWrite(titleId, infoPath, 0, boxbuf, bh->box_info_size);
        }
    }
    return 0;
}

} // namespace

// -----------------------------------------------------------------------------
//  CecdTransport
// -----------------------------------------------------------------------------
CecdTransport::CecdTransport(uint32_t cecTitleId)
    : titleId_(cecTitleId ? cecTitleId : kCecProgramId) {}

CecdTransport::~CecdTransport() { shutdown(); }

bool CecdTransport::init() {
    if (R_FAILED(cecdOpen())) {
        PP_ERR("cecd:u unavailable");
        available_ = false;
        return false;
    }

    // Confirm the CEC daemon is responsive and record its state.
    cmdGetState(&cecState_);

    // 1) Build the FULL message-box structure. cecd's Open(Create) makes a single
    //    dir/file at a time (it does NOT create nested paths), so we build them in
    //    order. This matters because WriteMessage opens a file *inside* OutBox__,
    //    so that directory and its BoxInfo must already exist or the write fails
    //    with 0xC8A10BF0 (CEC / InvalidState) - the exact bug we hit.
    u32 sz = 0;

    // /CEC/<id>/ then MBoxInfo (let cecd create it so it fills private_id/hmac).
    cmdOpen(titleId_, Path::MboxDir, FlagCreate | FlagRead | FlagWrite, &sz);
    Result rc = cmdOpen(titleId_, Path::MboxInfo, FlagRead, &sz);
    if (R_FAILED(rc)) {
        rc = cmdOpen(titleId_, Path::MboxInfo,
                     FlagCreate | FlagRead | FlagWrite, &sz);
    }
    boxReady_ = R_SUCCEEDED(rc);
    if (R_FAILED(rc)) lastError_ = static_cast<u32>(rc);

    // /CEC/<id>/InBox___/ and /CEC/<id>/OutBox__/ directories.
    cmdOpen(titleId_, Path::InboxDir,  FlagCreate | FlagRead | FlagWrite, &sz);
    cmdOpen(titleId_, Path::OutboxDir, FlagCreate | FlagRead | FlagWrite, &sz);

    // Each box needs an empty BoxInfo header. Create them ONLY if missing, so we
    // never clobber messages already waiting in an existing box.
    CecBoxInfoHeader bi{};
    bi.magic            = 0x6262;
    bi.box_info_size    = sizeof(CecBoxInfoHeader);
    bi.max_box_size     = 0x8000;
    bi.max_message_num  = 8;
    bi.max_batch_size   = 8;
    bi.max_message_size = 0x1000;
    if (R_FAILED(cmdOpen(titleId_, Path::OutboxInfo, FlagRead, &sz)))
        cmdOpenAndWrite(titleId_, Path::OutboxInfo,
                        FlagCreate | FlagWrite, &bi, sizeof(bi));
    if (R_FAILED(cmdOpen(titleId_, Path::InboxInfo, FlagRead, &sz)))
        cmdOpenAndWrite(titleId_, Path::InboxInfo,
                        FlagCreate | FlagWrite, &bi, sizeof(bi));

    // 2) Flag the box StreetPass-enabled so the system will scan/relay it.
    if (boxReady_) ensureBoxEnabled(titleId_);

    // 2b) Best-effort: give the box a title + icon so the StreetPass management
    //     applet and NetPass treat it as a "real" box. The exact on-disk formats
    //     are the least-documented part of CECD, so these are our best guess
    //     (title = UTF-16LE string; icon = 48x48 RGB565 like an SMDH small icon).
    //     The write Results are surfaced by selfTest() so we can iterate on HW.
    //     Failures are harmless (bounded, non-fatal).
    if (boxReady_) {
        // Writable RAM (not .rodata) so the IPC buffer mapping is accepted.
        u16 title[16] = { 'P','e','t','P','a','l', 0 }; // UTF-16LE, rest zero
        titleRc_ = static_cast<u32>(
            cmdOpenAndWrite(titleId_, Path::BoxTitle, FlagWrite,
                            title, sizeof(title)));

        static u16 icon[48 * 48];
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x)
                icon[y * 48 + x] = ((x + y) & 8) ? 0xFD20 : 0x5B1F; // 2-tone
        iconRc_ = static_cast<u32>(
            cmdOpenAndWrite(titleId_, Path::BoxIcon, FlagWrite,
                            icon, sizeof(icon)));
        PP_LOG("box meta: title=%08lx icon=%08lx",
               (unsigned long)titleRc_, (unsigned long)iconRc_);
    }

    // 3) Nudge the CEC daemon into its scan/exchange cycle. The system then swaps
    //    our box with people we pass (local StreetPass) or with the relay
    //    (NetPass) - identical box, identical code path. Some daemon states
    //    reject START (e.g. already running); treat that as "already scanning".
    Result sr = cmdStart(CecCmdStart);
    scanning_ = R_SUCCEEDED(sr) || cecState_ != 0;
    if (R_FAILED(sr))
        PP_LOG("cecd Start(START) -> %08lx (state %u)",
               (unsigned long)sr, cecState_);

    available_ = true;
    PP_LOG("CECD ready (box=%08lx state=%u boxReady=%d scan=%d)",
           (unsigned long)titleId_, cecState_, (int)boxReady_, (int)scanning_);
    return true;
}

void CecdTransport::shutdown() {
    if (!available_) return;
    cecdClose();
    available_ = false;
}

bool CecdTransport::setOutbox(const uint8_t* data, size_t len) {
    if (!available_ || !data || len == 0) return false;
    outbox_.assign(data, data + len); // keep a local copy

    // Publish our packet as the single outbox message (proper CEC message +
    // HMAC + BoxInfo refresh). cecd hands it to anyone we StreetPass; NetPass
    // uploads it to the relay.
    Result rc = writeBoxMessage(titleId_, /*outbox=*/true, kOutboxMsgId,
                                data, static_cast<u32>(len));
    if (R_FAILED(rc)) {
        lastError_ = static_cast<u32>(rc);
        PP_WARN("outbox write failed: %08lx", (unsigned long)rc);
        return false;
    }
    return true;
}

int CecdTransport::drainInbox(std::vector<std::vector<uint8_t>>& out) {
    if (!available_) return -1;

    // Refresh the daemon state each poll so the status line stays live.
    cmdGetState(&cecState_);

    // 1) Read the inbox info blob: a CecBoxInfoHeader followed by one
    //    CecMessageHeader per waiting message.
    static u8 infoBuf[sizeof(CecBoxInfoHeader) + kMessageHeaderSize * kMaxInboxScan];
    u32 readSize = 0;
    Result rc = cmdOpenAndRead(titleId_, Path::InboxInfo, FlagRead,
                               infoBuf, sizeof(infoBuf), &readSize);
    if (R_FAILED(rc) || readSize < sizeof(CecBoxInfoHeader)) {
        if (R_FAILED(rc)) lastError_ = static_cast<u32>(rc);
        inboxWaiting_ = 0;
        return 0;
    }

    CecBoxInfoHeader hdr;
    std::memcpy(&hdr, infoBuf, sizeof(hdr));
    u32 count = hdr.message_num;
    inboxWaiting_ = static_cast<int>(count);          // true waiting count
    if (count > kMaxInboxScan) count = kMaxInboxScan; // clamp (safety)

    int drained = 0;
    for (u32 i = 0; i < count; ++i) {
        const size_t entry = sizeof(CecBoxInfoHeader) + static_cast<size_t>(i) * kMessageHeaderSize;
        if (entry + kMessageHeaderSize > readSize) break;

        u8 msgId[8];
        std::memcpy(msgId, infoBuf + entry + kMessageIdOffset, 8);

        // 2) Read this message ([CecMessageHeader | body]) and hand up the BODY
        //    (skip the 0x70 header - ReadMessage returns the full message).
        u8 msg[sizeof(CecMessageHeader) + 64];
        u32 got = 0;
        if (R_SUCCEEDED(cmdReadMessage(titleId_, /*outbox=*/false, msgId,
                                       msg, sizeof(msg), &got)) &&
            got > sizeof(CecMessageHeader)) {
            const u8* pkt = msg + sizeof(CecMessageHeader);
            const size_t pktLen = got - sizeof(CecMessageHeader);
            out.emplace_back(pkt, pkt + pktLen);
            ++drained;
        }

        // 3) Remove it so we don't process the same pet twice.
        cmdDelete(titleId_, Path::InboxMsg, /*outbox=*/false, msgId);
    }

    return drained;
}

StreetPassStatus CecdTransport::status() const {
    StreetPassStatus s;
    s.serviceUp    = available_;
    s.boxReady     = boxReady_;
    s.scanning     = scanning_;
    s.inboxWaiting = inboxWaiting_;
    s.cecState     = cecState_;
    s.lastError    = lastError_;
    return s;
}

// On-device stage-by-stage diagnostic so we can see EXACTLY which cecd call fails
// (rather than one opaque code). Runs: read MBoxInfo -> WriteMessageWithHMAC ->
// ReadMessage back, reporting the first failing stage with its Result.
std::string CecdTransport::selfTest() {
    if (!available_) return "CECD: unavailable";
    char buf[96];

    // Stage 1: find a readable MBoxInfo (magic 0x6363 + hmac key). Probe both the
    // unique-id form and the low-title-id form so we can tell an id-derivation
    // problem apart from "no valid box exists at all".
    CecMBoxInfo mbox;
    const u32 idA = titleId_;      // as configured (unique id 0x000F00D5)
    const u32 idB = 0x0F00D500u;   // low title-id form
    u32 gotA = 0, gotB = 0;
    Result rcA = cmdOpenAndRead(idA, Path::MboxInfo, 0, &mbox, sizeof(mbox), &gotA);
    const u16 magicA = mbox.magic;
    Result rcB = cmdOpenAndRead(idB, Path::MboxInfo, 0, &mbox, sizeof(mbox), &gotB);
    const u16 magicB = mbox.magic;

    const bool okA = R_SUCCEEDED(rcA) && gotA >= sizeof(mbox) && magicA == kMBoxInfoMagic;
    const bool okB = R_SUCCEEDED(rcB) && gotB >= sizeof(mbox) && magicB == kMBoxInfoMagic;
    const u32 useId = okA ? idA : (okB ? idB : 0);
    if (!useId) {
        std::snprintf(buf, sizeof(buf),
                      "S1 no box: A=%08lX/%04X B=%08lX/%04X",
                      (unsigned long)rcA, (unsigned)magicA,
                      (unsigned long)rcB, (unsigned)magicB);
        return buf;
    }
    // Re-read the good box so `mbox` (hmac key) matches useId.
    cmdOpenAndRead(useId, Path::MboxInfo, 0, &mbox, sizeof(mbox), &gotA);

    // Stage 2: write a proper [CecMessageHeader | body] with HMAC.
    u8 pat[48];
    for (int i = 0; i < 48; ++i) pat[i] = static_cast<u8>(0xA0 ^ i);
    u8 msgbuf[sizeof(CecMessageHeader) + 64];
    std::memset(msgbuf, 0, sizeof(CecMessageHeader));
    CecMessageHeader* h = reinterpret_cast<CecMessageHeader*>(msgbuf);
    h->magic             = kMessageMagic;
    h->message_size      = sizeof(CecMessageHeader) + sizeof(pat);
    h->total_header_size = sizeof(CecMessageHeader);
    h->body_size         = sizeof(pat);
    h->title_id          = useId;
    h->title_id2         = useId;
    h->batch_id          = 1;
    h->message_version   = 1;
    std::memcpy(h->message_id,  kOutboxMsgId, 8);
    std::memcpy(h->message_id2, kOutboxMsgId, 8);
    h->unopened          = 1;
    h->new_flag          = 1;
    std::memcpy(msgbuf + sizeof(CecMessageHeader), pat, sizeof(pat));

    Result wrc = cmdWriteMessageWithHMAC(useId, /*outbox=*/true, h->message_size,
                                         msgbuf, kOutboxMsgId, mbox.hmac_key);
    if (R_FAILED(wrc)) {
        std::snprintf(buf, sizeof(buf), "S2 WHMAC=%08lX id=%08lX en=%u",
                      (unsigned long)wrc, (unsigned long)useId, (unsigned)mbox.enabled);
        return buf;
    }

    // Stage 3: read it back and compare the body.
    u8 rmsg[sizeof(CecMessageHeader) + 64];
    u32 got = 0;
    Result rrc = cmdReadMessage(useId, /*outbox=*/true, kOutboxMsgId,
                                rmsg, sizeof(rmsg), &got);
    if (!outbox_.empty())
        writeBoxMessage(useId, /*outbox=*/true, kOutboxMsgId,
                        outbox_.data(), static_cast<u32>(outbox_.size()));
    if (R_FAILED(rrc)) {
        std::snprintf(buf, sizeof(buf), "S3 read=%08lX (write OK!)", (unsigned long)rrc);
        return buf;
    }

    const bool ok = got >= sizeof(CecMessageHeader) + sizeof(pat) &&
                    std::memcmp(rmsg + sizeof(CecMessageHeader), pat, sizeof(pat)) == 0;
    std::snprintf(buf, sizeof(buf), "CECD I/O %s id=%08lX (rd %lu)",
                  ok ? "OK" : "MISMATCH", (unsigned long)useId, (unsigned long)got);
    return buf;
}

} // namespace petpal

#endif // __3DS__
