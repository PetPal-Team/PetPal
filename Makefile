#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET       is the name of the output
# BUILD        is the directory where object files & intermediate files go
# SOURCES      is a list of directories containing source code
# DATA         is a list of directories containing data files
# INCLUDES     is a list of directories containing header files
# GRAPHICS     is a list of directories containing graphics (.t3s) for citro2d
# ROMFS        is the directory containing data mounted at romfs:/ on device
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# APP_TITLE / APP_DESCRIPTION / APP_AUTHOR populate the homebrew menu entry.
#---------------------------------------------------------------------------------
TARGET      := PetPal
BUILD       := build
SOURCES     := source \
               source/core \
               source/audio \
               source/pets \
               source/friends \
               source/journal \
               source/adventure \
               source/items \
               source/achievements \
               source/netpass \
               source/save \
               source/ui \
               source/ui/screens \
               source/util
DATA        := data
INCLUDES    := include
GRAPHICS    := gfx
ROMFS       := romfs

APP_TITLE       := PetPal
APP_DESCRIPTION := A StreetPass virtual pet
APP_AUTHOR      := PetPal Team
# 48x48 SMDH icon shown in the Homebrew Launcher (generated from assets/ui/logo.png).
ICON := assets/ui/icon.png
# Optional extra metadata (provide your own to enable):
# BANNER   := assets/ui/banner.png      # CIA/banner art
# JINGLE   := assets/music/jingle.wav   # banner jingle

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS  := -g -Wall -Wextra -O2 -mword-relocations \
           -ffunction-sections \
           $(ARCH)

CFLAGS  += $(INCLUDE) -D__3DS__

# Extra command-line defines, e.g. a debug build with on-device test hooks:
#   make EXTRA_DEFS=-DPETPAL_DEBUG=1     (also turns on PP_LOG logging)
CFLAGS  += $(EXTRA_DEFS)

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# curl + mbedTLS + zlib come from portlibs (dkp-pacman -S 3ds-curl). They MUST be
# listed before the ctru/citro libs so the linker resolves symbols in the right
# order (curl -> mbedtls -> z, then the ctru sockets/crypto they depend on).
LIBS    := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib. PORTLIBS is where dkp-pacman installs 3ds-curl.
#---------------------------------------------------------------------------------
LIBDIRS := $(CTRULIB) $(PORTLIBS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES:= $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

#---------------------------------------------------------------------------------
# .t3x files are built from .t3s graphics specs and mounted via romfs
#---------------------------------------------------------------------------------
ifeq ($(strip $(GFXFILES)),)
	export T3XFILES :=
else
	export T3XFILES := $(GFXFILES:.t3s=.t3x)
	export ROMFS_T3XFILES := $(patsubst %.t3s, $(CURDIR)/$(ROMFS)/gfx/%.t3x, $(GFXFILES))
endif

export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

#---------------------------------------------------------------------------------
# CIA packaging (makerom + bannertool). These are community tools, not part of
# devkitPro; the project ships copies under tools/cia. Override on the command
# line if you have them on PATH, e.g.  make cia MAKEROM=makerom BANNERTOOL=bannertool
#---------------------------------------------------------------------------------
MAKEROM    ?= $(CURDIR)/tools/cia/makerom/makerom.exe
BANNERTOOL ?= $(CURDIR)/tools/cia/bannertool/bannertool-1.2.2-windows/bannertool.exe
RSF        ?= $(CURDIR)/petpal.rsf
BANNER_IMG ?= $(CURDIR)/banner.png
BANNER_WAV ?= $(CURDIR)/audio.wav

.PHONY: $(BUILD) all clean cia audiofs

#---------------------------------------------------------------------------------
all: audiofs $(BUILD)

#---------------------------------------------------------------------------------
# Stage the source audio (assets/) into romfs so it's mounted at romfs:/audio/.
# The WAVs live in assets/ in git; romfs/audio is generated (and .gitignored),
# so the repo carries one copy of the audio rather than two.
#---------------------------------------------------------------------------------
audiofs:
	@mkdir -p $(ROMFS)/audio
	@cp -u assets/sfx/*.wav assets/music/*.wav $(ROMFS)/audio/ 2>/dev/null || true

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
# Build the installable CIA. Depends on a normal build (for PetPal.elf/.smdh),
# rebuilds the banner from banner.png + audio.wav, then packs everything.
#---------------------------------------------------------------------------------
cia: audiofs $(BUILD)
	@echo building banner.bnr ...
	@"$(BANNERTOOL)" makebanner -i "$(BANNER_IMG)" -a "$(BANNER_WAV)" -o "$(CURDIR)/banner.bnr"
	@echo building $(TARGET).cia ...
	@"$(MAKEROM)" -f cia -o "$(CURDIR)/$(TARGET).cia" -elf "$(CURDIR)/$(TARGET).elf" \
		-rsf "$(RSF)" -icon "$(CURDIR)/$(TARGET).smdh" -banner "$(CURDIR)/banner.bnr" \
		-exefslogo -target t
	@echo built ... $(TARGET).cia

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia banner.bnr $(ROMFS)/gfx $(ROMFS)/audio

#---------------------------------------------------------------------------------
else

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
else
$(OUTPUT).3dsx : $(OUTPUT).elf
endif

$(OFILES_SOURCES) : $(HFILES) $(ROMFS_T3XFILES)

$(OUTPUT).elf : $(OFILES)

#---------------------------------------------------------------------------------
# rule for building citro2d texture atlases from .t3s
#---------------------------------------------------------------------------------
$(TOPDIR)/$(ROMFS)/gfx/%.t3x : %.t3s
	@echo $(notdir $<)
	@[ -d $(@D) ] || mkdir -p $(@D)
	@tex3ds --atlas -i $< -H $*.h -d $*.d -o $@

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
