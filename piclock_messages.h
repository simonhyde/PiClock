#ifndef __PICLOCK_MESSAGES_H
#define __PICLOCK_MESSAGES_H

std::shared_ptr<std::string> get_arg_p(const std::string & input, int index, bool bTerminated = true)
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

std::string get_arg(const std::string & input, int index, bool bTerminated = true)
{
	auto ret = get_arg_p(input, index, bTerminated);
	if(ret)
		return *ret;
	return std::string();
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
std::shared_ptr<long long> get_arg_pll(const std::string &input, int index, bool bTerminated = true)
{
	auto str = get_arg(input, index, bTerminated);
	if(str.length() <= 0)
		return std::shared_ptr<long long>();
	return std::make_shared<long long>(std::stoull(str));
}


bool get_arg_bool(const std::string &input, int index, bool bTerminated = true)
{
	return get_arg_int(input, index, bTerminated) != 0;
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

class ClockMsg
{
public:
	static std::shared_ptr<int> ParseCmd(const std::string & message, std::string &cmd)
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
	//To make polymorphic
	virtual ~ClockMsg() = default;
};

//Because I'm lazy everything is public, should really be read only
class ClockMsg_SetGlobal : public ClockMsg
{
public:
	bool bLandscape;
	bool bScreenSaver;
	ClockMsg_SetGlobal(const std::string & message)
	{
		bLandscape = get_arg_bool(message,1);
		bScreenSaver = get_arg_bool(message,2);
	}
};

class ClockMsg_Region : public ClockMsg
{
public:
	bool bHasRegionIndex = false;
	int regionIndex = 0;
protected:
	ClockMsg_Region(const std::shared_ptr<int> &region, const std::string & message)
	{
		if(region)
		{
			bHasRegionIndex = true;
			regionIndex = *region;
		}
	}
};

class ClockMsg_SetLayout : public ClockMsg_Region
{
public:
	bool bAnalogueClock, bAnalogueClockLocal, bDigitalClockUTC,
		bDigitalClockLocal, bDate, bDateLocal, bNumbersPresent,
		bNumbersOutside;

	ClockMsg_SetLayout(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_Region(region, message)
	{
#define UPDATE_VAL(val,idx) (val) = get_arg_bool(message,(idx));
		UPDATE_VAL(bAnalogueClock,      1)
		UPDATE_VAL(bAnalogueClockLocal, 2)
		UPDATE_VAL(bDigitalClockUTC,    3)
		UPDATE_VAL(bDigitalClockLocal,  4)
		UPDATE_VAL(bDate,               5)
		UPDATE_VAL(bDateLocal,          6)
		//Skip Landscape parameter, this is now global, still transmitted by driver for legacy devices
		UPDATE_VAL(bNumbersPresent,8);
		UPDATE_VAL(bNumbersOutside,9);
#undef UPDATE_VAL
	}
	void Dump()
	{
		fprintf(stderr, "Analogue %d, AnalogueLocal %d, DigitalUTC %d, DigitalLocal %d, Date %d, DateLocal %d, NumbersPresent %d, NumbersOutside %d\n", bAnalogueClock, bAnalogueClockLocal, bDigitalClockUTC, bDigitalClockLocal, bDate, bDateLocal, bNumbersPresent, bNumbersOutside);
	}
};

class ClockMsg_SetLocation : public ClockMsg_Region
{
public:
	double x, y, width, height;
	ClockMsg_SetLocation(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_Region(region, message)
	{
#define UPDATE_VAL(val,idx) (val) = get_arg_f(message,(idx));
		UPDATE_VAL(x,		1)
		UPDATE_VAL(y,		2)
		UPDATE_VAL(width,	3)
		UPDATE_VAL(height,	4)
#undef UPDATE_VAL
	}
};

class ClockMsg_SetFontSizeZones : public ClockMsg_Region
{
public:
	std::shared_ptr<std::vector<std::vector<std::string>>> pData = std::make_shared<std::vector<std::vector<std::string>>>();
	ClockMsg_SetFontSizeZones(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_Region(region, message)
	{
		auto &data = *pData;
		std::string region_prefix = "R" + std::to_string(regionIndex);
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
};

class ClockMsg_SetRegionCount : public ClockMsg
{
public:
	int iCount;
	ClockMsg_SetRegionCount(const std::string & message)
	{
		iCount = get_arg_int(message,1);
	}
};

class ClockMsg_SetProfile : public ClockMsg
{
public:
	std::string sProfile;
	ClockMsg_SetProfile(const std::string & message)
	{
		sProfile = get_arg(message,1);
	}
};

class ClockMsg_RowCol_Generic : public ClockMsg_Region
{
protected:
	ClockMsg_RowCol_Generic(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_Region(region, message)
	{
		m_iRow = get_arg_int(message,1);
		m_iCol = get_arg_int(message,2);
	}
	int m_iRow, m_iCol;
};

class ClockMsg_SetSize : public ClockMsg_RowCol_Generic
{
public:
	const int &iRows, &iCols;
	ClockMsg_SetSize(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_RowCol_Generic(region, message),iRows(m_iRow),iCols(m_iCol)
	{
	}
};

class ClockMsg_SetRow : public ClockMsg_RowCol_Generic
{
public:
	const int &iRow, &iCols;
	ClockMsg_SetRow(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_RowCol_Generic(region, message),iRow(m_iRow),iCols(m_iCol)
	{
	}
};


class ClockMsg_RowCol : public ClockMsg_RowCol_Generic
{
public:
	const int &iRow, &iCol;
	ClockMsg_RowCol(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_RowCol_Generic(region, message),iRow(m_iRow),iCol(m_iCol)
	{
	}
};

class ClockMsg_SetIndicator: public ClockMsg_RowCol
{
public:
	TallyColour colFg, colBg;
	std::string sText;
protected:
	ClockMsg_SetIndicator(const std::shared_ptr<int> &region, const std::string & message, int textIndex)
		:ClockMsg_RowCol(region, message)
	{
		colFg = TallyColour(get_arg(message, 3));
		colBg = TallyColour(get_arg(message, 4));
		sText = get_arg(message, textIndex, false);
	}
};

class ClockMsg_SetCountdown: public ClockMsg_SetIndicator
{
public:
	struct timeval target;
	bool bHasFlashLimit;
	long long iFlashLimit = 0;
	ClockMsg_SetCountdown(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_SetIndicator(region, message, 8)
	{
		colFg = TallyColour(get_arg(message, 3));
		colBg = TallyColour(get_arg(message, 4));
		target.tv_sec = get_arg_ll(message, 5);
		target.tv_usec = get_arg_l(message, 6);
		auto flash = get_arg_pll(message, 7);
		if((bHasFlashLimit = (bool)flash))
			iFlashLimit = *flash;
	}
};


class ClockMsg_SetTally : public ClockMsg_SetIndicator
{
public:
	ClockMsg_SetTally(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_SetIndicator(region, message, 5)
	{
	}
};

class ClockMsg_SetLabel : public ClockMsg_RowCol
{
public:
	std::string sText;
	ClockMsg_SetLabel(const std::shared_ptr<int> &region, const std::string & message)
		:ClockMsg_RowCol(region, message)
	{
		sText = get_arg(message, 3, false);
	}
};

std::shared_ptr<ClockMsg> ClockMsg_Parse(const std::string &message)
{
	std::string cmd;
	auto region = ClockMsg::ParseCmd(message, cmd);
	if(cmd == "SETGLOBAL")
		return std::make_shared<ClockMsg_SetGlobal>(message);
	if(cmd == "SETREGIONCOUNT")
		return std::make_shared<ClockMsg_SetRegionCount>(message);
	if(cmd == "SETPROFILE")
		return std::make_shared<ClockMsg_SetProfile>(message);
	if(cmd == "SETLOCATION")
		return std::make_shared<ClockMsg_SetLocation>(region, message);
	if(cmd == "SETSIZE")
		return std::make_shared<ClockMsg_SetSize>(region, message);
	if(cmd == "SETROW")
		return std::make_shared<ClockMsg_SetRow>(region, message);
	if(cmd == "SETTALLY")
		return std::make_shared<ClockMsg_SetTally>(region, message);
	if(cmd == "SETCOUNTDOWN")
		return std::make_shared<ClockMsg_SetCountdown>(region, message);
	if(cmd == "SETLABEL")
		return std::make_shared<ClockMsg_SetLabel>(region, message);
	if(cmd == "SETLAYOUT")
		return std::make_shared<ClockMsg_SetLayout>(region, message);
	if(cmd == "SETFONTSIZEZONES")
		return std::make_shared<ClockMsg_SetFontSizeZones>(region, message);
	return std::shared_ptr<ClockMsg>();

}


#endif
