PROJECT=yacht
LD65_FLAGS=
CA65_FLAGS=
EMULATOR=/mnt/c/NESDev/Mesen.exe
NSF2DATA=/mnt/c/NESDev/famitone2d/NSF/nsf2data.exe
TEXT2DATA=/mnt/c/NESDev/famitone2d/text2data.exe
FAMITRACKER=/mnt/c/NESDev/famitracker/FamiTracker.exe
VERSION := $(shell git describe --exact-match --tags 2> /dev/null || git rev-parse --short HEAD)

TARGET=${PROJECT}.nes

.PHONY: debug run usage release FORCE

default: ${TARGET}

debug: LD65_FLAGS += --dbgfile ${PROJECT}.dbg
debug: CA65_FLAGS += -g -DDEBUG=1
debug: ${TARGET}

${TARGET}: src/main.o src/crt0.o src/lib/unrle.o \
           assets/nametables.o
	ld65 $^ -C MMC3.cfg nes.lib -m map.txt -o ${TARGET} ${LD65_FLAGS}

%.o: %.s
	ca65 $< ${CA65_FLAGS}

src/main.s: src/main.c assets/nametables.h
	cc65 -Oirs src/main.c --add-source ${CA65_FLAGS}

src/crt0.o: src/crt0.s src/mmc3/mmc3_code.asm src/lib/neslib.s src/lib/nesdoug.s assets/*.chr \
            src/music/soundtrack.s src/music/soundfx.s
	ca65 $< ${CA65_FLAGS}

assets/nametables.o: assets/nametables.s assets/nametables.h \
                     assets/nametables/main.rle \
                     assets/nametables/title.rle \
                     assets/nametables/help1.rle \
                     assets/nametables/help2.rle
	ca65 $< ${CA65_FLAGS}

src/music/soundtrack.s: src/music/soundtrack.txt
	${TEXT2DATA} $^ -ca65 -allin

src/music/soundtrack.txt: src/music/soundtrack.ftm
	${FAMITRACKER} $^ -export $@

src/music/soundfx.nsf: src/music/soundfx.ftm
	${FAMITRACKER} src/music/soundfx.ftm -export src/music/soundfx.nsf

src/music/soundfx.s: src/music/soundfx.nsf
	${NSF2DATA} src/music/soundfx.nsf -ca65 -ntsc

clean:
	rm src/*.o src/*/*.o assets/*.o *.nes *.dbg map.txt -f

run: debug
	${EMULATOR} ${TARGET}

usage: tools/ld65-map.json

tools/ld65-map.json: map.txt tools/ld65-map.rb
	ruby tools/ld65-map.rb map.txt 2 1 tools/ld65-map.json

release: ${TARGET}
	cp ${TARGET} ${PROJECT}-${VERSION}.nes

FORCE:
