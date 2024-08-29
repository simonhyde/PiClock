#ifndef __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#define __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#include "displaybox.h"
#include "regionstate.h"
#include <memory>
#include <map>
extern void DrawAnalogueClock(NVGcontext *vg, DisplayBox &db, const AnalogueClockState &clockState, ImagesMap &images, const tm &tm_now, const suseconds_t &usecs, const Fontinfo & font_hours);
#endif
