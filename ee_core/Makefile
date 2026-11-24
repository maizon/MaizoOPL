# Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
# Copyright 2006-2008 Polo
# Licenced under Academic Free License version 3.0
# Review OPNPS2LD README & LICENSE files for further details.
#
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Standard Makefile

EE_BIN = ee_core.elf
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/

GSMCORE_EE_OBJS = gsm_engine.o gsm_api.o dve_reg.o
IGSCORE_EE_OBJS = igs_api.o
CHEATCORE_EE_OBJS = cheat_engine.o cheat_api.o

EE_OBJS = main.o syshook.o iopmgr.o modmgr.o util.o patches.o patches_asm.o \
	  padhook.o cd_igr_rpc.o tlb.o asm.o crt0.o $(GSMCORE_EE_OBJS) $(CHEATCORE_EE_OBJS)
MAPFILE = ee_core.map

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include -Iinclude -I.
EE_CFLAGS = -D_EE -Os -G0 -Wall $(EE_INCS)

ifeq ($(EESIO_DEBUG),1)
    EE_CFLAGS += -D__EESIO_DEBUG
endif

ifeq ($(DTL_T10000),1)
    EE_CFLAGS += -D_DTL_T10000
endif

ifeq ($(LOAD_DEBUG_MODULES),1)
EE_CFLAGS += -D__LOAD_DEBUG_MODULES
    ifeq ($(DECI2_DEBUG),1)
    EE_CFLAGS += -D__DECI2_DEBUG
    endif
    ifeq ($(TTY_APPROACH),UDP)
    EE_CFLAGS += -DTTY_UDP
    else ifeq ($(TTY_APPROACH),PPC_UART)
    EE_CFLAGS += -DTTY_PPC
    endif
endif

# Hardware regression quarantine: Beta-39 e75bc88 launches games,
# while Beta-82 855d948 black-screens before gameplay with GSM, PadEmu
# and In-Game toggled OFF. To isolate the resident EE-core footprint,
# build the original OPL IGR by default in THIS DIAGNOSTIC BRANCH.
# Experimental overlay remains available for explicit compiler testing:
#   make MAIZO_IGR_EXPERIMENTAL=1
# No feature/UI preference may silently install the overlay otherwise.
ifeq ($(MAIZO_IGR_EXPERIMENTAL),1)
ifneq ($(IGS),1)
ifneq ($(EESIO_DEBUG),1)
EE_OBJS += maizo_igrmenu.o maizo_gs_runtime.o
EE_CFLAGS += -DMAIZO_IGR_MENU
endif
endif
endif

ifeq ($(IGS),1)
EE_OBJS += $(IGSCORE_EE_OBJS)
EE_CFLAGS += -DIGS
endif

EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_LDFLAGS = -nostartfiles -nostdlib -Tlinkfile -L$(PS2SDK)/ee/lib -s -Wl,-Map,$(MAPFILE)
# Enforce an 8 KiB minimum stack for the normal, release EE core.
# IGS screenshots and debug builds use different resident layouts and
# retain their pre-existing stack policy. The linker guard catches
# dangerous release growth at build time rather than on PS2 hardware.
ifneq ($(IGS),1)
ifneq ($(EESIO_DEBUG),1)
ifneq ($(LOAD_DEBUG_MODULES),1)
ifneq ($(DTL_T10000),1)
EE_LDFLAGS += -Wl,--defsym=maizo_enforce_8k_stack=1
endif
endif
endif
endif
EE_LIBS += -lpatches

ifeq ($(PADEMU),1)
EE_CFLAGS += -DPADEMU
endif

EE_LIBS += -lkernel -lgcc

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.S
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.s
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS)
	@echo "=== EE CORE MEMORY MAP (diagnostic) ==="
	@grep -E '(_end_bss|_stack_size|_stack =)' $(MAPFILE) | tail -n 8 || true

all: $(EE_BIN)

clean:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)

clean_all:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)

include $(PS2SDK)/samples/Makefile.pref
