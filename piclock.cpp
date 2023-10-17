// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <fstream>
#include <map>
#include <queue>
#include <cctype>
#include <memory>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <boost/program_options.hpp>
#include <netdb.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <Magick++.h>
#include "nanovg.h"
#include "ntpstat/ntpstat.h"
//Bit of a bodge, but generating a header file for this example would be a pain
#include "blocking_tcp_client.cpp"
//piface digital
#include "pifacedigital.h"
#include "piclock_messages.h"
#include "nvg_main.h"


#define FPS 25
#define FRAMES 0
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000

namespace po = boost::program_options;

std::string mac_address;
int bRunning = 1;
std::map<unsigned int,bool> bComms;
int GPI_MODE = 0;
int init_window_width = 0;
int init_window_height = 0;
std::string TALLY_SERVICE("6254");
std::string TALLY_SECRET("SharedSecretGoesHere");
std::string clean_exit_file("/tmp/piclock_clean_exit");
std::vector<std::string> tally_hosts;

class RegionState;
typedef std::map<int,std::shared_ptr<RegionState>> RegionsMap;
class ScalingImage;
typedef std::map<std::string, ScalingImage> ImagesMap;
void DrawFrame(NVGcontext *vg, int iwidth, int iheight);

typedef std::string Fontinfo;

#define FONT_PROP	"SerifTypeface"
#define FONT_HOURS	"SansTypeface"
#define FONT_MONO	"MonoTypeface"
#define FONT(x)		((x)? FONT_MONO : FONT_PROP)


void Rotate(NVGcontext *vg, float degrees)
{
	nvgRotate(vg, degrees* M_PI / 180.0f);
}

void Roundrect(NVGcontext *vg, float x, float y, float w, float h, float r)
{
	nvgBeginPath(vg);
	nvgRoundedRect(vg,x,y,w,h,r);
	nvgFill(vg);
}

void Rect(NVGcontext *vg, float x, float y, float w, float h)
{
	nvgBeginPath(vg);
	nvgRect(vg,x,y,w,h);
	nvgFill(vg);
}

void Line(NVGcontext *vg, float x1, float y1, float x2, float y2)
{
	nvgBeginPath(vg);
	nvgMoveTo(vg,x1,y1);
	nvgLineTo(vg,x2,y2);
	nvgStroke(vg);
}

static std::map<Fontinfo,float> fontHeights;

VGfloat TextHeight(NVGcontext *vg, const Fontinfo &f, int pointsize)
{
	static unsigned char *allChars = NULL;
	auto iter = fontHeights.find(f);
	float retBase;
	if(iter == fontHeights.end())
	{
		if(allChars == NULL)
		{
			allChars = new unsigned char[256];
			for(unsigned char i = 0; i < 255; i++)
			{
				allChars[255-i] = i;
			}
			allChars[0] = 255;
		}
		float bounds[4] = {0,0,0,0};
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
		nvgFontFace(vg, f.c_str());
		//nvgTextBounds seems to round to the nearest whole number, so go for a really big font size for our cache, powers of 2 are easier to divide down accurately
		nvgFontSize(vg, 1024);
		nvgTextBounds(vg, 0,0, (char *)allChars, NULL, bounds);
		retBase = fontHeights[f] = bounds[3];
	}
	else
	{
		retBase = iter->second;
	}
	return (retBase * (float)pointsize)/1024.f;
}

VGfloat TextWidth(NVGcontext *vg, const char * str, const Fontinfo &f, int pointsize)
{
	float bounds[4] = {0,0,0,0};
	nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextBounds(vg, 0,0, str, NULL, bounds);
	return abs(bounds[2]);
}

void TextMid(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
	nvgText(vg, x, y, s, NULL);
}

void Text(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
	nvgText(vg, x, y, s, NULL);
}

void drawimage(NVGcontext *vg, float x, float y, float w, float h, int img_handle)
{
	NVGpaint imgPaint = nvgImagePattern(vg, x, y, w, h,0,img_handle,1.0f);
	nvgBeginPath(vg);
	nvgRect(vg, x, y, w, h);
	nvgFillPaint(vg, imgPaint);
	nvgFill(vg);
}



std::shared_ptr<std::map<std::pair<std::string, float>, std::string> > clippedTextCache;
std::shared_ptr<std::map<std::pair<std::string, float>, std::string> > prevClippedTextCache;

void TextClip(NVGcontext *vg, float x, float y, const std::string &s, const Fontinfo &f, int pointsize, float clipwidth, const std::string &clip_str)
{
	std::string output_str = s;
	bool bThisCache = true;
	const auto cacheKey = std::pair<std::string, float>(f + "_FONT_" + s, clipwidth/(float) pointsize);
	auto iter = clippedTextCache->find(cacheKey);
	if(iter == clippedTextCache->end())
	{
		bThisCache = false;
		iter = prevClippedTextCache->find(cacheKey);
	}
	if(iter == prevClippedTextCache->end())
	{
		//nvgTextBounds seems to round to the nearest whole number, so go for a really big font size for our calculations, powers of 2 are easier to divide/multiply accurately
		float calcWidth = 1024.f*clipwidth / (float)pointsize; //Genericise for all point sizes
		//Do the work...
		float baseWidth = TextWidth(vg, s.c_str(), f, 1024);
		if(baseWidth > calcWidth)
		{
			std::string final_clip_str = clip_str;
			if(final_clip_str.length() > 0)
			{
				float clipLen = TextWidth(vg, final_clip_str.c_str(), f, 1024);
				if(clipLen >= calcWidth)
					final_clip_str = std::string();
				else
					calcWidth -= clipLen;
			}
			float string_percentage = calcWidth / baseWidth;
			std::string::size_type newLen = (unsigned)(((float)s.length())*string_percentage);
			float curWidth;
			//First find longest string which is too long from this point
			do
			{
				output_str = s.substr(0,newLen);
				curWidth = TextWidth(vg, output_str.c_str(), f, 1024);
			}
			while(curWidth < calcWidth && (++newLen <= s.length()));
			//Then work down to first string that is short enough from this point
			do
			{
				output_str = s.substr(0,newLen);
				curWidth = TextWidth(vg, output_str.c_str(), f, 1024);
			}
			while(curWidth > calcWidth && (--newLen > 0));
			output_str += final_clip_str;
		}
	}
	else
	{
		output_str = iter->second;
	}
	//Item has been used, from prev cache or generated, so re-cache for next pass
	if(!bThisCache)
	{
		(*clippedTextCache)[cacheKey] = output_str;
	}
	Text(vg,x,y,output_str.c_str(),f,pointsize);
}

void TextMidBottom(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM);
	nvgText(vg, x, y, s, NULL);
}


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

class MessageQueue
{
public:
	void Add(const std::shared_ptr<ClockMsg> & pMsg)
	{
		std::lock_guard<std::mutex> hold_lock(m_access_mutex);
		m_queue.push(pMsg);
	}
	bool Get(std::queue<std::shared_ptr<ClockMsg> > &output)
	{
		std::lock_guard<std::mutex> hold_lock(m_access_mutex);
		if(!m_queue.empty())
		{
			output.swap(m_queue);
			std::queue<std::shared_ptr<ClockMsg> > empty_q;
			m_queue.swap(empty_q);
			return true;
		}
		return false;
	}
private:
	std::queue<std::shared_ptr<ClockMsg> > m_queue;
	std::mutex m_access_mutex;
};

class ResizedImage: public ClockMsg
{
public:
	std::shared_ptr<Magick::Image> pSource;
	Magick::Geometry Geom;
	std::shared_ptr<Magick::Blob> pOutput;
	std::string Name;
	bool bQuick;
	ResizedImage(const Magick::Geometry & geom, std::shared_ptr<Magick::Image> & pSrc, const std::string & name, bool quick)
	:pSource(pSrc),Geom(geom),Name(name), bQuick(quick)
	{}
	void DoResize()
	{
		static Magick::Color black = Magick::Color(0,0,0,0);
		Magick::Image scaled(*pSource);
		scaled.flip();
		if(bQuick)
			scaled.filterType(Magick::PointFilter);
		scaled.resize(Geom);
		scaled.extent(Geom, black, Magick::CenterGravity);
		pOutput = std::make_shared<Magick::Blob>();
		scaled.write(pOutput.get(), "RGBA", 8);
	}
};

class ResizeQueue
{
public:
	void Add(const std::shared_ptr<ResizedImage> & pMsg)
	{
		std::lock_guard<std::mutex> hold_lock(m_access_mutex);
		m_queue.push(pMsg);
		m_new_data_condition.notify_all();
	}
	std::shared_ptr<ResizedImage> Get()
	{
		std::unique_lock<std::mutex> hold_lock(m_access_mutex);
		while(bRunning && m_queue.empty())
		{
			m_new_data_condition.wait(hold_lock);
		}
		if(bRunning)
		{
			auto front = m_queue.front();
			m_queue.pop();
			return front;
		}
		return std::shared_ptr<ResizedImage>();
	}
	void Abort()
	{
		m_new_data_condition.notify_all();
	}
private:
	std::queue<std::shared_ptr<ResizedImage> > m_queue;
	std::mutex m_access_mutex;
	std::condition_variable m_new_data_condition;
};

MessageQueue msgQueue;

ResizeQueue resizeQueue;

void background_resize_thread()
{
	while(bRunning)
	{
		auto data = resizeQueue.Get();
		if(data)
		{
			data->DoResize();
			//If this is a quick resize, then schedule a slow resize too
			if(data->bQuick)
				resizeQueue.Add(std::make_shared<ResizedImage>(data->Geom, data->pSource, data->Name, false));
			msgQueue.Add(data);
		}
	}
}

class ScalingImage
{
public:
	ScalingImage(std::shared_ptr<Magick::Image> pSrc, std::shared_ptr<Magick::Blob> pBlob)
	:pSource(pSrc), pSourceBlob(pBlob)
	{}
	//Empty constructor required for std::map, shouldn't get called;
	ScalingImage()
	{}
	bool IsValid()
	{
		return (bool)pSource;
	}
	const int GetImage(NVGcontext* vg, int w, int h, const std::string & name)
	{
		savedVg = vg;
		if(!IsValid())
			return 0;
		Magick::Geometry geom(w, h);
		const auto & iter = Scaled.find(geom);
		if(iter != Scaled.end())
		{
			if(iter->second.blob)
			{
				if(iter->second.handle == 0)
					iter->second.handle = nvgCreateImageRGBA(vg, w, h, 0, (const unsigned char *)iter->second.blob->data());
				return iter->second.handle;
			}
		}
		else
		{
			std::shared_ptr<ResizedImage> pResize = std::make_shared<ResizedImage>(geom, pSource, name, true);
			Scaled[geom] = ScaledData();
			resizeQueue.Add(pResize);
		}
		return 0;
	}
	void UpdateFromResize(std::shared_ptr<ResizedImage> pResize)
	{
		//Refuse to do this if our image has changed since then...
		if(pResize && pResize->pOutput && pResize->pSource == pSource)
		{
			const auto & geom = pResize->Geom;
			const auto & iter = Scaled.find(geom);
			if(iter == Scaled.end())
			{
				//Impossible happened, data is created before job is scheduled
				return;
			}
			if(savedVg != NULL && iter->second.handle != 0)
			{
				nvgDeleteImage(savedVg, iter->second.handle);
			}
			iter->second.handle = 0;
			iter->second.blob = pResize->pOutput;
		}
	}
	bool IsSameSource(std::shared_ptr<Magick::Blob> pOtherBlob)
	{
		return IsValid() && pSourceBlob && pOtherBlob 
			&& (pOtherBlob->length() == pSourceBlob->length())
			&& (memcmp(pOtherBlob->data(), pSourceBlob->data(), pSourceBlob->length()) == 0);
	}
	~ScalingImage()
	{
		if(savedVg == NULL)
			return;
		for(const auto &item : Scaled)
		{
			nvgDeleteImage(savedVg, item.second.handle);
		}
	}
private:
	class ScaledData
	{
		public:
			ScaledData() : handle(0)
			{}
			std::shared_ptr<Magick::Blob> blob;
			int handle;
	};
	NVGcontext *savedVg = NULL;
	std::shared_ptr<Magick::Image> pSource;
	std::shared_ptr<Magick::Blob> pSourceBlob;
	std::map<Magick::Geometry, ScaledData> Scaled;
};


class TallyState
{
public:
	virtual std::shared_ptr<TallyColour> FG(const struct timeval &curTime) const = 0;
	virtual std::shared_ptr<TallyColour> BG(const struct timeval &curTime) const = 0;
	virtual std::shared_ptr<std::string> Text(const struct timeval &curTime) const = 0;
	virtual std::shared_ptr<std::string> Label(const struct timeval &curTime) const = 0;
	virtual std::shared_ptr<TallyState>  SetLabel(const std::string &lbl) const = 0;
	virtual bool IsMonoSpaced() const = 0;
	virtual bool Equals(std::shared_ptr<TallyState> other) const = 0;
};

class SimpleTallyState  :  public TallyState
{
public:
	std::shared_ptr<TallyColour> FG(const struct timeval &curTime) const override
	{
		return m_FG;
	}
	std::shared_ptr<TallyColour> BG(const struct timeval &curTime) const override
	{
		return m_BG;
	}
	std::shared_ptr<std::string> Text(const struct timeval &curTime) const override
	{
		return m_text;
	}
	std::shared_ptr<std::string> Label(const struct timeval &curTime) const override
	{
		return m_label;
	}
	std::shared_ptr<TallyState> SetLabel(const std::string & label) const override
	{
		//If it's identical, then return empty pointer
		if((!m_label && label.empty())
	           ||(m_label && (label == *m_label)))
			return std::shared_ptr<TallyState>();
		std::shared_ptr<TallyState> ret = std::make_shared<SimpleTallyState>(*this);
		auto derived = dynamic_cast<SimpleTallyState *>(ret.get());
		if(!label.empty())
			derived->m_label = std::make_shared<std::string>(label);
		else
			derived->m_label = std::shared_ptr<std::string>();
		return ret;
	}
	bool IsMonoSpaced() const override
	{
		return false;
	}

	bool Equals(std::shared_ptr<TallyState> other) const override
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

	SimpleTallyState(const std::shared_ptr<ClockMsg_SetTally> &pMsg, const std::shared_ptr<TallyState> & pOld)
		:SimpleTallyState(*pMsg, pOld)
	{
	}

	SimpleTallyState(const ClockMsg_SetTally &msg, const std::shared_ptr<TallyState> & pOld)
		:SimpleTallyState(msg.colFg, msg.colBg, msg.sText, pOld)
	{
	}

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text)
	 :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
	{}
	SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text)
	 :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
	{}

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old)
	 :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
	{
		auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
		if(derived != NULL)
			m_label = derived->m_label;
		else
			m_label = std::shared_ptr<std::string>();
	}
	SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old)
	 :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
	{
		auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
		if(derived != NULL)
			m_label = derived->m_label;
		else
			m_label = std::shared_ptr<std::string>();
	}
protected:
	std::shared_ptr<TallyColour> m_FG, m_BG;
	std::shared_ptr<std::string> m_text, m_label;
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
	std::shared_ptr<TallyColour> FG(const struct timeval &curTime) const override
	{
		if(Invert(curTime))
			return SimpleTallyState::BG(curTime);
		return SimpleTallyState::FG(curTime);
	}
	std::shared_ptr<TallyColour> BG(const struct timeval &curTime) const override
	{
		if(Invert(curTime))
			return SimpleTallyState::FG(curTime);
		return SimpleTallyState::BG(curTime);
	}
	std::shared_ptr<std::string> Text(const struct timeval &curTime) const override
	{
		time_t secs = SecsLeft(curTime,m_target);
		char buf[256];
		char negChar = (secs < 0)? '-': ' ';
		secs = std::abs(secs);
		snprintf(buf, sizeof(buf) - 1, "%c%02ld:%02ld:%02ld",
			 negChar, secs/3600, (secs/60)%60, secs %60);
		return std::make_shared<std::string>(buf);
	}
	std::shared_ptr<TallyState> SetLabel(const std::string & label) const override
	{
		return std::shared_ptr<TallyState>();
	}
	std::shared_ptr<std::string> Label(const struct timeval &curTime) const override
	{
		return SimpleTallyState::Text(curTime);
	}
	bool IsMonoSpaced() const override
	{
		return true;
	}

	bool Equals(std::shared_ptr<TallyState> other) const override
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

	CountdownClock(const std::shared_ptr<ClockMsg_SetCountdown> &pMsg)
		:CountdownClock(*pMsg)
	{}

	CountdownClock(const ClockMsg_SetCountdown &msg)
		:CountdownClock(msg.colFg, msg.colBg, msg.sText, msg.target, msg.bHasFlashLimit? std::make_shared<long long>(msg.iFlashLimit) : std::shared_ptr<long long>())
	{}

	CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const struct timeval & _target, std::shared_ptr<long long> pflash)
	 :SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
	{}
	CountdownClock(const TallyColour & fg, const TallyColour &bg, const std::string &_label, const struct timeval &_target, std::shared_ptr<long long> pflash)
	 :SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
	{}
private:
	struct timeval m_target;
	std::shared_ptr<long long> m_pFlashLimit;
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
		//We want equivalent of ceil()
		auto ret = left.tv_sec;
		if(left.tv_usec > 0)
			ret += 1;
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
	std::map<int,std::map<int, std::shared_ptr<TallyState> > > displays;
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
	VGfloat top_y() const
	{
		return y - h;
	}
	VGfloat mid_x() const
	{
		return x + w/2.0f;
	}
	VGfloat mid_y() const
	{
		return y - h/2.0f;
	}
	void TextMid(NVGcontext * vg, const std::shared_ptr<std::string> &str, const Fontinfo & font, const int pointSize, int labelSize = -1, std::shared_ptr<std::string> label = std::shared_ptr<std::string>())
	{
		if(str)
			TextMid(vg,*str,font,pointSize,labelSize,label);
		else
			TextMid(vg,std::string(),font,pointSize,labelSize,label);

	}
	void TextMid(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, int labelSize = -1, std::shared_ptr<std::string> label = std::shared_ptr<std::string>())
	{
		VGfloat labelHeight = 0.0f;
		if(label)
		{
			if(labelSize <= 0)
				labelSize = pointSize/3;
			labelHeight = TextHeight(vg, FONT_PROP, labelSize);
			auto label_y = y - h + labelHeight;
			::TextClip(vg, x + m_corner, label_y,
				*label, FONT_PROP, labelSize, w - m_corner, "...");
		}
		auto text_height = TextHeight(vg, font, pointSize);
		VGfloat text_y = mid_y() + text_height *.1f + labelHeight*0.5f;
				
		::TextMid(vg, mid_x(), text_y, str.c_str(), font, pointSize);
	}
	void TextMidBottom(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, std::shared_ptr<std::string> label = std::shared_ptr<std::string>())
	{
		if(label)
		{
			auto label_y = y - h + TextHeight(vg, FONT_PROP, pointSize/3);
			::Text(vg, x + w*.05f, label_y,
				label->c_str(), FONT_PROP, pointSize/3);
		}
				
		::TextMidBottom(vg, mid_x(), y - h *.05f, str.c_str(), font, pointSize);
	}
	void Roundrect(NVGcontext *vg, VGfloat corner)
	{
		m_corner = corner;
		::Roundrect(vg, x, top_y(), w, h, corner);
	}
	void Rect(NVGcontext * vg)
	{
		m_corner = 0;
		::Rect(vg, x, top_y(), w, h);
	}
	void Zero()
	{
		m_corner = x = y = w = h = 0;
	}
	private:
	VGfloat m_corner;
};

int MaxPointSize(NVGcontext *vg, VGfloat width, VGfloat height, const std::string & text, const Fontinfo & f)
{
	int ret = 1;
	bool bOverflowed = false;
	nvgFontFace(vg, f.c_str());
	while(!bOverflowed)
	{
		ret++;
		float bounds[4] = {0,0,0,0};//xmin,ymin,xmax,ymax
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
		nvgFontSize(vg, ret);
		nvgTextBounds(vg, 0, 0, text.c_str(), NULL, bounds);
		if(abs(bounds[3/*ymax*/]) > height)
			bOverflowed = true;
		else if(width > 0.0f && abs(bounds[2/*xmax*/]) > width)
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
	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetGlobal> &pMsg)
	{
		UpdateFromMessage(*pMsg);
	}
	void UpdateFromMessage(const ClockMsg_SetGlobal &message)
	{
		m_bLandscape = message.bLandscape;
		m_bScreenSaver = message.bScreenSaver;
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

	bool LayoutEqual(std::shared_ptr<RegionState> pOther) const
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
	bool RecalcDimensions(NVGcontext* vg, const struct tm & utc, const struct tm & local, VGfloat width, VGfloat height, VGfloat displayWidth, VGfloat displayHeight, bool bStatus, bool bDigitalClockPrefix)
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
	
	void UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLayout> &pMsg)
	{
		UpdateFromMessage(*pMsg);
	}

	void UpdateFromMessage(const ClockMsg_SetLayout &msg)
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

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetLocation> &pMsg)
	{
		return UpdateFromMessage(*pMsg);
	}
	bool UpdateFromMessage(const ClockMsg_SetLocation &msg)
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

	bool UpdateFromMessage(const std::shared_ptr<ClockMsg_SetFontSizeZones> &pMsg)
	{
		return UpdateFromMessage(*pMsg);
	}

	bool UpdateFromMessage(const ClockMsg_SetFontSizeZones &msg)
	{
		if(!m_size_zones || ( *(msg.pData) != *(m_size_zones)))
		{
			m_size_zones = msg.pData;
			return true;
		}
		return false;
	}

	void SetDefaultZone(const std::string &def)
	{
		m_default_zone = def;
	}

	const std::string & GetZone(int row, int col)
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
	bool Rotate()
	{
		return m_bRotationReqd;
	}
	bool AnalogueClock(DisplayBox & dBox, bool &bLocal, std::shared_ptr<const std::map<int, VGfloat> > &hours_x, std::shared_ptr<const std::map<int, VGfloat> > &hours_y, int &numbers)
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
		prefix = "TOD ";
		return m_bDigitalClockLocal;
	}

	bool DigitalUTC(DisplayBox & dBox, int & pointSize, std::string & prefix)
	{
		dBox = m_boxUTC;
		pointSize = m_digitalPointSize;
		prefix = "UTC ";
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
		return 1.f - m_y;
	}

	VGfloat top_y() const
	{
		return y() - height();
	}

	bool hasDigitalUTC() const
	{
		return m_bDigitalClockUTC;
	}

	bool hasDigitalLocal() const
	{
		return m_bDigitalClockLocal;
	}


	static bool DigitalClockPrefix(const RegionsMap &regions)
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
private:

	bool m_bRecalcReqd;
	bool m_bRotationReqd;
	DisplayBox m_boxAnalogue;
	int m_statusTextSize;
	DisplayBox m_boxStatus, m_boxUTC, m_boxLocal, m_boxDate, m_boxTallies;
	int m_digitalPointSize;
	int m_datePointSize;
	int lastDayOfMonth = -1;
	std::shared_ptr<std::map<int, VGfloat>> m_hours_x;
	std::shared_ptr<std::map<int, VGfloat>> m_hours_y;
	std::shared_ptr<std::vector<std::vector<std::string>>> m_size_zones;
	std::string m_default_zone;

	bool m_bAnalogueClock;
	bool m_bAnalogueClockLocal;
	bool m_bDigitalClockUTC;
	bool m_bDigitalClockLocal;
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

int handle_tcp_message(const std::string &message, client & conn)
{
	std::string cmd = get_arg(message,0);
	if(cmd == "PING")
	{
		conn.write_line("PONG", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		return 2;
	}
	else if(cmd == "CRYPT")
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
	else
	{
		auto parsed = ClockMsg_Parse(message);
		if(parsed)
		{
			msgQueue.Add(parsed);
		}
		else
		{
			//Unknwon command, just NACK
			conn.write_line("NACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		}
	}
	conn.write_line("ACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
	return 1;
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
                ("init_window_width", po::value<int>(&init_window_width)->default_value(0), "Initial window width, specifying 0 gives fullscreen mode")
                ("init_window_height", po::value<int>(&init_window_height)->default_value(0), "Initial window height, specifying 0 gives fullscreen mode")
		("tally_mode", po::value<int>(&GPI_MODE)->default_value(0), "Tally Mode, 0=disabled, 1=PiFace Digital, 2=TCP/IP")
		("tally_remote_host", po::value<std::vector<std::string>>(&tally_hosts), "Remote tally host, may be specified multiple times for multiple connections")
		("tally_remote_port", po::value<std::string>(&TALLY_SERVICE)->default_value("6254"), "Port (or service) to connect to on (default 6254)")
		("tally_shared_secret", po::value<std::string>(&TALLY_SECRET)->default_value("SharedSecretGoesHere"), "Shared Secret (password) for connecting to tally service")
		("clean_exit_file", po::value<std::string>(&clean_exit_file)->default_value("/tmp/piclock_clean_exit"), "Flag file created to indicate a clean exit (from keyboard request)")
	;
	
	std::ifstream settings_file(filename.c_str());
	
	vm = po::variables_map();

	po::store(po::parse_config_file(settings_file, desc), vm);
	po::notify(vm);
}

void cleanup()
{
	bRunning = 0;
	resizeQueue.Abort();

	//If there's a clean exit trigger file, create it
	if(!clean_exit_file.empty())
		open(clean_exit_file.c_str(), O_CREAT, 00666);
	exit(0);
}

void KeyCallback(unsigned char key, int x, int y)
{
	if(key == 'q')
	{
		cleanup();
	}
}


/* All variables which used to be local to main(), which we've had
   to make global now that drawing is done via a callback
 */
static struct timeval tval;
static ntpstate_t ntp_state_data;
static int vrate;
static int hrate;

int main(int argc, char *argv[]) {
	po::variables_map vm;

	std::string configFile = "piclock.cfg";

	
	if(argc > 1)
		configFile = argv[1];
	read_settings(configFile, vm);

	gettimeofday(&tval, NULL);//Just for handle_clock_messages on first pass


	srand(time(NULL));
	vrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	hrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	init_ntp_state();
	get_ntp_state(&ntp_state_data);
	pthread_t ntp_thread;
	pthread_attr_t ntp_attr;
	pthread_attr_init(&ntp_attr);
	pthread_create(&ntp_thread, &ntp_attr, &ntp_check_thread, &ntp_state_data);
	std::thread resize_thread(background_resize_thread);
	struct sched_param resize_param;
	resize_param.sched_priority = sched_get_priority_min(SCHED_IDLE);
	pthread_setschedparam(resize_thread.native_handle(), SCHED_IDLE, &resize_param);
	if(GPI_MODE == 1)
		pifacedigital_open(0);
	else if(GPI_MODE == 2)
		create_tcp_threads();

	nvg_main(DrawFrame, init_window_width, init_window_height);

	//Shouldn't ever get here, but no harm in cleaning up anyway
	cleanup();
}
void DrawFrame(NVGcontext *vg, int iwidth, int iheight)
{
	static VGfloat commsWidth = -1;
	static VGfloat commsTextHeight = 0;
	static RegionsMap regions;
	static VGfloat offset = iheight*.05f;

	static std::map<std::string, int> textSizes;
	static std::map<std::string, int> labelSizes;

	static time_t tm_last_comms_good = 0;

	static ImagesMap images;

	static VGfloat h_offset_pos = 0;
	static VGfloat v_offset_pos = 0;

	static long prev_sec = 0;

	static struct tm tm_local, tm_utc;

	static bool bRecalcTextsNext = true;

	static NVGcolor colWhite = nvgRGBf(1.0f,1.0f,1.0f);
	static NVGcolor colMidGray = nvgRGBf(0.5f,0.5f,0.5f);
	//Unused at the moment, uncomment to re-enable
	//static NVGcolor colBlack = nvgRGBf(0.0f,0.0f,0.0f);
	static NVGcolor colRed = nvgRGBf(1.0f,0.0f,0.0f);
	//Unused, at the moment, uncomment to re-enable
	//static NVGcolor colBlue = nvgRGBf(0.0f,0.0f,1.0f);
        static NVGcolor colNtpSynced = nvgRGB(0,100,0);
        static NVGcolor colNtpNotSync[2] = {nvgRGB(120,0,120),nvgRGB(120,0,0)};
	static NVGcolor colNtpText = nvgRGB(200,200,200);
	static NVGcolor colCommsOk = nvgRGB(0,100,0);
	static NVGcolor colCommsFail = nvgRGB(190,0,0);

	if(clippedTextCache)
	{
		prevClippedTextCache = clippedTextCache;
	}
	else
	{
		prevClippedTextCache = std::make_shared<std::map<std::pair<std::string, float>, std::string> >();
	}

	clippedTextCache = std::make_shared<std::map<std::pair<std::string, float>, std::string> >();

	if(regions.empty())
		//Add 1 region, for when we have no TCP connection, should probably only happen on first pass
		regions[0] = std::make_shared<RegionState>();
	//Handle any queued messages
	std::queue<std::shared_ptr<ClockMsg> > newMsgs;
	msgQueue.Get(newMsgs);
	bool bRecalcTexts = handle_clock_messages(newMsgs, regions, images, tval) || bRecalcTextsNext;
	bRecalcTextsNext = false;
	
	bool bDigitalClockPrefix = RegionState::DigitalClockPrefix(regions);

	//nvgTransform(vg, 1,iheight,0,-1,0,0);
	//Start(iwidth, iheight);					// Start the picture
	VGfloat display_width = iwidth;
	VGfloat display_height = iheight;
	if(globalState.ScreenSaver())
	{
		display_width -= offset;
		display_height -= offset;
	}
	if(globalState.RotationReqd(display_width,display_height))
	{
		Rotate(vg,90);
		//After rotating our output has now disappeared down below our co-ordinate space, so translate our co-ordinate space down to find it...
		nvgTranslate(vg,0,-iwidth);
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
		nvgTranslate(vg, h_offset_pos, v_offset_pos);
	}
	bool bFirst = true;
	for(const auto & region : regions)
	{
		auto &RS = *region.second;
		VGfloat inner_height = display_height * RS.height();
		VGfloat inner_width = display_width * RS.width();

		bRecalcTexts = RS.RecalcDimensions(vg, tm_utc, tm_local, inner_width, inner_height, display_width, display_height, bFirst, bDigitalClockPrefix) || bRecalcTexts;
		bFirst = false;
	}
	if(bRecalcTexts)
	{
		textSizes.clear();
		labelSizes.clear();
		for(const auto & region : regions)
		{
			auto &RS = *region.second;
			RS.SetDefaultZone("R" + std::to_string(region.first));
			DisplayBox db = RS.TallyBox();
			VGfloat row_height = (db.h)/((VGfloat)RS.TD.nRows);
			VGfloat col_width_default = db.w/((VGfloat)RS.TD.nCols_default);

			for(int row = 0; row < RS.TD.nRows; row++)
			{
				int col_count = RS.TD.nCols[row];
				VGfloat col_width = col_width_default;
				if(col_count <= 0)
					col_count = RS.TD.nCols_default;
				else
					col_width = db.w/((VGfloat)col_count);
				for(int col = 0; col < col_count; col++)
				{
					const std::string & zone = RS.GetZone(row, col);
					auto item = RS.TD.displays[row][col];
					if(!item)
						continue;
					auto label = item->Label(tval);
					auto text = item->Text(tval);
					auto iter = images.end();
					if(text && ((iter = images.find(*text)) == images.end() || !iter->second.IsValid()))
					{
						auto maxItemSize = MaxPointSize(vg, col_width * .9f, row_height * (label? .6f :.9f), item->Text(tval)->c_str(), FONT(item->IsMonoSpaced()));
						if(textSizes[zone] == 0 || textSizes[zone] > maxItemSize)
							textSizes[zone] = maxItemSize;
						if(label)
						{
							auto maxLabelSize = MaxPointSize(vg, -1, row_height *.2f, label->c_str(), FONT(false));
							if(labelSizes[zone] == 0 || labelSizes[zone] > maxLabelSize)
								labelSizes[zone] = maxLabelSize;
						}
					}
				}
			}
		}
		commsWidth = -1;
	}
	gettimeofday(&tval, NULL);
	localtime_r(&tval.tv_sec, &tm_local);
	gmtime_r(&tval.tv_sec, &tm_utc);
	for(const auto & region : regions)
	{
		std::string profName;
		int i;
		std::shared_ptr<RegionState> pRS = region.second;

		VGfloat return_x = display_width *pRS->x();
		VGfloat return_y = display_height *(pRS->top_y());
		nvgSave(vg);//Save current translation to stack to be restored later
		nvgTranslate(vg, return_x, return_y);

		nvgFillColor(vg, colWhite);					// white text
		nvgStrokeColor(vg, colWhite);				// White strokes
		nvgStrokeWidth(vg, 0);//Don't draw strokes until we've moved onto the analogue clock

		//Write out the text
		DisplayBox db;
		int pointSize;
		std::string prefix;
		if(pRS->DigitalLocal(db, pointSize, prefix))
		{
			std::string time_str = FormatTime(tm_local, tval.tv_usec);
			if(bDigitalClockPrefix)
				time_str = prefix + time_str;
			db.TextMidBottom(vg, time_str, FONT_MONO, pointSize);
		}
		if(pRS->DigitalUTC(db, pointSize, prefix))
		{
			std::string time_str = FormatTime(tm_utc, tval.tv_usec);
			if(bDigitalClockPrefix)
				time_str = prefix + time_str;
			db.TextMidBottom(vg, time_str, FONT_MONO, pointSize);
		}
		bool bLocal;
		if(pRS->Date(db, bLocal, pointSize))
		{
			db.TextMidBottom(vg, FormatDate(bLocal? tm_local: tm_utc),
						FONT_PROP, pointSize);
		}

		if(GPI_MODE == 1)
		{
			if(pRS->TD.nRows != 2)
				bRecalcTextsNext = true;
			pRS->TD.nRows = 2;
			pRS->TD.nCols_default = 1;
			pRS->TD.nCols.empty();
			uint8_t gpis = pifacedigital_read_reg(INPUT,0);
			uint8_t colour_weight = (gpis & 1) ? 50:255;
			uint8_t fill_weight = (gpis & 1)? 50:255;
			pRS->TD.displays[0][0] = std::make_shared<SimpleTallyState>(
				    TallyColour(fill_weight, fill_weight, fill_weight),
				    TallyColour(colour_weight, colour_weight*0.55,0),
				    "Mic Live");
			colour_weight = (gpis & 2) ? 50:255;
			fill_weight = (gpis & 2)? 50:255;
			pRS->TD.displays[1][0] = std::make_shared<SimpleTallyState>(
				    TallyColour(fill_weight, fill_weight, fill_weight),
				    TallyColour(colour_weight,0,0),
				    "On Air");
		}

		//Draw NTP sync status
		db = pRS->StatusBox(pointSize);
		if(db.w > 0.0f && db.h > 0.0f)
		{
			nvgFillColor(vg,colWhite);
			const char * sync_text;
			if(ntp_state_data.status == 0)
			{
				nvgFillColor(vg,colNtpSynced);
				sync_text = "Synchronised";
			}
			else
			{
				/* flash between red and purple once a second */
				nvgFillColor(vg,colNtpNotSync[(tval.tv_sec %2)]);
				if(ntp_state_data.status == 1)
					sync_text = "Synchronising..";
				else
					sync_text = "Unknown Synch!";
			}
			const char * header_text = "NTP-Derived Clock";
			auto statusBoxLen = std::max(TextWidth(vg, sync_text, FONT_PROP, pointSize),
						     TextWidth(vg, header_text, FONT_PROP, pointSize))*1.1f;
			auto statusTextHeight = TextHeight(vg, FONT_PROP, pointSize);
			//Draw a box around NTP status
			Rect(vg, db.x + db.w - statusBoxLen, db.top_y() , statusBoxLen, db.h);
			nvgFillColor(vg, colNtpText);
			//2 bits of text
			TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight *.8f, sync_text, FONT_PROP, pointSize);
			TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight*2.f, header_text, FONT_PROP, pointSize);

			db.w -= statusBoxLen;

			if(GPI_MODE == 2)
			{
				//Draw comms status
				if(commsWidth < 0)
				{
					commsWidth =
						std::max(TextWidth(vg, "Comms OK", FONT_PROP, pointSize),

							 TextWidth(vg, "Comms Failed", FONT_PROP, pointSize));
					for(unsigned int window = 0; window < tally_hosts.size(); window++)
					{
						char buf[512];
						buf[511] = '\0';
						snprintf(buf, 511, "Tally Server %d", window + 1);
						commsWidth = std::max(commsWidth, TextWidth(vg, buf, FONT_PROP, pointSize));
					}
					commsWidth *= 1.2f;
					commsTextHeight = TextHeight(vg, FONT_PROP, pointSize);
				}
				bool bAnyComms = false;
				for(unsigned int window = 0; window < tally_hosts.size(); window++)
				{
					bAnyComms = bAnyComms || bComms[window];
					if(bComms[window])
						nvgFillColor(vg,colCommsOk);
					else
						nvgFillColor(vg,colCommsFail);
					VGfloat base_x = db.x + db.w - commsWidth * (VGfloat)(tally_hosts.size()-window);
					Rect(vg, base_x, db.top_y(), commsWidth, db.h);
					nvgFillColor(vg,colNtpText);
					char buf[512];
					buf[511] = '\0';
					base_x += commsWidth*.5f;
					snprintf(buf, 511, "Tally Server %d", window + 1);
					TextMid(vg, base_x, db.y - commsTextHeight*2.0f, buf, FONT_PROP, pointSize);
					snprintf(buf, 511, "Comms %s", bComms[window]? "OK" : "Failed");
					TextMid(vg, base_x, db.y - commsTextHeight*0.8f, buf, FONT_PROP, pointSize);
				}
				if(bAnyComms)
				{
					tm_last_comms_good = tval.tv_sec;
				}
				//Stop showing stuff after 5 seconds of comms failed...
				else if(tm_last_comms_good < tval.tv_sec - 5)
				{
					for(auto & reg: regions)
					{
						reg.second->TD.clear();
					}
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
				//float y_offset = height/10.0f;
				for(int row = 0; row < pRS->TD.nRows; row++)
				{
					VGfloat base_y = db.y - ((VGfloat)row)*row_height;
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
						auto text = curTally->Text(tval);
						auto iter = images.end();
						int img_handle = 0;
						if(text && (iter = images.find(*text)) != images.end() && iter->second.IsValid())
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
							dbTally.Roundrect(vg, row_height/10.0f);
							curTally->FG(tval)->Fill(vg);
							const auto & zone = pRS->GetZone(row,col);
							dbTally.TextMid(vg, text, FONT(curTally->IsMonoSpaced()), textSizes[zone], labelSizes[zone], curTally->Label(tval));
						}
					}
				}
			}
		}
		//Done with displays, back to common code...
		db = pRS->StatusBox(pointSize);
		if(db.w > 0.0f && db.h > 0.0f && GPI_MODE == 2)
		{
			nvgFillColor(vg, colMidGray);
			char buf[8192];
			buf[sizeof(buf)-1] = '\0';
			if(profName.empty())
				snprintf(buf, sizeof(buf) -1, "MAC Address %s", mac_address.c_str());
			else
				snprintf(buf, sizeof(buf) -1, "MAC Address %s, %s", mac_address.c_str(), profName.c_str());
			Text(vg, db.x, db.y, buf, FONT_PROP, pointSize);
		}
		nvgFillColor(vg,colWhite);
		int numbers;
		std::shared_ptr<const std::map<int, VGfloat>> hours_x;
		std::shared_ptr<const std::map<int, VGfloat>> hours_y;
		if(pRS->AnalogueClock(db, bLocal, hours_x, hours_y, numbers))
		{
			//Right, now start drawing the clock

			//Move to the centre of the clock
			VGfloat move_x = db.mid_x();
			VGfloat move_y = db.mid_y();
			
			nvgTranslate(vg, move_x, move_y);

			if(numbers)
			{
				//Write out hour labels around edge of clock
				for(i = 0; i < 12; i++)
				{
					char buf[5];
					sprintf(buf, "%d", i? i:12);
					TextMid(vg, hours_x->at(i),hours_y->at(i), buf, FONT_HOURS, db.h/15.0f);
				}
			}
			//Spin clock around 180 degrees because this code originally worked with Y co-ordinates increasing as you went up
			Rotate(vg, 180.0f);
			//Go around drawing dashes around edge of clock
			nvgStrokeWidth(vg, db.w/100.0f);
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
			nvgStrokeColor(vg,colRed);
#endif
#define tm_now	(bLocal? tm_local : tm_utc)
			//As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
			for(i = 0; i < 60; i++)
			{
				if((i %5) == 0)
					Line(vg, 0, start, 0, end_long);
				else
					Line(vg, 0, start, 0, end_short);
				Rotate(vg, 6);
#ifndef NO_COLOUR_CHANGE
				//Fade red slowly out over first second
				if(i == tm_now.tm_sec)
				{
					if(i < 1)
					{
						//Example to fade over 2 seconds...
						//VGfloat fade = 128.0f * tm_now.tm_sec +  127.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
						VGfloat fade = ((VGfloat)tval.tv_usec)/200000.0f;
						if(fade > 1.0f)
							fade = 1.0f;
						nvgStrokeColor(vg, nvgRGBf(1.0f, fade, fade));
					}
					else
						nvgStrokeColor(vg, colWhite);
				}
#endif
			}
			nvgStrokeColor(vg, colWhite);
			//Again, rotate co-ordinate space so we're just drawing an upright line every time...
			nvgStrokeWidth(vg, db.w/200.0f);
			//VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
			VGfloat sec_rotation = (6.0f * tm_now.tm_sec);
			VGfloat sec_part = sec_rotation;
			if(tval.tv_usec > MOVE_HAND_AT)
				sec_rotation += ((VGfloat)(tval.tv_usec - MOVE_HAND_AT)*6.0f/100000.0f);
			//Take into account microseconds when calculating position of minute hand (and to minor extent hour hand), so it doesn't jump every second
			sec_part += ((VGfloat)tval.tv_usec)*6.0f/1000000.0f;
			VGfloat min_rotation = 6.0f *tm_now.tm_min + sec_part/60.0f;
			VGfloat hour_rotation = 30.0f *tm_now.tm_hour + min_rotation/12.0f;
			Rotate(vg, hour_rotation);
			Line(vg, 0,0,0,min_dim/4.0f); /* half-length hour hand */
			Rotate(vg, min_rotation - hour_rotation);
			Line(vg, 0,0,0,min_dim/2.0f); /* minute hand */
			Rotate(vg, sec_rotation - min_rotation);
			nvgStrokeColor(vg, colRed);
			Line(vg, 0,-db.h/10.0f,0,min_dim/2.0f); /* second hand, with overhanging tail */
			//Draw circle in centre
			nvgFillColor(vg, colRed);
			nvgBeginPath(vg);
			nvgCircle(vg, 0,0,db.w/150.0f);
			nvgFill(vg);
			Rotate(vg, -sec_rotation -180.0f);
			//Now draw some dots for seconds...
#ifdef SECOND_DOTS
			nvgStrokeWidth(vg, db.w/100.0f);
			nvgStrokeColor(vg, colBlue);
			VGfloat pos = db.h*7.9f/20.0f;
			for(i = 0; i < 60; i++)
			{
				if(i <= tm_now.tm_sec)
				{
					Line(vg, 0, pos, 0, pos -10);
				}
				Rotate(vg, -6);
			}
#endif

		}
		//Translate back to the origin...
		nvgRestore(vg);
	}
}
