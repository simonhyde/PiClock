#ifndef __PICLOCK_MESSAGES_H
#define __PICLOCK_MESSAGES_H

#include <b64/decode.h>
#include <memory>
#include <nanovg.h>
#include <mutex>
#include <queue>
#include <Magick++.h>
#include "tallycolour.h"

typedef float VGfloat;

extern std::string get_arg(const std::string & input, int index, bool bTerminated = true);

#if 0
extern std::shared_ptr<std::string> get_arg_p(const std::string & input, int index, bool bTerminated = true);

extern std::string get_arg_b64(const std::string & input, int index, bool bTerminated = true);

extern int get_arg_int(const std::string &input, int index, bool bTerminated = true);

extern long get_arg_l(const std::string &input, int index, bool bTerminated = true);

extern long long get_arg_ll(const std::string &input, int index, bool bTerminated = true);

extern VGfloat get_arg_f(const std::string &input, int index, bool bTerminated = true);

extern std::shared_ptr<long long> get_arg_pll(const std::string &input, int index, bool bTerminated = true);

extern bool get_arg_bool(const std::string &input, int index, bool bTerminated = true);
#endif

class ClockMsg
{
public:
	static std::shared_ptr<int> ParseCmd(const std::string & message, std::string &cmd);
	//To make polymorphic
	virtual ~ClockMsg() = default;
};

class ClockMsg_SetGPO: public ClockMsg
{
public:
	int gpoIndex;
	bool bValue;
	ClockMsg_SetGPO(const std::string & message);
};

class ClockMsg_ClearImages : public ClockMsg
{
public:
	ClockMsg_ClearImages();
};

class ClockMsg_StoreImage : public ClockMsg
{
public:
	std::string name;
	std::shared_ptr<Magick::Blob> pSourceBlob;
	std::shared_ptr<Magick::Image> pParsedImage;
	static Magick::Blob base64_to_blob(const std::string & message, int idx);
	ClockMsg_StoreImage(const std::string & message);
};

//Because I'm lazy everything is public, should really be read only
class ClockMsg_SetGlobal : public ClockMsg
{
public:
	bool bLandscape;
	bool bScreenSaver;
	ClockMsg_SetGlobal(const std::string & message);
};

class ClockMsg_Region : public ClockMsg
{
public:
	bool bHasRegionIndex = false;
	int regionIndex = 0;
protected:
	ClockMsg_Region(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetLayout : public ClockMsg_Region
{
public:
	bool bAnalogueClock, bAnalogueClockLocal, bDigitalClockUTC,
		bDigitalClockLocal, bDate, bDateLocal, bNumbersPresent,
		bNumbersOutside;

	ClockMsg_SetLayout(const std::shared_ptr<int> &region, const std::string & message);
	void Dump();
};

class ClockMsg_SetLocation : public ClockMsg_Region
{
public:
	double x, y, width, height;
	ClockMsg_SetLocation(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetFontSizeZones : public ClockMsg_Region
{
public:
	std::shared_ptr<std::vector<std::vector<std::string>>> pData = std::make_shared<std::vector<std::vector<std::string>>>();
	ClockMsg_SetFontSizeZones(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetRegionCount : public ClockMsg
{
public:
	int iCount;
	ClockMsg_SetRegionCount(const std::string & message);
};

class ClockMsg_SetProfile : public ClockMsg
{
public:
	std::string sProfile;
	ClockMsg_SetProfile(const std::string & message);
};

class ClockMsg_RowCol_Generic : public ClockMsg_Region
{
protected:
	ClockMsg_RowCol_Generic(const std::shared_ptr<int> &region, const std::string & message);
	int m_iRow, m_iCol;
};

class ClockMsg_SetSize : public ClockMsg_RowCol_Generic
{
public:
	const int &iRows, &iCols;
	ClockMsg_SetSize(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetRow : public ClockMsg_RowCol_Generic
{
public:
	const int &iRow, &iCols;
	ClockMsg_SetRow(const std::shared_ptr<int> &region, const std::string & message);
};


class ClockMsg_RowCol : public ClockMsg_RowCol_Generic
{
public:
	const int &iRow, &iCol;
	ClockMsg_RowCol(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetIndicator: public ClockMsg_RowCol
{
public:
	TallyColour colFg, colBg;
	std::string sText;
protected:
	ClockMsg_SetIndicator(const std::shared_ptr<int> &region, const std::string & message, int textIndex);
};

class ClockMsg_SetCountdown: public ClockMsg_SetIndicator
{
public:
	struct timeval target;
	bool bHasFlashLimit;
	long long iFlashLimit = 0;
	ClockMsg_SetCountdown(const std::shared_ptr<int> &region, const std::string & message);
};


class ClockMsg_SetTally : public ClockMsg_SetIndicator
{
public:
	ClockMsg_SetTally(const std::shared_ptr<int> &region, const std::string & message);
};

class ClockMsg_SetLabel : public ClockMsg_RowCol
{
public:
	std::string sText;
	ClockMsg_SetLabel(const std::shared_ptr<int> &region, const std::string & message);
};

class MessageQueue
{
public:
	void Add(const std::shared_ptr<ClockMsg> & pMsg);
	bool Get(std::queue<std::shared_ptr<ClockMsg> > &output);
private:
	std::queue<std::shared_ptr<ClockMsg> > m_queue;
	std::mutex m_access_mutex;
};

extern std::shared_ptr<ClockMsg> ClockMsg_Parse(const std::string &message);

#endif
