all: piclockOVG piclockOGL

piclock.o:	piclock.cpp blocking_tcp_client.cpp  piclock_messages.h
	gcc -O4 -Wall -I/opt/vc/include `pkg-config Magick++ --cflags` -Ilibmcp23s17/src -Ilibpifacedigital/src -I/opt/vc/include/interface/vcos/pthreads -Iopenvg -c -o piclock.o piclock.cpp

PICLOCK_DEPENDS=piclock.o ntpstat.o libmcp23s17/libmcp23s17.a libpifacedigital/libpifacedigital.a
piclockOVG:	$(PICLOCK_DEPENDS) openvg/rpiinit.o openvg/libshapes.o
	gcc -O4 -Wall -o piclockOVG piclock.o ntpstat.o openvg/libshapes.o openvg/rpiinit.o -L/opt/vc/lib -Llibmcp23s17 -Llibpifacedigital -lbrcmGLESv2 -lbrcmEGL -lbcm_host -ljpeg -lpthread -lm -lpifacedigital -lmcp23s17 -lpthread -lstdc++ -lboost_system -lboost_program_options -lssl -lcrypto -std=c++11 `pkg-config Magick++ --libs` -lb64
piclockOGL:	$(PICLOCK_DEPENDS) openvg/oglinit.o openvg/libshapesOGL.o
	gcc -O4 -Wall -o piclockOGL piclock.o ntpstat.o openvg/libshapesOGL.o openvg/oglinit.o -Lopenvg/ShivaVG/build/src -Llibmcp23s17 -Llibpifacedigital -ljpeg -lpthread -lm -lOpenVGStatic -lpifacedigital -lmcp23s17 -lpthread -lstdc++ -lboost_system -lboost_program_options -lssl -lcrypto -lOpenVGStatic -lglut -lGLEW -lGLU -lGL -std=c++11 `pkg-config Magick++ --libs` -lb64
ntpstat.o: ntpstat/ntpstat.c
	gcc -O2 -c ntpstat/ntpstat.c -o ntpstat.o
openvg/libshapes.o openvg/libshapesOGL.o openvg/rpiinit.o openvg/oglinit.o:
	$(MAKE) -C openvg

libmcp23s17/libmcp23s17.a:
	$(MAKE) -C libmcp23s17

libpifacedigital/libpifacedigital.a:
	$(MAKE) -C libpifacedigital

