#ifndef __PICLOCK_COUNTDOWNCLOCK_H_INCLUDED
#define __PICLOCK_COUNTDOWNCLOCK_H_INCLUDED
#include "tally.h"

class CountdownClock : public SimpleTallyState
{
public:
	std::shared_ptr<TallyColour> FG(const sys_clock_data &now) const override;
	std::shared_ptr<TallyColour> BG(const sys_clock_data &now) const override;
	std::shared_ptr<std::string> Text(const sys_clock_data &now) const override;
	std::shared_ptr<TallyState> SetLabel(const std::string & label) const override;
	std::shared_ptr<std::string> Label(const sys_clock_data &now) const override;
	bool IsDigitalClock() const override;
	bool Equals(std::shared_ptr<TallyState> other) const override;
	CountdownClock(const std::shared_ptr<ClockMsg_SetCountdown> &pMsg);

	CountdownClock(const ClockMsg_SetCountdown &msg);
	CountdownClock(const std::string &fg, const std::string &bg, const std::string &_label, const sys_clock_data & _target, std::shared_ptr<long long> pflash, int daysMode);
	CountdownClock(const TallyColour &fg, const TallyColour &bg, const std::string &_label, const sys_clock_data &_target, std::shared_ptr<long long> pflash, int daysMode);

private:
	bool Invert(const sys_clock_data &now) const;
	sys_clock_data m_target;
	std::shared_ptr<long long> m_pFlashLimit;
	int m_daysMode = 0;
	static std::chrono::milliseconds TimeLeft(const sys_clock_data & now, const sys_clock_data & target);
	static time_t SecsLeft(const sys_clock_data & now, const sys_clock_data & target);
};

#endif
