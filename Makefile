# WARP64U: build the C64 program, and run the host tests.
#
#   make          build/warp64u.prg and build/warp64u.d64 (cc65, ip65 from vendor/)
#   make test     the host tests (scripts/ci-test.sh)
#   make vice     start it in VICE from the disk image
#   make clean

CL65 ?= cl65
C1541 ?= c1541
IP65 = vendor/ip65
IP65LIB = $(IP65)/ip65/ip65_tcp.lib
DRIVERLIB = $(IP65)/drivers/ip65_c64.lib

SOURCES = src/main.c src/store.c src/http.c src/shelf.c src/config.c src/crc32.c src/text.c \
          src/disk_cbm.c src/net_uci.c src/net_ip65.c src/net_modem.c

.PHONY: all test vice clean

all: build/warp64u.prg build/warp64u.d64

$(IP65LIB):
	$(MAKE) -C $(IP65)/ip65 ip65_tcp.lib

$(DRIVERLIB):
	$(MAKE) -C $(IP65)/drivers ip65_c64.lib

build/warp64u.prg: $(SOURCES) src/*.h $(IP65LIB) $(DRIVERLIB)
	mkdir -p build
	$(CL65) -t c64 -O -I $(IP65)/inc -m build/warp64u.map -o $@ $(SOURCES) $(IP65LIB) $(DRIVERLIB)

build/warp64u.d64: build/warp64u.prg
	rm -f $@
	$(C1541) -format "warp64u,64" d64 $@ -write build/warp64u.prg warp64u >/dev/null

test:
	sh scripts/ci-test.sh

vice: build/warp64u.d64
	x64sc -autostart build/warp64u.d64

clean:
	rm -rf build
	$(MAKE) -C $(IP65)/ip65 clean
	$(MAKE) -C $(IP65)/drivers clean
