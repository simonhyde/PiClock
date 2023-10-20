#ifndef __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#define __PICLOCK_VECTOR_CLOCK_H_INCLUDED
#include "displaybox.h"
#include <memory>
#include <map>
extern void DrawVectorClock(NVGcontext *vg, DisplayBox &db, const std::shared_ptr<const std::map<int, VGfloat> > &hours_x, const std::shared_ptr<const std::map<int, VGfloat> > &hours_y, const int &numbers, const tm &tm_now, const suseconds_t &usecs);
#endif