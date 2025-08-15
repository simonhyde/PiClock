#include "overallstate.h"
#include "countdownclock.h"
#include "globals.h"
#include "gpio.h"
#include <set>

bool OverallState::RotationReqd(VGfloat width, VGfloat height) const
{
    if(m_bLandscape)
        return width < height;
    else
        return width > height;
}

bool OverallState::Landscape() const
{
    return m_bLandscape;
}

bool OverallState::ScreenSaver() const
{
    return m_bScreenSaver;
}
const std::string & OverallState::FontTally() const
{
	return font_Tally;
}
const std::string & OverallState::FontTally(bool bIsDigitalClock) const
{
	return bIsDigitalClock? font_Digital: font_Tally;
}
const std::string & OverallState::FontTallyLabel() const
{
	return font_TallyLabel;
}

const std::string & OverallState::FontStatus() const
{
	return font_Status;
}
const std::string & OverallState::FontDigital() const
{
	return font_Digital;
}
const std::string & OverallState::FontDate() const
{
	return font_Date;
}
const std::string & OverallState::FontHours() const
{
	return font_Hours;
}


void OverallState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetGlobal> &pMsg)
{
    UpdateFromMessage(*pMsg);
}
bool OverallState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFonts> &pMsg)
{
    return UpdateFromMessage(*pMsg);
}
void OverallState::UpdateFromMessage(const ClockMsg_SetGlobal &message)
{
    m_bLandscape = message.bLandscape;
    m_bScreenSaver = message.bScreenSaver;
}

bool OverallState::updateFont(std::string & target, const std::string & newVal, const std::string & defaultVal)
{
	if(newVal.size() > 0 && FontData.find(newVal) != FontData.end())
	{
		if(target == newVal)
			return false;
		target = newVal;
		return true;
	}
	if(target == defaultVal)
		return false;
	target = defaultVal;
	return true;
}

void OverallState::resetFonts(NVGcontext *vg, const std::string &remove_font)
{
    nvgClearFonts(vg);
    if(remove_font.size() > 0)
    {
	FontData.erase(remove_font);
    }
    else
    {
	FontData.clear();
    }
    
    //Standard default fonts
    nvgCreateFont(vg, FONT_SERIF,"/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf");
    nvgCreateFont(vg, FONT_SANS,"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    nvgCreateFont(vg, FONT_MONO,"/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");

    //Fonts that weren't removed
    for(const auto & pair : FontData)
    {
        nvgCreateFontMem(vg, pair.first.c_str(), (unsigned char *)pair.second.data(), pair.second.size(), 0);
    }
}

void OverallState::NvgInit(NVGcontext *vg)
{
    resetFonts(vg,std::string());
}

bool OverallState::UpdateFromMessage(const ClockMsg_SetFonts &message)
{
	//Use bitwise or to force full evaluation
	bool bChanged = updateFont(font_Tally, message.tally, DEFAULT_FONT_TALLY);
	bChanged = updateFont(font_TallyLabel, message.tally_label, font_Tally) || bChanged;
	bChanged = updateFont(font_Status, message.status, font_Tally) || bChanged;
	bChanged = updateFont(font_Digital, message.digital, DEFAULT_FONT_DIGITAL) || bChanged;
	bChanged = updateFont(font_Date, message.date, DEFAULT_FONT_DATE) || bChanged;
	bChanged = updateFont(font_Hours, message.hours, DEFAULT_FONT_HOURS) || bChanged;
	return bChanged;
}


void OverallState::SetLandscape(bool bLandscape)
{
    m_bLandscape = bLandscape;
}


bool OverallState::HandleClockMessages(NVGcontext *vg, std::queue<std::shared_ptr<ClockMsg> > &msgs, struct timeval & tvCur)
{
	bool bSizeChanged = false;
	while(!msgs.empty())
	{
		auto pMsg = msgs.front();
		msgs.pop();

		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetGPO>(pMsg))
		{
			write_gpo(castCmd->gpoIndex, castCmd->bValue);
			continue;
		}

		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetGlobal>(pMsg))
		{
			UpdateFromMessage(castCmd);
			continue;
		}
		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetFonts>(pMsg))
		{
			if(UpdateFromMessage(castCmd))
			{
				for(auto &item :Regions)
				{
					item.second->ForceRecalc();
				}
				bSizeChanged = true;
			}
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
				if(UpdateRegionCount(1))
				{
					bSizeChanged = true;
				}
				bMaxRegion = true;
			}
		}
		if(!Regions[regionIndex])
		{
			Regions[regionIndex] = std::make_shared<RegionState>();
		}
		std::shared_ptr<RegionState> pRS = Regions[regionIndex];

		if(bMaxRegion && (pRS->x() != 0.0f || pRS->y() != 1.0f || pRS->width() != 1.0f || pRS->height() != 1.0f))
		{
			bSizeChanged = true;
			pRS->UpdateFromMessage(std::make_shared<ClockMsg_SetLocation>(std::make_shared<int>(regionIndex), "SETLOCATION:0:0:1:1"));
		}

		if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_ClearImages>(pMsg))
		{
			Images.clear();
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_ClearFonts>(pMsg))
		{
			resetFonts(vg, std::string());
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_StoreImage>(pMsg))
		{
			if(!Images[castCmd->name].IsSameSource(castCmd->pSourceBlob))
				Images[castCmd->name] = ScalingImage(castCmd->pParsedImage, castCmd->pSourceBlob);
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_StoreFont>(pMsg))
	        {
			const auto iter = FontData.find(castCmd->name);
			if(iter != FontData.end())
			{
				//If it's identical to the font we already have, ignore it
				if(iter->second == castCmd->data)
				{
					continue;
				}
				//To delete the old font we have to reset all the fonts and not re-add it
				resetFonts(vg, castCmd->name);
			}
			FontData[castCmd->name] = castCmd->data;
			auto & data = FontData[castCmd->name];
			nvgCreateFontMem(vg, castCmd->name.c_str(), (unsigned char *)data.data(), data.size(), 0);
	        }
		else if(auto castCmd = std::dynamic_pointer_cast<ResizedImage>(pMsg))
		{
			Images[castCmd->Name].UpdateFromResize(castCmd);
		}
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetRegionCount>(pMsg))
		{
			if(UpdateRegionCount(castCmd->iCount))
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
				bool bDigitalOld = pOldState->IsDigitalClock();
				bool bDigitalNew = pNewState->IsDigitalClock();
				if(bDigitalOld != bDigitalNew)
					bSizeChanged = true;
				else if((bool)textOld != (bool)textNew)
					bSizeChanged = true;
				else if(textOld && textNew)
				{
					if(bDigitalNew)
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
		else if(auto castCmd = std::dynamic_pointer_cast<ClockMsg_SetTimezones>(pMsg))
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

bool OverallState::UpdateRegionCount(int newCount)
{
	std::set<int> removeIndices;
	for(const auto & item: Regions)
	{
		if(item.first >= newCount || item.first < 0)
			removeIndices.insert(item.first);
	}
	if(!removeIndices.empty())
	{
		for(int index: removeIndices)
		{
			Regions.erase(index);
		}
		return true;
	}
	return false;

}
