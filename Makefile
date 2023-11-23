all: piclock
CPP_OBJS=piclock.o blocking_tcp_client.o control_tcp.o globals.o piclock_messages.o nvg_helpers.o tally.o tallycolour.o countdownclock.o regionstate.o displaybox.o imagescaling.o overallstate.o vectorclock.o
C_OBJS=nvg_main.o ntpstat/ntpstat.o
PICLOCK_SRCS=$(CPP_OBJS:%.o=%.cpp) $(C_OBJS:%.o:%.c)
PICLOCK_OBJECTS=$(CPP_OBJS:%=build/%) $(C_OBJS:%=build/%)
DEPS=$(PICLOCK_OBJECTS:%.o=%.d)
PICLOCK_DEPENDS=libmcp23s17/libmcp23s17.a libpifacedigital/libpifacedigital.a $(PICLOCK_OBJECTS)
ifeq ($(PKG_CONFIG),)
PKG_CONFIG := pkg-config
endif
CFLAGS= -Inanovg/src `$(PKG_CONFIG) Magick++ --cflags` -Ilibmcp23s17/src -Ilibpifacedigital/src

build/%.o:	%.cpp 
	mkdir -p $(@D)
	$(CXX) -O4 -Wall $(CFLAGS) -Wno-psabi -MMD -c -o $@ $<

build/ntpstat/ntpstat.o: ntpstat/ntpstat.c
	mkdir -p $(@D)
	$(CC) -O2 $(CFLAGS) -MMD -c -o $@ $<
build/nvg_main.o:	nvg_main.c
	mkdir -p $(@D)
	$(CC) -O4 -Wall $(CFLAGS) -MMD -c -o $@ $<

piclock: nanovg/build/libnanovg.a $(PICLOCK_DEPENDS)
	$(CC) -O4 -Wall -o piclock $(PICLOCK_DEPENDS) `$(PKG_CONFIG) --libs glfw3` -Lnanovg/build -Llibmcp23s17 -Llibpifacedigital -ljpeg -lpthread -lm -lnanovg -lpifacedigital -lmcp23s17 -lpthread -lstdc++ -lboost_system -lboost_program_options -lssl -lcrypto -lGLEW -lGLU -lGL -std=c++11 `$(PKG_CONFIG) Magick++ --libs` -lb64

nanovg/build/libnanovg.a: nanovg/src/nanovg.c nanovg/build/Makefile
	$(MAKE) -C nanovg/build config=release nanovg

nanovg/build/Makefile: nanovg/premake4.lua
	cd nanovg && premake4 gmake

libmcp23s17/libmcp23s17.a:
	$(MAKE) -C libmcp23s17

libpifacedigital/libpifacedigital.a:
	$(MAKE) -C libpifacedigital

clean:
	rm -f piclock piclockNVG piclockOVG piclockOGL $(PICLOCK_OBJECTS) $(DEPS)

distclean: clean
	make -C libmcp23s17 distclean
	make -C libpifacedigital distclean
	cd nanovg && premake4 clean

ifeq ($(PREFIX),)
PREFIX := /usr
endif

install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 piclock $(DESTDIR)$(PREFIX)/bin/
	install -m 755 piclock-noblank-wrapper $(DESTDIR)$(PREFIX)/bin/
	install -m 755 piclock-startx-wrapper $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)/etc/
	install -m 644 piclock.cfg $(DESTDIR)/etc/

-include $(DEPS)
