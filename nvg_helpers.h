#ifndef __PICLOCK_NVG_HELPERS_H_INCLUDED
#define __PICLOCK_NVG_HELPERS_H_INCLUDED

#include <nanovg.h>
#include <string>
#include "fonts.h"

//Legacy type from old OpenVG libraries...
typedef float VGfloat;

extern void Rotate(NVGcontext *vg, float degrees);
extern void Roundrect(NVGcontext *vg, float x, float y, float w, float h, float r);
extern void Rect(NVGcontext *vg, float x, float y, float w, float h);
extern void Line(NVGcontext *vg, float x1, float y1, float x2, float y2);
extern float TextHeight(NVGcontext *vg, const Fontinfo &f, int pointsize);
extern float TextWidth(NVGcontext *vg, const char * str, const Fontinfo &f, int pointsize);
extern void TextMid(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize);
extern void Text(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize);
extern void drawimage(NVGcontext *vg, float x, float y, float w, float h, int img_handle);
extern void RotateTextClipCache();
extern void TextClip(NVGcontext *vg, float x, float y, const std::string &s, const Fontinfo &f, int pointsize, float clipwidth, const std::string &clip_str);
extern void TextMidBottom(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize);
extern int MaxPointSize(NVGcontext *vg, float width, float height, const std::string & text, const Fontinfo & f);


#endif