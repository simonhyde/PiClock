#ifndef __PICLOCK_TALLYCOLOUR_H_INCLUDED
#define __PICLOCK_TALLYCOLOUR_H_INCLUDED

#include <nanovg.h>
#include <cstdint>
#include <string>

class TallyColour
{
public:
	uint32_t R() const;
	uint32_t G() const;
	uint32_t B() const;
	TallyColour(const std::string &input);
	TallyColour(uint8_t r, uint8_t g, uint8_t b);
	TallyColour();
	bool Equals(const TallyColour & other) const;
	void Fill(NVGcontext *vg);
	void Stroke(NVGcontext *vg);
protected:
	TallyColour(const uint32_t data);
	NVGcolor nvgCol;
	uint32_t col;
};


#endif