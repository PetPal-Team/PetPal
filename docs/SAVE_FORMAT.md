# PetPal Save Format

Binary, little-endian, versioned, and CRC-validated. Produced by
`SaveManager::serialize()` and parsed by `SaveManager::deserialize()`
(`source/save/SaveManager.cpp`). This document is the source of truth for the
byte layout — keep it in sync with `encodeBody`/`decodeBody`.

## Files on the SD card

```
sdmc:/3ds/PetPal/
  save.bin          # current save
  save.bak          # previous good save (rotated in on each write)
  save.bin.tmp      # transient; written then renamed (atomic swap)
  backups/
    petpal_YYYYMMDD_HHMMSS.ppsav   # portable exports (same format)
```

### Write strategy (crash-safe)
1. Serialize state to a byte vector.
2. Write `save.bin.tmp`.
3. If `save.bin` exists: delete `save.bak`, rename `save.bin` → `save.bak`.
4. Rename `save.bin.tmp` → `save.bin`.

A crash at any step leaves either the old `save.bin` or a valid `save.bak`.

### Read strategy
`load()` reads `save.bin`; if it is missing or fails validation it transparently
falls back to `save.bak`.

## File header (16 bytes)

| Offset | Type  | Field    | Notes |
|-------:|-------|----------|-------|
| 0      | u32   | magic    | `0x4C415050` (`kSaveMagic`, "PPAL") |
| 4      | u16   | version  | `kSaveVersion` (currently `1`) |
| 6      | u16   | reserved | `0` |
| 8      | u32   | bodyLen  | byte length of the body that follows |
| 12     | u32   | bodyCrc  | CRC-32 (IEEE) of the body bytes |

Validation order: magic → version (`0` invalid, `> kSaveVersion` ⇒
`Unsupported`) → `16 + bodyLen ≤ fileLen` → CRC match → body parse.

## Conventions

* **Integers**: little-endian, fixed width (`u8/u16/u32/u64`).
* **Strings**: `u16` length prefix followed by that many UTF-8 bytes, **no**
  null terminator. Pet names are capped at `kMaxPetNameLen` (12).
* **Enums**: stored as their underlying integer (`u8` unless noted). Values are
  append-only and never reordered.

## Body layout

All sections are concatenated in this exact order.

### 1. Pet
| Type | Field |
|------|-------|
| u64  | id |
| u8   | species |
| str  | name |
| u8   | primaryColor |
| u8   | secondaryColor |
| u8   | accentColor |
| u8   | equippedAccessory |
| u8   | equippedStyle |
| u16  | accessoryUnlockMask (bit i ⇒ `Accessory(i)` unlocked) |
| u16  | styleUnlockMask (bit i ⇒ `Style(i)` unlocked) |
| u16  | level |
| u32  | experience (within current level) |
| u8   | happiness (0–100) |
| u8   | energy (0–100) |
| u32  | friendship |
| u16  | adventureRank |
| u32  | streetpassEncounters |
| u32  | uniquePetsMet |
| u32  | adventuresCompleted |
| u8   | stage (EvolutionStage) |
| u16  | favoriteItem (ItemId) |

### 2. Inventory
| Type | Field |
|------|-------|
| u16  | itemCount (= `kItemCount` at save time) |
| u16 × itemCount | counts, indexed by `ItemId` |
| u32  | coins |

> Forward-compatible: on load, counts beyond the build's `kItemCount` are
> skipped, and missing trailing items default to 0, so adding item types does
> not break old saves.

### 3. Friends
| Type | Field |
|------|-------|
| u32  | friendCount |

Then `friendCount` records:
| Type | Field |
|------|-------|
| u64  | id |
| str  | name |
| u8   | species |
| u16  | level |
| u8   | stage |
| u8   | primaryColor |
| u8   | style |
| u16  | favoriteItem |
| u64  | dateMet (Unix seconds) |
| u32  | meetings |
| u32  | friendshipPoints |

### 4. Journal
| Type | Field |
|------|-------|
| u32  | entryCount |

Then `entryCount` records (stored newest-first):
| Type | Field |
|------|-------|
| u64  | when (Unix seconds) |
| u8   | category (JournalCategory) |
| str  | text |

### 5. Adventure (active trip, if any)
| Type | Field |
|------|-------|
| u8   | active (0/1) |
| u8   | location |
| u8   | duration |
| u64  | startTime (Unix seconds) |

### 6. Achievements
| Type | Field |
|------|-------|
| u32  | unlockMask (bit i ⇒ `AchievementId(i)` earned) |

### 7. World
| Type | Field |
|------|-------|
| u16  | locationUnlockMask (bit i ⇒ `Location(i)` unlocked) |

### 8. Settings
| Type | Field |
|------|-------|
| u8   | musicVolume (0–100) |
| u8   | sfxVolume (0–100) |
| u8   | autoSave (0/1) |
| u8   | rumble (0/1) |

### 9. Meta
| Type | Field |
|------|-------|
| u64  | createdAt |
| u64  | lastSavedAt |
| u64  | playSeconds |
| u8   | onboarded (0/1) |

### 10. Pet transform (v2+)
| Type | Field |
|------|-------|
| u8   | transformForm |
| u64  | transformExpiry (Unix seconds) |

### 11. Account (v3+) — server identity / 3DS link
| Type | Field |
|------|-------|
| str  | id (`PP-XXXX-XXXX`, empty until first online boot) |
| str  | token (per-device credential; empty after linking) |
| u8   | linked (0/1 — adopted a phone account's id) |

## Size budget

A fresh save is well under 1 KB. Caps keep growth bounded:
`FriendList::kMaxFriends = 512` and `Journal::kMaxEntries = 200`. Worst case is
on the order of tens of KB — trivial for the SD card and fast to write each
autosave.

## Versioning & migration

When a field must change:
1. Bump `kSaveVersion`.
2. In `deserialize`, branch on `header.version` and parse the old body, filling
   new fields with defaults.
3. Never repurpose an existing enum value; append instead.

Saves from a newer build than the running app are rejected with
`SaveError::Unsupported` (the app keeps the existing data rather than corrupting
it).
