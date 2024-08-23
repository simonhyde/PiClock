#include "vectorclock.h"
#include <nanovg.h>
#include "nvg_helpers.h"
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000
#define MOVE_HAND_AT_FLOAT  900000.0f

void DrawVectorClock(NVGcontext *vg, DisplayBox &db, const std::shared_ptr<const std::map<int, VGfloat> > &hours_x, const std::shared_ptr<const std::map<int, VGfloat> > &hours_y, const int &numbers, const tm &tm_now, const suseconds_t &usecs, const Fontinfo & font_hours)
{
    VGfloat fUsecs = (VGfloat)usecs;
    //Save current NVG state so we can move back to it at the end...
    nvgSave(vg);
    //Move to the centre of the clock
    VGfloat move_x = db.mid_x();
    VGfloat move_y = db.mid_y();
    
    nvgTranslate(vg, move_x, move_y);
    nvgStrokeColor(vg, colWhite);
	nvgFillColor(vg, colWhite);

    if(numbers)
    {
        //Write out hour labels around edge of clock
        for(int i = 0; i < 12; i++)
        {
            char buf[5];
            sprintf(buf, "%d", i? i:12);
            TextMid(vg, hours_x->at(i),hours_y->at(i), buf, font_hours, db.h/15.0f);
        }
    }
    //Spin clock around 180 degrees because this code originally worked with Y co-ordinates increasing as you went up
    Rotate(vg, 180.0f);
    //Go around drawing dashes around edge of clock
    nvgStrokeWidth(vg, db.w/100.0f);
    VGfloat start, end_short, end_long;
    VGfloat min_dim = std::min(db.h,db.w);
    if(numbers == 1)
    {
        start = min_dim *7.5f/20.0f;
        end_short = min_dim *6.7f/20.0f;
        end_long = min_dim *6.3f/20.0f;
    }
    else
    {
        start = min_dim *9.5f/20.0f;
        end_short = min_dim *8.8f/20.0f;
        end_long = min_dim *8.4f/20.0f;
    }
#ifndef NO_COLOUR_CHANGE
    nvgStrokeColor(vg,colRed);
#endif
    //As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
    for(int i = 0; i < 60; i++)
    {
        if((i %5) == 0)
            Line(vg, 0, start, 0, end_long);
        else
            Line(vg, 0, start, 0, end_short);
        Rotate(vg, 6);
#ifndef NO_COLOUR_CHANGE
        //Fade red slowly out over first second
        if(i == tm_now.tm_sec)
        {
            if(i < 1)
            {
                //Example to fade over 2 seconds...
                //VGfloat fade = 0.5f * tm_now.tm_sec +  0.5f * fUsecs/1000000.0f;
                //Fade over half a second...
                VGfloat fade = fUsecs/200000.0f;
                if(fade > 1.0f)
                    fade = 1.0f;
                nvgStrokeColor(vg, nvgRGBf(1.0f, fade, fade));
            }
            else
                nvgStrokeColor(vg, colWhite);
        }
#endif
    }
    nvgStrokeColor(vg, colWhite);
    //Again, rotate co-ordinate space so we're just drawing an upright line every time...
    nvgStrokeWidth(vg, db.w/200.0f);
    //VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
    VGfloat sec_rotation = (6.0f * tm_now.tm_sec);
    VGfloat sec_part = sec_rotation;
    if(usecs > MOVE_HAND_AT)
        sec_rotation += (fUsecs - MOVE_HAND_AT_FLOAT)*6.0f/100000.0f;
    //Take into account microseconds when calculating position of minute hand (and to minor extent hour hand), so it doesn't jump every second
    sec_part += fUsecs*6.0f/1000000.0f;
    VGfloat min_rotation = 6.0f *tm_now.tm_min + sec_part/60.0f;
    VGfloat hour_rotation = 30.0f *tm_now.tm_hour + min_rotation/12.0f;
    Rotate(vg, hour_rotation);
    Line(vg, 0,0,0,min_dim/4.0f); /* half-length hour hand */
    Rotate(vg, min_rotation - hour_rotation);
    Line(vg, 0,0,0,min_dim/2.0f); /* minute hand */
    Rotate(vg, sec_rotation - min_rotation);
    nvgStrokeColor(vg, colRed);
    Line(vg, 0,-db.h/10.0f,0,min_dim/2.0f); /* second hand, with overhanging tail */
    //Draw circle in centre
    nvgFillColor(vg, colRed);
    nvgBeginPath(vg);
    nvgCircle(vg, 0,0,db.w/150.0f);
    nvgFill(vg);
    Rotate(vg, -sec_rotation -180.0f);
#ifdef SECOND_DOTS
    //Now draw some dots for seconds...
    nvgStrokeWidth(vg, db.w/100.0f);
    nvgStrokeColor(vg, colBlue);
    VGfloat pos = db.h*7.9f/20.0f;
    for(i = 0; i < 60; i++)
    {
        if(i <= tm_now.tm_sec)
        {
            Line(vg, 0, pos, 0, pos -10);
        }
        Rotate(vg, -6);
    }
#endif
    nvgRestore(vg);
}
