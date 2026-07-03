# PetPal host tests

These tests link **only** the portable model layer (`Pet`, `Inventory`,
`FriendList`, `Journal`, `Adventure`, `Achievements`, `SaveManager`,
`NetPassManager` + utilities). None of them include `<3ds.h>` or citro2d, so
they build and run on any desktop with a C++17 compiler — handy for fast
iteration on gameplay rules and the save format without flashing a 3DS.

## Run

```sh
make -C tests run
```

or directly:

```sh
g++ -std=c++17 -I include tests/test_main.cpp \
    source/core/Names.cpp source/pets/Pet.cpp source/items/Inventory.cpp \
    source/friends/Friend.cpp source/friends/FriendList.cpp \
    source/journal/Journal.cpp source/adventure/Adventure.cpp \
    source/achievements/Achievements.cpp source/netpass/NetPassManager.cpp \
    source/save/SaveManager.cpp source/util/Crc32.cpp source/util/Time.cpp \
    -o petpal_tests && ./petpal_tests
```

On Windows with the devkitPro MSYS2 shell, the same `g++` works. Expected
output ends with `N checks, 0 failure(s)`.

## What is covered

| Area              | Checks |
|-------------------|--------|
| Pet progression   | naming, XP/level-up, evolution thresholds |
| Inventory         | add/remove saturation, coin spend guard |
| NetPass pipeline  | packet build/validate/CRC, loopback poll, friend dedupe |
| Achievements      | unlock-once semantics |
| Adventure         | timing + reward rolls |
| Save format       | serialize → deserialize equality, corruption detection |
