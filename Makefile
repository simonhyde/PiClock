all: piclock

piclock:	piclock.c openvg/libshapes.o openvg/oglinit.o
	gcc   -Wall -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -Iopenvg -o piclock piclock.c openvg/libshapes.o openvg/oglinit.o -L/opt/vc/lib -lGLESv2 -ljpeg

openvg/libshapes.o:
	$(MAKE) -C openvg

openvg/oglinit.o:
	$(MAKE) -C openvg

