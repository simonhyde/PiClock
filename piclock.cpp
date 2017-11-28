// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
#include <map>
#include <cctype>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <netdb.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <openssl/sha.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include "ntpstat/ntpstat.h"
//Bit of a bodge, but generating a header file for this example would be a pain
#include "blocking_tcp_client.cpp"
//piface digital
#include "pifacedigital.h"

#define FPS 25
#define FRAMES 0
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000

namespace po = boost::program_options;

std::string mac_address;
int bRunning = 1;
std::map<unsigned int,bool> bComms;
int GPI_MODE = 0;
std::string TALLY_SERVICE("6254");
std::string TALLY_SECRET("SharedSecretGoesHere");
std::vector<std::string> tally_hosts;

#define FONT_PROP	(SerifTypeface)
#define FONT_HOURS	(SansTypeface)
#define FONT_MONO	(MonoTypeface)
#define FONT(x)		((x)? FONT_MONO : FONT_PROP)


void * ntp_check_thread(void * arg)
{
	ntpstate_t * data = (ntpstate_t *)arg;
	while(bRunning)
	{
		get_ntp_state(data);
		sleep(1);
	}
	return NULL;
}

class TallyColour
{
public:
	uint32_t R() const
	{
		return (col >> 16)&0xFF;
	}
	uint32_t G() const
	{
		return (col >> 8)&0xFF;
	}
	uint32_t B() const
	{
		return col & 0xFF;
	}
	TallyColour(const std::string &input)
		:col(std::stoul(input, NULL, 16))
	{
	}
	TallyColour(uint8_t r, uint8_t g, uint8_t b)
		:col( (r<<16) | (g<<8) | b )
	{
	}
	TallyColour()
		:col(0) //Default to black
	{
	}
	bool Equals(const TallyColour & other) const
	{
		return other.col == col;
	}

	void Fill()
	{
		::Fill(R(),G(),B(),1);
	}

	void Stroke()
	{
		::Stroke(R(),G(),B(),1);
	}

protected:
	uint32_t col;
};


class TallyState
{
public:
	virtual boost::shared_ptr<TallyColour> FG(const struct timeval &curTime) const = 0;
	virtual boost::shared_ptr<TallyColour> BG(const struct timeval &curTime) const = 0;
	virtual boost::shared_ptr<std::string> Text(const struct timeval &curTime) const = 0;
	virtual boost::shared_ptr<std::string> Label(const struct timeval &curTime) const = 0;
	virtual boost::shared_ptr<TallyState>  SetLabel(const std::string &lbl) const = 0;
	virtual bool IsMonoSpaced() const = 0;
	virtual bool Equals(boost::shared_ptr<TallyState> other) const = 0;
};

class SimpleTallyState  :  public TallyState
{
public:
	boost::shared_ptr<TallyColour> FG(const struct timeval &curTime) const override
	{
		return m_FG;
	}
	boost::shared_ptr<TallyColour> BG(const struct timeval &curTime) const override
	{
		return m_BG;
	}
	boost::shared_ptr<std::string> Text(const struct timeval &curTime) const override
	{
		return m_text;
	}
	boost::shared_ptr<std::string> Label(const struct timeval &curTime) const override
	{
		return m_label;
	}
	boost::shared_ptr<TallyState> SetLabel(const std::string & label) const override
	{
		boost::shared_ptr<TallyState> ret(new SimpleTallyState(*this));
		auto derived = dynamic_cast<SimpleTallyState *>(ret.get());
		derived->m_label = boost::shared_ptr<std::string>(new std::string(label));
		return ret;
	}
	bool IsMonoSpaced() const override
	{
		return false;
	}

	bool Equals(boost::shared_ptr<TallyState> other) const override
	{
		if(!other)
			return false;
		if(typeid(*other) != typeid(*this))
			return false;
		auto derived = dynamic_cast<SimpleTallyState *>(other.get());
		return derived != NULL 
			&& derived->m_FG->Equals(*m_FG)
			&& derived->m_BG->Equals(*m_BG)
			&& *(derived->m_text) == *m_text;
	}

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text)
	 :m_FG(new TallyColour(fg)),m_BG(new TallyColour(bg)),m_text(new std::string(_text)), m_label(new std::string())
	{}
	SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text)
	 :m_FG(new TallyColour(fg)),m_BG(new TallyColour(bg)),m_text(new std::string(_text)), m_label(new std::string())
	{}

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text, const boost::shared_ptr<TallyState> &_old)
	 :m_FG(new TallyColour(fg)),m_BG(new TallyColour(bg)),m_text(new std::string(_text))
	{
		auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
		if(derived != NULL)
			m_label = derived->m_label;
		else
			m_label.reset(new std::string());
	}
	SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text, const boost::shared_ptr<TallyState> &_old)
	 :m_FG(new TallyColour(fg)),m_BG(new TallyColour(bg)),m_text(new std::string(_text))
	{
		auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
		if(derived != NULL)
			m_label = derived->m_label;
		else
			m_label.reset(new std::string());
	}
protected:
	boost::shared_ptr<TallyColour> m_FG, m_BG;
	boost::shared_ptr<std::string> m_text, m_label;
};

class CountdownClock : public SimpleTallyState
{
public:
	bool Invert(const struct timeval &curTime) const
	{
		if(m_pFlashLimit)
		{
			auto left = TimeLeft(curTime, m_target);
			if((left.tv_sec <= *m_pFlashLimit)
				&& (std::abs(left.tv_usec) < 500000))
			return true;
		}
		return false;
	}
	boost::shared_ptr<TallyColour> FG(const struct timeval &curTime) const override
	{
		if(Invert(curTime))
			return SimpleTallyState::BG(curTime);
		return SimpleTallyState::FG(curTime);
	}
	boost::shared_ptr<TallyColour> BG(const struct timeval &curTime) const override
	{
		if(Invert(curTime))
			return SimpleTallyState::FG(curTime);
		return SimpleTallyState::BG(curTime);
	}
	boost::shared_ptr<std::string> Text(const struct timeval &curTime) const override
	{
		time_t secs = SecsLeft(curTime,m_target);
		char buf[256];
		char negChar = (secs < 0)? '-': ' ';
		secs = std::abs(secs);
		snprintf(buf, sizeof(buf) - 1, "%c%02ld:%02ld:%02ld",
			 negChar, secs/3600, (secs/60)%60, secs %60);
		return boost::shared_ptr<std::string>(new std::string(buf));
	}
	boost::shared_ptr<std::string> Label(const struct timeval &curTime) const override
	{
		return SimpleTallyState::Text(curTime);
	}
	bool IsMonoSpaced() const override
	{
		return true;
	}

	bool Equals(boost::shared_ptr<TallyState> other) const override
	{
		if(!other)
			return false;
		if(typeid(*other) != typeid(*this))
			return false;
		auto derived = dynamic_cast<CountdownClock *>(other.get());
		return derived != NULL 
			&& derived->m_FG->Equals(*m_FG)
			&& derived->m_BG->Equals(*m_BG)
			&& derived->m_target.tv_sec == m_target.tv_sec
			&& derived->m_target.tv_usec == m_target.tv_usec
			&& *(derived->m_text) == *(m_text)
			&& ((!m_pFlashLimit && !derived->m_pFlashLimit.get())
			    || (m_pFlashLimit && derived->m_pFlashLimit
			       && *m_pFlashLimit == *(derived->m_pFlashLimit)));
	}

	CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const struct timeval & _target, boost::shared_ptr<long long> pflash)
	 :SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
	{}
	CountdownClock(const TallyColour & fg, const TallyColour &bg, const std::string &_label, const struct timeval &_target, boost::shared_ptr<long long> pflash)
	 :SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
	{}
private:
	struct timeval m_target;
	boost::shared_ptr<long long> m_pFlashLimit;
	static struct timeval TimeLeft(const timeval & current, const timeval & target)
	{
		struct timeval ret;
		ret.tv_sec = target.tv_sec - current.tv_sec;
		ret.tv_usec = target.tv_usec - current.tv_usec;
		while(ret.tv_usec < 0 && ret.tv_sec > 0)
		{
			ret.tv_usec += 1000000;
			ret.tv_sec -= 1;
		}
		while(ret.tv_usec > 0 && ret.tv_sec < 0)
		{
			ret.tv_usec -= 1000000;
			ret.tv_sec += 1;
		}
		return ret;
	}
	static time_t SecsLeft(const timeval & current, const timeval & target)
	{
		auto left = TimeLeft(current,target);
		//Round to nearest half second for now...
		auto ret = left.tv_sec;
		if(left.tv_usec >= 500000)
			ret += 1;
		else if(left.tv_usec <= -500000)
			ret -= 1;
		return ret;
	}
};


class TallyDisplays
{
public:
	int nRows;
	int nCols_default;
	int textSize;
	std::map<int,int> nCols;
	std::string sProfName;
	std::map<int,std::map<int,boost::shared_ptr<TallyState> > > displays;
	TallyDisplays()
	 :nRows(0), nCols_default(0), textSize(-1), sProfName()
	{
	}
	void clear()
	{
		nRows = nCols_default = 0;
		sProfName = "";
		textSize = -1;
		nCols.clear();
		displays.clear();
	}
};

class DisplayBox
{
public:
	VGfloat x,y,w,h;
	DisplayBox()
		:x(0),y(0),w(0),h(0),m_corner(0)
	{}
	DisplayBox(VGfloat _x, VGfloat _y, VGfloat _w, VGfloat _h)
		:x(_x),y(_y),w(_w),h(_h),m_corner(0)
	{
	}
	VGfloat mid_x()
	{
		return x + w/2.0f;
	}
	VGfloat mid_y()
	{
		return y + h/2.0f;
	}
	void TextMid(const std::string & str, const Fontinfo & font, const int pointSize, boost::shared_ptr<std::string> label = boost::shared_ptr<std::string>())
	{
		auto text_height = TextHeight(font, pointSize);
		VGfloat text_y = mid_y() - text_height *.33f;
		if(label)
		{
			auto labelHeight = TextHeight(FONT_PROP, pointSize/3);
			auto label_y = y + h - labelHeight;
			::TextClip(x + m_corner, label_y,
				label->c_str(), FONT_PROP, pointSize/3, w - m_corner, '.',3);
			if(text_y + text_height > label_y)
			{
				text_y = label_y - text_height*1.05f;
			}
		}
				
		::TextMid(mid_x(), text_y, str.c_str(), font, pointSize);
	}
	void TextMidBottom(const std::string & str, const Fontinfo & font, const int pointSize, boost::shared_ptr<std::string> label = boost::shared_ptr<std::string>())
	{
		if(label)
		{
			auto label_y = y + h - TextHeight(FONT_PROP, pointSize/3);
			::Text(x + w*.05f, label_y,
				label->c_str(), FONT_PROP, pointSize/3);
		}
				
		::TextMid(mid_x(), y + h *.05f, str.c_str(), font, pointSize);
	}
	void Roundrect(VGfloat corner)
	{
		m_corner = corner;
		::Roundrect(x, y, w, h, corner, corner);
	}
	void Rect()
	{
		m_corner = 0;
		::Rect(x,y,w,h);
	}
	void Zero()
	{
		m_corner = x = y = w = h = 0;
	}
private:
	VGfloat m_corner;
};

int MaxPointSize(VGfloat width, VGfloat height, const std::string & text, const Fontinfo & f)
{
	int ret = 1;
	bool bOverflowed = false;
	while(!bOverflowed)
	{
		ret++;
		if(TextHeight(f, ret) > height)
			bOverflowed = true;
		else if(TextWidth(text.c_str(), f, ret) > width)
			bOverflowed = true;
	}
	return ret - 1;
}

std::string FormatDate(const struct tm &data)
{
	char buf[512];
	strftime(buf, sizeof(buf), "%a %d %b %Y", &data);
	return buf;
}

std::string FormatTime(const struct tm &data, time_t usecs)
{
	char buf[512];
#if FRAMES
       sprintf(buf,"%02d:%02d:%02d:%02ld",data.tm_hour,data.tm_min,data.tm_sec,usec *FPS/1000000);
#else
       sprintf(buf,"%02d:%02d:%02d",data.tm_hour,data.tm_min,data.tm_sec);
#endif
       return buf;
}

std::string get_arg(const std::string & input, int index, bool bTerminated = true)
{
	size_t start_index = 0;
	while(index > 0)
	{
		start_index = input.find(':', start_index) + 1;
		index--;
	}

	if(!bTerminated)
		return input.substr(start_index);

	size_t end = input.find(':',start_index);
	if(end != std::string::npos)
		end -= start_index;
	return input.substr(start_index,end);
}

int get_arg_int(const std::string &input, int index, bool bTerminated = true)
{
	return std::stoi(get_arg(input, index, bTerminated));
}

long get_arg_l(const std::string &input, int index, bool bTerminated = true)
{
	return std::stol(get_arg(input, index, bTerminated));
}

long long get_arg_ll(const std::string &input, int index, bool bTerminated = true)
{
	return std::stoll(get_arg(input, index, bTerminated));
}

VGfloat get_arg_f(const std::string &input, int index, bool bTerminated = true)
{
	return std::stod(get_arg(input, index, bTerminated));
}
boost::shared_ptr<long long> get_arg_pll(const std::string &input, int index, bool bTerminated = true)
{
	auto str = get_arg(input, index, bTerminated);
	if(str.length() <= 0)
		return boost::shared_ptr<long long>();
	return boost::shared_ptr<long long>(new long long(std::stoull(str)));
}


bool get_arg_bool(const std::string &input, int index, bool bTerminated = true)
{
	return get_arg_int(input, index, bTerminated) != 0;
}


class OverallState
{
public:
	bool RotationReqd(VGfloat width, VGfloat height)
	{
		if(m_bLandscape)
			return width < height;
		else
			return width > height;
	}

	bool Landscape()
	{
		return m_bLandscape;
	}

	bool ScreenSaver()
	{
		return m_bScreenSaver;
	}
	void UpdateFromMessage(const std::string & message)
	{
		m_bLandscape = get_arg_bool(message,1);
		m_bScreenSaver = get_arg_bool(message,2);
	}

	void SetLandscape(bool bLandscape)
	{
		m_bLandscape = bLandscape;
	}

private:
	bool m_bLandscape = true;
	bool m_bScreenSaver = true;
};

OverallState globalState;

class RegionState
{
public:
	TallyDisplays TD;
	RegionState()
	: m_bRecalcReqd(true), m_bAnalogueClock(true),m_bAnalogueClockLocal(true),m_bDigitalClockUTC(false),m_bDigitalClockLocal(true),m_bDate(true), m_bDateLocal(true), m_AnalogueNumbers(1)
	{}

	bool LayoutEqual(boost::shared_ptr<RegionState> pOther) const
	{
		return pOther && LayoutEqual(*pOther);
	}
	bool LayoutEqual(const RegionState & other) const
	{
		return  other.m_bAnalogueClock        == m_bAnalogueClock &&
			other.m_bAnalogueClockLocal   == m_bAnalogueClockLocal   &&
			other.m_bDigitalClockUTC      == m_bDigitalClockUTC      &&
			other.m_bDigitalClockLocal    == m_bDigitalClockLocal    &&
			other.m_bDate                 == m_bDate                 &&
			other.m_bDateLocal            == m_bDateLocal;
	}
	bool RecalcDimensions(const struct tm & utc, const struct tm & local, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus)
	{
		auto day = (m_bDateLocal? local:utc).tm_mday;
		bool bBoxLandscape = width > height;
		if(m_bRecalcReqd || lastDayOfMonth != day || prev_width != width || prev_height != height)
		{
			prev_width = width;
			prev_height = height;
			//Firstly the status text, which will always be in the bottom left corner
			m_statusTextSize = std::min(displayWidth,displayHeight)/130.0f;
			auto statusTextHeight = bStatus? TextHeight(FONT_PROP, m_statusTextSize) : 0.0f;
			//Start off by assuming the text is the full width, this will change if we have an analogue clock and we're in landscape
			auto textWidth = width;
			
			if(m_bAnalogueClock)
			{
				auto dim = std::min(width, height);
				if(bBoxLandscape)
					textWidth -= dim;
				//Always in the top right corner
				m_boxAnalogue = DisplayBox(width - dim, height - dim, dim, dim);
				m_hours_x.reset(new std::map<int, VGfloat>());
				m_hours_y.reset(new std::map<int, VGfloat>());
				int i;
				VGfloat factor = 9.0f/20.0f;
				if(m_AnalogueNumbers == 2)
					factor = 7.0f/20.0f;
				for(i = 0; i < 12; i++)
				{
					(*m_hours_x)[i] = cosf(M_PI*i/6.0f) * dim*factor;
					(*m_hours_x)[i] -= dim/30.0f;
					(*m_hours_y)[i] = sinf(M_PI*i/6.0f) * dim*factor;
				}
			}
			else
			{
				m_boxAnalogue.Zero();
				m_hours_x.reset();
				m_hours_y.reset();
			}

			m_boxStatus = DisplayBox(0,0, textWidth, statusTextHeight*3.2f);
			VGfloat textTop = height;
#define textHeight (textTop - m_boxStatus.h)
			if(!bBoxLandscape)
				textTop -= m_boxAnalogue.h;
			VGfloat digitalColWidth = textWidth;
			VGfloat digitalHeight = 0;
			if(m_bDigitalClockUTC || m_bDigitalClockLocal)
			{
				std::string clockStr = "99:99:99";
#if FRAMES
				clockStr = clockStr + ":99";
#endif
				if(m_bDigitalClockUTC && m_bDigitalClockLocal)
					clockStr = "TOD " + clockStr;
				m_digitalPointSize = MaxPointSize(digitalColWidth*.95f, textHeight, clockStr, FONT_MONO);
				digitalHeight = TextHeight(FONT_MONO, m_digitalPointSize)*1.1f;
				DisplayBox topDigital(0, textTop - digitalHeight, digitalColWidth, digitalHeight);
				textTop -= digitalHeight;
				if(m_bDigitalClockUTC && m_bDigitalClockLocal)
				{
					DisplayBox bottomDigital(0, textTop - digitalHeight, digitalColWidth, digitalHeight);
					textTop -= digitalHeight;
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
				m_datePointSize = MaxPointSize(digitalColWidth*.9f, textHeight, dateStr, FONT_PROP);
				auto dateHeight = TextHeight(FONT_PROP, m_datePointSize);
				m_boxDate = DisplayBox(0, textTop - dateHeight *1.1, digitalColWidth, dateHeight*1.2);

			}
			else
			{
				m_datePointSize = 0;
				m_boxDate.Zero();
			}
			textTop -= m_boxDate.h;
			m_boxTallies = DisplayBox(0,textTop - textHeight, textWidth, textHeight);
			m_bRecalcReqd = false;
			lastDayOfMonth = day;
			return true;
		}
		return false;
	}

	void UpdateFromMessage(const std::string & message)
	{
		bool newVal;
#define UPDATE_VAL(val,idx) newVal = get_arg_bool(message,(idx)); \
			    m_bRecalcReqd = m_bRecalcReqd || ((val) != newVal); \
			    (val) = newVal;
		UPDATE_VAL(m_bAnalogueClock,      1)
		UPDATE_VAL(m_bAnalogueClockLocal, 2)
		UPDATE_VAL(m_bDigitalClockUTC,    3)
		UPDATE_VAL(m_bDigitalClockLocal,  4)
		UPDATE_VAL(m_bDate,               5)
		UPDATE_VAL(m_bDateLocal,          6)
		//Skip Landscape parameter, this is now global, still transmitted by driver for legacy devices
		bool numbersPresent = m_AnalogueNumbers != 0;
		bool numbersOutside = m_AnalogueNumbers != 2;
		UPDATE_VAL(numbersPresent,8);
		UPDATE_VAL(numbersOutside,9);
#undef UPDATE_VAL
		m_AnalogueNumbers = numbersPresent? (numbersOutside? 1 : 2)
						    : 0;
		m_bDigitalClockPrefix = m_bDigitalClockUTC && m_bDigitalClockLocal;
	}

	bool UpdateLocationFromMessage(const std::string & message)
	{
		VGfloat newVal;
		bool changed = false;
#define UPDATE_VAL(val,idx) newVal = get_arg_f(message,(idx)); \
			    changed = changed || ((val) != newVal); \
			    (val) = newVal;
		UPDATE_VAL(m_x,		1)
		UPDATE_VAL(m_y,		2)
		UPDATE_VAL(m_width,	3)
		UPDATE_VAL(m_height,	4)
#undef UPDATE_VAL
		m_bRecalcReqd = m_bRecalcReqd || changed;
		return changed;
	}

//Simple accessor methods
	bool Rotate()
	{
		return m_bRotationReqd;
	}
	bool AnalogueClock(DisplayBox & dBox, bool &bLocal, boost::shared_ptr<const std::map<int, VGfloat> > &hours_x, boost::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers)
	{
		dBox = m_boxAnalogue;
		bLocal = m_bAnalogueClockLocal;
		hours_x = m_hours_x;
		hours_y = m_hours_y;
		numbers = m_AnalogueNumbers;
		return m_bAnalogueClock;
	}

	bool Date(DisplayBox & dBox, bool &bLocal, int & pointSize)
	{
		dBox = m_boxDate;
		bLocal = m_bDateLocal;
		pointSize = m_datePointSize;
		return m_bDate;
	}

	bool DigitalLocal(DisplayBox & dBox, int & pointSize, std::string & prefix)
	{
		dBox = m_boxLocal;
		pointSize = m_digitalPointSize;
		if(m_bDigitalClockPrefix)
			prefix = "TOD ";
		else
			prefix = std::string();
		return m_bDigitalClockLocal;
	}

	bool DigitalUTC(DisplayBox & dBox, int & pointSize, std::string & prefix)
	{
		dBox = m_boxUTC;
		pointSize = m_digitalPointSize;
		if(m_bDigitalClockPrefix)
			prefix = "UTC ";
		else
			prefix = std::string();
		return m_bDigitalClockUTC;
	}

	DisplayBox StatusBox(int &pointSize)
	{
		pointSize = m_statusTextSize;
		return m_boxStatus;
	}

	DisplayBox TallyBox()
	{
		return m_boxTallies;
	}

	VGfloat width() const
	{
		return m_width;
	}

	VGfloat height() const
	{
		return m_height;
	}

	VGfloat x() const
	{
		return m_x;
	}

	VGfloat y() const
	{
		return m_y;
	}

private:
	bool m_bRecalcReqd;
	bool m_bRotationReqd;
	DisplayBox m_boxAnalogue;
	int m_statusTextSize;
	DisplayBox m_boxStatus, m_boxUTC, m_boxLocal, m_boxDate, m_boxTallies;
	int m_digitalPointSize;
	int m_datePointSize;
	int lastDayOfMonth = -1;
	boost::shared_ptr<std::map<int, VGfloat>> m_hours_x;
	boost::shared_ptr<std::map<int, VGfloat>> m_hours_y;

	bool m_bAnalogueClock;
	bool m_bAnalogueClockLocal;
	bool m_bDigitalClockUTC;
	bool m_bDigitalClockLocal;
	bool m_bDigitalClockPrefix = false;
	bool m_bDate;
	bool m_bDateLocal;
	bool m_bLandscape;
	int m_AnalogueNumbers;
	VGfloat m_x = 0.0f;
	VGfloat m_y = 0.0f;
	VGfloat m_width = 1.0;
	VGfloat m_height = 1.0;
	VGfloat prev_height = 0.0;
	VGfloat prev_width = 0.0;
};

typedef std::map<int,boost::shared_ptr<RegionState>> RegionsMap_Base;
typedef boost::shared_ptr<RegionsMap_Base> RegionsMap;

RegionsMap pGlobalRegions(new RegionsMap_Base());

bool UpdateCount(RegionsMap pRegions, int newCount)
{
	std::set<int> removeIndices;
	for(const auto & item: *pRegions)
	{
		if(item.first >= newCount || item.first < 0)
			removeIndices.insert(item.first);
	}
	if(!removeIndices.empty())
	{
		for(int index: removeIndices)
		{
			pRegions->erase(index);
		}
		return true;
	}
	return false;

}

int handle_tcp_message(const std::string &message, client & conn)
{
	if(message == "PING")
	{
		conn.write_line("PONG", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		return 2;
	}
	std::string cmd = get_arg(message,0);
	if(cmd == "CRYPT")
	{
		uint8_t sha_buf[SHA512_DIGEST_LENGTH];
		std::string to_digest = get_arg(message,1,false) + TALLY_SECRET;
		SHA512((const uint8_t *)to_digest.c_str(), to_digest.length(),
			sha_buf);
		char out_buf[SHA512_DIGEST_LENGTH*2 + 1];
		for(int i = 0; i < SHA512_DIGEST_LENGTH; i++)
			sprintf(out_buf + i*2, "%02x", sha_buf[i]);
		std::string to_write = std::string("AUTH:") + std::string(out_buf)
				   + std::string(":") + mac_address;
		conn.write_line(to_write, boost::posix_time::time_duration(0,0,10),'\r');
		return 3;
	}
	RegionsMap pRegions(new RegionsMap_Base(*pGlobalRegions));
	
	if(cmd == "SETGLOBAL")
	{
		globalState.UpdateFromMessage(message);
	}
	else
	{
		bool bChanged = false;
		bool bSizeChanged = false;
		bool bMaxRegion = false;
		int regionIndex = 0;

		if(isdigit(cmd[0]))
		{
			char * pEnd;
			regionIndex = strtoul(cmd.c_str(), &pEnd, 10);
			//This step to stop possible confusion when you try to overwrite cmd with its own contents...
			std::string newCmd = pEnd;
			cmd = newCmd;
		}
		else if(cmd != "SETREGIONCOUNT" && cmd != "SETPROFILE")
		{
			if(UpdateCount(pRegions, 1))
			{
				bChanged = true;
				bSizeChanged = true;
			}
			bMaxRegion = true;
		}
		if(!(*pRegions)[regionIndex])
		{
			(*pRegions)[regionIndex] = boost::shared_ptr<RegionState>(new RegionState());
		}
		boost::shared_ptr<RegionState> pOld = (*pRegions)[regionIndex];
		boost::shared_ptr<RegionState> pNew = boost::shared_ptr<RegionState>(new RegionState(*pOld));

		if(bMaxRegion && (pNew->x() != 0.0f || pNew->y() != 0.0f || pNew->width() != 1.0f || pNew->height() != 1.0f))
		{
			bChanged = true;
			bSizeChanged = true;
			pNew->UpdateLocationFromMessage("SETLOCATION:0:0:1:1");
		}

		if(cmd == "SETREGIONCOUNT")
		{
			int newCount = get_arg_int(message, 1);
			if(UpdateCount(pRegions, newCount))
			{
				bChanged = true;
				bSizeChanged = true;
			}
		}
		else if(cmd == "SETSIZE")
		{
			pNew->TD.nRows = get_arg_int(message,1);
			pNew->TD.nCols_default = get_arg_int(message,2);
			bChanged = pOld->TD.nRows != pNew->TD.nRows || pOld->TD.nCols_default != pNew->TD.nCols_default;
			bSizeChanged = bChanged;
		}
		else if(cmd == "SETLOCATION")
		{
			if(pNew->UpdateLocationFromMessage(message))
			{
				bChanged = bSizeChanged = true;
			}
		}
		else if(cmd == "SETROW")
		{
			int row = get_arg_int(message,1);
			int col_count = get_arg_int(message,2);
			pNew->TD.nCols[row] = col_count;
			bChanged = bChanged || (pOld->TD.nCols[row] != col_count);
		}
		else if(cmd == "SETTALLY" || cmd == "SETCOUNTDOWN")
		{
			int row = get_arg_int(message,1);
			int col = get_arg_int(message,2);
			if(cmd == "SETCOUNTDOWN")
			{
				struct timeval target;
				target.tv_sec = get_arg_ll(message,5);
				target.tv_usec = get_arg_l(message,6);

				pNew->TD.displays[row][col].reset(new CountdownClock(get_arg(message,3),
							  get_arg(message,4),
							  get_arg(message,8,false),
							  target,
							  get_arg_pll(message,7)));
			}
			else
			{
				pNew->TD.displays[row][col].reset(new SimpleTallyState(get_arg(message,3),
							  get_arg(message,4),
							  get_arg(message,5,false),
							  pOld->TD.displays[row][col]));
			}
			bChanged = bChanged || (!pNew->TD.displays[row][col]->Equals(pOld->TD.displays[row][col]));
			struct timeval tvTmp;
			tvTmp.tv_sec = tvTmp.tv_usec = 0;
			bSizeChanged = bSizeChanged || !(pOld->TD.displays[row][col])
				      || pNew->TD.displays[row][col]->Text(tvTmp) != pOld->TD.displays[row][col]->Text(tvTmp)
				      || pNew->TD.displays[row][col]->IsMonoSpaced() != pOld->TD.displays[row][col]->IsMonoSpaced();
		}
		else if(cmd == "SETLABEL")
		{
			int row = get_arg_int(message,1);
			int col = get_arg_int(message,2);
			if(pNew->TD.displays[row][col])
			{
				pNew->TD.displays[row][col] = pNew->TD.displays[row][col]->SetLabel(get_arg(message,3,false));
				bChanged = true;
			}
		}
		else if(cmd == "SETPROFILE")
		{
			pNew->TD.sProfName = get_arg(message, 1);
			bChanged = pNew->TD.sProfName != pOld->TD.sProfName;
		}
		else if(cmd == "SETLAYOUT")
		{
			pNew->UpdateFromMessage(message);
			bChanged = !pNew->LayoutEqual(pOld);
		}
		else
		{
			//Unknwon command, just NACK
			conn.write_line("NACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
			return 1;
		}
		if(bChanged)
		{
			if(bSizeChanged)
				pNew->TD.textSize = -1;
			(*pRegions)[regionIndex] = pNew;
			pGlobalRegions = pRegions;
		}
	}
	conn.write_line("ACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
	return 1;
}

void tcp_thread(const std::string remote_host, bool * pbComms)
{
	int retryDelay = 0;
	while(bRunning)
	{
		try
		{
			*pbComms = false;
			client conn;
			//Allow 30 seconds for connection
			conn.connect(remote_host, TALLY_SERVICE,
				boost::posix_time::time_duration(0,0,30,0));
			while(bRunning)
			{
				//Nothing for 5 seconds should prompt a reconnect
				std::string data = conn.read_line(
				boost::posix_time::time_duration(0,0,5,0),'\r');

				int ret = handle_tcp_message(data, conn);
				if(ret == 0)
					break;
				if(ret != 3) //Login attempt might not be good
				{
					retryDelay = 0;
					*pbComms = true;
				}
			}
		}
		catch(...)
		{}
		if(retryDelay++ > 30)
			retryDelay = 30;
		sleep(retryDelay);
	}
}


void create_tcp_threads()
{
	//Determin MAC address...
	struct ifreq s;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	strcpy(s.ifr_name, "eth0");
	if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
	{
		char mac_buf[80];
		sprintf(mac_buf,"%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned char) s.ifr_addr.sa_data[0],
				(unsigned char) s.ifr_addr.sa_data[1],
				(unsigned char) s.ifr_addr.sa_data[2],
				(unsigned char) s.ifr_addr.sa_data[3],
				(unsigned char) s.ifr_addr.sa_data[4],
				(unsigned char) s.ifr_addr.sa_data[5]);
		mac_address = mac_buf;
	}
	else
	{
		mac_address = "UNKNOWN";
	}
	for(unsigned int i = 0; i < tally_hosts.size(); i++)
	{
		std::thread t(tcp_thread, tally_hosts[i], &(bComms[i]));
		t.detach();
	}
}

void read_settings(const std::string & filename,
		   po::variables_map& vm)
{
	po::options_description desc("Options");
	desc.add_options()
		("tally_mode", po::value<int>(&GPI_MODE)->default_value(0), "Tally Mode, 0=disabled, 1=PiFace Digital, 2=TCP/IP")
		("tally_remote_host", po::value<std::vector<std::string>>(&tally_hosts), "Remote tally host, may be specified multiple times for multiple connections")
		("tally_remote_port", po::value<std::string>(&TALLY_SERVICE)->default_value("6254"), "Port (or service) to connect to on (default 6254)")
		("tally_shared_secret", po::value<std::string>(&TALLY_SECRET)->default_value("SharedSecretGoesHere"), "Shared Secret (password) for connecting to tally service")
	;
	
	std::ifstream settings_file(filename.c_str());
	
	vm = po::variables_map();

	po::store(po::parse_config_file(settings_file, desc), vm);
	po::notify(vm);
}

void clearRegions()
{
	RegionsMap pRegions(new std::map<int,boost::shared_ptr<RegionState>>());
	(*pRegions)[0] = boost::shared_ptr<RegionState>(new RegionState());
	pGlobalRegions = pRegions;
}

int main(int argc, char *argv[]) {
	int iwidth, iheight;
	fd_set rfds;
	struct timeval timeout;
	VGfloat commsWidth = -1;
	VGfloat commsTextHeight = 0;
	time_t tm_last_comms_good = 0;
	timeout.tv_usec = timeout.tv_sec = 0;
	std::string configFile = "piclock.cfg";
	std::string last_date_string;
	clearRegions();
	if(argc > 1)
		configFile = argv[1];
	po::variables_map vm;
	read_settings(configFile, vm);
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);
	

	init(&iwidth, &iheight);					// Graphics initialization

	struct timeval tval;
	struct tm tm_local, tm_utc;

	VGfloat offset = iheight*.05f;

	srand(time(NULL));
	int vrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	int hrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	VGfloat h_offset_pos = 0;
	VGfloat v_offset_pos = 0;
	long prev_sec = 0;
	ntpstate_t ntp_state_data;
	init_ntp_state();
	get_ntp_state(&ntp_state_data);
	pthread_t ntp_thread;
	pthread_attr_t ntp_attr;
	pthread_attr_init(&ntp_attr);
	pthread_create(&ntp_thread, &ntp_attr, &ntp_check_thread, &ntp_state_data);
	if(GPI_MODE == 1)
		pifacedigital_open(0);
	else if(GPI_MODE == 2)
		create_tcp_threads();
	while(1)
	{
		//Copy reference to current set of states, this then shouldn't change whilst we're processing it...
		RegionsMap pRegions = pGlobalRegions;
		gettimeofday(&tval, NULL);
		localtime_r(&tval.tv_sec, &tm_local);
		gmtime_r(&tval.tv_sec, &tm_utc);
		bool bFirst = true;

		Start(iwidth, iheight);					// Start the picture
		Background(0, 0, 0);					// Black background
		VGfloat display_width = iwidth;
		VGfloat display_height = iheight;
		if(globalState.ScreenSaver())
		{
			display_width -= offset;
			display_height -= offset;
		}
		if(globalState.RotationReqd(display_width,display_height))
		{
			Rotate(90);
			//After rotating our output has now disappeared down below our co-ordinate space, so translate our co-ordinate space down to find it...
			Translate(0,-iwidth);
			std::swap(display_width,display_height);
		}

		//Screen saver offsets...
		if(globalState.ScreenSaver())
		{
			if(tval.tv_sec != prev_sec)
			{
				prev_sec =  tval.tv_sec;
				h_offset_pos = abs(prev_sec%(hrate*2) - hrate)*offset/hrate;
				v_offset_pos = abs(prev_sec%(vrate*2) - vrate)*offset/vrate;
			}
			Translate(h_offset_pos, v_offset_pos);
		}
		for(const auto & region : *pRegions)
		{
			std::string profName;
			int i;
			boost::shared_ptr<RegionState> pRS = region.second;

			VGfloat inner_height = display_height * pRS->height();
			VGfloat inner_width = display_width * pRS->width();

			VGfloat return_x = display_width *pRS->x();
			VGfloat return_y = display_height *pRS->y();
			Translate(return_x, return_y);

			if(pRS->RecalcDimensions(tm_utc, tm_local, inner_width, inner_height, display_width, display_height, bFirst))
			{
				//Force recalc...
				commsWidth = -1;
				pRS->TD.textSize = -1;
			}
			Fill(255, 255, 255, 1);					// white text
			Stroke(255, 255, 255, 1);				// White strokes
			StrokeWidth(0);//Don't draw strokes until we've moved onto the analogue clock

			//Write out the text
			DisplayBox db;
			int pointSize;
			std::string prefix;
			if(pRS->DigitalLocal(db, pointSize, prefix))
			{
				std::string time_str = prefix + FormatTime(tm_local,tval.tv_usec);
				db.TextMidBottom(time_str.c_str(), FONT_MONO, pointSize);
			}
			if(pRS->DigitalUTC(db, pointSize, prefix))
			{
				std::string time_str = prefix + FormatTime(tm_utc,tval.tv_usec);
				db.TextMidBottom(time_str.c_str(), FONT_MONO, pointSize);
			}
			bool bLocal;
			if(pRS->Date(db, bLocal, pointSize))
			{
				db.TextMidBottom(FormatDate(bLocal? tm_local: tm_utc),
							FONT_PROP, pointSize);
			}

			if(GPI_MODE == 1)
			{
				if(pRS->TD.nRows != 2)
					pRS->TD.textSize = -1;
				pRS->TD.nRows = 2;
				pRS->TD.nCols_default = 1;
				pRS->TD.nCols.empty();
				uint8_t gpis = pifacedigital_read_reg(INPUT,0);
				uint8_t colour_weight = (gpis & 1) ? 50:255;
				uint8_t fill_weight = (gpis & 1)? 50:255;
				pRS->TD.displays[0][0].reset(
					new SimpleTallyState(
					    TallyColour(fill_weight, fill_weight, fill_weight),
					    TallyColour(colour_weight, colour_weight*0.55,0),
					    "Mic Live"));
				colour_weight = (gpis & 2) ? 50:255;
				fill_weight = (gpis & 2)? 50:255;
				pRS->TD.displays[1][0].reset(
					new SimpleTallyState(
					    TallyColour(fill_weight, fill_weight, fill_weight),
					    TallyColour(colour_weight, colour_weight*0.55,0),
					    "On Air"));
			}

			//Draw NTP sync status
			db = pRS->StatusBox(pointSize);
			if(db.w > 0.0f && db.h > 0.0f)
			{
				Fill(255,255,255,1);
				const char * sync_text;
				if(ntp_state_data.status == 0)
				{
					Fill(0,100,0,1);
					sync_text = "Synchronised";
				}
				else
				{
					/* flash between red and purple once a second */
					Fill(120,0,(tval.tv_sec %2)*120,1);
					if(ntp_state_data.status == 1)
						sync_text = "Synchronising..";
					else
						sync_text = "Unknown Synch!";
				}
				const char * header_text = "NTP-Derived Clock";
				auto statusBoxLen = std::max(TextWidth(sync_text, FONT_PROP, pointSize),
							     TextWidth(header_text, FONT_PROP, pointSize))*1.1f;
				auto statusTextHeight = TextHeight(FONT_PROP, pointSize);
				//Draw a box around NTP status
				Rect(db.x + db.w - statusBoxLen, db.y, statusBoxLen, db.h);
				Fill(200,200,200,1);
				//2 bits of text
				TextMid(db.x + db.w - statusBoxLen*.5f, db.y + statusTextHeight *.5f, sync_text, FONT_PROP, pointSize);
				TextMid(db.x + db.w - statusBoxLen*.5f, db.y + statusTextHeight*1.8f, header_text, FONT_PROP, pointSize);

				db.w -= statusBoxLen;

				if(GPI_MODE == 2)
				{
					//Draw comms status
					if(commsWidth < 0)
					{
						commsWidth =
							std::max(TextWidth("Comms OK", FONT_PROP, pointSize),

								 TextWidth("Comms Failed", FONT_PROP, pointSize));
						for(unsigned int window = 0; window < tally_hosts.size(); window++)
						{
							char buf[512];
							buf[511] = '\0';
							snprintf(buf, 511, "Tally Server %d", window + 1);
							commsWidth = std::max(commsWidth, TextWidth(buf, FONT_PROP, pointSize));
						}
						commsWidth *= 1.2f;
						commsTextHeight = TextHeight(FONT_PROP, pointSize);
					}
					bool bAnyComms = false;
					for(unsigned int window = 0; window < tally_hosts.size(); window++)
					{
						bAnyComms = bAnyComms || bComms[window];
						if(bComms[window])
							Fill(0,100,0,1);
						else
							Fill(190,0,0,1);
						VGfloat base_x = db.x + db.w - commsWidth * (VGfloat)(tally_hosts.size()-window);
						VGfloat base_y = db.y;
						Rect( base_x, base_y, commsWidth, db.h);
						Fill(200,200,200,1);
						char buf[512];
						buf[511] = '\0';
						base_x += commsWidth*.5f;
						snprintf(buf, 511, "Tally Server %d", window + 1);
						TextMid(base_x, base_y + commsTextHeight*1.8f, buf, FONT_PROP, pointSize);
						snprintf(buf, 511, "Comms %s", bComms[window]? "OK" : "Failed");
						TextMid(base_x, base_y + commsTextHeight*0.4f, buf, FONT_PROP, pointSize);
					}
					if(bAnyComms)
					{
						tm_last_comms_good = tval.tv_sec;
					}
					//Stop showing stuff after 5 seconds of comms failed...
					else if(tm_last_comms_good < tval.tv_sec - 5)
					{
						//clearRegions();
						auto allRegions = pGlobalRegions;
						for(auto & reg: *allRegions)
						{
							boost::shared_ptr<RegionState> newRS(new RegionState(*(reg.second)));
							newRS->TD.clear();
							reg.second = newRS;
						}
						pGlobalRegions = allRegions;
					}
				}
			}
			if(GPI_MODE)
			{
				profName = pRS->TD.sProfName;
				if(pRS->TD.nRows > 0)
				{
					DisplayBox db = pRS->TallyBox();
					VGfloat row_height = (db.h)/((VGfloat)pRS->TD.nRows);
					VGfloat buffer = row_height*.02f;
					VGfloat col_width_default = db.w/((VGfloat)pRS->TD.nCols_default);
					if(pRS->TD.textSize < 0)
					{
						pRS->TD.textSize = 0xFFFFFFF;
						for(int row = 0; row < pRS->TD.nRows; row++)
						{
							int col_count = pRS->TD.nCols[row];
							VGfloat col_width = col_width_default;
							if(col_count <= 0)
								col_count = pRS->TD.nCols_default;
							else
								col_width = db.w/((VGfloat)col_count);
							for(int col = 0; col < col_count; col++)
							{
								auto item = pRS->TD.displays[row][col];
								if(!item)
									continue;
								auto maxItemSize = MaxPointSize(col_width * .9f, row_height * (item->Label(tval)? .6f :.9f), item->Text(tval)->c_str(), FONT(item->IsMonoSpaced()));
								pRS->TD.textSize = std::min(pRS->TD.textSize, maxItemSize);
							}
						}
					}
					//float y_offset = height/10.0f;
					for(int row = 0; row < pRS->TD.nRows; row++)
					{
						VGfloat base_y = ((VGfloat)row)*row_height + db.y;
						int col_count = pRS->TD.nCols[row];
						VGfloat col_width = col_width_default;
						if(col_count <= 0)
							col_count = pRS->TD.nCols_default;
						else
							col_width = db.w/((VGfloat)col_count);
						for(int col = 0; col < col_count; col++)
						{
							auto curTally = pRS->TD.displays[row][col];
							if(!curTally)
								continue;
							DisplayBox dbTally(((VGfloat)col)*col_width + db.w/100.0f, base_y, col_width - buffer, row_height - buffer);
							curTally->BG(tval)->Fill();
							dbTally.Roundrect(row_height/10.0f);
							curTally->FG(tval)->Fill();
							dbTally.TextMid(curTally->Text(tval)->c_str(), FONT(curTally->IsMonoSpaced()), pRS->TD.textSize, curTally->Label(tval));
						}
					}
				}
			}
			//Done with displays, back to common code...
			db = pRS->StatusBox(pointSize);
			if(db.w > 0.0f && db.h > 0.0f)
			{
				Fill(127, 127, 127, 1);
				int quitSize = pointSize/1.5f;
				Text(db.x, db.y, "Press Ctrl+C to quit", FONT_PROP, quitSize);
				if(GPI_MODE == 2)
				{
					char buf[8192];
					buf[sizeof(buf)-1] = '\0';
					if(profName.empty())
						snprintf(buf, sizeof(buf) -1, "MAC Address %s", mac_address.c_str());
					else
						snprintf(buf, sizeof(buf) -1, "MAC Address %s, %s", mac_address.c_str(), profName.c_str());
					Text(db.x, db.y + TextHeight(FONT_PROP,quitSize)*1.5f, buf, FONT_PROP, pointSize);
				}
			}
			Fill(255,255,255,1);
			int numbers;
			boost::shared_ptr<const std::map<int, VGfloat>> hours_x;
			boost::shared_ptr<const std::map<int, VGfloat>> hours_y;
			if(pRS->AnalogueClock(db, bLocal, hours_x, hours_y, numbers))
			{
				//Right, now start drawing the clock

				//Move to the centre of the clock
				VGfloat move_x = db.x + db.w/2.0f;
				VGfloat move_y = db.y + db.h/2.0f;
				
				Translate(move_x, move_y);
				return_x += move_x;
				return_y += move_y;

				if(numbers)
					//Write out hour labels around edge of clock
					for(i = 0; i < 12; i++)
					{
						char buf[5];
						sprintf(buf, "%d", i? i:12);
						TextMid(hours_y->at(i),hours_x->at(i), buf, FONT_HOURS, db.h/15.0f);
					}
				//Go around drawing dashes around edge of clock
				StrokeWidth(db.w/100.0f);
				VGfloat start, end_short, end_long;
				VGfloat min_dim = std::min(db.h,db.w);
				if(numbers == 1)
				{
					start = min_dim *7.5f/20.0f;
					end_short = min_dim *6.7f/20.0f;
					end_long = min_dim *6.3f/20.0f;
				}
				else
				{
					start = min_dim *9.5f/20.0f;
					end_short = min_dim *8.8f/20.0f;
					end_long = min_dim *8.4f/20.0f;
				}
#ifndef NO_COLOUR_CHANGE
				Stroke(255,0,0,1);
#endif
#define tm_now	(bLocal? tm_local : tm_utc)
				//As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
				for(i = 0; i < 60; i++)
				{
					if((i %5) == 0)
						Line(0, start, 0, end_long);
					else
						Line(0, start, 0, end_short);
					Rotate(-6);
#ifndef NO_COLOUR_CHANGE
					//Fade red slowly out over first second
					if(i == tm_now.tm_sec)
					{
						if(i < 1)
						{
							//Example to fade over 2 seconds...
							//VGfloat fade = 128.0f * tm_now.tm_sec +  127.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
							VGfloat fade = 255.0f * ((VGfloat)tval.tv_usec)/200000.0f;
							if(fade > 255.0f)
								fade = 255.0f;
							Stroke(255, fade, fade, 1);
						}
						else
							Stroke(255,255,255,1);
					}
#endif
				}
				Stroke(255,255,255,1);
				//Again, rotate co-ordinate space so we're just drawing an upright line every time...
				StrokeWidth(db.w/200.0f);
				//VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
				VGfloat sec_rotation = -(6.0f * tm_now.tm_sec);
				VGfloat sec_part = sec_rotation;
				if(tval.tv_usec > MOVE_HAND_AT)
					sec_rotation -= ((VGfloat)(tval.tv_usec - MOVE_HAND_AT)*6.0f/100000.0f);
				//Take into account microseconds when calculating position of minute hand (and to minor extent hour hand), so it doesn't jump every second
				sec_part -= ((VGfloat)tval.tv_usec)*6.0f/1000000.0f;
				VGfloat min_rotation = -6.0f *tm_now.tm_min + sec_part/60.0f;
				VGfloat hour_rotation = -30.0f *tm_now.tm_hour + min_rotation/12.0f;
				Rotate(hour_rotation);
				Line(0,0,0,min_dim/4.0f); /* half-length hour hand */
				Rotate(min_rotation - hour_rotation);
				Line(0,0,0,min_dim/2.0f); /* minute hand */
				Rotate(sec_rotation - min_rotation);
				Stroke(255,0,0,1);
				Line(0,-db.h/10.0f,0,min_dim/2.0f); /* second hand, with overhanging tail */
				//Draw circle in centre
				Fill(255,0,0,1);
				Circle(0,0,db.w/150.0f);
				Rotate(-sec_rotation);
				//Now draw some dots for seconds...
#ifdef SECOND_DOTS
				StrokeWidth(db.w/100.0f);
				Stroke(0,0,255,1);
				VGfloat pos = db.h*7.9f/20.0f;
				for(i = 0; i < 60; i++)
				{
					if(i <= tm_now.tm_sec)
					{
						Line(0, pos, 0, pos -10);
					}
					Rotate(-6);
				}
#endif

			}
			//Translate back to the origin...
			if(return_x != 0.0 || return_y != 0.0)
				Translate(-return_x, -return_y);
			bFirst = false;
		}
		End();						   			// End the picture
	}

	bRunning = 0;
	finish();					            // Graphics cleanup
	exit(0);
}
