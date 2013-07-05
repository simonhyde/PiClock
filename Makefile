all: piclock

location_test:	location_test.c openvg/libshapes.o openvg/oglinit.o
	gcc   -Wall -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -Iopenvg -o location_test location_test.c openvg/libshapes.o openvg/oglinit.o -L/opt/vc/lib -lGLESv2 -ljpeg
piclock:	piclock.c openvg/libshapes.o openvg/oglinit.o ntpstat.o
	gcc   -Wall -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -Iopenvg -o piclock piclock.c ntpstat.o openvg/libshapes.o openvg/oglinit.o -L/opt/vc/lib -lGLESv2 -ljpeg

ntpstat.o:
	gcc -c ntpstat/ntpstat.c -o ntpstat.o
openvg/libshapes.o:
	$(MAKE) -C openvg

openvg/oglinit.o:
	$(MAKE) -C openvg

