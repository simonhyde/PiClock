#ifndef __PICLOCK_TALLYDISPLAYS_H_INCLUDED
#define __PICLOCK_TALLYDISPLAYS_H_INCLUDED

#include <map>
#include <string>
#include <memory>
#include "tally.h"

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
#endif