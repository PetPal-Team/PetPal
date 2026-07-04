# PetPal — Progress & Handoff

A StreetPass-style **virtual pet homebrew game for the Nintendo 3DS**, built with
devkitPro / libctru / citro2d in C++17, plus a companion Cloudflare site
(teampetpal.com) that powers a code-redemption feature.

This file is the single source of truth for **where the project is, what works,
what just broke, and exactly what to do next.** Read §0 first.

---

## 0. Current state at a glance (read me first)

- ✅ **The whole game builds and runs.** `PetPal.3dsx` and `PetPal.cia` both
  compile clean; the CIA **boots on real hardware** (past the earlier boot hang).
- ✅ **All core systems are implemented and host-tested** (pet, stats, evolution,
  inventory, friends, journal, adventures, achievements, versioned save).
- ✅ **UI is fully procedural** — 7 hand-shaped pets (2 custom colors each,
  Idle/Happy/Sad anims), procedural per-location backgrounds, Nintendo Switch
  button glyphs, StreetPass-Plaza-style menus.
- ✅ **Code-redemption feature is built end-to-end** — server (Cloudflare Pages
  Function), 3DS client, background thread + loading spinner, and a public
  `codes.html` page on the site.
- 🟢 **The `httpc` blocker is fixed in code.** The redeem transport has been
  **migrated from `httpc` to `3ds-curl`** (libcurl + mbedTLS). The app builds and
  links clean (`PetPal.3dsx` + `PetPal.cia`). This replaces the old path where
  httpc completed the TLS handshake to Cloudflare but **could not read the HTTP
  response** (`No reply (D8A0A003)` = HTTP module, *invalid state*). See §5/§7.
- 🟡 **Pending: on-device confirmation.** The migration compiles here but has not
  been run on a console yet (no hardware on this machine). Redeploy the site and
  flash the new CIA, then redeem a code to confirm end-to-end.

**If you are picking this up:** the code migration is done (§7). The next step is
purely verification — deploy `redeem.js` and test a code on hardware (§6).

### Important testing constraint
The dev machine has devkitPro (**can build**) but **no 3DS hardware**. All
on-device behavior (networking, CECD/StreetPass, rendering) is verified by the
**user on their console** and reported back. Only the device-independent model
layer is covered by automated host tests. Do not claim on-device features "work"
— say "builds clean; needs on-device confirmation."

---

## 1. Purpose

Recreate the StreetPass-era magic (StreetPass Mii Plaza / Tomodachi Life feel) as
a virtual pet. You raise one pet; every player you pass — locally via StreetPass
or worldwide via NetPass — becomes a friend that feeds your pet XP, gifts, and
progress toward evolution. Full spec is in [PROMPT.md](PROMPT.md).

- **Platform:** Old/New 3DS & 2DS (homebrew, CIA + .3dsx)
- **Toolchain:** devkitPro (devkitARM), libctru, citro2d/citro3d, C++17
- **Companion site:** teampetpal.com (Cloudflare, static site + Pages Function)

---

## 2. Architecture (built)

The model layer is **device-independent** (no `<3ds.h>`) so it's host-testable; UI
and transports are device-bound behind `#ifdef __3DS__`.

| Area | Key files | Status |
|------|-----------|--------|
| Core types/tuning | `include/core/Types.h`, `source/core/Names.cpp` | ✅ |
| Orchestrator + main loop | `core/Game.*`, `source/main.cpp` | ✅ |
| Pet (stats, XP, evolution, transform) | `pets/Pet.*` | ✅ |
| Inventory / items | `items/Inventory.*` | ✅ |
| Friends (dedupe, levels) | `friends/Friend*.*` | ✅ |
| Journal (auto-written) | `journal/Journal.*` | ✅ |
| Adventures (timed, loot) | `adventure/Adventure.*` | ✅ |
| Achievements | `achievements/Achievements.*` | ✅ |
| Save (versioned, CRC, atomic + backup) | `save/SaveManager.*`, `save/SaveData.h` | ✅ (v2) |
| StreetPass/NetPass packet + manager | `netpass/NetPassManager.*`, `PetPalPacket.h` | ✅ |
| Pet exchange transport (internet relay) | `netpass/HttpPassTransport.cpp` + `functions/api/pass.js` + `tools/make_pass.py` | ✅ VERIFIED on hardware (seed → download → friend) |
| CECD/StreetPass (abandoned) | `netpass/CecdTransport.cpp` | ⛔ dead end — homebrew can't register a CEC box (kept for reference, unused) |
| Redeem codes (client, now on 3ds-curl) | `netpass/RedeemManager.*`, `netpass/RedeemTask.*` | 🟡 migrated to curl; needs on-device test (§5/§7) |
| UI (screens, widgets, anim, icons) | `ui/*`, `ui/screens/*` | ✅ |
| Audio (ndsp): streamed BGM + SFX + synth blips | `audio/Audio.*`, `romfs/audio/*.wav` | ✅ |
| Shop (buy items with coins) | `ui/screens/ShopScreen.cpp` | ✅ |
| Screen transitions + celebration confetti | `ui/UIManager.cpp` | ✅ |
| Procedural pet renderer | `ui/PetRenderer.cpp` | ✅ |
| Docs | `docs/*.md`, `README.md` | ✅ |
| Host tests (model layer) | `tests/*` | ✅ |

Design docs: [ARCHITECTURE](docs/ARCHITECTURE.md), [SAVE_FORMAT](docs/SAVE_FORMAT.md),
[NETPASS_INTEGRATION](docs/NETPASS_INTEGRATION.md), [UI_SYSTEM](docs/UI_SYSTEM.md),
[ROADMAP](docs/ROADMAP.md).

---

## 3. Timeline — what was built & what worked

1. **Full scaffold** — all systems above, Makefile, docs, host tests. Iterated
   through first-compile errors (missing includes, an enum/type name clash in
   PetScreen, `C2D_DrawEllipse` vs `…Solid`, Makefile `.PHONY` for `$(BUILD)`,
   header self-containment). **Result: `PetPal.3dsx` builds clean.** ✅

2. **CIA packaging** — fetched `makerom` + `bannertool` (not shipped with
   devkitPro), built `banner.bnr` from `banner.png` + `audio.wav`, generated a
   48×48 SMDH icon, wrote `petpal.rsf`, added a `make cia` target. ✅

3. **CIA boot hang → fixed** — root cause was the RSF: **`AffinityMask: 0`** (no
   CPU core assigned → main thread never scheduled → infinite homebrew-boot
   loop). Rebuilt the RSF from ftpd's proven template: `AffinityMask: 1`,
   `SystemMode: 64MB`, `Backdoor` SVC, DSP `IORegisterMapping`, N3DS settings,
   `FileSystemAccess: DirectSdmc(+Write)`. **CIA now boots on hardware.** ✅

4. **Real StreetPass groundwork** — discovered **libctru has no CECD bindings**,
   so wrote a from-scratch `cecd:u` IPC client (`CecdTransport.cpp`) using the
   exact command IDs/structs from 3dbrew + Citra. ABI-correct and compiles; box
   exchange is **unverified on hardware.** 🔨

5. **UI overhaul to "Nintendo level"** → then **fully procedural** — first curated
   an icon atlas; then, per request, dropped all custom sprite art in favor of
   low-detail **procedural** pets: 7 species (Fox/Cat/Bunny/Dragon/Slime/Robot/
   Axolotl), **two customizable colors**, **Idle/Happy/Sad** animations
   (`PetRenderer.cpp`). Backgrounds are simple procedural scenes per location.
   Added **Nintendo Switch button glyphs** (A/B/X/Y/L/R). Deleted all now-unused
   assets (old icon/button packs, axolotl sprites, bg images). ✅

6. **Dropped the 3rd (accent) color** from customization. ✅

7. **Code redemption system** (the current focus)
   - **Server:** Cloudflare Pages Function `functions/api/redeem.js`. Codes:
     `Bonzi` (24h BonziBuddy transform), `Apple4Life` (1000 apples, one-time per
     pet via KV), `Belzer` (Dog Hat). Returns `key=value` text/plain (trivial for
     the 3DS to parse). Accepts **both GET (`?code=&pet=`) and POST**. **No
     secrets live in the 3DS binary** — all reward logic is server-side (hard
     product constraint from the user).
   - **3DS:** `RedeemManager` (network) + `RedeemTask` (background thread) +
     Settings → **Enter code** (swkbd) + a **loading spinner** so the UI never
     freezes during the request. New `DogHat` accessory, `Bonzi` transform, save
     bumped to **v2** (backward compatible: v2 fields only read when
     `version >= 2`).
   - **Site:** `codes.html` page listing active codes (tap-to-copy chips), linked
     in nav/footer of every page.

**What definitively works:** building both targets, CIA boot, all model logic
(host tests green), procedural rendering, the full UI, the save format, and the
**server side of redeem** (a PC `GET`/`POST` to the live endpoint returns `200`
with the correct reward payload).

---

## 4. Build & deploy reference

```sh
# 3DS app (from D:\petpal, in the devkitPro / MSYS2 shell)
make            # -> PetPal.3dsx
make cia        # -> PetPal.cia   (needs tools/cia/makerom + bannertool)
make clean

# Host model tests
make -C tests run

# Website (static + Pages Function) — from the site folder:
#   SITE_exclude/site/teampetpal-site/teampetpal
wrangler pages deploy .        # deploys static assets AND functions/
```

- devkitPro at `C:\devkitPro`; `makerom` / `bannertool` / `tex3ds` used for
  packaging/atlases (`tex3ds --atlas`, not short `-a` which was buggy in v2.3.0).
- **Deploy gotcha:** the site MUST be deployed with **`wrangler pages deploy`**.
  A plain Worker static-asset upload **silently ignores `functions/`**, so
  `/api/redeem` 404s. This already bit us once (§5).
- Local site preview: `.claude/launch.json` runs a python static server — note it
  **cannot run Pages Functions**, only the static HTML/CSS/JS.

---

## 5. The redeem/networking saga — the obstacle we just hit

**Symptom:** redeeming a code on the 3DS fails, even though the server is fine.

The debug chain (each row is a thing we tried and what we learned):

| Step | Finding |
|------|---------|
| "Pages functions not supported" warning in dashboard | Site had been uploaded via a **Worker static uploader**, which ignores `functions/`. Fixed by redeploying with `wrangler pages deploy`. |
| 3DS "Server is busy" | Server is actually fine — a PC request returns `200` + correct reward payload. |
| My own automated fetch got `403` | Cloudflare **bot protection** blocks non-browser clients by default. |
| Disabled Bot Fight Mode, Browser Integrity Check, lowered Security Level, set Min TLS 1.0 | **No change** to the 3DS behavior. |
| 3DS "Server error (3276144640)" → then **`No reply (D8A0A003)`** | `D8A0A003` decodes to **level Permanent / summary InvalidState / module HTTP (0x28) / desc 3**. Crucial insight: the 3DS gets **past connect**, so **TLS to Cloudflare succeeds**, but httpc **cannot read the response**. |
| Tried plain HTTP (no TLS) | **Cloudflare Pages force-upgrades HTTP→HTTPS** and it can't be disabled at the zone level. HTTP is a dead end here. |
| Back to HTTPS + `HTTPC_KEEPALIVE_ENABLED` | Same "No reply". |
| **Switched request from POST → GET** (`?code=&pet=`), server now accepts GET | Removes the POST shared-memory-buffer path that produced the invalid-state on the POST attempt. GET returns a clean `200` **from a PC**. **On-device result: still pending** user redeploy + flash + test. |

**Where it stands:** `RedeemManager::redeem()` currently does
`GET https://teampetpal.com/api/redeem?code=…&pet=<hex>` via `httpc` with
`SSLCOPT_DisableVerify` (safe — no secrets are sent), keep-alive, and a
`PetPal-3DS` user agent. Response is parsed as `key=value` lines by
`field()` / `parseInto()`.

**Root-cause hypothesis (why we're changing transports):** libctru **`httpc`**
rides on the console's ancient built-in **`ssl:C`** module and has fragile
response handling. TLS negotiates, but reading Cloudflare's modern HTTPS response
lands httpc in an invalid state. This is not a server problem and not (as far as
we can tell) fixable by toggling Cloudflare settings — it's the 3DS HTTP client.
Hence the migration to `3ds-curl` (§7).

---

## 6. Next specific steps (in priority order)

1. **Redeploy the site + test redeem on hardware.** `redeem.js` was updated
   (health check, `v=` version stamp, CORS) — deploy with `wrangler pages deploy`,
   flash the freshly built `PetPal.cia`, and redeem a code. Sanity-check the
   deploy first by opening `https://teampetpal.com/api/redeem` in a browser: it
   should return `status=ready` and `v=curl-1`.
   - If curl still errors, the message now reads `Net error: <curl reason>` (e.g.
     "Couldn't resolve host", "Timeout") — that string tells you what to fix.
2. **Configure the KV binding `PETPAL_KV`** on the Pages project — otherwise
   `Apple4Life` is not actually limited to once-per-pet (the code no-ops the
   guard when KV is unbound). See `functions/README.md`.
4. **Pet exchange = internet relay** — ✅ **DONE & verified on hardware.**
   CECD/StreetPass was a **dead end** for homebrew (can't register a CEC box).
   Replaced with PetPal's own relay (`HttpPassTransport` + `functions/api/pass.js`):
   a background worker uploads our pet packet to `teampetpal.com/api/pass` and
   downloads others; the friends/journal pipeline is unchanged. `PETPAL_KV` is
   bound and a seeded pet was confirmed to arrive and become a friend on-device.
   Seed/generate packets with `tools/make_pass.py` (note the non-zlib CRC gotcha
   in [NETPASS_INTEGRATION](docs/NETPASS_INTEGRATION.md)). `CecdTransport` stays in
   the tree (unused) for reference.
5. **Audio** — ✅ DONE. `source/audio/Audio.cpp` (ndsp): streamed looping BGM
   (`romfs:/audio/main.wav`, chunked on a worker thread), WAV SFX (eating/bonzi),
   and synthesized chip blips (navigate/select/back/coin/level-up/error). Wired
   into `UIManager::celebrate()`, nav/menu input, feeding, redeem, and the
   Settings volume sliders (live). WAVs live in `romfs/audio/` (copied from
   `assets/`); adds ~30 MB to the build.
6. **Polish** — ✅ mostly DONE: confetti particle bursts on celebrations, screen
   **slide transitions** (`C2D_ViewTranslate` in `UIManager::tick`), and a **Shop**
   screen (`ui/screens/ShopScreen.cpp`, `ScreenId::Shop`, 8-tile 4×2 main menu).
   Still open: render remaining accessories/styles, a food-picker feeding UI, an
   evolution cutscene, and nav sounds on the last few list screens.
7. **D-pad / START prompts** — still rendered as text (the Switch glyph pack has
   no art for those).

---

## 7. Migration: httpc → **3ds-curl** (DONE in code — builds clean)

> **Status:** implemented and building. `3ds-curl-8.4.0` + `3ds-mbedtls-2.28.8`
> installed via pacman; `Makefile` links them; `Game::init/shutdown` own the
> `soc`+`curl_global` lifecycle; `RedeemManager::redeem()` uses `curl_easy_*`.
> The subsections below are the reference for what was changed / how to redo it.


Goal: replace libctru's `httpc` (outdated built-in `ssl:C`, fragile response
handling — the source of `D8A0A003`) with **`3ds-curl`**, a devkitPro port of
**libcurl** backed by **mbedTLS**. This gives modern TLS 1.2/1.3, proper redirect
handling, clean requests, and its own CA handling — exactly what redeem (and any
future networking, e.g. NetPass-over-internet) needs.

### 7.1 Install the portlibs
```sh
dkp-pacman -S 3ds-curl        # pulls 3ds-mbedtls (zlib was already installed)
```
**Gotcha on this machine:** `C:\devkitPro\tools\bin\dkp-pacman.exe` was a broken
9-byte stub (contents: "Not Found"), so `dkp-pacman` didn't work. Worked around it
by calling the real msys2 pacman directly (the `dkp-libs` repo is already in its
`pacman.conf`):
```powershell
& "C:\devkitPro\msys2\usr\bin\bash.exe" -lc "pacman -Sy --needed --noconfirm 3ds-curl"
```
Result: installed `3ds-curl-8.4.0` + `3ds-mbedtls-2.28.8` into
`C:\devkitPro\portlibs\3ds\{include,lib}`.

### 7.2 Makefile — link order matters (curl before ctru)
```make
LIBS := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lcitro2d -lcitro3d -lctru -lm
```
`LIBDIRS := $(CTRULIB) $(PORTLIBS)` (portlibs is normally already on the include
path via `3ds_rules`; if not, add `$(DEVKITPRO)/portlibs/3ds`).

### 7.3 Sockets — curl needs `soc` initialized once (not per request)
libcurl uses BSD sockets, so `socInit` must run before any curl call and `socExit`
at shutdown. Do this in `Game::init` / `Game::shutdown`:
```c
#include <malloc.h>
#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000
static u32* s_socBuf = NULL;
// init:
s_socBuf = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
socInit(s_socBuf, SOC_BUFFERSIZE);
curl_global_init(CURL_GLOBAL_DEFAULT);
// shutdown:
curl_global_cleanup();
socExit();
// free(s_socBuf) after socExit
```
`soc:U` service access is **already granted** in `petpal.rsf`.

### 7.4 Rewrite ONLY `RedeemManager::redeem()` (threading/UI/parse unchanged)
```c
#include <curl/curl.h>

static size_t writeCb(char* ptr, size_t sz, size_t n, void* ud) {
    ((std::string*)ud)->append(ptr, sz * n);
    return sz * n;
}

RedeemResult RedeemManager::redeem(const char* code, uint64_t petId) {
    RedeemResult r;
    if (!code || !code[0]) { r.message = "Enter a code first."; return r; }

    // reuse the existing urlEncode() helper for `code`; pet as hex
    std::string url = "https://teampetpal.com/api/redeem?code=";
    url += urlEncode(code);
    char pethex[20];
    std::snprintf(pethex, sizeof(pethex), "%016llX", (unsigned long long)petId);
    url += "&pet="; url += pethex;

    CURL* c = curl_easy_init();
    if (!c) { r.message = "curl init failed"; return r; }

    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);   // handles CF http->https
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "PetPal-3DS");
    // No secrets are sent, so skipping verification is acceptable and avoids
    // shipping a CA bundle. To verify instead: put cacert.pem in romfs and use
    // CURLOPT_CAINFO="romfs:/cacert.pem".
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) {
        char m[64]; std::snprintf(m, sizeof(m), "Net error (%d)", (int)res);
        r.message = m; return r;
    }
    if (http != 200) {
        char m[32]; std::snprintf(m, sizeof(m), "HTTP %ld", http);
        r.message = m; return r;
    }
    parseInto(body, r);   // existing key=value parser stays as-is
    return r;
}
```

### 7.5 Notes / cautions for whoever implements this
- Keep the `key=value` wire format and `parseInto()` — **only the transport
  changes.** The host-build `#else` stub ("Online codes require a 3DS.") stays.
- `RedeemTask` (background thread + `std::atomic` state) and the spinner are
  unchanged; they just call the new curl-based `redeem()`.
- `curl_easy_perform()` is **blocking** — that's fine because it runs on the
  `RedeemTask` worker thread, not the UI thread. Keep it there.
- With real TLS (mbedTLS) + `CURLOPT_FOLLOWLOCATION`, the Cloudflare HTTP→HTTPS
  upgrade and modern TLS both "just work", eliminating the `D8A0A003` httpc quirk.
- After curl is proven on hardware, delete the `httpc` code path from
  `RedeemManager.cpp` and drop the `httpcInit`/context boilerplate.
- **Don't forget the `socInit` step (§7.3)** — the most common failure mode for a
  first curl port on 3DS is forgetting to init sockets, which surfaces as an early
  curl connect error rather than a clear message.

---

## 8. Cross-AI / cross-session notes

Things that aren't obvious from the code and will save the next agent time:

- **No 3DS hardware on this machine.** Verify by building; the user tests
  on-device and reports exact on-screen error strings. Treat those strings as
  ground truth (e.g. `D8A0A003` came straight off the console).
- **Hard product constraint:** the 3DS binary must contain **no site secrets** —
  all redeem logic stays server-side. Don't move reward decisions into the client
  or embed API keys.
- **The server is not the problem** in the redeem saga; the 3DS HTTP client is.
  Don't re-debug Cloudflare settings — that path is exhausted (Bot Fight Mode,
  BIC, Security Level, Min TLS all tried).
- **Deploy the site with `wrangler pages deploy`**, never a Worker static upload,
  or `functions/` (the whole API) silently disappears.
- **Save format is versioned (`kSaveVersion = 2`).** When adding fields, append in
  `encodeBody` and read them in `decodeBody` **only when `version >= N`** — this
  is how v2 stays backward-compatible with v1 saves. Follow that pattern.
- **RSF is derived from ftpd.** `AffinityMask: 1` is load-bearing (0 = boot hang).
  Service access already granted: `cecd:u`, `http:C`, `ssl:C`, `soc:U`. UniqueId
  `0xf00d5` (changed from the template default `0xff3ff` to avoid a TitleID collision).
- **CECD is hand-rolled IPC** (`CecdTransport.cpp`) because libctru has none —
  command IDs/structs came from 3dbrew + Citra. It compiles but is the single
  biggest untested-on-hardware risk.
- **Rendering is 100% procedural** now (`PetRenderer.cpp` / `UIManager.cpp`) — do
  not reintroduce sprite-atlas dependencies for pets/backgrounds; the user
  explicitly moved away from custom art. Switch button glyphs are the exception.
- A header comment style in `redeem.js` was intentionally changed externally
  (`====` → `===`) — don't "fix" it back.
- On Windows here, `Remove-Item` can be blocked by the sandbox; deleting files
  went through `[System.IO.File]::Delete` / `[System.IO.Directory]::Delete`.

**Bottom line:** the game is done and boots; the only thing between here and a
working code-redemption feature is the HTTP transport. The decided fix is the
`3ds-curl` migration in §7 — it just needs to be implemented and tested on a
console.
