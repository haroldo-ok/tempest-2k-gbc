GBDK ?= /tmp/gbdk/gbdk
LCC  := $(GBDK)/bin/lcc
CFLAGS := -Wa-l -Wl-m -Wl-j -Isrc -Igen
LFLAGS := -Wm-yC -Wm-yt0x19 -Wm-yo32 -Wm-yn"T2K-GBC" -Wl-yp0x143=0xC0
OBJS := build/main.o build/game.o build/sound.o build/fmv.o build/assets.o

.PHONY: all clean assets rom

all: rom/t2k.gbc

assets:
	cd tools && python3 build.py
	python3 tools/compose.py
	python3 tools/vox.py

build/sound.o: src/sound.c src/sound.h gen/tunes.h gen/voices.h
	@mkdir -p build
	$(LCC) $(CFLAGS) -c -o $@ $<

build/%.o: src/%.c src/*.h gen/assets.h
	@mkdir -p build
	$(LCC) $(CFLAGS) -c -o $@ $<

build/assets.o: gen/assets.c gen/assets.h
	@mkdir -p build
	$(LCC) $(CFLAGS) -c -o $@ $<

build/code.gbc: $(OBJS)
	$(LCC) $(CFLAGS) $(LFLAGS) -o $@ $(OBJS)

rom/t2k.gbc: build/code.gbc gen/blob.bin gen/tunes.bin gen/voices.bin tools/mkrom.py
	@mkdir -p rom
	python3 tools/mkrom.py build/code.gbc gen/blob.bin $@ gen/tunes.bin gen/voices.bin

clean:
	rm -rf build rom/t2k.gbc
