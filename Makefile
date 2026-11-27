# WE64: build the C64 program, and run the host tests.
#
#   make          build/we64.prg and build/we64.d64 (cc65, ip65 from vendor/)
#   make we-c64.prg   the PRG where the WarpEngine c64 pipeline (build: make) wants it
#   make test     the host tests (scripts/ci-test.sh)
#   make vice     start it in VICE from the disk image
#   make clean

CL65 ?= cl65
C1541 ?= c1541
IP65 = vendor/ip65
IP65LIB = $(IP65)/ip65/ip65_tcp.lib
DRIVERLIB = $(IP65)/drivers/ip65_c64.lib

SOURCES = src/main.c src/store.c src/http.c src/directory.c src/config.c src/crc32.c src/text.c \
          src/disk_cbm.c src/net_uci.c src/net_ip65.c src/net_modem.c src/preview.c

.PHONY: all test check vice clean

all: build/we64.prg build/we64.d64

$(IP65LIB):
	$(MAKE) -C $(IP65)/ip65 ip65_tcp.lib

$(DRIVERLIB):
	$(MAKE) -C $(IP65)/drivers ip65_c64.lib

build/we64.prg: $(SOURCES) src/*.h $(IP65LIB) $(DRIVERLIB)
	mkdir -p build
	$(CL65) -t c64 -O -I $(IP65)/inc -Wl -D,__STACKSIZE__=0x600 -m build/we64.map -o $@ $(SOURCES) $(IP65LIB) $(DRIVERLIB)

we-c64.prg: build/we64.prg
	cp build/we64.prg $@

build/we64.d64: build/we64.prg
	rm -f $@
	$(C1541) -format "we64,64" d64 $@ -write build/we64.prg we64 >/dev/null

test:
	sh scripts/ci-test.sh

check: test

vice: build/we64.d64
	x64sc -autostart build/we64.d64

clean:
	rm -rf build we-c64.prg
	$(MAKE) -C $(IP65)/ip65 clean
	$(MAKE) -C $(IP65)/drivers clean
