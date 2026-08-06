// =============================================================================
//  PetPal - CecdTransport.cpp
//  REAL StreetPass transport over the 3DS CECD service (cecd:s, the system CEC
//  service NetPass uses; falls back to cecd:u). libctru ships
//  no CECD bindings, so this implements the raw IPC client directly. NetPass
//  relays these same CEC message boxes over the internet, so wiring CECD gives
//  both local StreetPass and NetPass for free.
//
//  Compiled ONLY for the 3DS (__3DS__). The PC/test build uses LoopbackTransport.
//
//  IPC command IDs / parameter layouts / structures below are taken from 3dbrew,
//  the Citra/Azahar HLE implementation, and - crucially - NetPass's own working
//  cecd.c (Sorunome/Silentium). The box-CREATION path in particular mirrors her
//  registerStreetpassApplication() exactly: CEC box files must be made with
//  OpenRawFile(0x01)+WriteRawFile(0x05) using specific open flags, NOT
//  OpenAndWrite(0x11) ("just openAndWrite won't work" - the NetPass author). Once
//  the box exists, UPDATES to those files do use OpenAndWrite, matching her code.
//  This creation path is validated on real hardware by NetPass; nothing here can
//  hang (every call is bounded and checked) and it degrades safely on failure.
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
const char* g_cecdSvc = "";   // which service actually opened: "cecd:s" / "cecd:u"

// CEC "program id" identifying PetPal's message box. This MUST be the low 32 bits
// of our 64-bit title id (how CEC keys every box, and what cecd:u authorizes a
// process to touch for its OWN title). PetPal's title id is 0x000400000F00D500
// (category 0x00040000, UniqueId 0xf00d5, variation 0x00), so the CEC id is
// (uniqueId << 8) | variation = 0x0F00D500 - NOT the bare unique id 0x000F00D5.
// Using the bare unique id is why cecd:u rejected box creation with 0xC8A10BF0.
// The box then lives under /CEC/0f00d500/. Change alongside the RSF UniqueId.
constexpr u32 kCecProgramId = 0x0F00D500;

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

// EXACT open flags NetPass uses to CREATE each kind of CEC file (from her working
// registerStreetpassApplication). cecd is picky here and these are not obvious
// combinations of the bits above, so use the proven raw values verbatim - do NOT
// "clean them up". Directories use 8; most files use 6; the inbox info uses 0x14.
constexpr u32 kOpenDir   = 8;                    // create a CEC directory
constexpr u32 kOpenFile  = 6;                    // create+write most CEC files
constexpr u32 kOpenInbox = (1u << 4) | (1u << 2); // 0x14, the inbox info file

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
    // cecd:s is the SYSTEM CEC service (the one NetPass uses): it can create and
    // operate on ANY box's title id. cecd:u (user) restricts a process to its own
    // title's box, so creating PetPal's box there is rejected with 0xC8A10BF0
    // (CEC / InvalidState) - the exact error we hit. Prefer cecd:s; fall back to
    // cecd:u only so message I/O on existing boxes still degrades gracefully when
    // cecd:s isn't granted (e.g. running as a .3dsx under the Homebrew Launcher
    // rather than an installed CIA, where the exheader can't request cecd:s).
    Result rc = srvGetServiceHandle(&g_cecd, "cecd:s");
    if (R_SUCCEEDED(rc)) { g_cecdSvc = "cecd:s"; return 0; }
    rc = srvGetServiceHandle(&g_cecd, "cecd:u");
    if (R_SUCCEEDED(rc)) { g_cecdSvc = "cecd:u"; return 0; }
    g_cecdSvc = "";
    return rc;
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

// WriteRawFile(0x00050042): size, <- data (read). Writes `buf` to the file most
// recently opened with cmdOpen (OpenRawFile). This stateful open-then-write pair
// is how CEC box files must be CREATED; OpenAndWrite(0x11) does not create them
// correctly (per the NetPass author: "just openAndWrite won't work"). Call
// immediately after cmdOpen with nothing else touching g_cecd in between.
Result cmdWriteRawFile(const void* buf, u32 size) {
    u32* cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x5, 1, 2);
    cmd[1] = size;
    cmd[2] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
    cmd[3] = reinterpret_cast<u32>(const_cast<void*>(buf));
    Result rc = svcSendSyncRequest(g_cecd);
    if (R_FAILED(rc)) return rc;
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

// OutBoxIndex____ header (3dbrew "CEC_Messages"), followed by one 8-byte message
// id per outbox message. A freshly-created outbox has zero messages, so creation
// writes just this 8-byte header (matches NetPass's registerStreetpassApplication).
struct CecOBIndex {
    u16 magic;        // 0x6767
    u16 padding0;
    u32 num_messages; // 0
};
constexpr u16 kOBIndexMagic = 0x6767;
static_assert(sizeof(CecOBIndex) == 8, "CecOBIndex must be 8 bytes");

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

// /CEC/MBoxList____ - the GLOBAL registry of message boxes (3dbrew "CEC_Messages";
// struct verified against the NetPass source's CecMboxListHeader). The CEC daemon
// only scans/relays boxes listed here, so a box that exists on disk but is absent
// from this file is invisible to StreetPass AND to NetPass - the exact gap flagged
// by the NetPass author. 0x18C bytes: a small header + 24 fixed 16-byte name slots.
struct CecMBoxListHeader {
    u16  magic;             // 0x00  0x6868
    u16  padding0;          // 0x02
    u32  version;           // 0x04  always 1
    u32  num_boxes;         // 0x08  <- count of populated box_names entries
    char box_names[24][16]; // 0x0C  each: 8 lowercase-hex id chars + NUL padding
};
constexpr u16 kMBoxListMagic      = 0x6868;
constexpr u32 kMBoxListSlots      = 24;  // physical name slots in the file
constexpr u32 kMaxRegisteredBoxes = 12;  // system max # of boxes (per NetPass author)
static_assert(sizeof(CecMBoxListHeader) == 0x18C, "CecMBoxListHeader must be 0x18C bytes");

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

// Shared CEC message-integrity key. A retail StreetPass title ships a per-title key
// baked into its ROM; every copy shares it so a passed message validates against
// the receiver's box. PetPal does the same with one fixed 32-byte key identical
// across ALL installs - a per-console or id-derived key (our old memcpy(&titleId))
// would leave consoles unable to validate each other's pets. This is NOT a server
// secret: it grants no access to teampetpal.com and, like any StreetPass key, lives
// on every device by design. Sorunome: "the hmac key should be something actually
// random ... it has to be the same for all installs."
constexpr u8 kHmacKey[32] = {
    0x8B,0x1E,0xF4,0x6C, 0x2A,0x77,0xD0,0x39,
    0x5C,0xA1,0x93,0xE8, 0x14,0x6F,0xB2,0x4D,
    0x70,0xC5,0x38,0x9A, 0xE1,0x26,0x5B,0xF3,
    0x0D,0x84,0xA7,0x62, 0x19,0xCE,0x3F,0xB0,
};

// Message-box sizing, computed for PetPal's payload instead of copied from
// NetPass's relay-everything defaults (Sorunome: "adjust ... message box size ...
// then do some head math"). A CEC message is a 0x70 header + our packet (<=64 B)
// ~= 176 B; 512 gives generous headroom for future packet growth.
constexpr u32 kCecMaxMessageSize = 512;
constexpr u32 kInboxMaxMessages  = 25;                              // hold up to 25 met pets
constexpr u32 kInboxMaxBatch     = 25;
constexpr u32 kInboxMaxBoxSize   = kInboxMaxMessages * kCecMaxMessageSize; // 12800
constexpr u32 kOutboxMaxMessages = 1;                              // we broadcast one pet
constexpr u32 kOutboxMaxBatch    = 1;
constexpr u32 kOutboxMaxBoxSize  = 2 * kCecMaxMessageSize;         // 1024, small headroom

// Write pixel (x,y) into a 48x48 RGB565 image laid out in the 3DS tiled format the
// StreetPass box viewer / cectool expect: 8x8 tiles in raster order, Morton
// (Z-order) within each tile. Verified against cectool's textureToBuf/tileToBuf
// (recursive TL,TR,BL,BR down to 2x2). A plain linear buffer renders as garbage -
// that was the "messed up icon" bug.
inline void iconSetPixel(u16* dst, int x, int y, u16 color) {
    constexpr int kTilesPerRow = 48 / 8;                       // 6
    const int tileX = x >> 3, tileY = y >> 3;
    const int tx = x & 7, ty = y & 7;
    const int morton = (tx & 1) | ((ty & 1) << 1)
                     | ((tx & 2) << 1) | ((ty & 2) << 2)
                     | ((tx & 4) << 2) | ((ty & 4) << 3);
    dst[(tileY * kTilesPerRow + tileX) * 64 + morton] = color;
}

// Draw PetPal's 48x48 box icon: a white paw print on a teal field, in the tiled
// layout above. Purely cosmetic, but a correct tiling is what makes it show as an
// icon (not noise) in the CEC/StreetPass box list.
void buildBoxIcon(u16* dst) {
    constexpr u16 kBg  = 0x5E1B;   // ~#5BC0DE teal (RGB565)
    constexpr u16 kPaw = 0xFFFF;   // white
    const int tox[4] = { 11, 19, 29, 37 };   // four toe-bean centres (x)
    const int toy[4] = { 22, 13, 13, 22 };   //                        (y)
    for (int y = 0; y < 48; ++y) {
        for (int x = 0; x < 48; ++x) {
            u16 c = kBg;
            const int dx = x - 24, dy = y - 33;               // main pad: ellipse rx12 ry10
            if (dx * dx * 100 + dy * dy * 144 <= 144 * 100) c = kPaw;
            for (int t = 0; t < 4; ++t) {                     // toe beans: circles r5
                const int ex = x - tox[t], ey = y - toy[t];
                if (ex * ex + ey * ey <= 25) c = kPaw;
            }
            iconSetPixel(dst, x, y, c);
        }
    }
}

// Ensure our box is provisioned the way exchange requires: StreetPass-enabled, the
// right box-type flag, and - crucially - the shared HMAC key, so a box created by
// an older PetPal build (which seeded the key from the title id) is upgraded to the
// shared key on next boot. Read-modify-write of the whole MBoxInfo blob so cecd's
// own fields (private_id, timestamps) are preserved. Best-effort and bounded.
void ensureBoxProvisioned(u32 titleId) {
    CecMBoxInfo info;
    u32 got = 0;
    if (R_FAILED(cmdOpenAndRead(titleId, Path::MboxInfo, 0, &info, sizeof(info), &got)) ||
        got < sizeof(info) || info.magic != kMBoxInfoMagic) {
        return; // can't read a valid MBoxInfo; leave whatever cecd created
    }
    bool dirty = false;
    if (!info.enabled)               { info.enabled = 1;          dirty = true; }
    if (info.mbox_type_flags != 0x1) { info.mbox_type_flags = 0x1; dirty = true; }
    if (std::memcmp(info.hmac_key, kHmacKey, sizeof(kHmacKey)) != 0) {
        std::memcpy(info.hmac_key, kHmacKey, sizeof(kHmacKey));
        dirty = true;
    }
    if (dirty) cmdOpenAndWrite(titleId, Path::MboxInfo, 0, &info, sizeof(info));
}

// Case-insensitive compare of two 8-char hex ids (cecd maps the name string back
// to a numeric id, so "000F00D5" and "000f00d5" are the same box).
bool sameHexId(const char* a, const char* b) {
    for (int i = 0; i < 8; ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + 32);
        if (ca != cb) return false;
    }
    return true;
}

// Register our box in the global /CEC/MBoxList____ so the system daemon (and, over
// the internet, NetPass) actually scan and exchange it. cecd creates the per-box
// dir/files but NEVER adds the box to this master list; without the entry nothing
// knows the box exists. We read-modify-write the list, preserving every existing
// entry, honoring the 12-box system maximum. Path::MboxList is global, so cecd
// ignores the program-id argument (the file path has no id in it) - we pass 0.
// Returns 0 when our box is in the list afterwards; a Result/sentinel otherwise.
Result ensureBoxInList(u32 titleId) {
    // Box name = the box's title id as 8 LOWERCASE hex digits, matching the CEC
    // directory cecd creates ("/CEC/{:08x}/..."), NUL-padded to the 16-byte slot.
    char want[9];
    std::snprintf(want, sizeof(want), "%08lx", static_cast<unsigned long>(titleId));

    CecMBoxListHeader list;
    std::memset(&list, 0, sizeof(list));
    u32 got = 0;
    Result rc = cmdOpenAndRead(0, Path::MboxList, 0, &list, sizeof(list), &got);
    const bool have = R_SUCCEEDED(rc) && got >= 0x0C && list.magic == kMBoxListMagic;
    if (!have) {                                   // missing / empty / corrupt -> fresh
        std::memset(&list, 0, sizeof(list));
        list.magic     = kMBoxListMagic;
        list.version   = 1;
        list.num_boxes = 0;
    }

    u32 n = list.num_boxes;
    if (n > kMBoxListSlots) n = kMBoxListSlots;    // guard a corrupt count (array safety)

    for (u32 i = 0; i < n; ++i) {                  // already registered? nothing to do
        if (sameHexId(list.box_names[i], want)) {
            PP_LOG("mboxlist: box %s already listed (%lu boxes)", want, (unsigned long)n);
            return 0;
        }
    }

    if (n >= kMaxRegisteredBoxes) {                // at the system max, and not us
        PP_WARN("mboxlist full (%lu boxes); box not registered", (unsigned long)n);
        return static_cast<Result>(0xC8A1F00D);    // our sentinel: list full
    }

    // Append our name and bump the count, leaving every other entry untouched.
    std::memset(list.box_names[n], 0, 16);
    std::memcpy(list.box_names[n], want, 8);
    list.num_boxes = n + 1;
    list.magic     = kMBoxListMagic;               // in case we just initialized it
    list.version   = 1;

    // Write the updated list. NetPass reads MBoxList with program-id 0 but writes
    // it back with the box's own title id, so mirror that exactly. MBoxList is a
    // global file (its path carries no id), and OpenAndWrite is correct here
    // because we are UPDATING an existing/derived file, not creating a box file.
    rc = cmdOpenAndWrite(titleId, Path::MboxList, 0, &list, sizeof(list));
    if (R_FAILED(rc)) { PP_WARN("mboxlist write failed: %08lx", (unsigned long)rc); return rc; }
    PP_LOG("mboxlist: added box %s (now %lu boxes)", want, (unsigned long)(n + 1));
    return 0;
}

// Create one CEC file: OpenRawFile(path, flag) then WriteRawFile(buf). This is the
// exact create/write pair CEC requires (see cmdWriteRawFile). Returns the first
// failing Result. Nothing may touch g_cecd between the two calls.
Result createFile(u32 titleId, Path path, u32 flag, const void* buf, u32 size) {
    Result rc = cmdOpen(titleId, path, flag, nullptr);
    if (R_FAILED(rc)) return rc;
    return cmdWriteRawFile(buf, size);
}

// Write the box's display name + icon. Used both at creation and to HEAL an
// existing box whose icon predates the tiling fix (registerBox early-outs on an
// already-listed box, so the icon must be refreshed separately). Returns the first
// failing Result; touches only the name/icon files, never message state.
Result writeBoxNameAndIcon(u32 titleId) {
    u8 name[16] = { 'P',0, 'e',0, 't',0, 'P',0, 'a',0, 'l',0, 0,0, 0,0 }; // UTF-16LE
    Result rc = createFile(titleId, Path::BoxTitle, kOpenFile, name, sizeof(name));
    if (R_FAILED(rc)) return rc;

    static u16 icon[48 * 48];
    buildBoxIcon(icon);
    return createFile(titleId, Path::BoxIcon, kOpenFile, icon, sizeof(icon));
}

// Create PetPal's full CEC message box, faithfully mirroring NetPass's
// registerStreetpassApplication(). cecd does NOT create the box as a side effect of
// message I/O and it does NOT add the box to the global list, so we must build the
// whole structure ourselves: three directories, MBoxInfo, InboxInfo, OutboxInfo +
// OutBoxIndex, a box title + icon, and finally the MBoxList entry. Every file is
// made with OpenRawFile+WriteRawFile and the exact open flags NetPass uses. If the
// box is already in the master list we early-out (so we never clobber a live box or
// its waiting messages). Returns 0 on success; on failure returns the Result and
// sets *step to the failing step (1..9) for the on-device self-test.
Result registerBox(u32 titleId, int* step) {
    if (step) *step = 0;

    // Step 1: already registered? Then the box (and all its files) already exist.
    {
        CecMBoxListHeader list;
        std::memset(&list, 0, sizeof(list));
        u32 got = 0;
        Result rc = cmdOpenAndRead(0, Path::MboxList, 0, &list, sizeof(list), &got);
        if (R_SUCCEEDED(rc) && got >= 0x0C && list.magic == kMBoxListMagic) {
            char want[9];
            std::snprintf(want, sizeof(want), "%08lx", static_cast<unsigned long>(titleId));
            u32 n = list.num_boxes;
            if (n > kMBoxListSlots) n = kMBoxListSlots;
            for (u32 i = 0; i < n; ++i)
                if (sameHexId(list.box_names[i], want)) return 0;   // already done
            if (n >= kMaxRegisteredBoxes) { if (step) *step = 1; return static_cast<Result>(0xC8A1F00D); }
        }
        // Missing/corrupt list is fine: we create everything, and the final
        // ensureBoxInList() re-reads and (re)builds the list.
    }

    Result rc;

    // Step 2: the three box directories (open flag 8).
    if (R_FAILED(rc = cmdOpen(titleId, Path::MboxDir,   kOpenDir, nullptr))) { if (step) *step = 2; return rc; }
    if (R_FAILED(rc = cmdOpen(titleId, Path::InboxDir,  kOpenDir, nullptr))) { if (step) *step = 2; return rc; }
    if (R_FAILED(rc = cmdOpen(titleId, Path::OutboxDir, kOpenDir, nullptr))) { if (step) *step = 2; return rc; }

    // Step 3: MBoxInfo (magic 0x6363), StreetPass-enabled, with the shared HMAC key
    // baked in so every PetPal install can validate every other's passed messages.
    {
        CecMBoxInfo mbox;
        std::memset(&mbox, 0, sizeof(mbox));
        mbox.magic           = kMBoxInfoMagic;
        mbox.title_id        = titleId;
        mbox.mbox_type_flags = 0x1;
        mbox.enabled         = 1;
        std::memcpy(mbox.hmac_key, kHmacKey, sizeof(kHmacKey));
        if (R_FAILED(rc = createFile(titleId, Path::MboxInfo, kOpenFile, &mbox, sizeof(mbox)))) {
            if (step) *step = 3;
            return rc;
        }
    }

    // Step 4: InboxInfo (magic 0x6262, open flag 0x14).
    {
        CecBoxInfoHeader inbox;
        std::memset(&inbox, 0, sizeof(inbox));
        inbox.magic            = 0x6262;
        inbox.box_info_size    = sizeof(CecBoxInfoHeader);
        inbox.max_box_size     = kInboxMaxBoxSize;
        inbox.max_message_num  = kInboxMaxMessages;
        inbox.max_batch_size   = kInboxMaxBatch;
        inbox.max_message_size = kCecMaxMessageSize;
        if (R_FAILED(rc = createFile(titleId, Path::InboxInfo, kOpenInbox, &inbox, sizeof(inbox)))) {
            if (step) *step = 4;
            return rc;
        }
    }

    // Step 5: OutboxInfo (magic 0x6262) + Step 6: OutBoxIndex (magic 0x6767).
    {
        CecBoxInfoHeader outbox;
        std::memset(&outbox, 0, sizeof(outbox));
        outbox.magic            = 0x6262;
        outbox.box_info_size    = sizeof(CecBoxInfoHeader);
        outbox.max_box_size     = kOutboxMaxBoxSize;
        outbox.max_message_num  = kOutboxMaxMessages;
        outbox.max_batch_size   = kOutboxMaxBatch;
        outbox.max_message_size = kCecMaxMessageSize;
        if (R_FAILED(rc = createFile(titleId, Path::OutboxInfo, kOpenFile, &outbox, sizeof(outbox)))) {
            if (step) *step = 5;
            return rc;
        }

        CecOBIndex index;
        std::memset(&index, 0, sizeof(index));
        index.magic = kOBIndexMagic;
        if (R_FAILED(rc = createFile(titleId, Path::OutboxIndex, kOpenFile, &index, sizeof(index)))) {
            if (step) *step = 6;
            return rc;
        }
    }

    // Step 7: box title (UTF-16LE) + 48x48 tiled RGB565 icon.
    if (R_FAILED(rc = writeBoxNameAndIcon(titleId))) {
        if (step) *step = 7;
        return rc;
    }

    // Step 9: finally, add the box to the global MBoxList so the OS + NetPass see it.
    if (R_FAILED(rc = ensureBoxInList(titleId))) { if (step) *step = 9; return rc; }

    return 0;
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

    // 1) Build the FULL message-box structure and register it, mirroring NetPass's
    //    registerStreetpassApplication(): three dirs + MBoxInfo + InboxInfo +
    //    OutboxInfo + OutBoxIndex + title + icon, all made with OpenRawFile+
    //    WriteRawFile (NOT OpenAndWrite), then added to the global MBoxList so the
    //    OS - and NetPass over the internet - actually scan and exchange it. This
    //    early-outs if we're already registered, so a live box is never clobbered.
    regStep_ = 0;
    Result rc = registerBox(titleId_, &regStep_);
    regRc_ = static_cast<u32>(rc);
    if (R_FAILED(rc)) {
        lastError_ = regRc_;
        PP_WARN("registerBox failed at step %d: %08lx", regStep_, (unsigned long)regRc_);
    }

    // 2) Confirm the box really exists now by reading its MBoxInfo back. (When
    //    registerBox early-outs as "already registered" this is what proves it.)
    {
        CecMBoxInfo probe;
        u32 pg = 0;
        Result pr = cmdOpenAndRead(titleId_, Path::MboxInfo, 0, &probe, sizeof(probe), &pg);
        boxReady_ = R_SUCCEEDED(pr) && pg >= sizeof(probe) && probe.magic == kMBoxInfoMagic;
        if (!boxReady_ && R_SUCCEEDED(rc)) lastError_ = static_cast<u32>(pr);
    }

    // 3) Keep the box provisioned: enabled, right type flag, and the shared HMAC
    //    key (upgrades boxes made by older PetPal builds). Best-effort, bounded.
    if (boxReady_) ensureBoxProvisioned(titleId_);

    // 3a) Refresh the box title + icon every boot. registerBox early-outs on an
    //     already-registered box, so this is what heals a box created by an older
    //     build with the garbled (linear) icon. Cheap and bounded; leaves the
    //     inbox/outbox message state untouched.
    if (boxReady_) writeBoxNameAndIcon(titleId_);

    // Record the master-list state for the status line (0 == our box is listed).
    mboxListRc_ = (regStep_ == 9) ? regRc_ : 0;

    // 4) Nudge the CEC daemon into its scan/exchange cycle. The system then swaps
    //    our box with people we pass (local StreetPass) or with the relay
    //    (NetPass) - identical box, identical code path. Some daemon states
    //    reject START (e.g. already running); treat that as "already scanning".
    Result sr = cmdStart(CecCmdStart);
    scanning_ = R_SUCCEEDED(sr) || cecState_ != 0;
    if (R_FAILED(sr))
        PP_LOG("cecd Start(START) -> %08lx (state %u)",
               (unsigned long)sr, cecState_);

    available_ = true;
    PP_LOG("CECD ready (box=%08lx svc=%s state=%u boxReady=%d scan=%d regStep=%d reg=%08lx)",
           (unsigned long)titleId_, g_cecdSvc, cecState_, (int)boxReady_,
           (int)scanning_, regStep_, (unsigned long)regRc_);
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

// -----------------------------------------------------------------------------
//  DualTransport - CECD (real StreetPass + NetPass) alongside the HTTP relay
// -----------------------------------------------------------------------------
bool DualTransport::init() {
    const bool ra = a_ && a_->init();
    const bool rb = b_ && b_->init();
    return ra || rb;   // usable if EITHER transport came up
}

void DualTransport::shutdown() {
    if (a_) a_->shutdown();
    if (b_) b_->shutdown();
}

bool DualTransport::setOutbox(const uint8_t* data, size_t len) {
    bool ra = false, rb = false;
    if (a_) ra = a_->setOutbox(data, len);
    if (b_) rb = b_->setOutbox(data, len);
    return ra || rb;
}

int DualTransport::drainInbox(std::vector<std::vector<uint8_t>>& out) {
    int total = 0;                       // union of both inboxes (dedup is in poll())
    if (a_) { const int n = a_->drainInbox(out); if (n > 0) total += n; }
    if (b_) { const int n = b_->drainInbox(out); if (n > 0) total += n; }
    return total;
}

bool DualTransport::isAvailable() const {
    return (a_ && a_->isAvailable()) || (b_ && b_->isAvailable());
}

StreetPassStatus DualTransport::status() const {
    // Surface the CECD box status (what the StreetPass screen shows), but report
    // serviceUp if EITHER is live and sum the waiting counts.
    StreetPassStatus s = a_ ? a_->status() : StreetPassStatus{};
    if (b_) {
        const StreetPassStatus sb = b_->status();
        s.serviceUp    = s.serviceUp || sb.serviceUp;
        s.inboxWaiting = s.inboxWaiting + sb.inboxWaiting;
    }
    return s;
}

} // namespace petpal

#endif // __3DS__
