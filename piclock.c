// first OpenVG program
// Anthony Starks (ajstarks@gmail.com)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"

#define FPS 25
#define FRAMES 0

int main() {
	int width, height;
	fd_set rfds;
	struct timeval timeout;
	timeout.tv_usec = timeout.tv_sec = 0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);
	

	init(&width, &height);					// Graphics initialization

	struct timeval tval;
	struct tm tm_now;

	while(1)
	{
		char buf[256];
		gettimeofday(&tval, NULL);
		localtime_r(&tval.tv_sec, &tm_now);
		Start(width, height);					// Start the picture
		Background(0, 0, 0);					// Black background
		Fill(255, 255, 255, 1);					// White text
#if FRAMES
		sprintf(buf,"%02d:%02d:%02d:%02ld",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec,tval.tv_usec *FPS/1000000);
		TextMid(width / 2, height / 2, buf, MonoTypeface, width / 10);	// Timecode
#else
		sprintf(buf,"%02d:%02d:%02d",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
		TextMid(width / 2, height / 2, buf, MonoTypeface, width / 7);	// Timecode
#endif
		TextMid(width / 2, height / 10, "Press Ctrl+C to quit", SerifTypeface, width / 80);	// Timecode
		End();						   			// End the picture
#if !FRAMES
		usleep(1000000 - tval.tv_usec*95/100);
#endif
	}

	finish();					            // Graphics cleanup
	exit(0);
}
