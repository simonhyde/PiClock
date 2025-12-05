#include "countdownclock.h"

bool CountdownClock::Invert(const sys_clock_data &now) const
{
	if(m_pFlashLimit)
	{
		auto left = TimeLeft(now, m_target);
		if((std::chrono::ceil<std::chrono::seconds>(left).count() <= *m_pFlashLimit)
			&& (std::abs(left.count() % 1000) > 500))
		return true;
	}
	return false;
}
std::shared_ptr<TallyColour> CountdownClock::FG(const sys_clock_data &now) const
{
	if(Invert(now))
		return SimpleTallyState::BG(now);
	return SimpleTallyState::FG(now);
}
std::shared_ptr<TallyColour> CountdownClock::BG(const sys_clock_data &now) const
{
	if(Invert(now))
		return SimpleTallyState::FG(now);
	return SimpleTallyState::BG(now);
}
std::shared_ptr<std::string> CountdownClock::Text(const sys_clock_data &now) const
{
	time_t secs = SecsLeft(now,m_target);
	char buf[256];
	char negChar = (secs < 0)? '-': ' ';
	secs = std::abs(secs);
	if(m_daysMode == 1 || (m_daysMode == 2 && secs > 86400))
	{
		snprintf(buf, sizeof(buf) - 1, "%c%02lld:%02lld:%02lld:%02lld",
			negChar, secs/86400, (secs/3600)%24, (secs/60)%60, secs %60);
	}
	else
	{
		snprintf(buf, sizeof(buf) - 1, "%c%02lld:%02lld:%02lld",
			negChar, secs/3600, (secs/60)%60, secs %60);
	}
	return std::make_shared<std::string>(buf);
}
std::shared_ptr<TallyState> CountdownClock::SetLabel(const std::string & label) const
{
	return std::shared_ptr<TallyState>();
}
std::shared_ptr<std::string> CountdownClock::Label(const sys_clock_data &now) const
{
	return SimpleTallyState::Text(now);
}
bool CountdownClock::IsDigitalClock() const
{
	return true;
}

bool CountdownClock::Equals(std::shared_ptr<TallyState> other) const
{
	if(!other)
		return false;
	if(typeid(*other) != typeid(*this))
		return false;
	auto derived = dynamic_cast<CountdownClock *>(other.get());
	return derived != NULL 
		&& derived->m_FG->Equals(*m_FG)
		&& derived->m_BG->Equals(*m_BG)
		&& derived->m_target == m_target
		&& *(derived->m_text) == *(m_text)
		&& ((!m_pFlashLimit && !derived->m_pFlashLimit.get())
			|| (m_pFlashLimit && derived->m_pFlashLimit
				&& *m_pFlashLimit == *(derived->m_pFlashLimit)));
}

CountdownClock::CountdownClock(const std::shared_ptr<ClockMsg_SetCountdown> &pMsg)
	:CountdownClock(*pMsg)
{}

CountdownClock::CountdownClock(const ClockMsg_SetCountdown &msg)
	:CountdownClock(msg.colFg, msg.colBg, msg.sText, msg.target, msg.bHasFlashLimit? std::make_shared<long long>(msg.iFlashLimit) : std::shared_ptr<long long>(), msg.daysMode)
{}

CountdownClock::CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const sys_clock_data & _target, std::shared_ptr<long long> pflash, int daysMode)
	:SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash), m_daysMode(daysMode)
{}
CountdownClock::CountdownClock(const TallyColour & fg, const TallyColour &bg, const std::string &_label, const sys_clock_data &_target, std::shared_ptr<long long> pflash, int daysMode)
	:SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash), m_daysMode(daysMode)
{}
std::chrono::milliseconds CountdownClock::TimeLeft(const sys_clock_data & now, const sys_clock_data & target)
{
	return std::chrono::ceil<std::chrono::milliseconds>(target - now);
}
time_t CountdownClock::SecsLeft(const sys_clock_data & now, const sys_clock_data & target)
{
	return std::chrono::ceil<std::chrono::seconds>(target - now).count();
}
