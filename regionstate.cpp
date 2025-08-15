#include "regionstate.h"
#include "overallstate.h"
#include "analogueclock.h"
#include <algorithm>

static const date::time_zone * lookup_tz(const std::string & name)
{
    if(name == "LOCAL")
	return date::current_zone();
    return date::locate_zone(name);
}


RegionState::RegionState()
: m_bRecalcReqd(true), m_bAnalogueClock(true),m_bDate(true)
{
    tz_local = lookup_tz("LOCAL");
    tz_utc = lookup_tz("Etc/UTC");
    m_AnalogueClockZone = tz_local;
    m_DigitalClocks.push_back(ClockData(tz_local,"TOD"));
    m_DateZone = tz_local;
}

void RegionState::ForceRecalc()
{
	m_bRecalcReqd = true;
}

bool RegionState::LayoutEqual(std::shared_ptr<RegionState> pOther) const
{
	return pOther && LayoutEqual(*pOther);
}
bool RegionState::LayoutEqual(const RegionState & other) const
{
	return  other.m_bAnalogueClock        == m_bAnalogueClock &&
		other.m_AnalogueClockZone     == m_AnalogueClockZone   &&
		other.m_DigitalClocks         == m_DigitalClocks      &&
		other.m_bDate                 == m_bDate                 &&
		other.m_DateZone              == m_DateZone;
}
bool RegionState::RecalcDimensions(NVGcontext* vg, const OverallState & global, const sys_clock_data & now, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus, bool bDigitalClockPrefix)
{
	auto date_time = m_DateZone->to_local(now);
	auto date = date::year_month_day(date::floor<date::days>(date_time));
	int day = static_cast<unsigned>(date.day());
	bool bBoxLandscape = width > height;
	m_bStatusBox = bStatus;
	if(m_bRecalcReqd || lastDayOfMonth != day || prev_width != width || prev_height != height)
	{
		prev_width = width;
		prev_height = height;
		//Firstly the status text, which will always be in the bottom left corner
		m_statusTextSize = std::min(displayWidth,displayHeight)/130.0f;
		auto statusTextHeight = m_bStatusBox? TextHeight(vg, global.FontStatus(), m_statusTextSize) : 0.0f;
		//Start off by assuming the text is the full width, this will change if we have an analogue clock and we're in landscape
		auto textWidth = width;
		
		if(m_bAnalogueClock)
		{
			auto dim = std::min(width, height);
			if(bBoxLandscape)
				textWidth -= dim;
			//Always in the top right corner
			m_boxAnalogue = DisplayBox(width - dim, dim, dim, dim);
			m_clockState.hours_x = std::make_shared<std::map<int, VGfloat> >();
			m_clockState.hours_y = std::make_shared<std::map<int, VGfloat> >();
			int i;
			VGfloat factor = 9.0f/20.0f;
			if(m_clockState.Numbers == 2)
				factor = 7.0f/20.0f;
			for(i = 0; i < 12; i++)
			{
				(*m_clockState.hours_y)[i] = -cosf(M_PI*i/6.0f) * dim*factor;
				//(*m_clockState.hours_y)[i] -= dim/30.0f;
				(*m_clockState.hours_x)[i] = sinf(M_PI*i/6.0f) * dim*factor;
			}
		}
		else
		{
			m_boxAnalogue.Zero();
			m_clockState.hours_x.reset();
			m_clockState.hours_y.reset();
		}
		float statusHeight = statusTextHeight*3.2f;
		m_boxStatus = DisplayBox(0, height, textWidth, statusHeight);
		VGfloat textTop = 0;
#define textHeight (height - textTop - m_boxStatus.h)
		if(!bBoxLandscape)
			textTop += m_boxAnalogue.h;
		VGfloat digitalColWidth = textWidth;
		VGfloat digitalHeight = 0;
		int rowCount = m_DigitalClocks.size() + (m_bDate?1:0);
		if(!m_DigitalClocks.empty())
		{
			std::string clockStr = "99:99:99";
#if FRAMES
			clockStr = clockStr + ":99";
#endif
			if(bDigitalClockPrefix)
				clockStr = "TOD " + clockStr;
			m_digitalPointSize = MaxPointSize(vg, digitalColWidth*.95f, textHeight/rowCount, clockStr, global.FontDigital());
			digitalHeight = TextHeight(vg, global.FontDigital(), m_digitalPointSize)*1.1f;
			for(auto & digital_clock: m_DigitalClocks)
			{
				digital_clock.pBox = std::make_shared<DisplayBox>(0, textTop + digitalHeight, digitalColWidth, digitalHeight);
				textTop += digitalHeight;
			}
		}
		else
		{
			m_digitalPointSize = 0;
		}
		//Right, digital clocks done, move onto the date
		if(m_bDate)
		{
			auto dateStr = FormatDate(date_time);
			m_datePointSize = MaxPointSize(vg, digitalColWidth*.9f, textHeight, dateStr, global.FontDate());
			auto dateHeight = TextHeight(vg, global.FontDate(), m_datePointSize);
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

void RegionState::RecalcTexts(NVGcontext *vg, OverallState &globalState, const timeval &tval)
{
	DisplayBox &db = m_boxTallies;
	VGfloat row_height = (db.h)/((VGfloat)TD.nRows);
	VGfloat col_width_default = db.w/((VGfloat)TD.nCols_default);

	for(int row = 0; row < TD.nRows; row++)
	{
		int col_count = TD.nCols[row];
		VGfloat col_width = col_width_default;
		if(col_count <= 0)
			col_count = TD.nCols_default;
		else
			col_width = db.w/((VGfloat)col_count);
		for(int col = 0; col < col_count; col++)
		{
			const std::string & zone = GetZone(row, col);
			auto item = TD.displays[row][col];
			if(!item)
				continue;
			auto label = item->Label(tval);
			auto text = item->Text(tval);
			auto iter = globalState.Images.end();
			if(text && ((iter = globalState.Images.find(*text)) == globalState.Images.end() || !iter->second.IsValid()))
			{
				auto maxItemSize = MaxPointSize(vg, col_width * .9f, row_height * (label? .6f :.9f), item->Text(tval)->c_str(), globalState.FontTally(item->IsDigitalClock()));
				if(globalState.TextSizes[zone] == 0 || globalState.TextSizes[zone] > maxItemSize)
					globalState.TextSizes[zone] = maxItemSize;
				if(label)
				{
					auto maxLabelSize = MaxPointSize(vg, -1, row_height *.2f, label->c_str(), globalState.FontTallyLabel());
					if(globalState.LabelSizes[zone] == 0 || globalState.LabelSizes[zone] > maxLabelSize)
						globalState.LabelSizes[zone] = maxLabelSize;
				}
			}
		}
	}
	comms_width = -1.0f;
}

void RegionState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLayout> &pMsg)
{
	UpdateFromMessage(*pMsg);
}

void RegionState::UpdateFromMessage(const ClockMsg_SetLayout &msg)
{
	bool recalcReqd = false;
#define UPDATE_VAL(val,param) { auto newVal = msg.param; \
				recalcReqd = recalcReqd || ((val) != newVal); \
				(val) = newVal; }
	if(m_bLegacyClockMode)
	{
		UPDATE_VAL(m_bAnalogueClock,      bAnalogueClock)
		UPDATE_VAL(m_bLegacyAnalogueClockLocal, bLegacyAnalogueClockLocal)
		UPDATE_VAL(m_bLegacyDigitalClockUTC,    bLegacyDigitalClockUTC)
		UPDATE_VAL(m_bLegacyDigitalClockLocal,  bLegacyDigitalClockLocal)
		UPDATE_VAL(m_bDate,               bDate)
		UPDATE_VAL(m_bLegacyDateLocal,          bLegacyDateLocal)
	}
	bool numbersPresent = m_clockState.Numbers != 0;
	bool numbersOutside = m_clockState.Numbers != 2;
	UPDATE_VAL(numbersPresent,	bNumbersPresent);
	UPDATE_VAL(numbersOutside,	bNumbersOutside);
	UPDATE_VAL(m_clockState.SecondsSweep,		bSecondsSweep);
	UPDATE_VAL(m_clockState.ImageClockFace,		sImageClockFace);
	UPDATE_VAL(m_clockState.ImageClockHours,	sImageClockHours);
	UPDATE_VAL(m_clockState.ImageClockMinutes,	sImageClockMinutes);
	UPDATE_VAL(m_clockState.ImageClockSeconds,	sImageClockSeconds);
#undef UPDATE_VAL
	m_clockState.Numbers = numbersPresent? (numbersOutside? 1 : 2)
						: 0;
	m_bRecalcReqd = m_bRecalcReqd || recalcReqd;
	if(recalcReqd && m_bLegacyClockMode)
	{
		m_DigitalClocks.clear();
		if(m_bLegacyDigitalClockLocal)
		{
			m_DigitalClocks.push_back(ClockData(tz_local,"TOD"));
		}
		if(m_bLegacyDigitalClockUTC)
		{
			m_DigitalClocks.push_back(ClockData(tz_utc,"UTC"));
		}
		if(!m_bLegacyAnalogueClockLocal && m_DigitalClocks.size() > 1)
		{
#if 0
			auto swap_internal = m_DigitalClocks[0];
			m_DigitalClocks[0] = m_DigitalClocks[1];
			m_DigitalClocks[1] = swap_internal;
#else
			std::swap(m_DigitalClocks[0], m_DigitalClocks[1]);
#endif
		}
		m_DateZone = m_bLegacyDateLocal? tz_local: tz_utc;
	}
}

void RegionState::UpdateFromMessage(const std::shared_ptr<ClockMsg_SetClocks> &pMsg)
{
	UpdateFromMessage(*pMsg);
}

void RegionState::UpdateFromMessage(const ClockMsg_SetClocks &msg)
{
	if(m_bLegacyClockMode)
	{
		m_bLegacyClockMode = false;
		m_bRecalcReqd = true;
	}
	std::vector<ClockData> digitalClocks;
	for(const auto & data: msg.tzDigitals)
	{
		try
		{
			digitalClocks.push_back(ClockData(lookup_tz(data.first),data.second));
		}
		catch (...)
		{
			std::cerr << "Error parsing timezone: " <<data.first <<", skipping\n";
		}

	}
	if(digitalClocks != m_DigitalClocks)
	{
		m_DigitalClocks = digitalClocks;
		m_bRecalcReqd = true;
	}

	bool bOldValue = m_bDate;
	try
	{
		if(msg.tzDate.empty())
		{
			m_bDate = false;
		}
		else
		{
			m_bDate = true;
			auto tz = lookup_tz(msg.tzDate);
			if(tz != m_DateZone)
			{
				m_DateZone = tz;
				m_bRecalcReqd = true;
			}
		}
	}
	catch (...)
	{
		std::cerr << "Error parsing timezone: " <<msg.tzDate <<", ignoring\n";
	}

	m_bRecalcReqd = m_bRecalcReqd || (m_bDate != bOldValue);

	bOldValue = m_bAnalogueClock;
	try
	{
		if(msg.tzAnalogue.empty())
		{
			m_bAnalogueClock = false;
		}
		else
		{
			m_bAnalogueClock = true;
			auto tz = lookup_tz(msg.tzAnalogue);
			if(tz != m_AnalogueClockZone)
			{
				m_AnalogueClockZone = tz;
				//No size recalc required for an analogue clock timezone change...
			}
		}
	}
	catch (...)
	{
		std::cerr << "Error parsing timezone: " <<msg.tzAnalogue <<", ignoring\n";
	}
	m_bRecalcReqd = m_bRecalcReqd || (m_bAnalogueClock != bOldValue);

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
bool RegionState::DrawAnalogueClock(NVGcontext *vg, const sys_clock_data & now, const Fontinfo &font_hours, ImagesMap &images)
{
	if(m_bAnalogueClock)
	{
		m_clockState.font_hours = font_hours;
		m_clockState.Draw(vg, m_boxAnalogue, images, m_AnalogueClockZone->to_local(now));
	}
	return m_bAnalogueClock;
}
/*
bool RegionState::AnalogueClock(DisplayBox & dBox, bool &bLocal, std::shared_ptr<const std::map<int, VGfloat> > &hours_x, std::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers)
{
	dBox = m_boxAnalogue;
	bLocal = m_bAnalogueClockLocal;
	hours_x = m_clockState.hours_x;
	hours_y = m_clockState.hours_y;
	numbers = m_AnalogueNumbers;
	return m_bAnalogueClock;
}
*/

bool RegionState::DrawDate(NVGcontext *vg, const OverallState &globalState, const sys_clock_data &now)
{
	if(m_bDate)
	{
		m_boxDate.TextMidBottom(vg, FormatDate(m_DateZone->to_local(now)), globalState.FontDate(), m_datePointSize);
		return true;
	}
	return false;
}

bool RegionState::DrawDigitals(NVGcontext *vg, const OverallState & globalState, bool bPrefix, const sys_clock_data & now)
{
	bool bRet = false;
	for(const auto & clock: m_DigitalClocks)
	{
		if(clock.tz && clock.pBox)
		{
			std::string time_str = FormatTime(clock.tz->to_local(now));
			if(bPrefix)
			{
				time_str = clock.label + " " + time_str;
			}
			clock.pBox->TextMidBottom(vg, time_str, globalState.FontDigital(), m_digitalPointSize);
			bRet = true;
		}
	}
	return bRet;
}


bool RegionState::HasStatusBox()
{
	return m_bStatusBox;
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

bool RegionState::DigitalClockPrefix(const RegionsMap &regions)
{
	bool bFirst = true;
	std::string labelFound;
	for(const auto & kv : regions)
	{
		for(const auto & cd: kv.second->m_DigitalClocks)
		{
			if(!bFirst && labelFound != cd.label)
			{
			    return true;
			}
			bFirst = false;
			labelFound = cd.label;
		}
	}
	return false;
}

std::string RegionState::FormatDate(const sys_clock_data &time)
{
	return FormatDate(m_DateZone->to_local(time));
}

std::string RegionState::FormatDate(const time_info &time)
{
	return date::format("%a %d %b %Y", time);
}

std::string RegionState::FormatTime(const time_info &data)
{
	auto whole_seconds = std::chrono::floor<std::chrono::seconds>(data);
#if FRAMES
	int frames = static_cast<int>(duration_cast<duration<double>>(now - whole_seconds).count() *FPS);
	std::ostringstream oss;
	date::to_stream(oss, "%T:", whole_seconds);
	oss << std::setw(2) << std::setfill('0') << frames;
	return oss.str();
#else
	return date::format("%T", whole_seconds);
#endif
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
		dbTally.TextMid(vg, text, global.FontTally(curTally->IsDigitalClock()), global.TextSizes[zone], global.LabelSizes[zone], curTally->Label(tval), global.FontTallyLabel());
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


bool RegionState::DrawStatusArea(NVGcontext *vg, int ntp_state, bool bFlashPhase, unsigned int connCount, const std::map<unsigned int, bool> &connComms, const std::string &mac_addr, const Fontinfo &font)
{
	if(!m_bStatusBox)
		return false;
	bool bRet = true;
	DisplayBox db = m_boxStatus;
	nvgSave(vg);
	DrawNtpState(vg, db, ntp_state, bFlashPhase, font);
	if(connCount > 0)
	{
		bRet = DrawConnComms(vg, db, connCount, connComms, font)? bRet : false;
		//Only draw MAC address/profile name if we're network-connected
		DrawMacAddress(vg, db, mac_addr, font);
	}
	nvgRestore(vg);
	return bRet;
}

bool RegionState::DrawConnComms(NVGcontext *vg, DisplayBox &db, unsigned int connCount, const std::map<unsigned int,bool> &connComms, const Fontinfo & font)
{
	//Draw comms status
	if(comms_width < 0)
	{
		comms_width =
			std::max(TextWidth(vg, "Comms OK", font, m_statusTextSize),

					TextWidth(vg, "Comms Failed", font, m_statusTextSize));
		for(unsigned int window = 0; window < connCount; window++)
		{
			char buf[512];
			buf[511] = '\0';
			snprintf(buf, 511, "Tally Server %d", window + 1);
			comms_width = std::max(comms_width, TextWidth(vg, buf, font, m_statusTextSize));
		}
		comms_width *= 1.2f;
		comms_text_height = TextHeight(vg, font, m_statusTextSize);
	}
	bool bAnyComms = false;
	for(unsigned int window = 0; window < connCount; window++)
	{
		bool bComms = false;
		//Have to use this complicated lookup method because we're read-only accessing the std::map,
		//and the normal [] accessors can't be used because they would create an empty element if a new index was used.
		auto iter = connComms.find(window);
		if(iter != connComms.end())
			bComms = iter->second;
		bAnyComms = bAnyComms || bComms;
		if(bComms)
			nvgFillColor(vg,colCommsOk);
		else
			nvgFillColor(vg,colCommsFail);
		VGfloat base_x = db.x + db.w - comms_width * (VGfloat)(connCount-window);
		Rect(vg, base_x, db.top_y(), comms_width, db.h);
		nvgFillColor(vg,colNtpText);
		char buf[512];
		buf[511] = '\0';
		base_x += comms_width*.5f;
		snprintf(buf, 511, "Tally Server %d", window + 1);
		TextMid(vg, base_x, db.y - comms_text_height*2.0f, buf, font, m_statusTextSize);
		snprintf(buf, 511, "Comms %s", bComms? "OK" : "Failed");
		TextMid(vg, base_x, db.y - comms_text_height*0.8f, buf, font, m_statusTextSize);
	}
	return bAnyComms;
}

void RegionState::DrawMacAddress(NVGcontext *vg, DisplayBox & db, const std::string &mac_addr, const Fontinfo &font)
{
	nvgFillColor(vg, colMidGray);
	char buf[8192];
	buf[sizeof(buf)-1] = '\0';
	if(TD.sProfName.empty())
		snprintf(buf, sizeof(buf) -1, "MAC Address %s", mac_addr.c_str());
	else
		snprintf(buf, sizeof(buf) -1, "MAC Address %s, %s", mac_addr.c_str(), TD.sProfName.c_str());
	Text(vg, db.x, db.y, buf, font, m_statusTextSize);
}


void RegionState::DrawNtpState(NVGcontext *vg, DisplayBox &db, int ntp_state, bool bFlashPhase, const Fontinfo & font)
{
	nvgFillColor(vg,colWhite);
	const char * sync_text;
	if(ntp_state == 0)
	{
		nvgFillColor(vg,colNtpSynced);
		sync_text = "Synchronised";
	}
	else
	{
		/* flash between red and purple once a second */
		nvgFillColor(vg,colNtpNotSync[bFlashPhase?0:1]);
		if(ntp_state == 1)
			sync_text = "Synchronising..";
		else
			sync_text = "Unknown Synch!";
	}
	const char * header_text = "NTP-Derived Clock";
	auto statusBoxLen = std::max(TextWidth(vg, sync_text, font, m_statusTextSize),
						TextWidth(vg, header_text, font, m_statusTextSize))*1.1f;
	auto statusTextHeight = TextHeight(vg, font, m_statusTextSize);
	//Draw a box around NTP status
	Rect(vg, db.x + db.w - statusBoxLen, db.top_y() , statusBoxLen, db.h);
	nvgFillColor(vg, colNtpText);
	//2 bits of text
	TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight *.8f, sync_text, font, m_statusTextSize);
	TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight*2.f, header_text, font, m_statusTextSize);

	db.w -= statusBoxLen;
}
