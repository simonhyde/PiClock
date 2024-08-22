#ifndef __PICLOCK_REGIONSTATE_H_INCLUDED
#define __PICLOCK_REGIONSTATE_H_INCLUDED

#include <memory>
#include <nanovg.h>
#include "tallydisplays.h"
#include "displaybox.h"
#include "imagescaling.h"

class RegionState;
class OverallState;

typedef std::map<int,std::shared_ptr<RegionState>> RegionsMap;

class AnalogueClockState
{
public:
	int Numbers = 1;
        bool SecondsSweep = false;
        std::string ImageClockFace, ImageClockHours, ImageClockMinutes, ImageClockSeconds;
	std::shared_ptr<std::map<int, VGfloat>> hours_x;
	std::shared_ptr<std::map<int, VGfloat>> hours_y;
};

class RegionState
{
public:
	TallyDisplays TD;
	RegionState();

	bool LayoutEqual(std::shared_ptr<RegionState> pOther) const;
	bool LayoutEqual(const RegionState & other) const;
	bool RecalcDimensions(NVGcontext* vg, const OverallState & global, const struct tm & utc, const struct tm & local, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus, bool bDigitalClockPrefix);

	void RecalcTexts(NVGcontext *vg, OverallState &globalState, const timeval &tval);
	
	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLayout> &pMsg);

	void UpdateFromMessage(const ClockMsg_SetLayout &msg);

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLocation> &pMsg);

	bool UpdateFromMessage(const ClockMsg_SetLocation &msg);

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFontSizeZones> &pMsg);

	bool UpdateFromMessage(const ClockMsg_SetFontSizeZones &msg);

	void SetDefaultZone(const std::string &def);
	std::string FormatDate(const struct tm &data);

	std::string FormatTime(const struct tm &data, time_t usecs);


	const std::string & GetZone(int row, int col);

	void DrawTallies(NVGcontext * vg, OverallState &global, const timeval & tval);

	bool DrawAnalogueClock(NVGcontext *vg, const tm &tm_local, const tm &tm_utc, const suseconds_t &usecs, const Fontinfo & font_hours, ImagesMap &images);

	bool DrawStatusArea(NVGcontext *vg, int ntp_state, bool bFlashPhase, unsigned int connCount, const std::map<unsigned int, bool> &connComms, const std::string &mac_addr, const Fontinfo & font);

//Simple accessor methods
	bool Rotate();
/*
	bool AnalogueClock(DisplayBox & dBox, bool &bLocal, std::shared_ptr<const std::map<int, VGfloat> > &hours_x, std::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers);
        */

	bool Date(DisplayBox & dBox, bool &bLocal, int & pointSize);

	bool DigitalLocal(DisplayBox & dBox, int & pointSize, std::string & prefix);
	bool DigitalUTC(DisplayBox & dBox, int & pointSize, std::string & prefix);

	bool HasStatusBox();

	DisplayBox StatusBox(int &pointSize);

	DisplayBox TallyBox();

	VGfloat width() const;

	VGfloat height() const;

	VGfloat x() const;

	VGfloat y() const;

	VGfloat top_y() const;

	bool hasDigitalUTC() const;

	bool hasDigitalLocal() const;

	static bool DigitalClockPrefix(const RegionsMap &regions);

	void ForceRecalc();

private:
	void DrawTally(NVGcontext* vg, DisplayBox &dbTally, const int row, const int col, OverallState & global, const timeval &tval);
	void DrawNtpState(NVGcontext *vg, DisplayBox &db, int ntp_state, bool bFlashState, const Fontinfo & font);
	bool DrawConnComms(NVGcontext *vg, DisplayBox &db, unsigned int connCount, const std::map<unsigned int,bool> &connComms, const Fontinfo & font);
	void DrawMacAddress(NVGcontext *vg, DisplayBox &db, const std::string &mac_addr, const Fontinfo & font);


	bool m_bRecalcReqd;
	bool m_bRotationReqd;
	bool m_bStatusBox;
        bool m_bImageClock;
	DisplayBox m_boxAnalogue;
	int m_statusTextSize;
	DisplayBox m_boxStatus, m_boxUTC, m_boxLocal, m_boxDate, m_boxTallies;
	int m_digitalPointSize;
	int m_datePointSize;
	int lastDayOfMonth = -1;
	std::shared_ptr<std::vector<std::vector<std::string>>> m_size_zones;
	std::string m_default_zone;

	bool m_bAnalogueClock;
	bool m_bAnalogueClockLocal;
	bool m_bDigitalClockUTC;
	bool m_bDigitalClockLocal;
	bool m_bDate;
	bool m_bDateLocal;
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
