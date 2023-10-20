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

private:
	bool m_bLandscape = true;
	bool m_bScreenSaver = true;
};

extern bool handle_clock_messages(std::queue<std::shared_ptr<ClockMsg> > &msgs, RegionsMap & regions, ImagesMap & images, struct timeval & tvCur);

extern bool UpdateCount(RegionsMap &regions, int newCount);

#endif