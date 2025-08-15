#include <b64/decode.h>
#include <memory>
#include <sstream>
#include <queue>
#include <nanovg.h>
#include "piclock_messages.h"

static std::shared_ptr<std::string> get_arg_p(const std::string & input, int index, bool bTerminated = true)
{
	size_t start_index = 0;
	while(index > 0)
	{
		auto found = input.find(':', start_index);
		if(found == std::string::npos)
			return std::shared_ptr<std::string>();
		start_index = found + 1;
		index--;
	}

	if(!bTerminated)
		return std::make_shared<std::string>(input.substr(start_index));

	size_t end = input.find(':',start_index);
	if(end != std::string::npos)
		end -= start_index;
	return std::make_shared<std::string>(input.substr(start_index,end));
}

std::string get_arg(const std::string & input, int index, bool bTerminated)
{
	auto ret = get_arg_p(input, index, bTerminated);
	if(ret)
		return *ret;
	return std::string();
}

#ifndef TCP_TEST_CLIENT
//this is probably horribly inefficient, because it gets copied multiple times, but it does work, and
//it won't be called in our main display thread
static std::string get_arg_b64(const std::string & input, int index, bool bTerminated = true)
{
	auto source = get_arg_p(input, index, bTerminated);
	if(!source)
		return std::string();
	base64::decoder dec;
	std::ostringstream oss;
	std::istringstream iss(*source);
	dec.decode(iss,oss);
	return oss.str();
}

static int get_arg_int(const std::string &input, int index, bool bTerminated = true)
{
	try
	{
		auto arg = get_arg_p(input, index, bTerminated);
		if(arg && arg->size() > 0)
		    return std::stoi(*arg);
	}
	catch(...)
	{
	}
	return 0;
}

static long get_arg_l(const std::string &input, int index, bool bTerminated = true)
{
	try
	{
		auto arg = get_arg_p(input, index, bTerminated);
		if(arg && arg->size() > 0)
		    return std::stol(*arg);
	}
	catch(...)
	{
	}
	return 0;
}

static long long get_arg_ll(const std::string &input, int index, bool bTerminated = true)
{
	try
	{
		auto arg = get_arg_p(input, index, bTerminated);
		if(arg && arg->size() > 0)
		    return std::stoll(*arg);
	}
	catch(...)
	{
	}
	return 0;
}

static VGfloat get_arg_f(const std::string &input, int index, bool bTerminated = true)
{
	return std::stod(get_arg(input, index, bTerminated));
}
static std::shared_ptr<long long> get_arg_pll(const std::string &input, int index, bool bTerminated = true)
{
	auto str = get_arg(input, index, bTerminated);
	if(str.length() <= 0)
		return std::shared_ptr<long long>();
	return std::make_shared<long long>(std::stoull(str));
}


static bool get_arg_bool(const std::string &input, int index, bool bTerminated = true)
{
	return get_arg_int(input, index, bTerminated) != 0;
}


std::shared_ptr<int> ClockMsg::ParseCmd(const std::string & message, std::string &cmd)
{
	cmd = get_arg(message,0);
	if(cmd.length() > 0 && isdigit(cmd[0]))
	{
		char * pEnd;
		int ret = strtoul(cmd.c_str(), &pEnd, 10);
		std::string newCmd = pEnd;
		cmd = newCmd;
		return std::make_shared<int>(ret);
	}
	else
	{
		return std::shared_ptr<int>();
	}
}


ClockMsg_SetGPO::ClockMsg_SetGPO(const std::string & message)
{
	gpoIndex = get_arg_int(message,1);
	bValue = get_arg_bool(message,2);
}

ClockMsg_ClearImages::ClockMsg_ClearImages()
{}

ClockMsg_ClearFonts::ClockMsg_ClearFonts()
{}

ClockMsg_StoreFont::ClockMsg_StoreFont(const std::string & message)
	:name(get_arg(message, 1)),
		data(get_arg_b64(message, 2, false))
{
}

Magick::Blob ClockMsg_StoreImage::base64_to_blob(const std::string & message, int idx)
{
	std::string data = get_arg_b64(message,2,false);
	return Magick::Blob((void *)data.data(), data.size());
}

ClockMsg_StoreImage::ClockMsg_StoreImage(const std::string & message)
	:name(get_arg(message, 1)),
		pSourceBlob(std::make_shared<Magick::Blob>(base64_to_blob(message, 2))),
		pParsedImage(std::make_shared<Magick::Image>(*pSourceBlob))
{
	//ImageMagick does scan lines, etc in the opposite order to OpenVG, so flip vertically
	pParsedImage->flip();
}

ClockMsg_SetGlobal::ClockMsg_SetGlobal(const std::string & message)
{
	bLandscape = get_arg_bool(message,1);
	bScreenSaver = get_arg_bool(message,2);
}

ClockMsg_SetFonts::ClockMsg_SetFonts(const std::string & message)
	:tally(get_arg(message,1)),
	 tally_label(get_arg(message,2)),
	 status(get_arg(message,3)),
	 digital(get_arg(message,4)),
	 date(get_arg(message,5)),
	 hours(get_arg(message,6))
{
}

ClockMsg_Region::ClockMsg_Region(const std::shared_ptr<int> &region, const std::string & message)
{
	if(region)
	{
		bHasRegionIndex = true;
		regionIndex = *region;
	}
}

ClockMsg_SetLayout::ClockMsg_SetLayout(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_Region(region, message)
{
#define UPDATE_VAL(val,idx) (val) = get_arg_bool(message,(idx));
	UPDATE_VAL(bAnalogueClock,      1)
	UPDATE_VAL(bLegacyAnalogueClockLocal, 2)
	UPDATE_VAL(bLegacyDigitalClockUTC,    3)
	UPDATE_VAL(bLegacyDigitalClockLocal,  4)
	UPDATE_VAL(bDate,               5)
	UPDATE_VAL(bLegacyDateLocal,          6)
	//Skip Landscape parameter, this is now global, still transmitted by driver for legacy devices
	UPDATE_VAL(bNumbersPresent,	8);
	UPDATE_VAL(bNumbersOutside,	9);
	UPDATE_VAL(bSecondsSweep,	10);
#undef UPDATE_VAL
#define UPDATE_VAL(val,idx) (val) = get_arg(message,(idx));
	UPDATE_VAL(sImageClockFace,	11);
	UPDATE_VAL(sImageClockHours,	12);
	UPDATE_VAL(sImageClockMinutes,	13);
	UPDATE_VAL(sImageClockSeconds,	14);
#undef UPDATE_VAL
}
void ClockMsg_SetLayout::Dump()
{
	fprintf(stderr, "Analogue %d, LegacyAnalogueLocal %d, LegacyDigitalUTC %d, LegacyDigitalLocal %d, Date %d, LegacyLegacyDateLocal %d, NumbersPresent %d, NumbersOutside %d\n", bAnalogueClock, bLegacyAnalogueClockLocal, bLegacyDigitalClockUTC, bLegacyDigitalClockLocal, bDate, bLegacyDateLocal, bNumbersPresent, bNumbersOutside);
}

ClockMsg_SetTimezones::ClockMsg_SetTimezones(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_Region(region, message)
{
    tzDate = get_arg(message, 1);
    tzAnalogue = get_arg(message, 2);
    int idx = 3;
    while(true)
    {
	auto tz = get_arg_p(message, idx++);
	auto label = get_arg_p(message, idx++);
	if(tz)
	{
	    if(label)
	    {
		tzDigitals.push_back(std::pair<std::string,std::string>(*tz, *label));
	    }
	    else
	    {
		tzDigitals.push_back(std::pair<std::string,std::string>(*tz, ""));
	    }
	}
	else
	{
	    break;
	}
    }
}


ClockMsg_SetLocation::ClockMsg_SetLocation(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_Region(region, message)
{
#define UPDATE_VAL(val,idx) (val) = get_arg_f(message,(idx));
	UPDATE_VAL(x,		1)
	UPDATE_VAL(y,		2)
	UPDATE_VAL(width,	3)
	UPDATE_VAL(height,	4)
#undef UPDATE_VAL
}

ClockMsg_SetFontSizeZones::ClockMsg_SetFontSizeZones(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_Region(region, message)
{
	auto &data = *pData;
	std::string region_prefix = std::to_string(regionIndex);
        region_prefix = "R" + region_prefix;
	int i = 0;
	while(auto pRow = get_arg_p(message, ++i))
	{
		std::vector<std::string> cols;
		size_t start_index = -1;
		const std::string & row = *pRow;
		do
		{
			start_index++;
			size_t end_index = row.find(',',start_index);
			size_t length;
			if(end_index == std::string::npos)
				length = std::string::npos;
			else
				length = end_index - start_index;
			std::string newVal = row.substr(start_index, length);
			if(newVal.empty() || newVal[0] != 'G')
				newVal = region_prefix + newVal;
			cols.push_back(newVal);
			start_index = end_index;
		}
		while(start_index != std::string::npos);
		//Trim any empty elements off the end of the list
		unsigned newSize;
		for(newSize = cols.size(); newSize > 0 && (cols[newSize - 1]).empty(); newSize--)
			;
		if(newSize != cols.size())
			cols.resize(newSize);
		data.push_back(cols);
	}
	//Trim any empty elements off the end of the outer list
	unsigned newSize;
	for(newSize = data.size(); newSize > 0 && (data[newSize - 1]).empty(); newSize--)
			;
	if(newSize != data.size())
		data.resize(newSize);
}

ClockMsg_SetRegionCount::ClockMsg_SetRegionCount(const std::string & message)
{
	iCount = get_arg_int(message,1);
}

ClockMsg_SetProfile::ClockMsg_SetProfile(const std::string & message)
{
	sProfile = get_arg(message,1);
}

ClockMsg_RowCol_Generic::ClockMsg_RowCol_Generic(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_Region(region, message)
{
	m_iRow = get_arg_int(message,1);
	m_iCol = get_arg_int(message,2);
}

ClockMsg_SetSize::ClockMsg_SetSize(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_RowCol_Generic(region, message),iRows(m_iRow),iCols(m_iCol)
{
}

ClockMsg_SetRow::ClockMsg_SetRow(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_RowCol_Generic(region, message),iRow(m_iRow),iCols(m_iCol)
{
}


ClockMsg_RowCol::ClockMsg_RowCol(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_RowCol_Generic(region, message),iRow(m_iRow),iCol(m_iCol)
{
}

ClockMsg_SetIndicator::ClockMsg_SetIndicator(const std::shared_ptr<int> &region, const std::string & message, int textIndex)
	:ClockMsg_RowCol(region, message)
{
	colFg = TallyColour(get_arg(message, 3));
	colBg = TallyColour(get_arg(message, 4));
	sText = get_arg(message, textIndex, false);
}

ClockMsg_SetCountdown::ClockMsg_SetCountdown(const std::shared_ptr<int> &region, const std::string & message, bool bExtendedArguments)
	:ClockMsg_SetIndicator(region, message, bExtendedArguments? 8 : 14)
{
	colFg = TallyColour(get_arg(message, 3));
	colBg = TallyColour(get_arg(message, 4));
	target.tv_sec = get_arg_ll(message, 5);
	target.tv_usec = get_arg_l(message, 6);
	auto flash = get_arg_pll(message, 7);
	if((bHasFlashLimit = (bool)flash))
		iFlashLimit = *flash;
	if(bExtendedArguments)
	{
		daysMode = get_arg_int(message, 8);
	}
}


ClockMsg_SetTally::ClockMsg_SetTally(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_SetIndicator(region, message, 5)
{
}

ClockMsg_SetLabel::ClockMsg_SetLabel(const std::shared_ptr<int> &region, const std::string & message)
	:ClockMsg_RowCol(region, message)
{
	sText = get_arg(message, 3, false);
}

std::shared_ptr<ClockMsg> ClockMsg_Parse(const std::string &message)
{
	std::string cmd;
	auto region = ClockMsg::ParseCmd(message, cmd);
	if(cmd == "SETGPO")
		return std::make_shared<ClockMsg_SetGPO>(message);
	if(cmd == "CLEARFONTS")
		return std::make_shared<ClockMsg_ClearFonts>();
	if(cmd == "CLEARIMAGES")
		return std::make_shared<ClockMsg_ClearImages>();
	if(cmd == "STOREIMAGE")
	{
		try
		{
			return std::make_shared<ClockMsg_StoreImage>(message);
		}
		catch(const std::exception &e)
		{
			std::cerr << "Caught exception handling STOREIMAGE \"" << e.what() << "\"\n";
			return std::shared_ptr<ClockMsg>();
		}
		catch(...)
		{
			std::cerr << "Caught unknown exception handling STOREIMAGE\n";
			return std::shared_ptr<ClockMsg>();
		}
	}
	if(cmd == "STOREFONT")
	{
		try
		{
			return std::make_shared<ClockMsg_StoreFont>(message);
		}
		catch(const std::exception &e)
		{
			std::cerr << "Caught exception handling STOREFONT \"" << e.what() << "\"\n";
			return std::shared_ptr<ClockMsg>();
		}
		catch(...)
		{
			std::cerr << "Caught unknown exception handling STOREFONT\n";
			return std::shared_ptr<ClockMsg>();
		}
	}
	if(cmd == "SETGLOBAL")
		return std::make_shared<ClockMsg_SetGlobal>(message);
	if(cmd == "SETREGIONCOUNT")
		return std::make_shared<ClockMsg_SetRegionCount>(message);
	if(cmd == "SETPROFILE")
		return std::make_shared<ClockMsg_SetProfile>(message);
	if(cmd == "SETFONTS")
		return std::make_shared<ClockMsg_SetFonts>(message);
	if(cmd == "SETLOCATION")
		return std::make_shared<ClockMsg_SetLocation>(region, message);
	if(cmd == "SETSIZE")
		return std::make_shared<ClockMsg_SetSize>(region, message);
	if(cmd == "SETROW")
		return std::make_shared<ClockMsg_SetRow>(region, message);
	if(cmd == "SETTALLY")
		return std::make_shared<ClockMsg_SetTally>(region, message);
	if(cmd == "SETCOUNTDOWN")
		return std::make_shared<ClockMsg_SetCountdown>(region, message, false);
	if(cmd == "SETCOUNTDOWNEXTENDED")
		return std::make_shared<ClockMsg_SetCountdown>(region, message, true);
	if(cmd == "SETLABEL")
		return std::make_shared<ClockMsg_SetLabel>(region, message);
	if(cmd == "SETLAYOUT")
		return std::make_shared<ClockMsg_SetLayout>(region, message);
	if(cmd == "SETTIMEZONES")
		return std::make_shared<ClockMsg_SetTimezones>(region, message);
	if(cmd == "SETFONTSIZEZONES")
		return std::make_shared<ClockMsg_SetFontSizeZones>(region, message);
	return std::shared_ptr<ClockMsg>();

}

void MessageQueue::Add(const std::shared_ptr<ClockMsg> & pMsg)
{
    std::lock_guard<std::mutex> hold_lock(m_access_mutex);
    m_queue.push(pMsg);
}
bool MessageQueue::Get(std::queue<std::shared_ptr<ClockMsg> > &output)
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
#endif
