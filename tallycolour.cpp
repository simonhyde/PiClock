
#include "tallycolour.h"
#include <nanovg.h>

uint32_t TallyColour::R() const
{
	return (col >> 16)&0xFF;
}
uint32_t TallyColour::G() const
{
	return (col >> 8)&0xFF;
}
uint32_t TallyColour::B() const
{
	return col & 0xFF;
}
TallyColour::TallyColour(const std::string &input)
	//Support both colours with and without a leading # character
	:TallyColour(std::stoul((input.size() > 0 && input[0] == '#')? input.substr(1) : input, NULL, 16))
{
}
TallyColour::TallyColour(uint8_t r, uint8_t g, uint8_t b)
	:TallyColour( (r<<16) | (g<<8) | b )
{
}
TallyColour::TallyColour()
	:TallyColour(0) //Default to black
{
}
bool TallyColour::Equals(const TallyColour & other) const
{
	return other.col == col;
}

void TallyColour::Fill(NVGcontext *vg)
{
	nvgFillColor(vg,nvgCol);
}

void TallyColour::Stroke(NVGcontext *vg)
{
	nvgStrokeColor(vg,nvgCol);
}

TallyColour::TallyColour(const uint32_t data)
	:col(data)
{
	nvgCol = nvgRGB(R(),G(),B());
}
