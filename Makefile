all: piclockNVG

piclock.o:	piclock.cpp blocking_tcp_client.cpp  piclock_messages.h
	gcc -O4 -Wall -I../nanovg/src `pkg-config Magick++ --cflags` -Ilibmcp23s17/src -Ilibpifacedigital/src -I/opt/vc/include/interface/vcos/pthreads -c -o piclock.o piclock.cpp

nvg_main.o:	nvg_main.c
	gcc -O4 -Wall -I../nanovg/src -c -o nvg_main.o nvg_main.c

PICLOCK_DEPENDS=piclock.o ntpstat.o libmcp23s17/libmcp23s17.a libpifacedigital/libpifacedigital.a
piclockOVG:	$(PICLOCK_DEPENDS) openvg/rpiinit.o openvg/libshapes.o
	gcc -O4 -Wall -o piclockOVG piclock.o ntpstat.o openvg/libshapes.o openvg/rpiinit.o -L/opt/vc/lib -Llibmcp23s17 -Llibpifacedigital -lbrcmGLESv2 -lbrcmEGL -lbcm_host -ljpeg -lpthread -lm -lpifacedigital -lmcp23s17 -lpthread -lstdc++ -lboost_system -lboost_program_options -lssl -lcrypto -std=c++11 `pkg-config Magick++ --libs` -lb64
piclockNVG:	$(PICLOCK_DEPENDS) nvg_main.o nanovg/build/libnanovg.a
	gcc -O4 -Wall -o piclockNVG piclock.o ntpstat.o nvg_main.o `pkg-config --libs glfw3` -Lnanovg/build -Llibmcp23s17 -Llibpifacedigital -ljpeg -lpthread -lm -lnanovg -lpifacedigital -lmcp23s17 -lpthread -lstdc++ -lboost_system -lboost_program_options -lssl -lcrypto -lGLEW -lGLU -lGL -std=c++11 `pkg-config Magick++ --libs` -lb64
ntpstat.o: ntpstat/ntpstat.c
	gcc -O2 -c ntpstat/ntpstat.c -o ntpstat.o

nanovg/build/libnanovg.a: nanovg/src/nanovg.c nanovg/build/Makefile
	$(MAKE) -C nanovg/build config=release nanovg

nanovg/build/Makefile: nanovg/premake4.lua
	cd nanovg && premake4 gmake

libmcp23s17/libmcp23s17.a:
	$(MAKE) -C libmcp23s17

libpifacedigital/libpifacedigital.a:
	$(MAKE) -C libpifacedigital

