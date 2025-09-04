#ifndef __PICLOCK_TALLY_H_INCLUDED
#define __PICLOCK_TALLY_H_INCLUDED

#include <memory>
#include "tallycolour.h"
#include "piclock_messages.h"
#include "tzinfo.h"

class TallyState
{
public:
	virtual std::shared_ptr<TallyColour> FG(const sys_clock_data &now) const = 0;
	virtual std::shared_ptr<TallyColour> BG(const sys_clock_data &now) const = 0;
	virtual std::shared_ptr<std::string> Text(const sys_clock_data &now) const = 0;
	virtual std::shared_ptr<std::string> Label(const sys_clock_data &now) const = 0;
	virtual std::shared_ptr<TallyState>  SetLabel(const std::string &lbl) const = 0;
	virtual bool IsDigitalClock() const = 0;
	virtual bool Equals(std::shared_ptr<TallyState> other) const = 0;
};

class SimpleTallyState  :  public TallyState
{
public:
	std::shared_ptr<TallyColour> FG(const sys_clock_data &now) const override
	{
		return m_FG;
	}
	std::shared_ptr<TallyColour> BG(const sys_clock_data &now) const override
	{
		return m_BG;
	}
	std::shared_ptr<std::string> Text(const sys_clock_data &now) const override
	{
		return m_text;
	}
	std::shared_ptr<std::string> Label(const sys_clock_data &now) const override
	{
		return m_label;
	}
	std::shared_ptr<TallyState> SetLabel(const std::string & label) const override;
	bool IsDigitalClock() const override
	{
		return false;
	}

	bool Equals(std::shared_ptr<TallyState> other) const override;
	SimpleTallyState(const std::shared_ptr<ClockMsg_SetTally> &pMsg, const std::shared_ptr<TallyState> & pOld);

	SimpleTallyState(const ClockMsg_SetTally &msg, const std::shared_ptr<TallyState> & pOld);

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text);
	SimpleTallyState(const TallyColour &fg, const TallyColour &bg, const std::string &_text);
	SimpleTallyState(const std::string &fg, const std::string &bg, std::shared_ptr<std::string> _text);
	SimpleTallyState(const TallyColour &fg, const TallyColour &bg, std::shared_ptr<std::string> _text);

	SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old);
	SimpleTallyState(const TallyColour &fg, const TallyColour &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old);
protected:
	std::shared_ptr<TallyColour> m_FG, m_BG;
	std::shared_ptr<std::string> m_text, m_label;
};


#endif
