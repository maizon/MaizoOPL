EE_BIN = maizo-gs-lab.elf
EE_OBJS = main.o kick.o autolink.o
EE_LDFLAGS += -Wl,--wrap=dma_channel_send_normal
EE_LIBS = -lgraph -ldma -lpad -lpacket2 -ldraw
EE_CFLAGS += -Wall -Wextra -I../../ee_core/include
all: $(EE_BIN)
autolink.o: ../../ee_core/adapters/maizo_gs_autolink.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@
kick.o: kick.vsm
	dvp-as $< -o $@
iso: all
	mkdir -p disc
	cp $(EE_BIN) disc/SLUS_999.99
	cp SYSTEM.CNF disc/SYSTEM.CNF
	mkisofs -quiet -o MAIZO-GS-LAB.iso disc
clean:
	rm -rf $(EE_OBJS) $(EE_BIN) disc MAIZO-GS-LAB.iso
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
