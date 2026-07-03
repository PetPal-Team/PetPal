// =============================================================================
//  PetPal - Audio.h
//  Tiny sound layer over the 3DS DSP (ndsp): a streamed, looping BGM plus one-
//  shot SFX (loaded WAVs + a handful of synthesized "chippy" UI blips). All
//  playback is fire-and-forget; the BGM streams on its own thread so nothing
//  here ever blocks the UI. Device-only - on a host build these are no-ops.
// =============================================================================
#pragma once

namespace petpal {
namespace audio {

// UI + gameplay sound effects. Navigate/Select/Back/Coin/LevelUp/Error are
// synthesized on the fly; Eat/Bonzi are the shipped WAVs.
enum class Sfx {
    Navigate,   // moving the cursor
    Select,     // opening a screen / confirming
    Back,       // closing a screen
    Coin,       // spending/earning coins, rewards
    LevelUp,    // level-up / evolution fanfare
    NewFriend,  // met someone new
    Error,      // can't do that
    Eat,        // feeding (eating.wav)
    Bonzi,      // BonziBuddy transform (bonzi.wav)
};

// Bring up ndsp, load/synthesize sounds, and start the looping BGM. Safe to call
// once; returns false if the DSP is unavailable (audio then silently no-ops).
bool init();
void exit();

void playSfx(Sfx s);

void setMusicVolume(float v01); // 0..1
void setSfxVolume(float v01);   // 0..1
void setMusicEnabled(bool on);  // pause/resume the stream

} // namespace audio
} // namespace petpal
