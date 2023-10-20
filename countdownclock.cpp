#include "countdownclock.h"

bool CountdownClock::Invert(const struct timeval &curTime) const
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
std::shared_ptr<TallyColour> CountdownClock::FG(const struct timeval &curTime) const
{
	if(Invert(curTime))
		return SimpleTallyState::BG(curTime);
	return SimpleTallyState::FG(curTime);
}
std::shared_ptr<TallyColour> CountdownClock::BG(const struct timeval &curTime) const
{
	if(Invert(curTime))
		return SimpleTallyState::FG(curTime);
	return SimpleTallyState::BG(curTime);
}
std::shared_ptr<std::string> CountdownClock::Text(const struct timeval &curTime) const
{
	time_t secs = SecsLeft(curTime,m_target);
	char buf[256];
	char negChar = (secs < 0)? '-': ' ';
	secs = std::abs(secs);
	snprintf(buf, sizeof(buf) - 1, "%c%02ld:%02ld:%02ld",
			negChar, secs/3600, (secs/60)%60, secs %60);
	return std::make_shared<std::string>(buf);
}
std::shared_ptr<TallyState> CountdownClock::SetLabel(const std::string & label) const
{
	return std::shared_ptr<TallyState>();
}
std::shared_ptr<std::string> CountdownClock::Label(const struct timeval &curTime) const
{
	return SimpleTallyState::Text(curTime);
}
bool CountdownClock::IsMonoSpaced() const
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
		&& derived->m_target.tv_sec == m_target.tv_sec
		&& derived->m_target.tv_usec == m_target.tv_usec
		&& *(derived->m_text) == *(m_text)
		&& ((!m_pFlashLimit && !derived->m_pFlashLimit.get())
			|| (m_pFlashLimit && derived->m_pFlashLimit
				&& *m_pFlashLimit == *(derived->m_pFlashLimit)));
}

CountdownClock::CountdownClock(const std::shared_ptr<ClockMsg_SetCountdown> &pMsg)
	:CountdownClock(*pMsg)
{}

CountdownClock::CountdownClock(const ClockMsg_SetCountdown &msg)
	:CountdownClock(msg.colFg, msg.colBg, msg.sText, msg.target, msg.bHasFlashLimit? std::make_shared<long long>(msg.iFlashLimit) : std::shared_ptr<long long>())
{}

CountdownClock::CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const struct timeval & _target, std::shared_ptr<long long> pflash)
	:SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
{}
CountdownClock::CountdownClock(const TallyColour & fg, const TallyColour &bg, const std::string &_label, const struct timeval &_target, std::shared_ptr<long long> pflash)
	:SimpleTallyState(fg,bg, _label), m_target(_target), m_pFlashLimit(pflash)
{}
struct timeval CountdownClock::TimeLeft(const timeval & current, const timeval & target)
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
time_t CountdownClock::SecsLeft(const timeval & current, const timeval & target)
{
	auto left = TimeLeft(current,target);
	//We want equivalent of ceil()
	auto ret = left.tv_sec;
	if(left.tv_usec > 0)
		ret += 1;
	return ret;
}
