#ifndef __PICLOCK_COUNTDOWNCLOCK_H_INCLUDED
#define __PICLOCK_COUNTDOWNCLOCK_H_INCLUDED
#include "tally.h"

class CountdownClock : public SimpleTallyState
{
public:
	std::shared_ptr<TallyColour> FG(const struct timeval &curTime) const override;
	std::shared_ptr<TallyColour> BG(const struct timeval &curTime) const override;
	std::shared_ptr<std::string> Text(const struct timeval &curTime) const override;
	std::shared_ptr<TallyState> SetLabel(const std::string & label) const override;
	std::shared_ptr<std::string> Label(const struct timeval &curTime) const override;
	bool IsDigitalClock() const override;
	bool Equals(std::shared_ptr<TallyState> other) const override;
	CountdownClock(const std::shared_ptr<ClockMsg_SetCountdown> &pMsg);

	CountdownClock(const ClockMsg_SetCountdown &msg);
	CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const struct timeval & _target, std::shared_ptr<long long> pflash);
	CountdownClock(const TallyColour &fg, const TallyColour &bg, const std::string &_label, const struct timeval &_target, std::shared_ptr<long long> pflash);

private:
	bool Invert(const struct timeval &curTime) const;
	struct timeval m_target;
	std::shared_ptr<long long> m_pFlashLimit;
	static struct timeval TimeLeft(const timeval & current, const timeval & target);
	static time_t SecsLeft(const timeval & current, const timeval & target);
};

#endif