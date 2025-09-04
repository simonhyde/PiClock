#ifndef __PICLOCK_REGIONSTATE_H_INCLUDED
#define __PICLOCK_REGIONSTATE_H_INCLUDED

#include <memory>
#include <nanovg.h>
#include "tallydisplays.h"
#include "displaybox.h"
#include "imagescaling.h"
#include "analogueclock.h"
#include "tzinfo.h"

class RegionState;
class OverallState;

typedef std::map<int,std::shared_ptr<RegionState>> RegionsMap;

class ClockData
{
    public:
	ClockData(const date::time_zone* _tz, const std::string &_label)
	:tz(_tz),label(_label)
	{
	}

	bool operator==(const ClockData& other) const
	{
	    //Don't compare box in comparison...
	    return tz == other.tz && label == other.label;
	}

	const date::time_zone *tz;
	std::string label;
	std::shared_ptr<DisplayBox> pBox;
};

class RegionState
{
public:
	TallyDisplays TD;
	RegionState();

	bool LayoutEqual(std::shared_ptr<RegionState> pOther) const;
	bool LayoutEqual(const RegionState & other) const;
	bool RecalcDimensions(NVGcontext* vg, const OverallState & global, const std::chrono::time_point<std::chrono::system_clock> & now, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus, bool bDigitalClockPrefix);

	void RecalcTexts(NVGcontext *vg, OverallState &globalState, const sys_clock_data &now);
	
	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLayout> &pMsg);

	void UpdateFromMessage(const ClockMsg_SetLayout &msg);

	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetClocks> &pMsg);

	void UpdateFromMessage(const ClockMsg_SetClocks &msg);

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLocation> &pMsg);

	bool UpdateFromMessage(const ClockMsg_SetLocation &msg);

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFontSizeZones> &pMsg);

	bool UpdateFromMessage(const ClockMsg_SetFontSizeZones &msg);

	void SetDefaultZone(const std::string &def);
	std::string FormatDate(const sys_clock_data &data);
	std::string FormatDate(const time_info &data);

	std::string FormatTime(const time_info &data);


	const std::string & GetZone(int row, int col);

	void DrawTallies(NVGcontext * vg, OverallState &global, const sys_clock_data & now);

	bool DrawAnalogueClock(NVGcontext *vg, const sys_clock_data & now, const Fontinfo & font_hours, ImagesMap &images);

	bool DrawStatusArea(NVGcontext *vg, int ntp_state, bool bFlashPhase, unsigned int connCount, const std::map<unsigned int, bool> &connComms, const std::string &mac_addr, const Fontinfo & font);

//Simple accessor methods
	bool Rotate();
/*
	bool AnalogueClock(DisplayBox & dBox, bool &bLocal, std::shared_ptr<const std::map<int, VGfloat> > &hours_x, std::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers);
	*/

	bool DrawDate(NVGcontext *vg, const OverallState & globalState, const sys_clock_data & now);
	bool DrawDigitals(NVGcontext *vg, const OverallState & globalState, bool bPrefix, const sys_clock_data & now);

	bool HasStatusBox();

	DisplayBox StatusBox(int &pointSize);

	DisplayBox TallyBox();

	VGfloat width() const;

	VGfloat height() const;

	VGfloat x() const;

	VGfloat y() const;

	VGfloat top_y() const;

	static bool DigitalClockPrefix(const RegionsMap &regions);

	void ForceRecalc();

private:
	void DrawTally(NVGcontext* vg, DisplayBox &dbTally, const int row, const int col, OverallState & global, const sys_clock_data &now);
	void DrawNtpState(NVGcontext *vg, DisplayBox &db, int ntp_state, bool bFlashState, const Fontinfo & font);
	bool DrawConnComms(NVGcontext *vg, DisplayBox &db, unsigned int connCount, const std::map<unsigned int,bool> &connComms, const Fontinfo & font);
	void DrawMacAddress(NVGcontext *vg, DisplayBox &db, const std::string &mac_addr, const Fontinfo & font);


	const date::time_zone * tz_local, *tz_utc;
	bool m_bRecalcReqd;
	bool m_bRotationReqd;
	bool m_bStatusBox;
	bool m_bImageClock;
	DisplayBox m_boxAnalogue;
	int m_statusTextSize;
	DisplayBox m_boxStatus, m_boxDate, m_boxTallies;
	int m_digitalPointSize;
	int m_datePointSize;
	int lastDayOfMonth = -1;
	std::shared_ptr<std::vector<std::vector<std::string>>> m_size_zones;
	std::string m_default_zone;

	bool m_bAnalogueClock;
	const date::time_zone * m_AnalogueClockZone;
	std::vector<ClockData> m_DigitalClocks;

	//Legacy clock information
	bool m_bLegacyClockMode = true;
	bool m_bLegacyAnalogueClockLocal = true;
	bool m_bLegacyDigitalClockUTC = false;
	bool m_bLegacyDigitalClockLocal = true;
	bool m_bLegacyDateLocal = true;
	bool m_bDate;
	const date::time_zone * m_DateZone;
	bool m_bLandscape;
	AnalogueClockState m_clockState;
	VGfloat m_x = 0.0f;
	VGfloat m_y = 0.0f;
	VGfloat m_width = 1.0;
	VGfloat m_height = 1.0;
	VGfloat prev_height = 0.0;
	VGfloat prev_width = 0.0;
	VGfloat comms_width = -1.0f;
	VGfloat comms_text_height = 0.0f;
};
#endif
