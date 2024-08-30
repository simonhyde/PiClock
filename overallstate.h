#ifndef __PICLOCK_OVERALLSTATE_H_INCLUDED
#define __PICLOCK_OVERALLSTATE_H_INCLUDED

#include <memory>
#include "piclock_messages.h"
#include "regionstate.h"
#include "imagescaling.h"

class OverallState
{
public:
	bool RotationReqd(VGfloat width, VGfloat height) const;

	bool Landscape() const;

	bool ScreenSaver() const;

	const std::string & FontTally() const;
	const std::string & FontTally(bool bIsDigitalClock) const;
	const std::string & FontTallyLabel() const;
	const std::string & FontStatus() const;
	const std::string & FontDigital() const;
	const std::string & FontDate() const;
	const std::string & FontHours() const;

	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetGlobal> &pMsg);
	void UpdateFromMessage(const ClockMsg_SetGlobal &message);
	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFonts> &pMsg);
	bool UpdateFromMessage(const ClockMsg_SetFonts &message);

	void SetLandscape(bool bLandscape);

        void NvgInit(NVGcontext *vg);

    RegionsMap Regions;

    ImagesMap Images;

    std::map<std::string, int> TextSizes;
	std::map<std::string, int> LabelSizes;

    bool HandleClockMessages(NVGcontext *vg, std::queue<std::shared_ptr<ClockMsg> > &msgs, struct timeval & tvCur);


private:
    void resetFonts(NVGcontext *vg, const std::string &remove_font);
    bool UpdateRegionCount(int newCount);
	bool updateFont(std::string & target, const std::string & newVal, const std::string & defaultVal);
	bool m_bLandscape = true;
	bool m_bScreenSaver = true;
	std::string font_Tally = DEFAULT_FONT_TALLY;
	std::string font_TallyLabel = DEFAULT_FONT_TALLYLABEL;
	std::string font_Status = DEFAULT_FONT_TALLY;
	std::string font_Digital =  DEFAULT_FONT_DIGITAL;
	std::string font_Date = DEFAULT_FONT_DATE;
	std::string font_Hours = DEFAULT_FONT_HOURS;
	std::map<std::string, std::string> FontData;
};

#endif
