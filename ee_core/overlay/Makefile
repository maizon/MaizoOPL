EE_BIN = maizo_overlay.elf
EE_INCS = -I.. -I../include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include
EE_CFLAGS = -D_EE -DMAIZO_IGR_MENU -DMAIZO_IGR_OVERLAY_SIDE -DMZ_GS_CLIENT -Os -G0 -fno-builtin -ffunction-sections -fdata-sections -Wall $(EE_INCS)
ifeq ($(MAIZO_MAGE_SKY_EXPERIMENTAL),1)
EE_CFLAGS += -DMAIZO_MAGE_SKY_EXPERIMENTAL
endif
ifeq ($(MAIZO_IGR_RESTORE_EXPERIMENTAL),1)
EE_CFLAGS += -DMAIZO_IGR_RESTORE
endif
all: $(EE_BIN)
$(EE_BIN): entry.c ../src/maizo_igrmenu.c linkfile $(wildcard ../include/*.h)
	$(EE_CC) $(EE_CFLAGS) -nostartfiles -nostdlib -Tlinkfile -Wl,--gc-sections -Wl,-Map,maizo_overlay.map -o $@ entry.c ../src/maizo_igrmenu.c -lgcc
clean:
	rm -f $(EE_BIN) maizo_overlay.map
include $(PS2SDK)/samples/Makefile.pref
