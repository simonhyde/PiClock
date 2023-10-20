#ifndef __PICLOCK_OVERALLSTATE_H_INCLUDED
#define __PICLOCK_OVERALLSTATE_H_INCLUDED

#include <memory>
#include "piclock_messages.h"
#include "regionstate.h"
#include "imagescaling.h"

class OverallState
{
public:
	bool RotationReqd(VGfloat width, VGfloat height);

	bool Landscape();

	bool ScreenSaver();
	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetGlobal> &pMsg);
	void UpdateFromMessage(const ClockMsg_SetGlobal &message);

	void SetLandscape(bool bLandscape);

    RegionsMap Regions;

    ImagesMap Images;
	
    std::map<std::string, int> TextSizes;
	std::map<std::string, int> LabelSizes;

    bool HandleClockMessages(std::queue<std::shared_ptr<ClockMsg> > &msgs, struct timeval & tvCur);


private:
    bool UpdateRegionCount(int newCount);
	bool m_bLandscape = true;
	bool m_bScreenSaver = true;
};

#endif