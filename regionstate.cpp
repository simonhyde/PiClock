#include "regionstate.h"
#include "overallstate.h"


RegionState::RegionState()
: m_bRecalcReqd(true), m_bAnalogueClock(true),m_bAnalogueClockLocal(true),m_bDigitalClockUTC(false),m_bDigitalClockLocal(true),m_bDate(true), m_bDateLocal(true), m_AnalogueNumbers(1)
{}

bool RegionState::LayoutEqual(std::shared_ptr<RegionState> pOther) const
{
	return pOther && LayoutEqual(*pOther);
}
bool RegionState::LayoutEqual(const RegionState & other) const
{
	return  other.m_bAnalogueClock        == m_bAnalogueClock &&
		other.m_bAnalogueClockLocal   == m_bAnalogueClockLocal   &&
		other.m_bDigitalClockUTC      == m_bDigitalClockUTC      &&
		other.m_bDigitalClockLocal    == m_bDigitalClockLocal    &&
		other.m_bDate                 == m_bDate                 &&
		other.m_bDateLocal            == m_bDateLocal;
}
bool RegionState::RecalcDimensions(NVGcontext* vg, const struct tm & utc, const struct tm & local, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus, bool bDigitalClockPrefix)
{
	auto day = (m_bDateLocal? local:utc).tm_mday;
	bool bBoxLandscape = width > height;
	if(m_bRecalcReqd || lastDayOfMonth != day || prev_width != width || prev_height != height)
	{
		prev_width = width;
		prev_height = height;
		//Firstly the status text, which will always be in the bottom left corner
		m_statusTextSize = std::min(displayWidth,displayHeight)/130.0f;
		auto statusTextHeight = bStatus? TextHeight(vg, FONT_PROP, m_statusTextSize) : 0.0f;
		//Start off by assuming the text is the full width, this will change if we have an analogue clock and we're in landscape
		auto textWidth = width;
		
		if(m_bAnalogueClock)
		{
			auto dim = std::min(width, height);
			if(bBoxLandscape)
				textWidth -= dim;
			//Always in the top right corner
			m_boxAnalogue = DisplayBox(width - dim, dim, dim, dim);
			m_hours_x = std::make_shared<std::map<int, VGfloat> >();
			m_hours_y = std::make_shared<std::map<int, VGfloat> >();
			int i;
			VGfloat factor = 9.0f/20.0f;
			if(m_AnalogueNumbers == 2)
				factor = 7.0f/20.0f;
			for(i = 0; i < 12; i++)
			{
				(*m_hours_y)[i] = -cosf(M_PI*i/6.0f) * dim*factor;
				//(*m_hours_y)[i] -= dim/30.0f;
				(*m_hours_x)[i] = sinf(M_PI*i/6.0f) * dim*factor;
			}
		}
		else
		{
			m_boxAnalogue.Zero();
			m_hours_x.reset();
			m_hours_y.reset();
		}
		float statusHeight = statusTextHeight*3.2f;
		m_boxStatus = DisplayBox(0, height, textWidth, statusHeight);
		VGfloat textTop = 0;
#define textHeight (height - textTop - m_boxStatus.h)
		if(!bBoxLandscape)
			textTop += m_boxAnalogue.h;
		VGfloat digitalColWidth = textWidth;
		VGfloat digitalHeight = 0;
		if(m_bDigitalClockUTC || m_bDigitalClockLocal)
		{
			std::string clockStr = "99:99:99";
#if FRAMES
			clockStr = clockStr + ":99";
#endif
			if(bDigitalClockPrefix)
				clockStr = "TOD " + clockStr;
			m_digitalPointSize = MaxPointSize(vg, digitalColWidth*.95f, textHeight, clockStr, FONT_MONO);
			digitalHeight = TextHeight(vg, FONT_MONO, m_digitalPointSize)*1.1f;
			DisplayBox topDigital(0, textTop + digitalHeight, digitalColWidth, digitalHeight);
			textTop += digitalHeight;
			if(m_bDigitalClockUTC && m_bDigitalClockLocal)
			{
				DisplayBox bottomDigital(0, textTop + digitalHeight, digitalColWidth, digitalHeight);
				textTop += digitalHeight;
				//Try to make digital clock below analogue clock the same
				if(m_bAnalogueClockLocal)
				{
					m_boxLocal = topDigital;
					m_boxUTC = bottomDigital;
				}
				else
				{
					m_boxUTC = topDigital;
					m_boxLocal = bottomDigital;
				}
			}
			else
				m_boxUTC = m_boxLocal = topDigital;
		}
		else
		{
			m_digitalPointSize = 0;
			m_boxUTC.Zero(); m_boxLocal.Zero();
		}
		//Right, digital clocks done, move onto the date
		if(m_bDate)
		{
			auto dateStr = FormatDate(m_bDateLocal? local:utc);
			m_datePointSize = MaxPointSize(vg, digitalColWidth*.9f, textHeight, dateStr, FONT_PROP);
			auto dateHeight = TextHeight(vg, FONT_PROP, m_datePointSize);
			m_boxDate = DisplayBox(0, textTop + dateHeight *1.1, digitalColWidth, dateHeight*1.2);

		}
		else
		{
			m_datePointSize = 0;
			m_boxDate.Zero();
		}
		textTop += m_boxDate.h;
		m_boxTallies = DisplayBox(0,textTop + textHeight, textWidth, textHeight);
		m_bRecalcReqd = false;
		lastDayOfMonth = day;
		return true;
	}
	return false;
}

void RegionState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLayout> &pMsg)
{
	UpdateFromMessage(*pMsg);
}

void RegionState::UpdateFromMessage(const ClockMsg_SetLayout &msg)
{
	bool newVal;
#define UPDATE_VAL(val,param) newVal = msg.param; \
				m_bRecalcReqd = m_bRecalcReqd || ((val) != newVal); \
				(val) = newVal;
	UPDATE_VAL(m_bAnalogueClock,      bAnalogueClock)
	UPDATE_VAL(m_bAnalogueClockLocal, bAnalogueClockLocal)
	UPDATE_VAL(m_bDigitalClockUTC,    bDigitalClockUTC)
	UPDATE_VAL(m_bDigitalClockLocal,  bDigitalClockLocal)
	UPDATE_VAL(m_bDate,               bDate)
	UPDATE_VAL(m_bDateLocal,          bDateLocal)
	bool numbersPresent = m_AnalogueNumbers != 0;
	bool numbersOutside = m_AnalogueNumbers != 2;
	UPDATE_VAL(numbersPresent,	bNumbersPresent);
	UPDATE_VAL(numbersOutside,	bNumbersOutside);
#undef UPDATE_VAL
	m_AnalogueNumbers = numbersPresent? (numbersOutside? 1 : 2)
						: 0;
}

bool RegionState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLocation> &pMsg)
{
	return UpdateFromMessage(*pMsg);
}
bool RegionState::UpdateFromMessage(const ClockMsg_SetLocation &msg)
{
	VGfloat newVal;
	bool changed = false;
#define UPDATE_VAL(val,param) newVal = msg.param; \
				changed = changed || ((val) != newVal); \
				(val) = newVal;
	UPDATE_VAL(m_x,		x)
	UPDATE_VAL(m_y,		y)
	UPDATE_VAL(m_width,	width)
	UPDATE_VAL(m_height,	height)
#undef UPDATE_VAL
	m_bRecalcReqd = m_bRecalcReqd || changed;
	return changed;
}

bool RegionState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFontSizeZones> &pMsg)
{
	return UpdateFromMessage(*pMsg);
}

bool RegionState::UpdateFromMessage(const ClockMsg_SetFontSizeZones &msg)
{
	if(!m_size_zones || ( *(msg.pData) != *(m_size_zones)))
	{
		m_size_zones = msg.pData;
		return true;
	}
	return false;
}

void RegionState::SetDefaultZone(const std::string &def)
{
	m_default_zone = def;
}

const std::string & RegionState::GetZone(int row, int col)
{
	if(m_size_zones)
	{
		const std::string *pRet;
		const auto & size_zones = *m_size_zones;
		if((row < (int)size_zones.size())
			&& (col < (int)size_zones[row].size())
			&& !(pRet = &(size_zones[row][col]))->empty())
		{
			return *pRet;
		}
	}
	return m_default_zone;
}

//Simple accessor methods
bool RegionState::Rotate()
{
	return m_bRotationReqd;
}
bool RegionState::AnalogueClock(DisplayBox & dBox, bool &bLocal, std::shared_ptr<const std::map<int, VGfloat> > &hours_x, std::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers)
{
	dBox = m_boxAnalogue;
	bLocal = m_bAnalogueClockLocal;
	hours_x = m_hours_x;
	hours_y = m_hours_y;
	numbers = m_AnalogueNumbers;
	return m_bAnalogueClock;
}

bool RegionState::Date(DisplayBox & dBox, bool &bLocal, int & pointSize)
{
	dBox = m_boxDate;
	bLocal = m_bDateLocal;
	pointSize = m_datePointSize;
	return m_bDate;
}

bool RegionState::DigitalLocal(DisplayBox & dBox, int & pointSize, std::string & prefix)
{
	dBox = m_boxLocal;
	pointSize = m_digitalPointSize;
	prefix = "TOD ";
	return m_bDigitalClockLocal;
}

bool RegionState::DigitalUTC(DisplayBox & dBox, int & pointSize, std::string & prefix)
{
	dBox = m_boxUTC;
	pointSize = m_digitalPointSize;
	prefix = "UTC ";
	return m_bDigitalClockUTC;
}

DisplayBox RegionState::StatusBox(int &pointSize)
{
	pointSize = m_statusTextSize;
	return m_boxStatus;
}

DisplayBox RegionState::TallyBox()
{
	return m_boxTallies;
}

VGfloat RegionState::width() const
{
	return m_width;
}

VGfloat RegionState::height() const
{
	return m_height;
}

VGfloat RegionState::x() const
{
	return m_x;
}

VGfloat RegionState::y() const
{
	return 1.f - m_y;
}

VGfloat RegionState::top_y() const
{
	return y() - height();
}

bool RegionState::hasDigitalUTC() const
{
	return m_bDigitalClockUTC;
}

bool RegionState::hasDigitalLocal() const
{
	return m_bDigitalClockLocal;
}


bool RegionState::DigitalClockPrefix(const RegionsMap &regions)
{
	bool bUTC = false;
	bool bLocal = false;
	for(const auto & kv : regions)
	{
		bUTC = bUTC || kv.second->hasDigitalUTC();
		bLocal = bLocal || kv.second->hasDigitalLocal();
		//Check on every loop so we don't necessarily have to check every region
		if(bLocal && bUTC)
			return true;
	}
	return false;
}
std::string RegionState::FormatDate(const struct tm &data)
{
	char buf[512];
	strftime(buf, sizeof(buf), "%a %d %b %Y", &data);
	return buf;
}

std::string RegionState::FormatTime(const struct tm &data, time_t usecs)
{
	char buf[512];
#if FRAMES
       sprintf(buf,"%02d:%02d:%02d:%02ld",data.tm_hour,data.tm_min,data.tm_sec,usec *FPS/1000000);
#else
       sprintf(buf,"%02d:%02d:%02d",data.tm_hour,data.tm_min,data.tm_sec);
#endif
    return buf;
}

void RegionState::DrawTally(NVGcontext* vg, DisplayBox &dbTally, const int row, const int col, OverallState & global, const timeval &tval)
{
	auto curTally = TD.displays[row][col];
	if(!curTally)
			return;
	auto text = curTally->Text(tval);
	auto iter = global.Images.end();
	int img_handle = 0;
	if(text && (iter = global.Images.find(*text)) != global.Images.end() && iter->second.IsValid())
	{
		int w = (int)dbTally.w;
		int h = (int)dbTally.h;
		img_handle = iter->second.GetImage(vg, w, h, *text);
		if(img_handle != 0)
			drawimage(vg, dbTally.x, dbTally.top_y(), w, h, img_handle);
		else
			text = std::shared_ptr<std::string>();
	}
	if(img_handle == 0)
	{
		curTally->BG(tval)->Fill(vg);
		dbTally.Roundrect(vg, dbTally.h/9.8f);
		curTally->FG(tval)->Fill(vg);
		const auto & zone = GetZone(row,col);
		dbTally.TextMid(vg, text, FONT(curTally->IsMonoSpaced()), global.TextSizes[zone], global.LabelSizes[zone], curTally->Label(tval));
	}

}

void RegionState::DrawTallies(NVGcontext * vg, OverallState &global, const timeval & tval)
{
	if(TD.nRows > 0)
	{
		DisplayBox db = TallyBox();
		VGfloat row_height = (db.h)/((VGfloat)TD.nRows);
		VGfloat buffer = row_height*.02f;
		VGfloat col_width_default = db.w/((VGfloat)TD.nCols_default);
		//float y_offset = height/10.0f;
		for(int row = 0; row < TD.nRows; row++)
		{
			VGfloat base_y = db.y - ((VGfloat)row)*row_height;
			int col_count = TD.nCols[row];
			VGfloat col_width = col_width_default;
			if(col_count <= 0)
				col_count = TD.nCols_default;
			else
				col_width = db.w/((VGfloat)col_count);
			for(int col = 0; col < col_count; col++)
			{
				DisplayBox dbTally(((VGfloat)col)*col_width + db.w/100.0f, base_y, col_width - buffer, row_height - buffer);
				DrawTally(vg, dbTally, row, col, global, tval);
			}
		}
	}
}


