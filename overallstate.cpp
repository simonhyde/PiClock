#include "overallstate.h"
#include "countdownclock.h"
#include "globals.h"
#include <set>

bool OverallState::RotationReqd(VGfloat width, VGfloat height)
{
    if(m_bLandscape)
        return width < height;
    else
        return width > height;
}

bool OverallState::Landscape()
{
    return m_bLandscape;
}

bool OverallState::ScreenSaver()
{
    return m_bScreenSaver;
}
void OverallState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetGlobal> &pMsg)
{
    UpdateFromMessage(*pMsg);
}
void OverallState::UpdateFromMessage(const ClockMsg_SetGlobal &message)
{
    m_bLandscape = message.bLandscape;
    m_bScreenSaver = message.bScreenSaver;
}

void OverallState::SetLandscape(bool bLandscape)
{
    m_bLandscape = bLandscape;
}


bool handle_clock_messages(std::queue<std::shared_ptr<ClockMsg> > &msgs, RegionsMap & regions, ImagesMap & images, struct timeval & tvCur)
{
	bool bSizeChanged = false;
	while(!msgs.empty())
	{
		auto pMsg = msgs.front();
		msgs.pop();
		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetGlobal>(pMsg))
		{
			globalState.UpdateFromMessage(castCmd);
			continue;
		}

		int regionIndex = 0;
		bool bMaxRegion = false;
#define UPDATE_CHECK(target, source)	{ auto tmp = (source); bSizeChanged = bSizeChanged || ((target) != tmp); (target) = tmp;}
		if(auto regionCmd = std::dynamic_pointer_cast<ClockMsg_Region>(pMsg))
		{
			if(regionCmd->bHasRegionIndex)
				regionIndex = regionCmd->regionIndex;
			else
			{
				if(UpdateCount(regions, 1))
				{
					bSizeChanged = true;
				}
				bMaxRegion = true;
			}
		}
		if(!regions[regionIndex])
		{
			regions[regionIndex] = std::make_shared<RegionState>();
		}
		std::shared_ptr<RegionState> pRS = regions[regionIndex];

		if(bMaxRegion && (pRS->x() != 0.0f || pRS->y() != 1.0f || pRS->width() != 1.0f || pRS->height() != 1.0f))
		{
			bSizeChanged = true;
			pRS->UpdateFromMessage(std::make_shared<ClockMsg_SetLocation>(std::make_shared<int>(regionIndex), "SETLOCATION:0:0:1:1"));
		}

		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_ClearImages>(pMsg))
		{
			images.clear();
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_StoreImage>(pMsg))
		{
			if(!images[castCmd->name].IsSameSource(castCmd->pSourceBlob))
				images[castCmd->name] = ScalingImage(castCmd->pParsedImage, castCmd->pSourceBlob);
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ResizedImage>(pMsg))
		{
			images[castCmd->Name].UpdateFromResize(castCmd);
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetRegionCount>(pMsg))
		{
			if(UpdateCount(regions, castCmd->iCount))
			{
				bSizeChanged = true;
			}
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetSize>(pMsg))
		{
			UPDATE_CHECK(pRS->TD.nRows, castCmd->iRows)
			UPDATE_CHECK(pRS->TD.nCols_default, castCmd->iCols)
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetLocation>(pMsg))
		{
			if(pRS->UpdateFromMessage(castCmd))
			{
				bSizeChanged = true;
			}
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetRow>(pMsg))
		{
			UPDATE_CHECK(pRS->TD.nCols[castCmd->iRow], castCmd->iCols);
		}
		else if(auto indCmd = std::dynamic_pointer_cast<ClockMsg_SetIndicator>(pMsg))
		{
			std::shared_ptr<TallyState> pNewState;
			int row = indCmd->iRow;
			int col = indCmd->iCol;
			auto pOldState = pRS->TD.displays[row][col];
			if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetCountdown>(pMsg))
			{
				pNewState = std::make_shared<CountdownClock>(castCmd);
			}
			else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetTally>(pMsg))
			{
				pNewState = std::make_shared<SimpleTallyState>(castCmd, pOldState);
			}
			if(!pNewState || !pOldState)
				bSizeChanged = true;
			else if(!bSizeChanged)
			{
				auto textOld = pOldState->Text(tvCur);
				auto textNew = pNewState->Text(tvCur);
				bool bMonoOld = pOldState->IsMonoSpaced();
				bool bMonoNew = pNewState->IsMonoSpaced();
				if(bMonoOld != bMonoNew)
					bSizeChanged = true;
				else if((bool)textOld != (bool)textNew)
					bSizeChanged = true;
				else if(textOld && textNew)
				{
					if(bMonoNew)
					{
						if(textOld->size() != textNew->size())
							bSizeChanged = true;
					}
					else if(*textOld != *textNew)
						bSizeChanged = true;
				}
			}
			pRS->TD.displays[row][col] = pNewState;
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetLabel>(pMsg))
		{
			int row = castCmd->iRow;
			int col = castCmd->iCol;
			if(pRS->TD.displays[row][col])
			{
				auto pNew = pRS->TD.displays[row][col]->SetLabel(castCmd->sText);
				//Returns an empty pointer if the label is the same (ie no change required)
				if(pNew)
				{
					bSizeChanged = true;
					pRS->TD.displays[row][col] = pNew;
				}
			}
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetProfile>(pMsg))
		{
			//No size calculation depends upon the setting of this string, so just store it.
			pRS->TD.sProfName = castCmd->sProfile;
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetLayout>(pMsg))
		{
			pRS->UpdateFromMessage(castCmd);
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetFontSizeZones>(pMsg))
		{
			bSizeChanged |= pRS->UpdateFromMessage(castCmd);
		}
		else
		{
			fprintf(stderr, "IMPOSSIBLE HAPPENED!! Received unknown command in message queue: %s\n",typeid(pMsg.get()).name());
		}
	}
	return bSizeChanged;
}

bool UpdateCount(RegionsMap &regions, int newCount)
{
	std::set<int> removeIndices;
	for(const auto & item: regions)
	{
		if(item.first >= newCount || item.first < 0)
			removeIndices.insert(item.first);
	}
	if(!removeIndices.empty())
	{
		for(int index: removeIndices)
		{
			regions.erase(index);
		}
		return true;
	}
	return false;

}
