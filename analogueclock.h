#ifndef __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#define __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#include "displaybox.h"
#include <memory>
#include <map>
#include <nanovg.h>
#include "imagescaling.h"

class AnalogueClockState
{
public:
	int Numbers = 1;
        bool SecondsSweep = false;
        std::string ImageClockFace, ImageClockHours, ImageClockMinutes, ImageClockSeconds;
	std::shared_ptr<std::map<int, VGfloat>> hours_x;
	std::shared_ptr<std::map<int, VGfloat>> hours_y;
        Fontinfo font_hours;
        void Draw(NVGcontext *vg, DisplayBox &db, ImagesMap &images, const tm &tm_now, const suseconds_t &usecs);
private:
        enum HandType { Hand_Second, Hand_Minute, Hand_Hour };

        void DrawFace_Vector(NVGcontext *vg, VGfloat min_dim, const tm &tm_now, VGfloat fUsecs);
        static bool TryDrawImage(NVGcontext *vg, ImagesMap &images, VGfloat min_dim, const std::string &name);
        void DrawFace(NVGcontext *vg, VGfloat min_dim, ImagesMap &images, const tm &tm_now, VGfloat fUsecs);
        void DrawHand(NVGcontext *vg, VGfloat min_dim, ImagesMap &images, HandType handType);
};
#endif
