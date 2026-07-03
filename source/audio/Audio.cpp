// =============================================================================
//  PetPal - Audio.cpp
//  ndsp-backed audio: one streamed looping BGM (a big WAV, read in chunks on a
//  worker thread) + one-shot SFX (loaded WAVs and synthesized chip blips). All
//  fire-and-forget. Degrades to silence if the DSP firmware is unavailable.
// =============================================================================
#include "audio/Audio.h"

#if defined(__3DS__)

#include "util/Log.h"

#include <3ds.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace petpal {
namespace audio {

namespace {

constexpr int   kBgmChn      = 0;             // BGM stream channel
constexpr int   kSfxChn0     = 1;             // SFX channels: 1..4 (round-robin)
constexpr int   kSfxChnCount = 4;
constexpr int   kSynthRate   = 32000;         // synthesized blip sample rate
constexpr u32   kBgmBufBytes = 32 * 1024;     // per stream buffer (mult. of 4)

bool  g_ok      = false;
float g_bgmVol  = 1.0f;
float g_sfxVol  = 1.0f;
int   g_sfxNext = 0;

// ---- one loaded/generated sound ---------------------------------------------
struct Sound {
    s16* data     = nullptr;  // linear memory (DSP-visible)
    u32  nsamples = 0;        // sample frames (per channel)
    u16  format   = NDSP_FORMAT_MONO_PCM16;
    float rate    = kSynthRate;
};
Sound g_snd[9];               // indexed by Sfx
ndspWaveBuf g_sfxWbuf[kSfxChnCount];

// ---- BGM streaming ----------------------------------------------------------
FILE*        g_bgmFile   = nullptr;
long         g_bgmStart  = 0;         // byte offset of PCM data in the file
u32          g_bgmSize   = 0;         // PCM data length
u32          g_bgmPos    = 0;         // bytes played into the data (for looping)
u16          g_bgmChans  = 2;
u8*          g_bgmBuf[2]  = { nullptr, nullptr };
ndspWaveBuf  g_bgmWbuf[2];
Thread       g_bgmThread = nullptr;
volatile bool g_bgmRun   = false;

void setChnMix(int chn, float vol) {
    float mix[12] = {0};
    mix[0] = mix[1] = vol;         // front left / right
    ndspChnSetMix(chn, mix);
}

// ---- minimal WAV (PCM) parser -----------------------------------------------
// Fills channels/rate and the [dataOffset, dataSize) of the PCM chunk.
bool wavInfo(FILE* f, long* dataOff, u32* dataSize, u16* channels, u32* rate) {
    u8 h[12];
    if (std::fread(h, 1, 12, f) != 12) return false;
    if (std::memcmp(h, "RIFF", 4) || std::memcmp(h + 8, "WAVE", 4)) return false;
    u16 ch = 2; u32 sr = 44100;
    for (;;) {
        u8 c[8];
        if (std::fread(c, 1, 8, f) != 8) return false;
        const u32 sz = c[4] | (c[5] << 8) | (c[6] << 16) | (u32)(c[7] << 24);
        if (!std::memcmp(c, "fmt ", 4)) {
            u8 fmt[16];
            if (std::fread(fmt, 1, 16, f) != 16) return false;
            ch = fmt[2] | (fmt[3] << 8);
            sr = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) | (u32)(fmt[7] << 24);
            if (sz > 16) std::fseek(f, sz - 16, SEEK_CUR);
        } else if (!std::memcmp(c, "data", 4)) {
            *dataOff = std::ftell(f);
            *dataSize = sz;
            *channels = ch;
            *rate = sr;
            return true;
        } else {
            std::fseek(f, sz, SEEK_CUR);
        }
    }
}

// Load a whole WAV into linear memory as a one-shot Sound.
bool loadWav(const char* path, Sound& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    long off; u32 size; u16 ch; u32 rate;
    if (!wavInfo(f, &off, &size, &ch, &rate)) { std::fclose(f); return false; }
    out.data = (s16*)linearAlloc(size);
    if (!out.data) { std::fclose(f); return false; }
    std::fseek(f, off, SEEK_SET);
    const size_t got = std::fread(out.data, 1, size, f);
    std::fclose(f);
    out.nsamples = (u32)(got / (ch * 2));
    out.format   = (ch == 2) ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16;
    out.rate     = (float)rate;
    DSP_FlushDataCache(out.data, size);
    return true;
}

// ---- blip synthesis ---------------------------------------------------------
void tone(std::vector<s16>& buf, float freq, int ms, float vol, bool square) {
    const int n = ms * kSynthRate / 1000;
    for (int i = 0; i < n; ++i) {
        const float t   = (float)i / kSynthRate;
        const float env = std::fmin(1.0f, i / (0.004f * kSynthRate)) * std::exp(-3.0f * i / n);
        float s = std::sin(2.0f * 3.14159265f * freq * t);
        if (square) s = (s >= 0.0f) ? 1.0f : -1.0f;
        buf.push_back((s16)(s * env * vol * 32000.0f));
    }
}

void makeSynth(Sound& out, const std::vector<s16>& samples) {
    const u32 bytes = (u32)samples.size() * 2;
    out.data = (s16*)linearAlloc(bytes);
    if (!out.data) return;
    std::memcpy(out.data, samples.data(), bytes);
    out.nsamples = (u32)samples.size();
    out.format   = NDSP_FORMAT_MONO_PCM16;
    out.rate     = kSynthRate;
    DSP_FlushDataCache(out.data, bytes);
}

void buildSynthSounds() {
    std::vector<s16> b;
    b.clear(); tone(b, 900, 45, 0.5f, true);                       makeSynth(g_snd[(int)Sfx::Navigate], b);
    b.clear(); tone(b, 700, 45, 0.5f, true); tone(b, 1050, 60, 0.5f, true); makeSynth(g_snd[(int)Sfx::Select], b);
    b.clear(); tone(b, 700, 45, 0.5f, true); tone(b, 460, 60, 0.5f, true);  makeSynth(g_snd[(int)Sfx::Back], b);
    b.clear(); tone(b, 988, 55, 0.5f, false); tone(b, 1319, 110, 0.5f, false); makeSynth(g_snd[(int)Sfx::Coin], b);
    b.clear(); tone(b, 523, 70, 0.5f, false); tone(b, 659, 70, 0.5f, false);
               tone(b, 784, 70, 0.5f, false); tone(b, 1047, 130, 0.5f, false); makeSynth(g_snd[(int)Sfx::LevelUp], b);
    b.clear(); tone(b, 784, 70, 0.5f, false); tone(b, 1047, 110, 0.5f, false); makeSynth(g_snd[(int)Sfx::NewFriend], b);
    b.clear(); tone(b, 200, 180, 0.45f, true);                     makeSynth(g_snd[(int)Sfx::Error], b);
}

// ---- BGM streaming worker ---------------------------------------------------
void fillBgm(int i) {
    u32 filled = 0;
    while (filled < kBgmBufBytes) {
        std::fseek(g_bgmFile, g_bgmStart + g_bgmPos, SEEK_SET);
        const u32 want = std::min(kBgmBufBytes - filled, g_bgmSize - g_bgmPos);
        const size_t got = std::fread(g_bgmBuf[i] + filled, 1, want, g_bgmFile);
        filled += (u32)got;
        g_bgmPos += (u32)got;
        if (g_bgmPos >= g_bgmSize) g_bgmPos = 0; // seamless loop
        if (got == 0) break;
    }
    std::memset(&g_bgmWbuf[i], 0, sizeof(ndspWaveBuf));
    g_bgmWbuf[i].data_pcm16 = (s16*)g_bgmBuf[i];
    g_bgmWbuf[i].nsamples   = filled / (g_bgmChans * 2);
    DSP_FlushDataCache(g_bgmBuf[i], filled);
    ndspChnWaveBufAdd(kBgmChn, &g_bgmWbuf[i]);
}

void bgmThread(void*) {
    fillBgm(0);
    fillBgm(1);
    while (g_bgmRun) {
        for (int i = 0; i < 2; ++i)
            if (g_bgmWbuf[i].status == NDSP_WBUF_DONE) fillBgm(i);
        svcSleepThread(12 * 1000 * 1000); // 12 ms
    }
}

} // namespace

// -----------------------------------------------------------------------------
bool init() {
    if (g_ok) return true;
    if (R_FAILED(ndspInit())) { PP_WARN("ndsp unavailable; audio off"); return false; }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    // SFX channels.
    for (int i = 0; i < kSfxChnCount; ++i) {
        const int ch = kSfxChn0 + i;
        ndspChnReset(ch);
        ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
        setChnMix(ch, g_sfxVol);
    }

    // Load the WAV SFX + synthesize the UI blips.
    loadWav("romfs:/audio/eating.wav", g_snd[(int)Sfx::Eat]);
    loadWav("romfs:/audio/bonzi.wav",  g_snd[(int)Sfx::Bonzi]);
    buildSynthSounds();

    // Start the streamed BGM if the file is there.
    g_bgmFile = std::fopen("romfs:/audio/main.wav", "rb");
    if (g_bgmFile) {
        long off; u32 size; u16 ch; u32 rate;
        if (wavInfo(g_bgmFile, &off, &size, &ch, &rate)) {
            g_bgmStart = off; g_bgmSize = size; g_bgmChans = ch; g_bgmPos = 0;
            ndspChnReset(kBgmChn);
            ndspChnSetInterp(kBgmChn, NDSP_INTERP_LINEAR);
            ndspChnSetRate(kBgmChn, (float)rate);
            ndspChnSetFormat(kBgmChn, (ch == 2) ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
            setChnMix(kBgmChn, g_bgmVol);
            g_bgmBuf[0] = (u8*)linearAlloc(kBgmBufBytes);
            g_bgmBuf[1] = (u8*)linearAlloc(kBgmBufBytes);
            if (g_bgmBuf[0] && g_bgmBuf[1]) {
                g_bgmRun = true;
                s32 prio = 0x30;
                svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
                g_bgmThread = threadCreate(bgmThread, nullptr, 8 * 1024, prio + 1, -1, false);
            }
        } else {
            std::fclose(g_bgmFile); g_bgmFile = nullptr;
        }
    }

    g_ok = true;
    return true;
}

void exit() {
    if (!g_ok) return;
    g_bgmRun = false;
    if (g_bgmThread) { threadJoin(g_bgmThread, UINT64_MAX); threadFree(g_bgmThread); g_bgmThread = nullptr; }
    ndspExit();
    for (auto& s : g_snd) if (s.data) { linearFree(s.data); s.data = nullptr; }
    for (auto& b : g_bgmBuf) if (b) { linearFree(b); b = nullptr; }
    if (g_bgmFile) { std::fclose(g_bgmFile); g_bgmFile = nullptr; }
    g_ok = false;
}

void playSfx(Sfx s) {
    if (!g_ok) return;
    const Sound& snd = g_snd[(int)s];
    if (!snd.data || snd.nsamples == 0) return;

    const int wi = g_sfxNext % kSfxChnCount;
    const int ch = kSfxChn0 + wi;
    g_sfxNext++;

    ndspChnWaveBufClear(ch);
    ndspChnSetFormat(ch, snd.format);
    ndspChnSetRate(ch, snd.rate);
    setChnMix(ch, g_sfxVol);

    std::memset(&g_sfxWbuf[wi], 0, sizeof(ndspWaveBuf));
    g_sfxWbuf[wi].data_pcm16 = snd.data;
    g_sfxWbuf[wi].nsamples   = snd.nsamples;
    ndspChnWaveBufAdd(ch, &g_sfxWbuf[wi]);
}

void setMusicVolume(float v01) {
    g_bgmVol = v01 < 0 ? 0 : (v01 > 1 ? 1 : v01);
    if (g_ok) setChnMix(kBgmChn, g_bgmVol);
}
void setSfxVolume(float v01) { g_sfxVol = v01 < 0 ? 0 : (v01 > 1 ? 1 : v01); }
void setMusicEnabled(bool on) { if (g_ok) ndspChnSetPaused(kBgmChn, !on); }

} // namespace audio
} // namespace petpal

#else // ---- host build: no-op stubs ----------------------------------------

namespace petpal { namespace audio {
bool init() { return false; }
void exit() {}
void playSfx(Sfx) {}
void setMusicVolume(float) {}
void setSfxVolume(float) {}
void setMusicEnabled(bool) {}
}}

#endif
