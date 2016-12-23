// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include "ntpstat/ntpstat.h"
//1 == piface digital
#define GPI_MODE	1

#if GPI_MODE == 1
#include "pifacedigital.h"
#endif

#define FPS 25
#define FRAMES 0
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000



void * ntp_check_thread(void * arg)
{
	ntpstate_t * data = (ntpstate_t *)arg;
	while(1)
	{
		get_ntp_state(data);
		sleep(1);
	}
}

int main() {
	int iwidth, iheight;
	fd_set rfds;
	struct timeval timeout;
	int i;
	VGfloat hours_x[12];
	VGfloat hours_y[12];
	timeout.tv_usec = timeout.tv_sec = 0;
#if GPI_MODE == 1
	pifacedigital_open(0);
#endif
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);
	

	init(&iwidth, &iheight);					// Graphics initialization

	struct timeval tval;
	struct tm tm_now;

	VGfloat offset = iheight*.1f;
	VGfloat width = iwidth - offset;
	VGfloat height = iheight - offset;

	VGfloat clock_width = height;
	VGfloat text_width = width - clock_width;

	for(i = 0; i < 12; i++)
	{
		hours_x[i] = cosf(M_PI*i/6.0f) * height*9.0f/20.0f;
		hours_x[i] -= height/30.0f;
		hours_y[i] = sinf(M_PI*i/6.0f) * height*9.0f/20.0f;
	}
	srand(time(NULL));
	int vrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	int hrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	VGfloat h_offset_pos = 0;
	VGfloat v_offset_pos = 0;
	long prev_sec = 0;
	ntpstate_t ntp_state_data;
	init_ntp_state();
	get_ntp_state(&ntp_state_data);
	pthread_t ntp_thread;
	pthread_attr_t ntp_attr;
	pthread_attr_init(&ntp_attr);
	pthread_create(&ntp_thread, &ntp_attr, &ntp_check_thread, &ntp_state_data);
	while(1)
	{
#if GPI_MODE == 1
		uint8_t gpis = pifacedigital_read_reg(INPUT,0);
#endif
		int i;
		char buf[256];
		gettimeofday(&tval, NULL);
		localtime_r(&tval.tv_sec, &tm_now);
		Start(iwidth, iheight);					// Start the picture
		Background(0, 0, 0);					// Black background
		Fill(255, 255, 255, 1);					// white text
		Stroke(255, 255, 255, 1);				// White strokes
		if(tval.tv_sec != prev_sec)
		{
			prev_sec =  tval.tv_sec;
			h_offset_pos = abs(prev_sec%(hrate*2) - hrate)*offset/hrate;
			v_offset_pos = abs(prev_sec%(vrate*2) - vrate)*offset/vrate;
		}
		//Write out the text
		Translate(h_offset_pos, v_offset_pos);
#if FRAMES
		sprintf(buf,"%02d:%02d:%02d:%02ld",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec,tval.tv_usec *FPS/1000000);
		TextMid(text_width / 2.0f, height * 8.0f/ 10.0f, buf, MonoTypeface, text_width / 10.0f);	// Timecode
#else
		sprintf(buf,"%02d:%02d:%02d",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
		TextMid(text_width / 2.0f, height * 8.0f/ 10.0f, buf, MonoTypeface, text_width / 7.0f);	// Timecode
#endif
		strftime(buf, sizeof(buf), "%A", &tm_now);
		TextMid(text_width / 2.0f, height * 7.0f/10.0f, buf, SerifTypeface, text_width/15.0f);
		strftime(buf, sizeof(buf), "%d %b %Y", &tm_now);
		TextMid(text_width / 2.0f, height * 6.0f/10.0f, buf, SerifTypeface, text_width/15.0f);
#ifdef GPI_MODE
		uint8_t colour_weight = (gpis & 1) ? 100:255;
		Fill(colour_weight,colour_weight*0.55,0,1);
		Roundrect(text_width/100.0f,height*0.5/10.0f,text_width *.98f,height*2.0f/10.0f,width/100.0f, height/100.0f);
		uint8_t fill_weight = (gpis & 1)? 100:255;
		Fill(fill_weight,fill_weight,fill_weight, 1);
		TextMid(text_width /2.0f, height *1.0f/10.0f, "Mic Live", SerifTypeface, text_width/10.0f);
		colour_weight = (gpis & 2) ? 100:255;
		Fill(colour_weight,0,0,1);
		Roundrect(text_width/100.0f,height*3.0/10.0f,text_width *.98f,height*2.0f/10.0f,width/100.0f, height/100.0f);
		fill_weight = (gpis & 2)? 100:255;
		Fill(fill_weight,fill_weight,fill_weight, 1);
		TextMid(text_width /2.0f, height *3.5f/10.0f, "On Air", SerifTypeface, text_width/10.0f);
		Fill(255, 255, 255, 1);
#endif
		Text(0,0, "Press Ctrl+C to quit", SerifTypeface, text_width / 150.0f);
		const char * sync_text;
		if(ntp_state_data.status == 0)
		{
			Fill(0,100,0,1);
			sync_text = "Synchronised";
		}
		else
		{
			/* flash between red and purple once a second */
			Fill(120,0,(tval.tv_sec %2)*120,1);
			if(ntp_state_data.status == 1)
				sync_text = "Synchronising..";
			else
				sync_text = "Unknown Synch!";
		}
		//Draw a box around NTP status
		Rect(width - clock_width*.2, 0, clock_width*.2, height*.05 );
		Fill(255,255,255,1);
		//2 bits of text
		TextMid(width - clock_width*.1, height/140.0f, sync_text, SerifTypeface, height/70.0f);
		TextMid(width - clock_width*.1, height*4.0f/140.0f, "NTP-Derived Clock", SerifTypeface, height/70.0f);
		Translate(text_width + clock_width/2.0f, height/2.0f);
		//Write out hour labels around edge of clock
		for(i = 0; i < 12; i++)
		{
			char buf[5];
			sprintf(buf, "%d", i? i:12);
			TextMid(hours_y[i],hours_x[i], buf, SansTypeface, height/15.0f);
		}
		//Go around drawing dashes around edge of clock
		StrokeWidth(clock_width/100.0f);
		VGfloat start = height *7.5f/20.0f;
		VGfloat end_short = height *6.7f/20.0f;
		VGfloat end_long = height *6.3f/20.0f;
#ifndef NO_COLOUR_CHANGE
		Stroke(255,0,0,1);
#endif
		//As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
		for(i = 0; i < 60; i++)
		{
			if((i %5) == 0)
				Line(0, start, 0, end_long);
			else
				Line(0, start, 0, end_short);
			Rotate(-6);
#ifndef NO_COLOUR_CHANGE
			//Fade red slowly out over first second
			if(i == tm_now.tm_sec)
			{
				if(i < 1)
				{
					//Example to fade over 2 seconds...
					//VGfloat fade = 128.0f * tm_now.tm_sec +  127.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
					VGfloat fade = 255.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
					Stroke(255, fade, fade, 1);
				}
				else
					Stroke(255,255,255,1);
			}
#endif
		}
		//Again, rotate co-ordinate space so we're just drawing an upright line every time...
		StrokeWidth(clock_width/200.0f);
		//VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
		VGfloat sec_rotation = -(6.0f * tm_now.tm_sec);
		if(tval.tv_usec > MOVE_HAND_AT)
			sec_rotation -= ((VGfloat)(tval.tv_usec - MOVE_HAND_AT)*6.0f/100000.0f);
		VGfloat min_rotation = -6.0f *tm_now.tm_min + sec_rotation/60.0f;
		VGfloat hour_rotation = -30.0f *tm_now.tm_hour + min_rotation/12.0f;
		Rotate(hour_rotation);
		Line(0,0,0,height/4.0f); /* half-length hour hand */
		Rotate(min_rotation - hour_rotation);
		Line(0,0,0,height/2.0f); /* minute hand */
		Rotate(sec_rotation - min_rotation);
		Stroke(255,0,0,1);
		Line(0,0,0,height/2.0f); /* second hand */
		Rotate(-sec_rotation);
		//Now draw some dots for seconds...
#ifdef SECOND_DOTS
		StrokeWidth(clock_width/100.0f);
		Stroke(0,0,255,1);
		VGfloat pos = height*7.9f/20.0f;
		for(i = 0; i < 60; i++)
		{
			if(i <= tm_now.tm_sec)
			{
				Line(0, pos, 0, pos -10);
			}
			Rotate(-6);
		}
#endif

		End();						   			// End the picture
	}

	finish();					            // Graphics cleanup
	exit(0);
}
