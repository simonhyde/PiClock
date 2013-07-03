// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
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
	int i;
	VGfloat hours_x[12];
	VGfloat hours_y[12];
	timeout.tv_usec = timeout.tv_sec = 0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);
	

	init(&width, &height);					// Graphics initialization

	struct timeval tval;
	struct tm tm_now;

	VGfloat clock_width = height;
	VGfloat text_width = width - height;

	for(i = 0; i < 12; i++)
	{
		hours_x[i] = cosf(M_PI*i/6) * height*9/20;
		hours_x[i] -= height/35;
		hours_y[i] = sinf(M_PI*i/6) * height*9/20;
	}
	while(1)
	{
		int i;
		char buf[256];
		gettimeofday(&tval, NULL);
		localtime_r(&tval.tv_sec, &tm_now);
		Start(width, height);					// Start the picture
		Background(0, 0, 0);					// Black background
		Fill(255, 255, 255, 1);					// White text
		Stroke(255, 255, 255, 1);				// White strokes
#if FRAMES
		sprintf(buf,"%02d:%02d:%02d:%02ld",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec,tval.tv_usec *FPS/1000000);
		TextMid(text_width / 2, height * 8/ 10, buf, MonoTypeface, text_width / 10);	// Timecode
#else
		sprintf(buf,"%02d:%02d:%02d",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
		TextMid(text_width / 2, height * 8/ 10, buf, MonoTypeface, text_width / 7);	// Timecode
#endif
		TextMid(text_width / 2, height / 10, "Press Ctrl+C to quit", SerifTypeface, text_width / 80);
		Translate(text_width + clock_width/2, height/2);
		for(i = 0; i < 12; i++)
		{
			char buf[5];
			sprintf(buf, "%d", i? i:12);
			TextMid(hours_y[i],hours_x[i], buf, MonoTypeface, height/15);
		}
		StrokeWidth(10);
		for(i = 0; i < 60; i++)
		{
			if((i %5) == 0)
				Line(0, height*8/20, 0, height*6/20);
			else
				Line(0, height*8/20, 0, height*7/20);
			Rotate(6);
		}
		StrokeWidth(5);
		VGfloat sec_rotation = -(6 * tm_now.tm_sec +((VGfloat)tval.tv_usec*6/1000000));
		VGfloat min_rotation = -6 *tm_now.tm_min + sec_rotation/60;
		VGfloat hour_rotation = -30 *tm_now.tm_hour + min_rotation/12;
		Rotate(hour_rotation);
		Line(0,0,0,height/4); /* half-length hour hand */
		Rotate(min_rotation - hour_rotation);
		Line(0,0,0,height/2); /* minute hand */
		Rotate(sec_rotation - min_rotation);
		Stroke(255,0,0,1);
		Line(0,0,0,height/2); /* minute hand */
		End();						   			// End the picture
#if !FRAMES
		//usleep(1000000 - tval.tv_usec*95/100);
#endif
	}

	finish();					            // Graphics cleanup
	exit(0);
}
