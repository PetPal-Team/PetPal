# Host build for the portable PetPal model layer (no 3DS / citro2d).
# Usage:  make -C tests        # build + nothing
#         make -C tests run     # build + run the tests
#
# Requires a C++17 host compiler. devkitPro ships one; or use system g++/clang++.

CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I../include
BUILD    := build
BIN      := $(BUILD)/petpal_tests

# Only the portable model sources compile on a host (UI + CecdTransport excluded).
SRCS := \
    test_main.cpp \
    ../source/core/Names.cpp \
    ../source/pets/Pet.cpp \
    ../source/items/Inventory.cpp \
    ../source/friends/Friend.cpp \
    ../source/friends/FriendList.cpp \
    ../source/journal/Journal.cpp \
    ../source/adventure/Adventure.cpp \
    ../source/achievements/Achievements.cpp \
    ../source/netpass/NetPassManager.cpp \
    ../source/save/SaveManager.cpp \
    ../source/util/Crc32.cpp \
    ../source/util/NameFilter.cpp \
    ../source/util/Time.cpp

all: $(BIN)

$(BIN): $(SRCS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
