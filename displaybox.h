#ifndef __PICLOCK_DISPLAYBOX_H_INCLUDED
#define __PICLOCK_DISPLAYBOX_H_INCLUDED

#include <nanovg.h>
#include <memory>
#include "nvg_helpers.h"

class DisplayBox
{
public:
	VGfloat x,y,w,h;
	DisplayBox();
	DisplayBox(VGfloat _x, VGfloat _y, VGfloat _w, VGfloat _h);
	VGfloat top_y() const;
	VGfloat mid_x() const;
	VGfloat mid_y() const;
	void TextMid(NVGcontext * vg, const std::shared_ptr<std::string> &str, const Fontinfo & font, const int pointSize, int labelSize = -1, std::shared_ptr<std::string> label = std::shared_ptr<std::string>(),  const Fontinfo & font_label = std::string());
	void TextMid(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, int labelSize = -1, std::shared_ptr<std::string> label = std::shared_ptr<std::string>(),  const Fontinfo & font_label = std::string());
	void TextMidBottom(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, std::shared_ptr<std::string> label = std::shared_ptr<std::string>(),  const Fontinfo & font_label = std::string());
	void Roundrect(NVGcontext *vg, VGfloat corner);
	void Rect(NVGcontext * vg);
	void Zero();
private:
	VGfloat m_corner;
};
#endif