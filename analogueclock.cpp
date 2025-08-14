#include "analogueclock.h"
#include <nanovg.h>
#include "nvg_helpers.h"
#include "imagescaling.h"
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000
#define MOVE_HAND_AT_FLOAT  900000.0f

void AnalogueClockState::DrawFace_Vector(NVGcontext *vg, VGfloat min_dim, const date::time_of_day<std::chrono::microseconds> &now)
{
    //Switch to white/white before drawing any hour text or strokes around the edge of the clock
    nvgStrokeColor(vg, colWhite);
    nvgFillColor(vg, colWhite);

    auto tm_sec = now.seconds().count();
    VGfloat fUsecs = now.subseconds().count();

    //We'll be translated to the centre of the displaybox

    if(Numbers)
    {
        //Write out hour labels around edge of clock
        for(int i = 0; i < 12; i++)
        {
            char buf[5];
            sprintf(buf, "%d", i? i:12);
            TextMid(vg, hours_x->at(i), hours_y->at(i), buf, font_hours, min_dim/15.0f);
        }
    }
    //Go around drawing dashes around edge of clock
    nvgStrokeWidth(vg, min_dim/100.0f);
    VGfloat start, end_short, end_long;
    if(Numbers == 1)
    {
        start = min_dim *7.5f/-20.0f;
        end_short = min_dim *6.7f/-20.0f;
        end_long = min_dim *6.3f/-20.0f;
    }
    else
    {
        start = min_dim *9.5f/-20.0f;
        end_short = min_dim *8.8f/-20.0f;
        end_long = min_dim *8.4f/-20.0f;
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
        if(i == tm_sec)
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
}

bool AnalogueClockState::TryDrawImage(NVGcontext *vg, ImagesMap &images, VGfloat min_dim, const std::string &name)
{
    auto iter = images.end();
    int img_handle = 0;
    if(name.size() == 0 || (iter = images.find(name)) == images.end() || !iter->second.IsValid())
    {
	return false;
    }
    img_handle = iter->second.GetImage(vg, min_dim, min_dim, name);
    if(img_handle == 0)
	return false;
    drawimage(vg, min_dim/-2, min_dim/-2, min_dim, min_dim, img_handle);
    return true;
}

void AnalogueClockState::DrawFace(NVGcontext *vg, VGfloat min_dim, ImagesMap &images, const date::time_of_day<std::chrono::microseconds> & now)
{
    if(!TryDrawImage(vg, images, min_dim, ImageClockFace))
	DrawFace_Vector(vg, min_dim, now);
}

static void handStrokeColour(NVGcontext *vg, const std::string &str, NVGcolor defCol)
{
    if(str.size() == 7 && str[0] == '#')
    {
	TallyColour col(str);
	col.Stroke(vg);
    }
    else
    {
	nvgStrokeColor(vg, defCol);
    }
}

static void handFillColour(NVGcontext *vg, const std::string &str, NVGcolor defCol)
{
    if(str.size() == 7 && str[0] == '#')
    {
	TallyColour col(str);
	col.Fill(vg);
    }
    else
    {
	nvgFillColor(vg, defCol);
    }
}


void AnalogueClockState::DrawHand(NVGcontext *vg, VGfloat min_dim, ImagesMap &images, HandType handType)
{
    nvgStrokeWidth(vg, min_dim/200.0f);
    switch(handType)
    {
        case Hand_Second:
	    if(TryDrawImage(vg, images, min_dim, ImageClockSeconds))
		break;
            handStrokeColour(vg, ImageClockSeconds, colRed);
            Line(vg, 0, min_dim/10.0f,0,min_dim/-2.0f); /* second hand, with overhanging tail */
	    //Draw circle in centre
	    handFillColour(vg, ImageClockSeconds, colRed);
	    nvgBeginPath(vg);
	    nvgCircle(vg, 0,0,min_dim/150.0f);
	    nvgFill(vg);
            break;
        case Hand_Minute:
	    if(TryDrawImage(vg, images, min_dim, ImageClockMinutes))
		break;
            handStrokeColour(vg, ImageClockMinutes, colWhite);
            Line(vg, 0,0,0,min_dim/-2.0f); /* minute hand */
            break;
        case Hand_Hour:
	    if(TryDrawImage(vg, images, min_dim, ImageClockHours))
		break;
            handStrokeColour(vg, ImageClockHours, colWhite);
            Line(vg, 0,0,0,min_dim/-4.0f); /* half-length hour hand */
            break;
    }
}


void AnalogueClockState::Draw(NVGcontext *vg, DisplayBox &db, ImagesMap &images, const time_info & now)
{
    auto tod = date::make_time(std::chrono::floor<std::chrono::microseconds>(now).time_since_epoch());
    int usecs = tod.subseconds().count();
    VGfloat fUsecs = usecs;
    //Save current NVG state so we can move back to it at the end...
    nvgSave(vg);
    //Move to the centre of the clock
    VGfloat move_x = db.mid_x();
    VGfloat move_y = db.mid_y();
    VGfloat min_dim = std::min(db.h,db.w);
    
    nvgTranslate(vg, move_x, move_y);
    DrawFace(vg, min_dim, images, tod);
    VGfloat sec_rotation = (6.0f * tod.seconds().count());
    VGfloat sec_part = sec_rotation;
    //Take into account microseconds when calculating position of minute hand (and to minor extent hour hand), so it doesn't jump every second
    sec_part += fUsecs*6.0f/1000000.0f;
    if(SecondsSweep)
	sec_rotation = sec_part;
    else if(usecs > MOVE_HAND_AT)
        sec_rotation += (fUsecs - MOVE_HAND_AT_FLOAT)*6.0f/100000.0f;
    VGfloat min_rotation = 6.0f *tod.minutes().count() + sec_part/60.0f;
    VGfloat hour_rotation = 30.0f * (tod.hours().count() % 12) + min_rotation/12.0f;
    Rotate(vg, hour_rotation);
    DrawHand(vg, min_dim, images, Hand_Hour);
    Rotate(vg, min_rotation - hour_rotation);
    DrawHand(vg, min_dim, images, Hand_Minute);
    Rotate(vg, sec_rotation - min_rotation);
    DrawHand(vg, min_dim, images, Hand_Second);
    nvgRestore(vg);
}
